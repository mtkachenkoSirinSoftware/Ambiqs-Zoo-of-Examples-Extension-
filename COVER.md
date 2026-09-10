# Cover letter — always-on KWS examples for neuralspotx

This drop closes the gap between two existing NSX examples:

| Example | What it does today | What it does not |
|---|---|---|
| `kws_infer` | one dummy-input heliaRT Invoke, SWO, PMU | no PCM, no MFCC, not a dataset clip |
| `audio_capture` | PDM peak on GPIO 50/51 | no DS-CNN |

Three thin apps share one portable library (`kws_core`) and one shipping INT8
graph. Same frontend, same 1 s window, three PCM origins:

| App | PCM origin | Labels |
|---|---|---|
| `kws_clip` | 1 s flash clip (golden `synthetic`) | SWO (`nsx view`) |
| `kws_uart` | host WAV, framed, FIFO-poll UART | SWO (`nsx view`) |
| `kws_pdm` | EVB PDM MEMS (GPIO 50/51) | SWO (`nsx view`) |

Host LiteRT on the flash/UART identity PCM predicts **`go`**. That is host
identity, not an Apollo510 latency number.

## Suggested overlay

Keep this folder as a sibling of `neuralspotx/`, or copy `kws_core/`,
`kws_clip/`, `kws_uart/`, `kws_pdm/`, `assets/`, and `tools/` under
`neuralspotx/examples/` (CMake expects `../kws_core`, `../assets`, `../tools`
from each app). Do not vendor AmbiqSuite, a Python venv, or a research tree.

PR order, if merged one app at a time: **clip → uart + `host/stream_wav.py` →
pdm**. Bring-up `kws_pdm` only after `kws_clip` prints `pred=go` on the same
EVB.

## Pins (not the `kws_infer` README defaults)

| Piece | Identity |
|---|---|
| Model | `assets/depgraph_r060_kd_int8.tflite` SHA256 `ae08012b…` |
| helia-rt | 1.19.0 / `cfab1523` |
| ns-cmsis-nn | 7.31.0 / `b386770a` |
| SDK | NSX `nsx-ambiqsuite` (v5.2.24 in the lock), not a sibling Suite tarball |

Mixing helia-rt 1.16.0 from the `kws_infer` README with this graph is a
different binary identity.

## Three claims these READMEs never make

1. Host milliseconds, MACs, or `.tflite` bytes are not Apollo510 latency.
2. A UART or flash-clip `pred` is not a PDM-microphone result.
3. `hpx profile` is not this application's `main`.

Full contract: [`EXAMPLES_CONTRACT.md`](EXAMPLES_CONTRACT.md).
Your own INT8 student: [`bring_your_model.md`](bring_your_model.md).
What is in / out of the tarball: [`PACKAGING.md`](PACKAGING.md).
