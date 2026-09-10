// SPDX-License-Identifier: Apache-2.0
// Byte-stream -> PCM-block pump. No HAL — apollo510_uart.cc owns the UART.
// Host tests compile this without KWS_AUDIO_SOURCE_* (uses audio_status.h).
#ifndef UART_AUDIO_PUMP_H_
#define UART_AUDIO_PUMP_H_

#include <stddef.h>
#include <stdint.h>

#include "audio/audio_status.h"
#include "audio/uart_clip_buffer.h"
#include "config/kws_config.h"
#include "protocol/uart_protocol.h"

namespace kws {
namespace proto {

class UartAudioPump {
 public:
  void Init();
  void Reset();

  void PushBytes(const uint8_t* data, size_t len);

  bool BlockReady() const;
  const AudioSample* AcquireBlock(uint32_t* out_samples);
  void ReleaseBlock();

  const AudioSourceStatus* status() const { return &status_; }

  uint32_t frames_decoded() const { return decoder_.frames_decoded(); }
  uint32_t crc_errors() const { return decoder_.crc_errors(); }
  bool stream_active() const { return stream_active_; }

  bool TakeStreamBegin();
  bool TakeStreamEnd();
  bool TakeReset();

  void SetClipBuffer(UartClipBuffer* buf) { clip_buf_ = buf; }
  void AddUartErrors(uint32_t n) { status_.uart_errors += n; }

 private:
  static void OnFrame(const DecodedFrame& fr, void* ctx);
  void HandleFrame(const DecodedFrame& fr);

  Decoder decoder_;
  AudioSourceStatus status_{};
  AudioSample pending_[2][KWS_AUDIO_BLOCK_SAMPLES]{};
  uint32_t pending_len_[2]{};
  uint32_t write_idx_ = 0;
  uint32_t read_idx_ = 0;
  bool stream_active_ = false;
  bool reset_requested_ = false;
  bool start_requested_ = false;
  bool end_requested_ = false;
  UartClipBuffer* clip_buf_ = nullptr;
};

}  // namespace proto
}  // namespace kws
#endif  // UART_AUDIO_PUMP_H_
