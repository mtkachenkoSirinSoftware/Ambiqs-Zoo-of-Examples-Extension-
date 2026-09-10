// SPDX-License-Identifier: Apache-2.0
// Apollo510 PDM microphone capture.
//
// HAL from AmbiqSuite / nsx-ambiq-sdk am_hal_pdm.h. Clock, FIFO threshold,
// 60 ms settle, and ping-pong DMA copied from
//   boards/apollo510_evb/examples/audio/pdm_fft/src/pdm_fft.c
//   boards/apollo510_evb/examples/audio/pdm_rtt_stream/src/pdm_rtt_stream.c
//
// Connect a PDM MEMS to GPIO 50 (CLK) / 51 (DATA) first. Live pred is not GATE 3.
#ifndef APOLLO510_AUDIO_H_
#define APOLLO510_AUDIO_H_

#include <stdbool.h>
#include <stdint.h>

#include "audio/pdm_pcm16.h"
#include "config/kws_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KWS_PDM_MODULE 0
#define KWS_PDM_CLK_GPIO 50   // AM_BSP_GPIO_PDM0_CLK_CB
#define KWS_PDM_DATA_GPIO 51  // AM_BSP_GPIO_PDM0_DATA_CB

#ifndef KWS_PDM_LR_SWAP
#define KWS_PDM_LR_SWAP 0
#endif

bool Apollo510AudioInit(void);
bool Apollo510AudioStart(void);
bool Apollo510AudioStop(void);

#ifdef __cplusplus
}
#endif
#endif  // APOLLO510_AUDIO_H_
