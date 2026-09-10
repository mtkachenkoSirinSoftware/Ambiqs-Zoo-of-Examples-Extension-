// SPDX-License-Identifier: Apache-2.0
#include "protocol/uart_audio_pump.h"

#include <cstring>

namespace kws {
namespace proto {

void UartAudioPump::Init() { Reset(); }

void UartAudioPump::Reset() {
  decoder_.Init(&UartAudioPump::OnFrame, this);
  decoder_.Reset();
  for (int i = 0; i < 2; ++i) {
    status_.state[i] = kBufferFree;
    pending_len_[i] = 0;
  }
  status_.blocks_received = 0;
  status_.blocks_processed = 0;
  status_.overruns = 0;
  status_.uart_errors = 0;
  status_.dma_faults = 0;
  status_.pcm_peak_abs = 0;
  status_.pcm_dc = 0;
  write_idx_ = 0;
  read_idx_ = 0;
  stream_active_ = false;
  reset_requested_ = false;
  start_requested_ = false;
  end_requested_ = false;
}

bool UartAudioPump::TakeStreamBegin() {
  const bool v = start_requested_;
  start_requested_ = false;
  return v;
}

bool UartAudioPump::TakeStreamEnd() {
  const bool v = end_requested_;
  end_requested_ = false;
  return v;
}

bool UartAudioPump::TakeReset() {
  const bool v = reset_requested_;
  reset_requested_ = false;
  return v;
}

void UartAudioPump::PushBytes(const uint8_t* data, size_t len) {
  if (data == nullptr || len == 0) return;
  decoder_.PushBytes(data, len);
}

bool UartAudioPump::BlockReady() const {
  return status_.blocks_received != status_.blocks_processed;
}

const AudioSample* UartAudioPump::AcquireBlock(uint32_t* out_samples) {
  if (!BlockReady()) return nullptr;
  const uint32_t idx = read_idx_ & 1u;
  status_.state[idx] = kBufferCpuProcessing;
  if (out_samples != nullptr) *out_samples = pending_len_[idx];
  return pending_[idx];
}

void UartAudioPump::ReleaseBlock() {
  const uint32_t idx = read_idx_ & 1u;
  status_.state[idx] = kBufferFree;
  pending_len_[idx] = 0;
  ++read_idx_;
  ++status_.blocks_processed;
}

void UartAudioPump::OnFrame(const DecodedFrame& fr, void* ctx) {
  static_cast<UartAudioPump*>(ctx)->HandleFrame(fr);
}

void UartAudioPump::HandleFrame(const DecodedFrame& fr) {
  switch (fr.type) {
    case MsgType::kStartStream:
      stream_active_ = true;
      start_requested_ = true;
      if (clip_buf_ != nullptr) UartClipBufferBegin(clip_buf_);
      break;
    case MsgType::kEndStream:
      stream_active_ = false;
      end_requested_ = true;
      if (clip_buf_ != nullptr) UartClipBufferFinalize(clip_buf_);
      break;
    case MsgType::kReset:
      reset_requested_ = true;
      stream_active_ = false;
      end_requested_ = false;
      break;
    case MsgType::kGetStatus:
      break;
    case MsgType::kAudioBlock: {
      if (fr.payload == nullptr || fr.payload_len != KWS_AUDIO_BLOCK_SAMPLES * sizeof(AudioSample)) {
        ++status_.uart_errors;
        return;
      }
      if (clip_buf_ != nullptr) {
        UartClipBufferPush(clip_buf_, reinterpret_cast<const AudioSample*>(fr.payload),
                           KWS_AUDIO_BLOCK_SAMPLES);
        ++status_.blocks_received;
        break;
      }
      const uint32_t idx = write_idx_ & 1u;
      if (status_.state[idx] == kBufferCpuProcessing || status_.state[idx] == kBufferReadyForCpu) {
        ++status_.overruns;
      }
      std::memcpy(pending_[idx], fr.payload, fr.payload_len);
      pending_len_[idx] = KWS_AUDIO_BLOCK_SAMPLES;
      status_.state[idx] = kBufferReadyForCpu;
      ++write_idx_;
      ++status_.blocks_received;
      break;
    }
    default:
      break;
  }
}

}  // namespace proto
}  // namespace kws
