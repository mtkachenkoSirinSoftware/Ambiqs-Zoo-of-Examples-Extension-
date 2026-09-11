// SPDX-License-Identifier: Apache-2.0
// PDM AudioSource via nsx-audio (EXTENSIONS 2.3). Hop is KWS_AUDIO_BLOCK_SAMPLES
// (320), not audio_capture's 480. Clock is nsx_audio_pdm_default (HFRC2_ADJ
// 16 kHz) — see host/nsx_audio_default.md. Not the pdm_fft PLL 2.048 MHz path.
#include "audio/audio_source.h"

#include "apollo510_audio.h"
#include "config/kws_config.h"

#include "nsx_audio.h"
#include "nsx_mem.h"

#include "am_util.h"

#include <string.h>

#define KWS_PDM_HOP KWS_AUDIO_BLOCK_SAMPLES

static NSX_MEM_SRAM_BSS uint32_t __attribute__((aligned(32)))
    g_dma[KWS_PDM_HOP * 2];
static int16_t g_pcm_nsx[KWS_PDM_HOP];
static AudioSample g_hop[KWS_PDM_HOP];
static nsx_audio_config_t g_audio;
static AudioSourceStatus g_status;
static volatile uint8_t g_ready;

static void audio_cb(nsx_audio_config_t* cfg, void* buffer, uint32_t num_samples) {
  (void)cfg;
  const int16_t* pcm = (const int16_t*)buffer;
  const uint32_t n = (num_samples > KWS_PDM_HOP) ? KWS_PDM_HOP : num_samples;
  if (g_ready) {
    ++g_status.overruns;
  }
  int32_t peak = 0;
  int64_t sum = 0;
  for (uint32_t i = 0; i < n; ++i) {
    const AudioSample s = (AudioSample)pcm[i];
    g_hop[i] = s;
    const int32_t a = (s < 0) ? -(int32_t)s : (int32_t)s;
    if (a > peak) peak = a;
    sum += (int32_t)s;
  }
  for (uint32_t i = n; i < KWS_PDM_HOP; ++i) {
    g_hop[i] = 0;
  }
  g_status.pcm_peak_abs = peak;
  g_status.pcm_dc = (n != 0) ? (int32_t)(sum / (int64_t)n) : 0;
  ++g_status.blocks_received;
  g_ready = 1;
}

bool AudioSourceInit(void) {
  for (int i = 0; i < 2; ++i) g_status.state[i] = kBufferFree;
  g_status.blocks_received = 0;
  g_status.blocks_processed = 0;
  g_status.overruns = 0;
  g_status.uart_errors = 0;
  g_status.dma_faults = 0;
  g_status.pcm_peak_abs = 0;
  g_status.pcm_dc = 0;
  g_ready = 0;

  memset(&g_audio, 0, sizeof(g_audio));
  g_audio.source = NSX_AUDIO_SOURCE_PDM;
  g_audio.num_channels = 1;
  g_audio.num_samples = KWS_PDM_HOP;
  g_audio.pdm = nsx_audio_pdm_default;
  g_audio.dma_buffer = g_dma;
  g_audio.dma_buffer_size = sizeof(g_dma);
  g_audio.pcm_buffer = g_pcm_nsx;
  g_audio.pcm_buffer_size = sizeof(g_pcm_nsx);
  g_audio.callback = audio_cb;
  g_audio.user_ctx = NULL;
  return nsx_audio_init(&g_audio) == 0;
}

bool AudioSourceStart(void) {
  am_util_delay_ms(KWS_PDM_SETTLE_MS);
  return nsx_audio_start(&g_audio) == 0;
}

bool AudioSourceStop(void) { return nsx_audio_stop(&g_audio) == 0; }

bool AudioSourceBlockReady(void) { return g_ready != 0; }

const AudioSample* AudioSourceAcquireBlock(uint32_t* out_samples) {
  if (!g_ready) return NULL;
  g_status.state[g_status.blocks_processed & 1u] = kBufferCpuProcessing;
  if (out_samples != NULL) *out_samples = KWS_PDM_HOP;
  return g_hop;
}

void AudioSourceReleaseBlock(void) {
  g_status.state[g_status.blocks_processed & 1u] = kBufferFree;
  ++g_status.blocks_processed;
  g_ready = 0;
}

const AudioSourceStatus* AudioSourceGetStatus(void) { return &g_status; }

bool AudioSourceTakeStreamBegin(void) { return false; }
bool AudioSourceTakeStreamEnd(void) { return false; }
bool AudioSourceTakeReset(void) { return false; }
