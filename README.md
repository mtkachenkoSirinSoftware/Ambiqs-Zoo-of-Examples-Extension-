# Keyword spotting examples (heliaRT)

Three NSX apps plus a shared library. Same INT8 DS-CNN, same 1 s window,
three audio sources. They sit between `neuralspotx/examples/kws_infer`
(dummy tensor, no MFCC) and `audio_capture` (PDM peak, no network).

| Directory | Modules (app-declared) | What it shows |
|---|---|---|
| `kws_clip` | `nsx-power`, `nsx-pmu-armv8m`, `nsx-helia-rt` | 1 s flash clip → MFCC → Invoke; labels on SWO |
| `kws_uart` | `nsx-power`, `nsx-helia-rt` (+ USB optional) | Host WAV over PRINT UART (or CDC / RPC) |
| `kws_pdm` | `nsx-power`, `nsx-audio`, `nsx-helia-rt` | Live PDM MEMS on GPIO 50/51 |
| `kws_core/` | *(library)* | Frontend, helia runner, recognizer |

Default board: **Apollo510 EVB**. Pins in each `nsx.yml`: helia-rt **1.19.0**
(`cfab1523`), ns-cmsis-nn **7.31.0** (`b386770a`). That is not the 1.16.0 /
7.29.2 pair in the `kws_infer` README — mixing them is a different binary.

## Quick start

```bash
cd kws_clip
nsx lock && nsx configure && nsx build && nsx flash
nsx view --app-dir .
```

If `nsx configure` fails with `file(WRITE /dev/stdout)` and no TTY:

```bash
script -q -e -c 'nsx configure --app-dir .' /tmp/nsx-configure.log
```

SWO should print `pred=go` on the embedded `synthetic` clip (16 kHz mono
int16, 16000 samples). `invoke_cycles=` is DWT of **this** `Invoke`.

## Hardware

| Need | For |
|---|---|
| Apollo510 EVB + J-Link | every app (`nsx flash`, `nsx view`) |
| `arm-none-eabi-gcc` + `nsx` | configure / build / flash |
| USB-UART to COM/PRINT | `kws_uart` (default) |
| PDM MEMS on GPIO 50 CLK / 51 DATA | `kws_pdm` |

Bring up Suite `pdm_rtt_stream` / `pdm_fft` or NSX `audio_capture` before
debugging `kws_pdm`.

## `kws_uart`

```bash
cd kws_uart
nsx lock && nsx configure && nsx build && nsx flash
nsx view --app-dir .
# second terminal, after SWO prints "uart poll-fifo":
python3 host/stream_wav.py --list-ports
python3 host/stream_wav.py host/synthetic.wav --port /dev/ttyUSB0 --baud 921600
```

Expect `pred=go`, `dropped=0`. Labels stay on SWO — the MCU does not TX
`pred` on the PCM UART. Hold DTR **low** on J-Link VCP (otherwise the MCU
resets). Optional CDC / RPC: see [`kws_uart/README.md`](kws_uart/README.md).

## `kws_pdm`

```bash
cd kws_pdm
nsx lock && nsx configure && nsx build && nsx flash
nsx view --app-dir .
```

Talk near the mic: `Frame N  peak=` should be nonzero, then `live pred=`.
That line is the live microphone, not the flash/UART clip. If `peak=` stays
~0, rebuild the HAL backend with L/R swap:

```bash
script -q -e -c 'cmake -DKWS_PDM_USE_NSX_AUDIO=OFF -DKWS_PDM_LR_SWAP=1 build/apollo510_evb' /tmp/cmake-pdm-lr.log
nsx build --app-dir .
```

## Swap the model

```bash
python3 tools/embed_model.py path/to/model.tflite --check
# class A or B → embed into kws_*/model/generated and nsx build
# class C → stop (wrong I/O)
```

After class B, `host/expected.json` is for the **shipping** SHA. Compare MCU
vs interpreter on **that** file and **identical** PCM.
[`bring_your_model.md`](bring_your_model.md).

## Host tests (no EVB)

```bash
make test
make dist
```

## Notes

- Host milliseconds, MACs, and `.tflite` bytes are not Apollo510 latency.
- Flash-clip / UART `pred=` is not a PDM result. Same graph, different PCM.
- `kws_infer` dummy-input cycles and heliaPROFILER (`hpx profile`) are other
  firmware. Label strings use **tfds** order here (`go` = index 1); `kws_infer`
  uses MLPerf order (`go` = index 11). [`assets/LABELS.md`](assets/LABELS.md).

Overlay into `neuralspotx/examples/`: [`COVER.md`](COVER.md).
Optional CMake flags: [`EXTENSIONS.md`](EXTENSIONS.md).
