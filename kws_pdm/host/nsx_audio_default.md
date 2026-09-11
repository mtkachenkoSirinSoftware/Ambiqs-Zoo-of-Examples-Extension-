# SPDX-License-Identifier: Apache-2.0
# 2.2 — nsx_audio_pdm_default as read after `nsx lock` (nsx-ambiq-sdk v5.2.24)
#
# File: modules/nsx-ambiq-sdk/modules/nsx-audio/src/apollo5/nsx_pdm_apollo5.c
# (vendored by NSX; not copied into git).

## Default struct (verbatim meaning)

| Field | Value |
|---|---|
| `clock` | `NSX_AUDIO_CLK_HFRC2_ADJ` |
| `clock_freq` | `NSX_AUDIO_PDM_CLK_750KHZ` |
| `mic` | `NSX_AUDIO_PDM_MIC0` |
| `sample_width` | `NSX_AUDIO_PDM_16BIT` |
| `left_gain` / `right_gain` | `NSX_AUDIO_PDM_GAIN_P180DB` |

Not the AmbiqSuite `pdm_fft` recipe (SYSPLL 24.576 MHz, CLKO_DIV5, OSR 64,
PDM_CLK_OUT 2.048 MHz). Driver comment: the PLL `am_hal_clkmgr_clock_config`
path can block if the crystal is missing, so the default avoids it.

## Sample rate the driver claims

HFRC2 FLL → 196.608 MHz, `/8` → 24.576 MHz. Then CLKO_DIV15 (÷32) and
`DecimationRate=48`:

    24.576e6 / 32 / 48 = 16 000 Hz

That is **exactly 16 kHz**, not HFRC 15.625 kHz. Gate 2.3 therefore proceeds
with `nsx-audio`, hop still **320** samples (20 ms). `audio_capture` uses 480
(30 ms = our STFT length, not our hop).

## Packing

16-bit path: `pcm[i] = (int16_t)(raw[i] & 0xFFFF)`. Suite HAL used bits
`[23:8]` of a 24-bit word. Live `pred` is still not GATE 3.

## What `nsx-audio` cannot take from our old HAL

Public API does not expose CLKO_DIV5 / OSR 64 or `bLRSwap`. `-DKWS_PDM_LR_SWAP`
needs `-DKWS_PDM_USE_NSX_AUDIO=OFF` (Suite PLL backend).

This note is a source reading of
`kws_pdm/modules/nsx-ambiq-sdk/modules/nsx-audio/src/apollo5/nsx_pdm_apollo5.c`
after `nsx lock` (nsx-ambiq-sdk v5.2.24 / `a9f4ec25`). Not a microphone
measurement.
