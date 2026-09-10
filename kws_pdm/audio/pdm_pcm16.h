// SPDX-License-Identifier: Apache-2.0
// Host-testable PDM clock identity and 24-bit DMA word -> PCM16 conversion.
//
// Clock recipe is AmbiqSuite 5.2.0 boards/apollo510_evb/examples/audio/pdm_fft
// (and pdm_rtt_stream): SYSPLL 24.576 MHz, MCLKDIV_1, PDMA_CLKO_DIV5,
// decimation 64.
//   PDM_CLK_OUT   = Fsrc / (eClkDivider + 1) / (ePDMAClkOutDivder + 1)
//   SAMPLING_FREQ = PDM_CLK_OUT / (ui32DecimationRate * 2)
// ERR021: OSR <= 128. Datasheet typical voice Fs = 16 kHz.
#ifndef PDM_PCM16_H_
#define PDM_PCM16_H_

#include <stddef.h>
#include <stdint.h>

#include "config/kws_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KWS_PDM_SRC_HZ 24576000u
#define KWS_PDM_MCLKDIV 1u
#define KWS_PDM_CLKO_DIV 5u
#define KWS_PDM_DECIMATION 64u
#define KWS_PDM_CLK_OUT_HZ \
  (KWS_PDM_SRC_HZ / (KWS_PDM_MCLKDIV + 1u) / (KWS_PDM_CLKO_DIV + 1u))
#define KWS_PDM_FS_HZ (KWS_PDM_CLK_OUT_HZ / (KWS_PDM_DECIMATION * 2u))
#define KWS_PDM_OSR KWS_PDM_DECIMATION
#define KWS_PDM_SAMPLE_SHIFT 8
#define KWS_PDM_FIFO_THRESHOLD 16u
#define KWS_PDM_SETTLE_MS 60u

static_assert(KWS_PDM_CLK_OUT_HZ == 2048000u, "PDM CLK OUT must be 2.048 MHz (pdm_fft)");
static_assert(KWS_PDM_FS_HZ == 16000u, "PLL recipe is exactly 16 kHz");
static_assert(KWS_PDM_FS_HZ == (uint32_t)KWS_SAMPLE_RATE_HZ, "PDM Fs must match frontend");
static_assert(KWS_PDM_OSR <= 128u, "ERR021: OSR > 128 aliases");
static_assert(KWS_PDM_CLK_OUT_HZ >= 512000u && KWS_PDM_CLK_OUT_HZ <= 3072000u,
              "datasheet DMIC clock range 512 kHz .. 3.072 MHz");

// Bits [23:8] of the 32-bit DMA word (16 MSBs of signed 24-bit PCM).
static inline AudioSample KwsPdmWordToPcm16(uint32_t word) {
  return (AudioSample)(uint16_t)((word >> KWS_PDM_SAMPLE_SHIFT) & 0xFFFFu);
}

static inline void KwsPdmBlockToPcm16(const uint32_t* src, AudioSample* dst, uint32_t n,
                                      int32_t* peak_abs, int32_t* dc) {
  int64_t sum = 0;
  int32_t peak = 0;
  for (uint32_t i = 0; i < n; ++i) {
    const AudioSample s = KwsPdmWordToPcm16(src[i]);
    dst[i] = s;
    const int32_t a = (s < 0) ? -(int32_t)s : (int32_t)s;
    if (a > peak) peak = a;
    sum += (int32_t)s;
  }
  if (peak_abs != NULL) *peak_abs = peak;
  if (dc != NULL) *dc = (n != 0) ? (int32_t)(sum / (int64_t)n) : 0;
}

#ifdef __cplusplus
}
#endif
#endif  // PDM_PCM16_H_
