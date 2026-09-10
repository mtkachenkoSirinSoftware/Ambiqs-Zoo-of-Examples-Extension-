// SPDX-License-Identifier: Apache-2.0
// UART bytes -> 1 s RAM clip -> FlashClipPlayer. No HAL, no RTT.
#ifndef UART_CLIP_SESSION_H_
#define UART_CLIP_SESSION_H_

#include "audio/flash_clip_player.h"
#include "audio/uart_clip_buffer.h"
#include "protocol/uart_audio_pump.h"

namespace kws {

class UartClipSession {
 public:
  void Init();
  void PushBytes(const uint8_t* data, size_t len);
  void AddUartErrors(uint32_t n);

  bool BlockReady();
  const AudioSample* AcquireBlock(uint32_t* out_samples);
  void ReleaseBlock();
  const AudioSourceStatus* status() const;

  bool TakeStreamBegin();
  bool TakeStreamEnd();
  bool TakeReset();

  const UartClipBuffer& buffer() const { return buf_; }
  uint32_t clip_dropped() const { return UartClipBufferDropped(&buf_); }
  bool playing() const { return playing_; }

 private:
  void MaybeStartPlayer();

  proto::UartAudioPump pump_;
  UartClipBuffer buf_{};
  FlashClipPlayer player_{};
  bool playing_ = false;
};

}  // namespace kws
#endif  // UART_CLIP_SESSION_H_
