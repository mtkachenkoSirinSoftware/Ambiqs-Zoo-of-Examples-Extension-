# kws_uart

Send a 1 s WAV from the host over UART; the MCU stores it in RAM and runs
the same graph as `kws_clip`. Labels print on **SWO** (`nsx view`).

PCM uses `AM_BSP_UART_PRINT_INST` at **921600 8N1**. RX is main-loop FIFO
poll (`am_hal_uart_fifo_read`). Do not enable UART RX-DMA or
`am_hal_uart_interrupt_stream_service` on this path — that hung the EVB.

Identity WAV: `host/synthetic.wav` (same PCM as the flash clip). Host
interpreter on that buffer predicts **`go`** (tfds index 1).
[`../assets/LABELS.md`](../assets/LABELS.md).

## Hardware

Apollo510 EVB + J-Link (flash + SWO) + USB-UART to the COM/PRINT port.

J-Link USB (`1366:1024`) is not the NSX CDC. Optional CDC / RPC needs the
EVB **device** USB as well (`lsusb` → `0xCafe`/`0x4011`).

## Build & Run

```bash
cd kws_uart
nsx lock      --app-dir .
nsx configure --app-dir .
nsx build     --app-dir .
nsx flash     --app-dir .
nsx view      --app-dir .
```

Second terminal, after SWO shows `uart poll-fifo`:

```bash
python3 host/stream_wav.py --list-ports
python3 host/stream_wav.py host/synthetic.wav --port /dev/ttyUSB0 --baud 921600
```

`--list-ports` classifies **J-Link VCP** vs **NSX CDC** (`0xCafe`/`0x4011`,
same as `usb_serial`). `--dtr auto` (default): **high** on CDC, **low** on
J-Link / PRINT UART. pyserial’s default DTR pulse on J-Link-OB resets the MCU.

Host check (no board): `make test-host`

## Expected output

```
kws_uart runtime=helia-rt arena_used=<N>
perf_mode=LOW cpu_hz=96000000
uart poll-fifo then 1s RAM infer @ 921600; PCM on PRINT UART; labels on SWO
model sha256=ae08012b5a5d
labels=tfds index 1=go (not kws_infer MLPerf index 11=go); assets/LABELS.md
reset
uart_clip samples=16000 dropped=0 poll=<n> bytes=<n>
pred=go score=0.980 fired=0|1 invoke_cycles=<DWT> invoke_us=<us>
```

The MCU does not TX `T_PREDICTION` on the PCM UART. Do not `tee` that port
for labels.

## Optional: USB CDC (`usb_serial`)

Same `0xA51C` frames, labels still on SWO. This `nsx` has no CMake-arg
passthrough:

```bash
script -q -e -c 'cmake -DKWS_UART_USB_CDC=ON build/apollo510_evb' /tmp/cmake-uart-cdc.log
nsx build --app-dir . && nsx flash --app-dir .
# plug device USB; DTR=True
python3 host/stream_wav.py host/synthetic.wav --port /dev/ttyACM0 --dtr high
```

## Optional: USB RPC (`usb_rpc` wire)

Same 4-byte LE length + nanopb `NsxRpcMessage` as
`neuralspotx/examples/usb_rpc`. **INFER** here is 16000 int16 through
`KwsApp` (tfds labels), not the five-class stub in their dispatch.

```bash
script -q -e -c 'cmake -DKWS_UART_USB_CDC=OFF -DKWS_UART_USB_RPC=ON build/apollo510_evb' /tmp/cmake-uart-rpc.log
nsx build --app-dir . && nsx flash --app-dir .
python3 host/rpc_infer.py host/synthetic.wav
```

SWO `pred=go` is the identity; USB `class_id=1` / `label=go` is the same
argmax. Do not compare stock `usb_rpc` (`idle`/`walk`/…) to `pred=go`.

## Framing

See [PROTOCOL.md](PROTOCOL.md). Magic `0xA51C`, hop 320 samples, 50 hops = 1 s.
Host bursts `RESET + START + AUDIO_BLOCK×50 + END`. Firmware stores RAM, then
plays the clip the same way as `kws_clip`.

## Key files

| File | Purpose |
|---|---|
| `src/main.cc` | init, poll, SWO |
| `src/apollo510_uart.cc` | FIFO-poll `AudioSource`; UART IRQ off |
| `protocol/` | MAGIC + CRC-16/CCITT-FALSE |
| `host/stream_wav.py` | WAV → frames; `--list-ports`; `--dtr` |
| `host/rpc_infer.py` | USB RPC INFER of 16000 int16 |
| `rpc/` | nanopb; `INFER.input` max 32000 B |
| `host/expected.json` | interpreter identity for `synthetic.wav` |

## Model

Same shipping INT8 as `kws_clip`. Swap: [`../bring_your_model.md`](../bring_your_model.md).

If `nsx configure` has no TTY:

```bash
script -q -e -c 'nsx configure --app-dir .' /tmp/nsx-configure-uart.log
```
