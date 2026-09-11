# kws_uart — host WAV → framed UART → 1 s RAM → heliaRT on Apollo510

Always-on KWS on a **1 s clip the host sends over UART**. Labels print on
**SWO** (`nsx view`). PCM uses `AM_BSP_UART_PRINT_INST` at 921600 8N1.

RX is **main-loop FIFO poll** (`am_hal_uart_fifo_read`). Do not enable UART
RX-DMA or `am_hal_uart_interrupt_stream_service` on this path — that combination
hung the EVB (stats froze on the first host byte).

The identity WAV is GATE-4 golden `synthetic` (`host/synthetic.wav`). Host
LiteRT on that PCM predicts **`go`**. MCU `pred=` is comparable only on this
PCM — not PDM, not a spoken GSC file unless you send that file's samples.
Names are **tfds** order (`go` = index 1), not `kws_infer` MLPerf (`go` =
index 11). See [`../assets/LABELS.md`](../assets/LABELS.md).

## Hardware

Apollo510 EVB + J-Link (flash + SWO) + USB-UART to the COM/PRINT port.

J-Link USB (`1366:1024`) is **not** the NSX CDC. Optional CDC / USB RPC
needs the EVB **device** USB as well; `lsusb` must show `0xCafe`/`0x4011`
before `stream_wav.py` / `rpc_infer.py` can open `/dev/ttyACM*`. DTR=True
on that port (usb_serial). Without that cable the firmware still prints
its identity on SWO; the host path is not a host measurement of CDC.

## Build

```bash
cd ambiq_kws_examples/kws_uart
nsx lock      --app-dir .
nsx configure --app-dir .
nsx build     --app-dir .
nsx flash     --app-dir .
nsx view      --app-dir .
```

In a second terminal, after SWO shows `uart poll-fifo`:

```bash
python3 host/stream_wav.py --list-ports
python3 host/stream_wav.py host/synthetic.wav --port /dev/ttyUSB0 --baud 921600
```

`--list-ports` classifies **J-Link VCP** vs **NSX CDC** (`0xCafe`/`0x4011`,
same as neuralspotx `usb_serial` — pick the right port). `--dtr auto`
(default) asserts DTR on the NSX CDC and holds it **low** on J-Link / PRINT
UART. Do not use pyserial's default DTR pulse on the J-Link-OB.

Optional **nsx-usb CDC** (same `0xA51C` protocol, labels still on SWO). This
`nsx` has no CMake-arg passthrough:

```bash
script -q -e -c 'cmake -DKWS_UART_USB_CDC=ON build/apollo510_evb' /tmp/cmake-uart-cdc.log
nsx build --app-dir .
nsx flash --app-dir .
# plug the EVB **device** USB as well as J-Link; DTR=True
python3 host/stream_wav.py host/synthetic.wav --port /dev/ttyACM0 --dtr high
```

Optional **USB RPC INFER** (16000 int16 → `KwsApp`). This is **not** the
neuralspotx `usb_rpc` 5-class toy:

```bash
script -q -e -c 'cmake -DKWS_UART_USB_CDC=OFF -DKWS_UART_USB_RPC=ON build/apollo510_evb' /tmp/cmake-uart-rpc.log
nsx build --app-dir .
nsx flash --app-dir .
python3 host/rpc_infer.py host/synthetic.wav
# SWO pred=go is the identity; USB class_id=1 label=go is the same argmax
```

Host identity (no board):

```bash
make embed      # synthetic.wav + embedded .tflite
make test-host
```

`open_pcm_serial` uses `--dtr`. J-Link-OB / PRINT UART needs **low** or the
MCU resets. NSX CDC (`usb_serial`) needs **high** or the device ignores RX.

If `nsx configure` fails with `file(WRITE /dev/stdout)` and no TTY:

```bash
script -q -e -c 'nsx configure --app-dir .' /tmp/nsx-configure-uart.log
```

## Expected output (SWO)

```
kws_uart runtime=helia-rt arena_used=<N>
perf_mode=LOW cpu_hz=96000000
uart poll-fifo then 1s RAM infer @ 921600; PCM on PRINT UART; labels on SWO
model sha256=ae08012b5a5d  (host LiteRT on identical PCM: host/expected.json)
labels=tfds index 1=go (not kws_infer MLPerf index 11=go); assets/LABELS.md
reset
uart_clip samples=16000 dropped=0 poll=<n> bytes=<n>
pred=go score=0.980 fired=0|1 invoke_cycles=<DWT> invoke_us=<CYCCNT/cpu_hz>
  invoke_us = CYCCNT / cpu_hz (see perf_mode=); not hpx profile
```

