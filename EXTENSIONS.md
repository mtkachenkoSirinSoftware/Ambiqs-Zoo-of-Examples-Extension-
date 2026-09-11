# SPDX-License-Identifier: Apache-2.0
# Strengthening neuralspotx / Suite examples from this zoo (not phase 7)

Phases 0–6 of this tree are closed on the host: contract, `kws_core`,
`kws_clip`, `kws_uart`, `kws_pdm`, bring-your-model, packaging. This page is
the map of **what can still be added without phase 7** (AmbiqSuite Make
`uart_pcm_ingest` without a network). UART ingest already exists here as NSX
`kws_uart`.

The rule: a new piece of work belongs here only if it **engineers up an
existing Ambiq example** onto the same graph and PCM contract. It is not a
fourth audio origin, not a second runtime, and not a research notebook.

Contract: [`EXAMPLES_CONTRACT.md`](EXAMPLES_CONTRACT.md).
Cover letter: [`COVER.md`](COVER.md).

## Canvas that must not break

| Already true | Do not “extend” by |
|---|---|
| 16 kHz mono int16, window exactly 1.000 s, hop 320 samples | another frontend or window sold as the same `pred` |
| Shipping SHA `ae08012b…`, **tfds** label order (`down`, `go`, …) | silently using `kws_infer` MLPerf order (`silence`, `unknown`, `yes`, …) |
| SWO = labels; PRINT UART = PCM only | TX of `pred` on the PCM UART |
| UART RX = main-loop FIFO poll | RX-DMA / `am_hal_uart_interrupt_stream_service` |
| PDM PLL 2.048 MHz → **exactly** 16 kHz | NSX/HFRC 15.625 kHz labelled “16 kHz” |
| `invoke_cycles=` is DWT of **this** `Invoke` | `hpx profile` or dummy-input `kws_infer` as always-on time |

A new directory is justified only when it is **their module on this graph**,
not a new PCM path and not a Suite-Make HAL demo.

Their NSX catalog this map refers to: `hello_world`, `kws_infer`,
`audio_capture`, `pmu_profiling`, `power_benchmark`, `usb_serial`, `usb_rpc`.
Suite audio used as preflight: `pdm_fft`, `pdm_rtt_stream`.

```
hello_world          already: nsx_system + SWO
kws_infer            + real PCM/MFCC     → kws_clip (+ optional PMU CSV)
audio_capture        + same graph        → kws_pdm (nsx-audio iff Fs is 16 kHz)
pdm_rtt_stream       + network, same channels → RTT ch1 dump in kws_pdm
pmu_profiling        + DS-CNN Invoke     → PMU on clip, separate column
power_benchmark      + this workload     → GPIO markers on clip
usb_serial           + this framing      → stream_wav DTR/port; optional CDC
usb_rpc INFER        toy → kws_core      → only if they ask for protobuf
uart_stream (Suite)  PCM, no NN          → do not add (that is phase 7)
```

---

## 1. `kws_infer` — dummy tensor, PMU, helia-rt 1.16

**Theirs today.** One Invoke of zeros, then a PRNG fill; `NsxPmuProfiler` CSV;
labels in **classic MLPerf** order; helia-rt **1.16.0** in `nsx.yml`; arena in
TCM; module `nsx-pmu-armv8m`; boards `apollo510_evb` / `apollo510b_evb` /
`apollo330mP_evb`; wall time printed as CYCCNT / 96 (LP).

**Gap.** No MFCC, no known PCM, different runtime pin, different class order.
`kws_clip` already supplies a real tensor. What is still missing is **their**
PMU and **their** multi-board list on **our** identity clip.

| ID | Add | Where | Done when | Do not |
|---|---|---|---|---|
| 1.1 | Label-order table (MLPerf vs tfds) | `assets/` + one README line on clip/uart | **landed** — `assets/LABELS.md`; clip/uart SWO + README | remap classes in firmware “for compatibility” |
| 1.2 | `nsx-pmu-armv8m` + `NsxPmuProfiler` on **clip** `Invoke` only; SWO block `--- Per-Layer PMU ---` | `kws_clip` | **landed** — CSV on synthetic clip only; not always-on, not `hpx` | PMU on every `kws_pdm` hop as product latency |
| 1.3 | Print `perf_mode=`; convert CYCCNT to µs only with that divisor | clip / uart / pdm | **landed** — `invoke_us` only from printed `cpu_hz` | guess 192 / 250 MHz |
| 1.4 | Document `embed_model.py` as the way to refresh **their** `kws_model_data.h` | `bring_your_model.md` | **landed** — `--kws-infer-header`; `kLabels[]` stays MLPerf | hand-edit their C array |
| 1.5 | `targets.supported` as in `kws_infer` — **clip only** (no PDM/UART pinout) | `kws_clip/nsx.yml` | `nsx build --board apollo510b_evb` links | guess GPIO 50/51 on 510b / 330mP |

