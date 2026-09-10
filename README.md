# Always-on KWS on Apollo510 (heliaRT)

Three NSX examples plus a shared library. Same INT8 DS-CNN, same 1 s PCM
window, three audio sources. This closes the gap between
`neuralspotx/examples/kws_infer` (dummy tensor, no MFCC) and `audio_capture`
(PDM peak, no network).

Contract: [`EXAMPLES_CONTRACT.md`](EXAMPLES_CONTRACT.md).
Cover letter: [`COVER.md`](COVER.md).
Bring-your-model: [`bring_your_model.md`](bring_your_model.md).
Tarball rules: [`PACKAGING.md`](PACKAGING.md).
What can still be added without Suite-Make phase 7: [`EXTENSIONS.md`](EXTENSIONS.md).

## Hardware / tools

| Need | For |
|---|---|
| Apollo510 EVB + J-Link | every example (`nsx flash`, `nsx view`) |
| ARM GCC (`arm-none-eabi-gcc`) + `nsx` | configure / build / flash |
| USB-UART to the COM/PRINT port | `kws_uart` only |
| PDM MEMS on GPIO 50/51 | `kws_pdm` only, after official `pdm_rtt_stream` |

Pins in `nsx.yml`: helia-rt **1.19.0** (`cfab1523`), ns-cmsis-nn **7.31.0**
(`b386770a`) — not the 1.16.0 / 7.29.2 pair in the `kws_infer` README.

If `nsx configure` fails with `file(WRITE /dev/stdout)` and no TTY:

```bash
script -q -e -c 'nsx configure --app-dir .' /tmp/nsx-configure.log
```

## 15 minutes — `kws_clip` (start here)

No USB-UART. No MEMS. Labels on SWO, same transport as `kws_infer`.

```bash
cd kws_clip
nsx lock      --app-dir .
nsx configure --app-dir .
nsx build     --app-dir .
nsx flash     --app-dir .
nsx view      --app-dir .
```

SWO should show `pred=go` on the embedded golden clip (`synthetic`, 16 kHz
mono int16, 16000 samples). Host LiteRT on that PCM is `go` — see
`kws_clip/host/expected.json`. That name is the check. `invoke_cycles=` is
DWT of **this** `Invoke`, not `hpx profile`. Linked `.bin` size is not latency.

Details: [`kws_clip/README.md`](kws_clip/README.md).

## Next — `kws_uart` (dataset WAV → same graph)

J-Link for flash + SWO. USB-UART for PCM only (921600 8N1). Do not enable
UART RX-DMA on this path.

```bash
cd kws_uart
nsx lock && nsx configure && nsx build && nsx flash
nsx view --app-dir .
# second terminal, after SWO prints "uart poll-fifo":
python3 host/stream_wav.py host/synthetic.wav --port /dev/ttyUSB0 --baud 921600
```

Expect SWO `pred=go`, `dropped=0`. Host script holds DTR/RTS **low** before
`open()` so CDC-ACM does not reset the MCU. The MCU does not TX labels on
UART. Protocol: [`kws_uart/PROTOCOL.md`](kws_uart/PROTOCOL.md).

## Then — `kws_pdm` (live MEMS)

Bring up AmbiqSuite `pdm_rtt_stream` / `pdm_fft` on GPIO 50/51 **first**.
Then flash `kws_pdm`. Done-when: nonzero `peak=` while talking, and some
`live pred=`. That line is not GATE 3 vs a GSC file.

If `peak=` stays ~0 with a live mic, rebuild with `-DKWS_PDM_LR_SWAP=1`.

## Bring your INT8 student

```bash
python3 tools/embed_model.py path/to/student_int8.tflite --check
# A or B → embed into kws_*/model/generated and nsx build
# C → stop
```

After Class B, `host/expected.json` is stale. Compare MCU vs LiteRT on **that**
SHA and **identical** PCM. Procedure: [`bring_your_model.md`](bring_your_model.md).

## Host checks (no board)

```bash
make test          # kws_core GoogleTest + clip/uart/pdm identity + --check
make dist          # submission tarball under dist/
```

## Honesty (do not print these as facts)

1. Host milliseconds, MACs, or `.tflite` bytes are not Apollo510 latency.
2. A UART or flash-clip `pred` is not a PDM-microphone result.
3. `hpx profile` is not this application's `main`.

## Layout

```
kws_core/    portable frontend + clip player + helia runner + recognizer
kws_clip/    1 s flash PCM
kws_uart/    host WAV, FIFO-poll UART
kws_pdm/     EVB PDM MEMS
assets/      shipping INT8 .tflite + MODEL.txt
tools/       embed_model.py, embed_clip.py, pack_tarball.py
```

Research trees (train/prune notebooks, dual firmware demos, SNR, ALSA host-mic)
are not in this drop.