| Line | What it is |
|---|---|
| `pred=go` | last raw argmax on **this** PCM; must match `host/expected.json` (tfds index 1) |
| `fired=` | recognizer (smooth / threshold / debounce); not GATE 3 |
| `invoke_cycles=` / `invoke_us=` | DWT of **this** `Invoke` / CYCCNT÷`cpu_hz`; not `hpx profile` |
| `dropped=` | samples past 16000 discarded at ingest |

CDC / RPC banners replace the PRINT-UART line:

```
uart cdc nsx-usb 0xA51C; DTR=True; VID/PID 0xCafe/0x4011; PCM on CDC; labels on SWO
```

```
usb rpc INFER=16000 int16 KwsApp; DTR=True; VID/PID 0xCafe/0x4011; labels on SWO
  this INFER is kws_core; neuralspotx usb_rpc INFER is a 5-class toy on another image
usb rpc ready (wait for host DTR)
```

`score=` on the MCU is softmax of int8 logits; the **name** `go` is the check.

Do not `tee` the audio UART for labels. The MCU does not TX `T_PREDICTION`.

SWO `pred=` is **this** firmware (`kws_uart` / `kws_clip`). neuralspotx
`usb_rpc` `INFER` is a **different image** whose handler sums bytes into five
toy classes (`idle`/`walk`/`run`/`gesture`/`unknown`). Do not compare that
toy class to `pred=go`. The optional `-DKWS_UART_USB_RPC=ON` build of
**this** tree runs `KwsApp` on 16000 int16 and still prints `pred=` on SWO.

## Framing

See [PROTOCOL.md](PROTOCOL.md). Magic `0xA51C`, hop = 320 samples (20 ms),
50 hops = 1 s. Host bursts `RESET + START + AUDIO_BLOCK×50 + END`. Firmware
stores RAM first, then plays through the same `FlashClipPlayer` as `kws_clip`.

## Key files

| File | Purpose |
|---|---|
| `src/main.cc` | `nsx_system_init`, poll UART, SWO `pred=` |
| `src/apollo510_uart.cc` | FIFO poll `AudioSource`; NVIC UART IRQ off |
| `protocol/` | MAGIC framing + CRC-16/CCITT-FALSE |
| `audio/` | 1 s RAM clip + player |
| `host/stream_wav.py` | WAV → frames; `--list-ports`; `--dtr {auto,low,high}` |
| `host/rpc_infer.py` | USB RPC INFER of 16000 int16; **not** the usb_rpc toy |
| `rpc/` | nanopb `NsxRpcMessage`; input max 32000 B |
| `host/expected.json` | host LiteRT identity for `synthetic.wav` |
| `nsx.yml` / `nsx.lock` | helia-rt 1.19.0 + ns-cmsis-nn 7.31.0 pins |

## Model

`assets/depgraph_r060_kd_int8.tflite` SHA256
`ae08012b5a5dd1fd59673bba3d4b1d1c43d7cd1f410537824d7eb6c2edb40fb7`.
helia-rt `cfab1523` (v1.19.0), ns-cmsis-nn `b386770a` (v7.31.0).

A Demo A student is usually **Class B**: see [`../bring_your_model.md`](../bring_your_model.md).
After a swap, `host/expected.json` is stale — UART vs LiteRT uses **that** SHA.

## Linked image (host, not latency)

`nsx build` produces `build/apollo510_evb/kws_uart` (ELF) and `.bin`. Linked
sizes on this tree (bytes), **not** Apollo510 latency:

| Image | `.text` | `.data` | `.bss` |
|---|---|---|---|
| PRINT UART (default) | 292760 | 2620 | 505280 |
| `-DKWS_UART_USB_CDC=ON` | 317544 | 3260 | 511360 |
| `-DKWS_UART_USB_RPC=ON` | 325008 | 3260 | 607672 |

`.text` is smaller than `kws_clip` because the 1 s PCM is not in flash — it
arrives over UART/CDC/RPC into RAM. `pred=` on SWO still needs `nsx flash`
+ `nsx view` + `stream_wav.py` or `rpc_infer.py`.
