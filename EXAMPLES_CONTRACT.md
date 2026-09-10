# SPDX-License-Identifier: Apache-2.0
# Example-zoo contract (phase 0). Numbers here are identity, not marketing.

This tree is the **clean** Ambiq-facing cut of Demo A (model file) and Demo B
(always-on KWS on Apollo510). Research notebooks, SNR sweeps, host-mic ALSA,
and GATE essays stay out.

## Canonical PCM

| Field | Value |
|---|---|
| Sample rate | 16 000 Hz |
| Channels | 1 (mono) |
| Sample format | signed int16 |
| Inference window | **exactly 16 000 samples (1.000 s)** |
| Hop / UART block | 320 samples (20 ms) |

Host tools must not peak-normalise. Training and firmware both divide by the
signed `max(x)` of that 1 s window.

## Canonical model

See [`assets/MODEL.txt`](assets/MODEL.txt). Shipping SHA256:

`ae08012b5a5dd1fd59673bba3d4b1d1c43d7cd1f410537824d7eb6c2edb40fb7`

Input `[1,49,10,1]` int8 → output `[1,12]` int8. Class B (same geometry, other
scale/SHA) is a re-embed, not a hand-edit of C arrays. Class C (wrong shape) is
a stop. Procedure: [`bring_your_model.md`](bring_your_model.md).

## Three claims these examples must never print

1. **Host milliseconds, MACs, or `.tflite` bytes are not Apollo510 latency.**
   On-target time, Helium/MVE stalls, cycles/inference, and RAM/flash of the
   linked image are measured on the EVB or labelled deferred.
2. **A UART or flash-clip `pred` is not a PDM-microphone result.** Same graph,
   different acoustic identity. Last raw argmax vs LiteRT is valid only on
   **identical PCM**.
3. **`hpx profile` is not this application's `main`.** heliaPROFILER flashes
   its own firmware with a zeroed tensor and no MFCC. Invoke cycles from
   `kws_infer`-style PMU belong in a separate column from always-on KWS.

## Audio sources (later examples)

| Example | Status | PCM origin |
|---|---|---|
| `kws_core` | **phase 1** | library only |
| `kws_clip` | **phase 2** | 1 s PCM in flash (`synthetic` golden → host LiteRT `go`) |
| `kws_uart` | **phase 3** | host WAV, framed, FIFO-poll UART (`synthetic.wav` → host LiteRT `go`) |
| `kws_pdm` | **phase 4** | EVB PDM MEMS (GPIO 50/51), after official `pdm_rtt_stream` |

UART ingest uses classic HAL FIFO poll. Do not enable UART RX-DMA /
`am_hal_uart_interrupt_stream_service` on this path — that combination hung
the EVB (1 Hz RTT stats froze on the first host byte).

## What a number is

| Kind | Allowed wording |
|---|---|
| Host LiteRT / golden MFCC | host, that SHA, that WAV |
| Flash-clip or UART last argmax vs LiteRT | same PCM, MCU vs host |
| PDM `pred` | live mic, not GSC GATE |
| DWT cycles from flashed `main` | Apollo510, this binary |
| `hpx` / dummy-input `kws_infer` | model-only, different firmware |

## Bring-your-model (phase 5)

[`bring_your_model.md`](bring_your_model.md): `tools/embed_model.py --check`
then re-embed. After Class B, UART/clip vs LiteRT uses **that** SHA, not
shipping `host/expected.json`.

## Packaging (phase 6)

[`PACKAGING.md`](PACKAGING.md) / `make dist`. The tarball is three examples +
`kws_core` + one WAV + one `.tflite` + host Python + README + LICENSE. No
AmbiqSuite SDK, no `.venv`, no SNR, no dual demo trees. Cover letter:
[`COVER.md`](COVER.md).

Further work that stays on this graph (PMU on clip, `nsx-audio` iff 16 kHz,
USB-CDC UART, …) and is **not** Suite-Make phase 7:
[`EXTENSIONS.md`](EXTENSIONS.md).
