// SPDX-License-Identifier: Apache-2.0
// Host-testable 1 s PCM staging buffer. UART AUDIO_BLOCKs accumulate here;
// END pads/truncates to KWS_CLIP_SAMPLES, then FlashClipPlayer replays it.
#ifndef UART_CLIP_BUFFER_H_
#define UART_CLIP_BUFFER_H_

#include <stdbool.h>
#include <stdint.h>

#include "config/kws_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UartClipBuffer {
  AudioSample pcm[KWS_CLIP_SAMPLES];
  uint32_t filled;
  uint32_t dropped;
  bool active;
  bool complete;
} UartClipBuffer;

void UartClipBufferInit(UartClipBuffer* b);
void UartClipBufferBegin(UartClipBuffer* b);
void UartClipBufferPush(UartClipBuffer* b, const AudioSample* samples, uint32_t n);
void UartClipBufferFinalize(UartClipBuffer* b);
bool UartClipBufferTakeReady(UartClipBuffer* b);

const AudioSample* UartClipBufferPcm(const UartClipBuffer* b);
uint32_t UartClipBufferFilled(const UartClipBuffer* b);
uint32_t UartClipBufferDropped(const UartClipBuffer* b);

#ifdef __cplusplus
}
#endif
#endif  // UART_CLIP_BUFFER_H_
