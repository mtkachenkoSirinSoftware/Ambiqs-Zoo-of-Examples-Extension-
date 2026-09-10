// SPDX-License-Identifier: Apache-2.0
// Framed UART transport. Byte-stream synchronisation is explicit: the decoder
// resynchronises on MAGIC after any corruption instead of trusting alignment.
//
// Wire format, little-endian:
//   0  u16 MAGIC   0xA51C
//   2  u8  VERSION 1
//   3  u8  TYPE
//   4  u16 SEQ
//   6  u16 PAYLOAD_LEN   (<= kMaxPayload)
//   8  u16 HEADER_CRC    CRC-16/CCITT-FALSE over bytes 0..7
//   10 ... payload
//      u16 PAYLOAD_CRC   CRC-16/CCITT-FALSE over payload
#ifndef UART_PROTOCOL_H_
#define UART_PROTOCOL_H_

#include <stddef.h>
#include <stdint.h>

namespace kws {
namespace proto {

constexpr uint16_t kMagic = 0xA51C;
constexpr uint8_t kVersion = 1;
constexpr size_t kHeaderSize = 10;
constexpr size_t kPayloadCrcSize = 2;
constexpr size_t kMaxPayload = 1024;

enum class MsgType : uint8_t {
  kStartStream = 0x01,
  kAudioBlock = 0x02,
  kEndStream = 0x03,
  kReset = 0x04,
  kGetStatus = 0x05,
  kStatus = 0x81,
  kPrediction = 0x82,
  kAck = 0x83,
  kNack = 0x84,
};

uint16_t Crc16Ccitt(const uint8_t* data, size_t len);

size_t Encode(MsgType type, uint16_t seq, const uint8_t* payload, size_t payload_len,
              uint8_t* out, size_t out_cap);

constexpr size_t FrameSize(size_t payload_len) {
  return kHeaderSize + payload_len + kPayloadCrcSize;
}

struct DecodedFrame {
  MsgType type = MsgType::kNack;
  uint16_t seq = 0;
  const uint8_t* payload = nullptr;
  size_t payload_len = 0;
};

class Decoder {
 public:
  using FrameHandler = void (*)(const DecodedFrame&, void* ctx);

  void Init(FrameHandler handler, void* ctx);
  void Reset();
  void PushBytes(const uint8_t* data, size_t len);

  uint32_t frames_decoded() const { return frames_decoded_; }
  uint32_t crc_errors() const { return crc_errors_; }
  uint32_t resyncs() const { return resyncs_; }

 private:
  enum class State { kSyncing, kHeader, kPayload };

  void HandleByte(uint8_t b);
  void RescanAfterFalseMagic();

  State state_ = State::kSyncing;
  uint8_t buf_[kHeaderSize + kMaxPayload + kPayloadCrcSize] = {};
  size_t have_ = 0;
  size_t need_ = 0;
  uint16_t payload_len_ = 0;

  FrameHandler handler_ = nullptr;
  void* ctx_ = nullptr;
  uint32_t frames_decoded_ = 0;
  uint32_t crc_errors_ = 0;
  uint32_t resyncs_ = 0;
};

}  // namespace proto
}  // namespace kws
#endif  // UART_PROTOCOL_H_
