// SPDX-License-Identifier: Apache-2.0
#include "audio/uart_clip_session.h"

namespace kws {

void UartClipSession::Init() {
  UartClipBufferInit(&buf_);
  pump_.Init();
  pump_.SetClipBuffer(&buf_);
  playing_ = false;
}

void UartClipSession::PushBytes(const uint8_t* data, size_t len) {
  pump_.PushBytes(data, len);
  MaybeStartPlayer();
}

void UartClipSession::AddUartErrors(uint32_t n) { pump_.AddUartErrors(n); }

void UartClipSession::MaybeStartPlayer() {
  if (playing_) return;
  if (!UartClipBufferTakeReady(&buf_)) return;
  FlashClipPlayerInit(&player_, UartClipBufferPcm(&buf_), KWS_CLIP_SAMPLES);
  FlashClipPlayerStart(&player_);
  playing_ = true;
}

bool UartClipSession::BlockReady() {
  MaybeStartPlayer();
  return FlashClipPlayerBlockReady(&player_);
}

const AudioSample* UartClipSession::AcquireBlock(uint32_t* out_samples) {
  return FlashClipPlayerAcquireBlock(&player_, out_samples);
}

void UartClipSession::ReleaseBlock() { FlashClipPlayerReleaseBlock(&player_); }

const AudioSourceStatus* UartClipSession::status() const {
  return playing_ ? FlashClipPlayerStatus(&player_) : pump_.status();
}

bool UartClipSession::TakeStreamBegin() { return FlashClipPlayerTakeStreamBegin(&player_); }

bool UartClipSession::TakeStreamEnd() {
  const bool end = FlashClipPlayerTakeStreamEnd(&player_);
  if (end) playing_ = false;
  return end;
}

bool UartClipSession::TakeReset() {
  if (!pump_.TakeReset()) return false;
  if (!playing_) {
    FlashClipPlayerStop(&player_);
    UartClipBufferInit(&buf_);
    pump_.SetClipBuffer(&buf_);
  }
  return true;
}

}  // namespace kws
