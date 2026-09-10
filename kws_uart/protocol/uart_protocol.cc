// SPDX-License-Identifier: Apache-2.0
#include "protocol/uart_protocol.h"

#include <string.h>

namespace kws {
namespace proto {
namespace {
inline void PutU16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>(v >> 8);
}
inline uint16_t GetU16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | static_cast<uint16_t>(p[1] << 8);
}
}  // namespace

uint16_t Crc16Ccitt(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; ++b) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                           : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

size_t Encode(MsgType type, uint16_t seq, const uint8_t* payload, size_t payload_len,
              uint8_t* out, size_t out_cap) {
  if (out == nullptr || payload_len > kMaxPayload) return 0;
  if (payload_len > 0 && payload == nullptr) return 0;
  const size_t total = FrameSize(payload_len);
  if (out_cap < total) return 0;

  PutU16(out + 0, kMagic);
  out[2] = kVersion;
  out[3] = static_cast<uint8_t>(type);
  PutU16(out + 4, seq);
  PutU16(out + 6, static_cast<uint16_t>(payload_len));
  PutU16(out + 8, Crc16Ccitt(out, 8));
  if (payload_len > 0) memcpy(out + kHeaderSize, payload, payload_len);
  PutU16(out + kHeaderSize + payload_len, Crc16Ccitt(out + kHeaderSize, payload_len));
  return total;
}

void Decoder::Init(FrameHandler handler, void* ctx) {
  handler_ = handler;
  ctx_ = ctx;
  Reset();
}

void Decoder::Reset() {
  state_ = State::kSyncing;
  have_ = 0;
  need_ = 0;
  payload_len_ = 0;
  frames_decoded_ = 0;
  crc_errors_ = 0;
  resyncs_ = 0;
}

void Decoder::PushBytes(const uint8_t* data, size_t len) {
  if (data == nullptr) return;
  for (size_t i = 0; i < len; ++i) HandleByte(data[i]);
}

void Decoder::RescanAfterFalseMagic() {
  uint8_t tmp[kHeaderSize];
  const size_t n = (have_ > 1) ? (have_ - 1) : 0;
  for (size_t i = 0; i < n; ++i) tmp[i] = buf_[i + 1];
  state_ = State::kSyncing;
  have_ = 0;
  for (size_t i = 0; i < n; ++i) HandleByte(tmp[i]);
}

void Decoder::HandleByte(uint8_t b) {
  switch (state_) {
    case State::kSyncing: {
      if (have_ == 0) {
        if (b == static_cast<uint8_t>(kMagic & 0xFF)) buf_[have_++] = b;
      } else {
        if (b == static_cast<uint8_t>(kMagic >> 8)) {
          buf_[have_++] = b;
          state_ = State::kHeader;
          need_ = kHeaderSize;
        } else {
          have_ = (b == static_cast<uint8_t>(kMagic & 0xFF)) ? 1 : 0;
          if (have_ == 1) buf_[0] = b;
          ++resyncs_;
        }
      }
      break;
    }
    case State::kHeader: {
      buf_[have_++] = b;
      if (have_ < need_) break;
      if (buf_[2] != kVersion || Crc16Ccitt(buf_, 8) != GetU16(buf_ + 8) ||
          GetU16(buf_ + 6) > kMaxPayload) {
        ++crc_errors_;
        RescanAfterFalseMagic();
        break;
      }
      payload_len_ = GetU16(buf_ + 6);
      state_ = State::kPayload;
      need_ = kHeaderSize + payload_len_ + kPayloadCrcSize;
      break;
    }
    case State::kPayload: {
      buf_[have_++] = b;
      if (have_ < need_) break;
      const uint16_t want = GetU16(buf_ + kHeaderSize + payload_len_);
      if (Crc16Ccitt(buf_ + kHeaderSize, payload_len_) != want) {
        ++crc_errors_;
      } else {
        ++frames_decoded_;
        if (handler_ != nullptr) {
          DecodedFrame f;
          f.type = static_cast<MsgType>(buf_[3]);
          f.seq = GetU16(buf_ + 4);
          f.payload = (payload_len_ > 0) ? (buf_ + kHeaderSize) : nullptr;
          f.payload_len = payload_len_;
          handler_(f, ctx_);
        }
      }
      state_ = State::kSyncing;
      have_ = 0;
      break;
    }
  }
}

}  // namespace proto
}  // namespace kws
