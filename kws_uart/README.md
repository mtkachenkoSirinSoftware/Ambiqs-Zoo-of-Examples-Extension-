# kws_uart — host WAV → framed UART → 1 s RAM → heliaRT on Apollo510

Always-on KWS on a **1 s clip the host sends over UART**. Labels print on
**SWO** (`nsx view`). PCM uses `AM_BSP_UART_PRINT_INST` at 921600 8N1.

RX is **main-loop FIFO poll** (`am_hal_uart_fifo_read`). Do not enable UART
RX-DMA or `am_hal_uart_interrupt_stream_service` on this path — that combination
hung the EVB (stats froze on the first host byte).

The identity WAV is GATE-4 golden `synthetic` (`host/synthetic.wav`). Host
LiteRT on that PCM predicts **`go`**. MCU `pred=` is comparable only on this
PCM — not PDM, not a spoken GSC file unless you send that file's samples.

## Hardware

Apollo510 EVB + J-Link (flash + SWO) + USB-UART to the COM/PRINT port.

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
python3 host/stream_wav.py host/synthetic.wav --port /dev/ttyUSB0 --baud 921600
```

Host identity (no board):

```bash
make embed      # synthetic.wav + embedded .tflite
make test-host
```

`open_pcm_serial` holds **DTR/RTS low** before `open()`. pyserial's default DTR
pulse resets CDC-ACM / J-Link-OB mid-stream.

If `nsx configure` fails with `file(WRITE /dev/stdout)` and no TTY:

```bash
script -q -e -c 'nsx configure --app-dir .' /tmp/nsx-configure-uart.log
```

## Expected output (SWO)

```
kws_uart runtime=helia-rt arena_used=<N>
uart poll-fifo then 1s RAM infer @ 921600; PCM on PRINT UART; labels on SWO
model sha256=ae08012b5a5d  (host LiteRT on identical PCM: host/expected.json)
reset
uart_clip samples=16000 dropped=0 poll=<n> bytes=<n>
pred=go score=0.980 fired=0|1 invoke_cycles=<DWT>
  invoke_cycles is DWT CYCCNT of this binary's Invoke, not hpx profile
```

| Line | What it is |
|---|---|
| `pred=go` | last raw argmax on **this** PCM; must match `host/expected.json` |
| `fired=` | recognizer (smooth / threshold / debounce); not GATE 3 |
| `invoke_cycles=` | DWT of **this** `Invoke`, not `hpx profile` |
| `dropped=` | samples past 16000 discarded at ingest |

`score=` on the MCU is softmax of int8 logits; the **name** `go` is the check.

Do not `tee` the audio UART for labels. The MCU does not TX `T_PREDICTION`.

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
| `host/stream_wav.py` | WAV → frames; DTR/RTS low |
| `host/expected.json` | host LiteRT identity for `synthetic.wav` |
| `nsx.yml` / `nsx.lock` | helia-rt 1.19.0 + ns-cmsis-nn 7.31.0 pins |

## Model

`assets/depgraph_r060_kd_int8.tflite` SHA256
`ae08012b5a5dd1fd59673bba3d4b1d1c43d7cd1f410537824d7eb6c2edb40fb7`.
helia-rt `cfab1523` (v1.19.0), ns-cmsis-nn `b386770a` (v7.31.0).

A Demo A student is usually **Class B**: see [`../bring_your_model.md`](../bring_your_model.md).
After a swap, `host/expected.json` is stale — UART vs LiteRT uses **that** SHA.

## Linked image (host, not latency)

`nsx build` produces `build/apollo510_evb/kws_uart` (ELF) and `.bin`. One
successful link on this tree:

| Segment | Bytes |
|---|---|
| `.text` | 292760 |
| `.data` | 2620 |
| `.bss` | 505280 |
| `.bin` | 289 KiB |

That is the **linked image**, not Apollo510 latency. `.text` is smaller than
`kws_clip` because the 1 s PCM is not in flash — it arrives over UART into RAM.
`pred=` on SWO still needs `nsx flash` + `nsx view` + `stream_wav.py`.