This is `kws_infer` with a real input, not a second profiler firmware.

---

## 2. `audio_capture` — `nsx-audio`, peak, 30 ms frames

**Theirs today.** `nsx_audio_init` PDM, **480** samples/frame (our
`KWS_FRAME_LENGTH`, not the hop), peak every ~3 s, DMA in SRAM
(`NSX_MEM_SRAM_BSS`). README: onboard PDM MEMS, no extra hardware.

**Ours / Suite.** `pdm_fft` and `kws_pdm`: **no onboard mic** on the Apollo510
EVB Quick Start; GPIO 50 CLK / 51 DATA on **VDDH2**.

**Gap.** Peak without a network; 30 ms frame vs 20 ms hop; `nsx_audio_pdm_default`
clock is **unconfirmed** vs the Suite PLL recipe; README disagrees with the BSP.

| ID | Add | Where | Done when | Gate |
|---|---|---|---|---|
| 2.1 | Honest preflight: their `audio_capture` **or** Suite `pdm_rtt_stream`; footnote onboard-mic claim vs BSP 50/51 | `kws_pdm/README.md` | **landed** — two bring-up paths; MEMS part still not guessed | do not “correct” their README as a board fact without their board file |
| 2.2 | Read `nsx_audio_pdm_default` after `nsx lock` | note under `kws_pdm/` | **landed** — HFRC2_ADJ → **exactly 16 kHz** (not Suite PLL, not 15.625 kHz) | do not adopt the module by name |
| 2.3 | `AudioSource` on `nsx-audio` instead of raw HAL; hop remains **320** | `kws_pdm` backend | **landed** — default `nsx-audio`, hop 320; HAL PLL via `-DKWS_PDM_USE_NSX_AUDIO=OFF` | change the frontend hop to 30 ms |
| 2.4 | Print `peak=` / `dc=` in the `Frame N peak=` shape | `kws_pdm` SWO | **landed** — `Frame N  peak=` | treat peak as a KWS score |

**2.3 is the main “their module on our graph” hop.** Without 2.2 it is an Fs
regression.

---

## 3. Suite `pdm_rtt_stream` / `pdm_fft` — PCM on RTT ch1, FFT on SWO

Already the documented first step for `kws_pdm`. Do not fork the HAL.

| ID | Add | Where | Done when |
|---|---|---|---|
| 3.1 | Optional hop-PCM dump on **RTT channel 1**; labels stay on SWO/ITM | `kws_pdm` compile flag | same channel split as `pdm_rtt_stream`; README: ch1 is PCM, not a label |
| 3.2 | Tiny `host/rtt_pcm_dump.py` in the style of their `rtt_logger.py` | `kws_pdm/host/` | 16 kHz int16 file; **not** LiteRT, not GATE 3 |

This is not phase 7.

---

## 4. `pmu_profiling` — CYCLES / INST_RETIRED on a dummy loop

**Theirs.** Workload is `i²`. **Ours.** Workload is DS-CNN `Invoke`.

| ID | Add | Where | Done when |
|---|---|---|---|
| 4.1 | Same two events as their README (CPU_CYCLES + INST_RETIRED) next to DWT | `kws_clip` | two numbers: DWT CYCCNT and PMU CPU_CYCLES; a mismatch is a fact, not a bug |
| 4.2 | Optional MVE / D-cache miss from `NSX_PMU_PRESET_ML_DEFAULT` (already in `kws_infer`) | clip only | column “model-only, this binary”; not always-on milliwatts |

Do not attach the PMU to the UART FIFO or the PDM ISR (ingest stalls ≠ conv stalls).

---

## 5. `power_benchmark` — GPIO + Joulescope

**Theirs.** Modes coremark / while1 / deepsleep and `joulescope_capture.py`.
**Ours.** KWS in LP + cache, no GPIO markers.

| ID | Add | Where | Done when |
|---|---|---|---|
| 5.1 | GPIO toggle around `Invoke`, CMake flag | `kws_clip` | their `joulescope_capture.py` sees active/idle on **this** clip |
| 5.2 | Second marker: hop loop vs `Invoke` | clip, then pdm | report has two phases; milliwatts only from Joulescope, else “deferred” |

