// SPDX-License-Identifier: Apache-2.0
// Host-only: PDM PLL recipe + 24-bit DMA word -> PCM16 (no EVB, no heliaRT).
#include "audio/pdm_pcm16.h"

#include <cstdio>
#include <vector>

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      return 1;                                                            \
    }                                                                      \
  } while (0)

int main() {
  CHECK(KWS_PDM_CLK_OUT_HZ == 2048000u);
  CHECK(KWS_PDM_FS_HZ == 16000u);
  CHECK(KWS_PDM_OSR == 64u);
  CHECK(KWS_PDM_FIFO_THRESHOLD == 16u);
  CHECK(KWS_PDM_SETTLE_MS == 60u);
  CHECK(KWS_PDM_OSR <= 128u);
  CHECK(KWS_PDM_CLK_OUT_HZ >= 512000u);
  CHECK(KWS_PDM_CLK_OUT_HZ <= 3072000u);
  CHECK(KWS_PDM_FS_HZ == (uint32_t)KWS_SAMPLE_RATE_HZ);

  CHECK(KwsPdmWordToPcm16(0x00A5B400u) == static_cast<AudioSample>(0xA5B4));
  CHECK(KwsPdmWordToPcm16(0x007FFF00u) == static_cast<AudioSample>(32767));
  CHECK(KwsPdmWordToPcm16(0x00800000u) == static_cast<AudioSample>(-32768));
  CHECK(KwsPdmWordToPcm16(0x00FFFF00u) == static_cast<AudioSample>(-1));

  uint32_t src[4] = {0x00100000u, 0x00F00000u, 0x00200000u, 0x00300000u};
  AudioSample dst[4] = {};
  int32_t peak = 0;
  int32_t dc = 0;
  KwsPdmBlockToPcm16(src, dst, 4, &peak, &dc);
  CHECK(dst[0] == static_cast<AudioSample>(4096));
  CHECK(dst[1] == static_cast<AudioSample>(-4096));
  CHECK(peak == 12288);
  CHECK(dc == 5120);

  std::vector<uint32_t> zeros(8, 0u);
  std::vector<AudioSample> out(8, 1);
  peak = -1;
  dc = -1;
  KwsPdmBlockToPcm16(zeros.data(), out.data(), 8, &peak, &dc);
  CHECK(peak == 0);
  CHECK(dc == 0);
  for (AudioSample s : out) CHECK(s == 0);

  std::printf("kws_pdm host checks OK (clock + PCM16; not a mic measurement)\n");
  return 0;
}
