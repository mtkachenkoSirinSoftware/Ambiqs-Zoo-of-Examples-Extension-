// SPDX-License-Identifier: Apache-2.0
// kws_uart — host WAV into kws_core. Labels on SWO (`nsx view`).
//
// Default: AM_BSP_UART_PRINT_INST @ 921600, FIFO poll, DTR/RTS low.
// -DKWS_UART_USB_CDC=ON: nsx-usb CDC, same 0xA51C, DTR=True (usb_serial).
// -DKWS_UART_USB_RPC=ON: nsx-usb RPC INFER of 16000 int16 through KwsApp.
// Their usb_rpc toy INFER is a different image — do not compare to pred=go.
#include "app/kws_app.h"
#include "config/kws_config.h"
#include "model/model_runner.h"
#include "profiling/kws_profile.h"
#include "profiling/kws_swo_perf.h"

#if !defined(KWS_UART_USB_RPC)
#include "audio/audio_source.h"
#include "apollo510_uart.h"
#endif

#include "nsx_core.h"
#include "nsx_system.h"

#if defined(KWS_UART_USB_RPC)
#include "kws_uart_rpc.h"
#endif

namespace {

const nsx_system_config_t kCfg = {
    .perf_mode = NSX_PERF_LOW,
    .enable_cache = true,
    .enable_sram = true,
    .debug = {.transport = NSX_DEBUG_ITM},
    .skip_bsp_init = true,
    .spot_mgr_profile = true,
};

#if !defined(KWS_UART_USB_RPC)
void PrintPred(const kws::KwsApp& app) {
  const int lab = app.last_label();
  const char* name = (lab >= 0 && lab < KWS_NUM_CLASSES) ? kws::kLabels[lab] : "-";
  nsx_printf("pred=%s score=%.3f fired=%d", name, app.last_score(),
             app.last_event().fired ? 1 : 0);
  KwsPrintInvokeTail(app.telemetry().inference_cycles_last);
}
#endif

}  // namespace

int main(void) {
  NSX_TRY(nsx_system_init(&kCfg), "System init failed\n");
  nsx_itm_printf_enable();
  KwsProfileInit();

  kws::KwsApp app;
  if (!app.Init()) {
    nsx_printf("ERROR: KwsApp::Init failed\n");
    for (;;) {
    }
  }

  nsx_printf("kws_uart runtime=%s arena_used=%u\n", kws::ModelRunner::runtime_name(),
             static_cast<unsigned>(kws::ModelRunner::arena_used_bytes()));
  KwsPrintPerfBanner(kCfg.perf_mode);
#if defined(KWS_UART_USB_RPC)
  nsx_printf("usb rpc INFER=16000 int16 KwsApp; DTR=True; VID/PID 0xCafe/0x4011; labels on SWO\n");
  nsx_printf("  this INFER is kws_core; neuralspotx usb_rpc INFER is a 5-class toy on another image\n");
#elif defined(KWS_UART_USB_CDC)
  nsx_printf("uart cdc nsx-usb 0xA51C; DTR=True; VID/PID 0xCafe/0x4011; PCM on CDC; labels on SWO\n");
#else
  nsx_printf("uart poll-fifo then 1s RAM infer @ %u; PCM on PRINT UART; labels on SWO\n",
             (unsigned)KWS_UART_BAUD);
#endif
  nsx_printf("model sha256=%.12s  (host LiteRT on identical PCM: host/expected.json)\n",
             KWS_MODEL_SHA256);
  nsx_printf("labels=tfds index 1=go (not kws_infer MLPerf index 11=go); assets/LABELS.md\n");

#if defined(KWS_UART_USB_RPC)
  if (!KwsUartRpcInit()) {
    nsx_printf("ERROR: USB RPC init failed\n");
    for (;;) {
    }
  }
  nsx_printf("usb rpc ready (wait for host DTR)\n");
  uint32_t now_ms = 0;
  for (;;) {
    if (KwsUartRpcPoll(&app, now_ms)) {
      now_ms += 1000u;
    }
  }
#else
  if (!AudioSourceInit() || !AudioSourceStart()) {
    nsx_printf("ERROR: UART source failed\n");
    for (;;) {
    }
  }

  uint32_t now_ms = 0;
  for (;;) {
    if (AudioSourceTakeReset()) {
      nsx_printf("reset\n");
      (void)app.ResetSession();
    }

    unsigned drained = 0;
    while (AudioSourceBlockReady() && drained < 8u) {
      uint32_t count = 0;
      const AudioSample* block = AudioSourceAcquireBlock(&count);
      if (block != nullptr) {
        app.OnAudioBlock(block, count, now_ms);
      }
      AudioSourceReleaseBlock();
      now_ms += KWS_AUDIO_BLOCK_MS;
      ++drained;
      if (app.InferenceDue()) {
        app.RunInference(now_ms);
      }
    }

    if (AudioSourceTakeStreamBegin()) {
      nsx_printf("uart_clip samples=%u dropped=%u poll=%u bytes=%u\n",
                 (unsigned)KWS_CLIP_SAMPLES, (unsigned)Apollo510UartClipDropped(),
                 (unsigned)Apollo510UartCbCount(), (unsigned)Apollo510UartRxBytes());
    }

    if (app.InferenceDue()) {
      app.RunInference(now_ms);
    }

    if (AudioSourceTakeStreamEnd()) {
      (void)app.FlushInference(now_ms);
      PrintPred(app);
    }
  }
#endif
}