Do not invent mW from host milliseconds. Do not add a `kws_power` example — it
is a build flag on `kws_clip`.

---

## 6. `usb_serial` — CDC echo, DTR = True

**Theirs.** CDC VID/PID `0xCafe` / `0x4011`; without DTR the device ignores RX.
**Ours.** J-Link-OB / PRINT UART; **DTR/RTS held low** or the MCU resets.

Two ports, inverted DTR contracts.

| ID | Add | Where | Done when |
|---|---|---|---|
| 6.1 | `stream_wav.py` distinguishes J-Link VCP vs NSX CDC (`list_ports`); `--dtr {low,high}` | `kws_uart/host/` | UART README cites their “pick the right port” |
| 6.2 | Optional PCM backend on `nsx-usb` CDC; labels still on SWO | same `0xA51C` protocol | `synthetic.wav` → `pred=go` over CDC; DTR=True as in `usb_serial` |

6.2 is the only new **transport** in this canvas (not a new graph). Useful when
the reviewer has one USB cable. Still not phase 7.

---

## 7. `usb_rpc` — protobuf toy `INFER`

**Theirs.** `INFER` already exists: raw bytes → class + confidence.

| ID | Add | Priority | Done when |
|---|---|---|---|
| 7.1 | One paragraph: SWO `pred=` is this firmware; their `INFER` is another image | low | nobody compares the toy class to `pred=go` |
| 7.2 | RPC `INFER` accepts 16000 int16 and runs `KwsApp` | only if they ask for RPC | last raw argmax vs LiteRT on **that** buffer |

7.2 is heavier and off the clip/uart style. Not the first slot after packaging.

---

## 8. `hello_world` / `nsx_system`

Already used: `nsx_system_init`, `skip_bsp_init`, ITM. Residual: one README
blurb per app matching `kws_infer` (LP, cache, SpotMgr) so the zoo does not
look like a bare HAL tree.

---

## 9. Their CI `tests/test_example_builds.py`

Copies an example, `nsx configure && nsx build`, asserts the ELF exists.
`kws_infer` is `.ci-skip` (TFLM from the monorepo). Our clip/uart **link
standalone**.

| ID | Add | Done when |
|---|---|---|
| 9.1 | Same pytest shape here, or an overlay in neuralspotx | clip + uart configure+build in tmp, no flash; pdm third, after green clip on **their** EVB |
| 9.2 | `make ci-firmware` as **their** CI step (needs `nsx`) | ELF + `.bin` size labelled linked image, not latency |

---

## Suggested order (value to Ambiq)

**Docs / host — no EVB required**

1. 1.1 label table (cheap fuse).
2. 6.1 DTR / port vs `usb_serial`.
3. 2.1 `audio_capture` vs `pdm_rtt_stream` preflight + BSP footnote.
4. 1.4 `embed_model.py` → `kws_infer`.
5. 9.1 pytest-build clip/uart.

**Their module in our binary — needs `nsx lock`, then EVB clip**

6. PMU CSV on `kws_clip` (1.2 + 4.1).
7. Read `nsx_audio_pdm_default` (2.2); **iff** PLL matches, `nsx-audio` backend (2.3).
8. Joulescope GPIO on the same clip (5.1).

**After SWO `pred=go` on an EVB**

9. RTT ch1 PCM dump (3.1–3.2).
10. Multi-board clip only (1.5).
11. USB-CDC uart backend (6.2) if one cable.
12. `usb_rpc` INFER (7.2) only if requested.

---

## Out of scope here (including phase 7)

- Suite Make `uart_pcm_ingest` without NN — phase 7; `kws_uart` already covers dataset WAV → graph.
- Second runtime (ExecuTorch / heliaCORE beside heliaRT).
- FreeRTOS task around KWS (`freertos_blinky`).
- BLE, CoreMark-as-KWS.
- ALSA `--mic`, SNR mixer, 12/12 GSC corpus as the Usage happy path.
- FVP; MACs or `.tflite` bytes as Apollo510 latency.
- Inferring **while** UART RX is filling RAM (today: 1 s then the same player as clip). Possible after a green EVB clip; hang risk, not an NSX-example strengthen.
- Upstream edit of `audio_capture` (“no onboard mic”) until their 510 vs 510b board file is checked.

Research trees (`demo_a`, `demo_b`, `apollo510_kws_demo`, notebooks) stay outside
this directory and outside the submission tarball. See [`PACKAGING.md`](PACKAGING.md).
