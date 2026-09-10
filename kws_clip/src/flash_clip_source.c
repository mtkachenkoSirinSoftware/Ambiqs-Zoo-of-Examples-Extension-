// SPDX-License-Identifier: Apache-2.0
// Flash-resident clip AudioSource. PCM is audio/generated/kws_clip_data.*
#include "audio/audio_source.h"
#include "audio/flash_clip_player.h"
#include "audio/generated/kws_clip_data.h"

static FlashClipPlayer g_player;

bool AudioSourceInit(void) {
  return FlashClipPlayerInit(&g_player, kws_clip_pcm, (uint32_t)KWS_CLIP_N_SAMPLES);
}

bool AudioSourceStart(void) { return FlashClipPlayerStart(&g_player); }
bool AudioSourceStop(void) { return FlashClipPlayerStop(&g_player); }

bool AudioSourceBlockReady(void) { return FlashClipPlayerBlockReady(&g_player); }

const AudioSample* AudioSourceAcquireBlock(uint32_t* out_samples) {
  return FlashClipPlayerAcquireBlock(&g_player, out_samples);
}

void AudioSourceReleaseBlock(void) { FlashClipPlayerReleaseBlock(&g_player); }

const AudioSourceStatus* AudioSourceGetStatus(void) { return FlashClipPlayerStatus(&g_player); }

bool AudioSourceTakeStreamBegin(void) { return FlashClipPlayerTakeStreamBegin(&g_player); }
bool AudioSourceTakeStreamEnd(void) { return FlashClipPlayerTakeStreamEnd(&g_player); }
bool AudioSourceTakeReset(void) { return FlashClipPlayerTakeReset(&g_player); }
