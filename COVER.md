# Cover — overlay into neuralspotx

This folder is three NSX apps plus `kws_core`. It fills the gap between:

| Example | Today | Missing |
|---|---|---|
| `kws_infer` | one dummy-input heliaRT Invoke, SWO, PMU | no PCM, no MFCC |
| `audio_capture` | PDM peak on GPIO 50/51 | no DS-CNN |

| App | PCM | Labels |
|---|---|---|
| `kws_clip` | 1 s flash clip (`synthetic`) | SWO |
| `kws_uart` | host WAV, FIFO-poll UART | SWO |
| `kws_pdm` | EVB PDM (GPIO 50/51) | SWO |

Keep the folder as a sibling of `neuralspotx/`, or copy `kws_core/`,
`kws_clip/`, `kws_uart/`, `kws_pdm/`, `assets/`, and `tools/` under
`neuralspotx/examples/` (each app CMake expects `../kws_core`, `../assets`,
`../tools`). Do not vendor AmbiqSuite or a Python venv.

If merging one app at a time: **clip → uart + `host/stream_wav.py` → pdm**.
Bring up `kws_pdm` only after `kws_clip` prints `pred=go` on the same EVB.

| Piece | Pin |
|---|---|
| Model | `assets/depgraph_r060_kd_int8.tflite` SHA256 `ae08012b…` |
| helia-rt | 1.19.0 / `cfab1523` |
| ns-cmsis-nn | 7.31.0 / `b386770a` |
| SDK | `nsx-ambiqsuite` v5.2.24 in the lock |

Mixing helia-rt 1.16.0 from the `kws_infer` README with this graph is a
different binary.

[`EXAMPLES_CONTRACT.md`](EXAMPLES_CONTRACT.md) ·
[`bring_your_model.md`](bring_your_model.md) ·
[`PACKAGING.md`](PACKAGING.md)
