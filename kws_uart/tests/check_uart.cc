// SPDX-License-Identifier: Apache-2.0
// Host-only checks: CRC, clip session PCM identity, Python encoder interop.
#include "audio/uart_clip_buffer.h"
#include "audio/uart_clip_session.h"
#include "protocol/uart_protocol.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using kws::UartClipSession;
using kws::proto::Crc16Ccitt;
using kws::proto::DecodedFrame;
using kws::proto::Decoder;
using kws::proto::Encode;
using kws::proto::FrameSize;
using kws::proto::MsgType;

#define CHECK(cond)                                                                         \
  do {                                                                                      \
    if (!(cond)) {                                                                          \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                  \
      return 1;                                                                             \
    }                                                                                       \
  } while (0)

namespace {

struct Sink {
  std::vector<MsgType> types;
  std::vector<uint16_t> seqs;
  std::vector<std::vector<uint8_t>> payloads;
  static void Handle(const DecodedFrame& f, void* ctx) {
    auto* s = static_cast<Sink*>(ctx);
    s->types.push_back(f.type);
    s->seqs.push_back(f.seq);
    s->payloads.emplace_back(f.payload, f.payload + f.payload_len);
  }
};

std::vector<uint8_t> Ctrl(MsgType t, uint16_t seq) {
  std::vector<uint8_t> out(FrameSize(0));
  out.resize(Encode(t, seq, nullptr, 0, out.data(), out.size()));
  return out;
}

std::vector<uint8_t> AudioHop(uint16_t seq, const AudioSample* hop) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(hop);
  const size_t nbytes = static_cast<size_t>(KWS_AUDIO_BLOCK_SAMPLES) * sizeof(AudioSample);
  std::vector<uint8_t> out(FrameSize(nbytes));
  out.resize(Encode(MsgType::kAudioBlock, seq, p, nbytes, out.data(), out.size()));
  return out;
}

bool LoadGoldenPcm(const std::string& path, std::vector<AudioSample>* pcm) {
  std::ifstream in(path);
  if (!in) return false;
  std::string tag;
  int n_samples = 0, frames = 0, coeff = 0;
  in >> tag >> n_samples >> frames >> coeff;
  if (!in || n_samples <= 0) return false;
  pcm->resize(static_cast<size_t>(n_samples));
  for (int i = 0; i < n_samples; ++i) {
    int v = 0;
    in >> v;
    (*pcm)[static_cast<size_t>(i)] = static_cast<AudioSample>(v);
  }
  return static_cast<bool>(in);
}

std::vector<uint8_t> WireClip(const std::vector<AudioSample>& pcm, bool with_reset) {
  std::vector<uint8_t> wire;
  auto append = [&](const std::vector<uint8_t>& f) {
    wire.insert(wire.end(), f.begin(), f.end());
  };
  if (with_reset) append(Ctrl(MsgType::kReset, 0));
  append(Ctrl(MsgType::kStartStream, 0));
  const uint32_t hops = static_cast<uint32_t>(pcm.size()) / KWS_AUDIO_BLOCK_SAMPLES;
  for (uint32_t i = 0; i < hops; ++i) {
    append(AudioHop(static_cast<uint16_t>(i + 1), pcm.data() + i * KWS_AUDIO_BLOCK_SAMPLES));
  }
  append(Ctrl(MsgType::kEndStream, static_cast<uint16_t>(hops + 1)));
  return wire;
}

}  // namespace

int main() {
  const char* vec = "123456789";
  CHECK(Crc16Ccitt(reinterpret_cast<const uint8_t*>(vec), 9) == 0x29B1);

  UartClipBuffer b{};
  UartClipBufferBegin(&b);
  AudioSample hop[KWS_AUDIO_BLOCK_SAMPLES];
  for (int i = 0; i < KWS_AUDIO_BLOCK_SAMPLES; ++i) hop[i] = static_cast<AudioSample>(i);
  UartClipBufferPush(&b, hop, KWS_AUDIO_BLOCK_SAMPLES);
  UartClipBufferFinalize(&b);
  CHECK(UartClipBufferTakeReady(&b));
  CHECK(UartClipBufferFilled(&b) == static_cast<uint32_t>(KWS_CLIP_SAMPLES));
  CHECK(UartClipBufferPcm(&b)[319] == 319);
  CHECK(UartClipBufferPcm(&b)[320] == 0);

  const char* golden = std::getenv("KWS_GOLDEN_SYNTHETIC");
  CHECK(golden != nullptr);
  std::vector<AudioSample> pcm;
  CHECK(LoadGoldenPcm(golden, &pcm));
  CHECK(pcm.size() == static_cast<size_t>(KWS_CLIP_SAMPLES));

  UartClipSession s;
  s.Init();
  const auto wire = WireClip(pcm, true);
  s.PushBytes(wire.data(), wire.size());
  CHECK(s.buffer().dropped == 0u);
  CHECK(s.TakeReset());

  std::vector<AudioSample> got;
  while (s.BlockReady()) {
    uint32_t n = 0;
    const AudioSample* blk = s.AcquireBlock(&n);
    CHECK(blk != nullptr);
    got.insert(got.end(), blk, blk + n);
    s.ReleaseBlock();
  }
  CHECK(s.TakeStreamEnd());
  CHECK(got == pcm);

  const char* py_wire = std::getenv("KWS_UART_PY_WIRE");
  if (py_wire != nullptr) {
    std::ifstream in(py_wire, std::ios::binary);
    CHECK(static_cast<bool>(in));
    const std::vector<uint8_t> blob((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    CHECK(!blob.empty());
    Sink sink;
    Decoder d;
    d.Init(&Sink::Handle, &sink);
    d.PushBytes(blob.data(), blob.size());
    CHECK(sink.types.size() == 3u);
    CHECK(sink.types[0] == MsgType::kStartStream);
    CHECK(sink.types[1] == MsgType::kAudioBlock);
    CHECK(sink.types[2] == MsgType::kEndStream);
    CHECK(d.crc_errors() == 0u);
    CHECK(sink.payloads[1].size() == KWS_AUDIO_BLOCK_SAMPLES * sizeof(int16_t));
    for (size_t i = 0; i < KWS_AUDIO_BLOCK_SAMPLES; ++i) {
      const int16_t sample = static_cast<int16_t>(
          static_cast<uint16_t>(sink.payloads[1][i * 2]) |
          (static_cast<uint16_t>(sink.payloads[1][i * 2 + 1]) << 8));
      CHECK(sample == static_cast<int16_t>(i * 101 - 16000));
    }
  }

  std::printf("kws_uart host checks OK\n");
  return 0;
}
