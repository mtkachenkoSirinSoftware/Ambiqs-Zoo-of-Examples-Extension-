// SPDX-License-Identifier: Apache-2.0
// Length-prefixed nanopb RPC (usb_rpc wire). INFER runs KwsApp on 16000 int16.
#include "kws_uart_rpc.h"

#include <string.h>

#include "app/kws_app.h"
#include "config/kws_config.h"
#include "kws_uart_usb.h"
#include "model/model_runner.h"
#include "nsx_rpc.pb.h"

#include "am_mcu_apollo.h"
#include "nsx_core.h"
#include "nsx_mem.h"
#include "pb_decode.h"
#include "pb_encode.h"

#include "profiling/kws_swo_perf.h"

#define NSX_RPC_FRAME_HDR_LEN 4
#define KWS_RPC_MAX_BYTES ((uint32_t)NsxRpcMessage_size)

static NSX_MEM_SRAM_BSS NsxRpcMessage g_req;
static NSX_MEM_SRAM_BSS NsxRpcMessage g_resp;
static NSX_MEM_SRAM_BSS uint8_t g_rx_frame[KWS_RPC_MAX_BYTES];
static NSX_MEM_SRAM_BSS uint8_t g_tx_payload[256];
static uint8_t g_hdr[NSX_RPC_FRAME_HDR_LEN];

typedef enum { kRxHdr = 0, kRxBody } RxState;
static RxState g_rx_state = kRxHdr;
static uint32_t g_rx_need = NSX_RPC_FRAME_HDR_LEN;
static uint32_t g_rx_got = 0;
static uint32_t g_rx_body = 0;

static uint32_t UptimeMs(void) { return am_hal_stimer_counter_get() / 3000u; }

static void ResetRx(void) {
  g_rx_state = kRxHdr;
  g_rx_need = NSX_RPC_FRAME_HDR_LEN;
  g_rx_got = 0;
  g_rx_body = 0;
}

static bool FeedByteStream(uint32_t* frame_len) {
  while (KwsUartUsbConnected()) {
    uint8_t* dst;
    uint32_t room;
    if (g_rx_state == kRxHdr) {
      dst = g_hdr + g_rx_got;
      room = NSX_RPC_FRAME_HDR_LEN - g_rx_got;
    } else {
      dst = g_rx_frame + g_rx_got;
      room = g_rx_body - g_rx_got;
    }
    const uint32_t n = KwsUartUsbRead(dst, room);
    if (n == 0u) {
      break;
    }
    g_rx_got += n;
    if (g_rx_state == kRxHdr && g_rx_got == NSX_RPC_FRAME_HDR_LEN) {
      const uint32_t len = (uint32_t)g_hdr[0] | ((uint32_t)g_hdr[1] << 8) |
                           ((uint32_t)g_hdr[2] << 16) | ((uint32_t)g_hdr[3] << 24);
      if (len == 0u || len > KWS_RPC_MAX_BYTES) {
        nsx_printf("RPC framing: bad length %u — resync\n", (unsigned)len);
        ResetRx();
        continue;
      }
      g_rx_body = len;
      g_rx_state = kRxBody;
      g_rx_got = 0;
    } else if (g_rx_state == kRxBody && g_rx_got == g_rx_body) {
      *frame_len = g_rx_body;
      ResetRx();
      return true;
    }
  }
  return false;
}

static void CopyLabel(NsxInferResponse* out, const char* name) {
  uint16_t n = 0;
  while (name[n] != '\0' && n < sizeof(out->label.bytes) - 1u) {
    ++n;
  }
  out->label.size = n;
  memcpy(out->label.bytes, name, n);
}

static void HandlePing(void) {
  g_resp.type = NsxRpcMsgType_NSX_MSG_PING_RESP;
  g_resp.which_payload = NsxRpcMessage_ping_resp_tag;
  g_resp.payload.ping_resp.seq = g_req.payload.ping_req.seq;
  g_resp.payload.ping_resp.uptime_ms = UptimeMs();
}

static void HandleStatus(void) {
  static const char kBoard[] = "kws_uart";
  g_resp.type = NsxRpcMsgType_NSX_MSG_STATUS_RESP;
  g_resp.which_payload = NsxRpcMessage_status_resp_tag;
  g_resp.payload.status_resp.firmware_version = 0x00010000u;
  g_resp.payload.status_resp.free_heap_bytes = 0;
  g_resp.payload.status_resp.board_name.size = (pb_size_t)(sizeof(kBoard) - 1u);
  memcpy(g_resp.payload.status_resp.board_name.bytes, kBoard, sizeof(kBoard) - 1u);
}

