# SPDX-License-Identifier: Apache-2.0
# `nsx_audio_pdm_default` (nsx-ambiq-sdk v5.2.24)

Source after `nsx lock`:
`modules/nsx-ambiq-sdk/modules/nsx-audio/src/apollo5/nsx_pdm_apollo5.c`
(vendored by NSX; not in git).

| Field | Value |
|---|---|
| `clock` | `NSX_AUDIO_CLK_HFRC2_ADJ` |
| `clock_freq` | `NSX_AUDIO_PDM_CLK_750KHZ` |
| `mic` | `NSX_AUDIO_PDM_MIC0` |
| `sample_width` | `NSX_AUDIO_PDM_16BIT` |
| `left_gain` / `right_gain` | `NSX_AUDIO_PDM_GAIN_P180DB` |

Not the AmbiqSuite `pdm_fft` recipe (SYSPLL 24.576 MHz, CLKO_DIV5, OSR 64,
PDM_CLK_OUT 2.048 MHz). Driver comment: the PLL
`am_hal_clkmgr_clock_config` path can block if the crystal is missing, so
the default avoids it.

## Sample rate

HFRC2 FLL → 196.608 MHz, `/8` → 24.576 MHz. Then CLKO_DIV15 (÷32) and
`DecimationRate=48`:

    24.576e6 / 32 / 48 = 16 000 Hz

That is **exactly 16 kHz**, not HFRC 15.625 kHz. Hop stays **320** samples
(20 ms). `audio_capture` uses 480 (30 ms = STFT length, not the hop).

## Packing

16-bit path: `pcm[i] = (int16_t)(raw[i] & 0xFFFF)`. Suite HAL used bits
`[23:8]` of a 24-bit word.

Public `nsx-audio` does not expose CLKO_DIV5 / OSR 64 or `bLRSwap`.
`-DKWS_PDM_LR_SWAP` needs `-DKWS_PDM_USE_NSX_AUDIO=OFF`.

This note is a source reading of that driver (tag v5.2.24 / `a9f4ec25`),
not a microphone measurement.
