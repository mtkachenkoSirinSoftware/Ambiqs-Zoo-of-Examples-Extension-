// SPDX-License-Identifier: Apache-2.0
#include "audio/uart_clip_buffer.h"

#include <string.h>

void UartClipBufferInit(UartClipBuffer* b) {
  if (b == NULL) return;
  memset(b->pcm, 0, sizeof(b->pcm));
  b->filled = 0;
  b->dropped = 0;
  b->active = false;
  b->complete = false;
}

void UartClipBufferBegin(UartClipBuffer* b) {
  UartClipBufferInit(b);
  if (b != NULL) b->active = true;
}

void UartClipBufferPush(UartClipBuffer* b, const AudioSample* samples, uint32_t n) {
  if (b == NULL || samples == NULL || n == 0) return;
  if (!b->active) UartClipBufferBegin(b);
  uint32_t room = (b->filled < (uint32_t)KWS_CLIP_SAMPLES)
                      ? ((uint32_t)KWS_CLIP_SAMPLES - b->filled)
                      : 0u;
  uint32_t take = n < room ? n : room;
  if (take > 0) {
    memcpy(b->pcm + b->filled, samples, take * sizeof(AudioSample));
    b->filled += take;
  }
  b->dropped += (n - take);
}

void UartClipBufferFinalize(UartClipBuffer* b) {
  if (b == NULL) return;
  if (b->filled < (uint32_t)KWS_CLIP_SAMPLES) {
    memset(b->pcm + b->filled, 0,
           ((uint32_t)KWS_CLIP_SAMPLES - b->filled) * sizeof(AudioSample));
  }
  b->filled = (uint32_t)KWS_CLIP_SAMPLES;
  b->active = false;
  b->complete = true;
}

bool UartClipBufferTakeReady(UartClipBuffer* b) {
  if (b == NULL || !b->complete) return false;
  b->complete = false;
  return true;
}

const AudioSample* UartClipBufferPcm(const UartClipBuffer* b) {
  return b != NULL ? b->pcm : NULL;
}

uint32_t UartClipBufferFilled(const UartClipBuffer* b) {
  return b != NULL ? b->filled : 0;
}

uint32_t UartClipBufferDropped(const UartClipBuffer* b) {
  return b != NULL ? b->dropped : 0;
}
