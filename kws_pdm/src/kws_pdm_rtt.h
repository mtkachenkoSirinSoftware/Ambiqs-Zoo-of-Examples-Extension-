// SPDX-License-Identifier: Apache-2.0
// Optional RTT channel-1 hop-PCM dump (Suite pdm_rtt_stream split).
// Labels stay on SWO. Not LiteRT, not GATE 3.
#ifndef KWS_PDM_RTT_H_
#define KWS_PDM_RTT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { kKwsPdmRttPcmChannel = 1 };

void KwsPdmRttInit(void);
void KwsPdmRttWriteHop(const int16_t* pcm, uint32_t n_samples);

#ifdef __cplusplus
}
#endif
#endif  // KWS_PDM_RTT_H_
