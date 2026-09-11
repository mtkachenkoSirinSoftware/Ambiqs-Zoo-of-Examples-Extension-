# SPDX-License-Identifier: Apache-2.0
# Identity for these examples

Numbers here are identity, not marketing.

## PCM

| Field | Value |
|---|---|
| Sample rate | 16 000 Hz |
| Channels | 1 (mono) |
| Format | signed int16 |
| Inference window | **exactly 16 000 samples (1.000 s)** |
| Hop / UART block | 320 samples (20 ms) |

Host tools must not peak-normalise. Training and firmware both divide by the
signed `max(x)` of that 1 s window.

## Model

[`assets/MODEL.txt`](assets/MODEL.txt). Shipping SHA256:

`ae08012b5a5dd1fd59673bba3d4b1d1c43d7cd1f410537824d7eb6c2edb40fb7`

Input `[1,49,10,1]` int8 → output `[1,12]` int8. Class B (same geometry,
other scale/SHA) is a re-embed, not a hand-edit of C arrays. Class C (wrong
shape) is a stop. [`bring_your_model.md`](bring_your_model.md).

## Do not print as Apollo510 facts

1. Host milliseconds, MACs, or `.tflite` bytes are not on-target latency.
   Cycles, Helium/MVE, RAM/flash of the linked image: measure on the EVB.
2. UART or flash-clip `pred=` is not a PDM result. Argmax vs interpreter is
   valid only on **identical PCM**.
3. Dummy-input `kws_infer` and heliaPROFILER (`hpx profile`) are other
   firmware. Their cycles are not this `main`.

## Audio origins

| Example | PCM |
|---|---|
| `kws_core` | library only |
| `kws_clip` | 1 s PCM in flash (`synthetic` → interpreter `go`) |
| `kws_uart` | host WAV, FIFO-poll UART (`synthetic.wav` → interpreter `go`) |
| `kws_pdm` | EVB PDM MEMS (GPIO 50/51) |

UART RX is classic HAL FIFO poll. Do not enable RX-DMA /
`am_hal_uart_interrupt_stream_service` on this path.

| Kind | Wording |
|---|---|
| Interpreter / golden MFCC | host, that SHA, that WAV |
| Flash-clip or UART argmax vs interpreter | same PCM, MCU vs host |
| PDM `pred` | live mic |
| DWT from flashed `main` | Apollo510, this binary |
| `hpx` / dummy `kws_infer` | model-only, different firmware |