static bool HandleInfer(kws::KwsApp* app, uint32_t now_ms) {
  const NsxInferRequest* req = &g_req.payload.infer_req;
  g_resp.type = NsxRpcMsgType_NSX_MSG_INFER_RESP;
  g_resp.which_payload = NsxRpcMessage_infer_resp_tag;
  g_resp.payload.infer_resp.model_id = req->model_id;
  g_resp.payload.infer_resp.class_id = 0;
  g_resp.payload.infer_resp.confidence = 0.0f;
  CopyLabel(&g_resp.payload.infer_resp, "-");

  if (req->input.size != (KWS_CLIP_SAMPLES * 2u)) {
    nsx_printf("RPC INFER: want %u bytes of int16, got %u (not toy usb_rpc)\n",
               (unsigned)(KWS_CLIP_SAMPLES * 2u), (unsigned)req->input.size);
    return false;
  }

  const AudioSample* pcm = reinterpret_cast<const AudioSample*>(req->input.bytes);
  (void)app->ResetSession();
  uint32_t t = now_ms;
  for (uint32_t hop = 0; hop < (uint32_t)(KWS_CLIP_SAMPLES / KWS_AUDIO_BLOCK_SAMPLES); ++hop) {
    app->OnAudioBlock(pcm + hop * KWS_AUDIO_BLOCK_SAMPLES, KWS_AUDIO_BLOCK_SAMPLES, t);
    t += KWS_AUDIO_BLOCK_MS;
    if (app->InferenceDue()) {
      (void)app->RunInference(t);
    }
  }
  (void)app->FlushInference(t);

  const int lab = app->last_label();
  const char* name = (lab >= 0 && lab < KWS_NUM_CLASSES) ? kws::kLabels[lab] : "-";
  g_resp.payload.infer_resp.class_id = (lab >= 0) ? static_cast<uint32_t>(lab) : 0u;
  g_resp.payload.infer_resp.confidence = app->last_score();
  CopyLabel(&g_resp.payload.infer_resp, name);

  nsx_printf("pred=%s score=%.3f fired=%d", name, app->last_score(),
             app->last_event().fired ? 1 : 0);
  KwsPrintInvokeTail(app->telemetry().inference_cycles_last);
  nsx_printf("  RPC INFER is kws_core on this 1 s buffer; not usb_rpc toy classes\n");
  return true;
}

static bool Dispatch(kws::KwsApp* app, uint32_t now_ms, uint32_t rx_len) {
  g_req = NsxRpcMessage_init_default;
  pb_istream_t in = pb_istream_from_buffer(g_rx_frame, rx_len);
  if (!pb_decode(&in, NsxRpcMessage_fields, &g_req)) {
    nsx_printf("RPC: decode error: %s\n", PB_GET_ERROR(&in));
    return false;
  }
  g_resp = NsxRpcMessage_init_default;
  switch (g_req.type) {
    case NsxRpcMsgType_NSX_MSG_PING_REQ:
      HandlePing();
      break;
    case NsxRpcMsgType_NSX_MSG_STATUS_REQ:
      HandleStatus();
      break;
    case NsxRpcMsgType_NSX_MSG_INFER_REQ:
      (void)HandleInfer(app, now_ms);
      break;
    default:
      nsx_printf("RPC: unknown msg type %d\n", (int)g_req.type);
      return false;
  }
  pb_ostream_t out = pb_ostream_from_buffer(g_tx_payload, sizeof(g_tx_payload));
  if (!pb_encode(&out, NsxRpcMessage_fields, &g_resp)) {
    nsx_printf("RPC: encode error: %s\n", PB_GET_ERROR(&out));
    return false;
  }
  const uint32_t n = (uint32_t)out.bytes_written;
  g_hdr[0] = (uint8_t)n;
  g_hdr[1] = (uint8_t)(n >> 8);
  g_hdr[2] = (uint8_t)(n >> 16);
  g_hdr[3] = (uint8_t)(n >> 24);
  return KwsUartUsbSend(g_hdr, NSX_RPC_FRAME_HDR_LEN) && KwsUartUsbSend(g_tx_payload, n);
}

bool KwsUartRpcInit(void) {
  ResetRx();
  return KwsUartUsbInit();
}

bool KwsUartRpcPoll(kws::KwsApp* app, uint32_t now_ms) {
  if (app == nullptr) {
    return false;
  }
  if (!KwsUartUsbConnected()) {
    ResetRx();
    return false;
  }
  uint32_t frame_len = 0;
  if (!FeedByteStream(&frame_len)) {
    return false;
  }
  return Dispatch(app, now_ms, frame_len);
}
