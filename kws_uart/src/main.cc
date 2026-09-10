// SPDX-License-Identifier: Apache-2.0
// kws_uart — host WAV over FIFO-poll UART into kws_core. Labels on SWO.
//
// PCM is AM_BSP_UART_PRINT_INST @ 921600. Do not nsx_printf on that UART.
// MCU pred= is comparable to host LiteRT only on identical PCM (host/expected.json).
// This is not PDM and not dummy-input kws_infer.
#include "app/kws_app.h"
#include "audio/audio_source.h"
#include "config/kws_config.h"
#include "model/model_runner.h"
#include "profiling/kws_profile.h"
#include "apollo510_uart.h"

#include "nsx_core.h"
#include "nsx_system.h"

namespace {

const nsx_system_config_t kCfg = {
    .perf_mode = NSX_PERF_LOW,
    .enable_cache = true,
    .enable_sram = true,
    .debug = {.transport = NSX_DEBUG_ITM},
    .skip_bsp_init = true,
    .spot_mgr_profile = true,
};

void PrintPred(const kws::KwsApp& app) {
  const int lab = app.last_label();
  const char* name = (lab >= 0 && lab < KWS_NUM_CLASSES) ? kws::kLabels[lab] : "-";
  nsx_printf("pred=%s score=%.3f fired=%d invoke_cycles=%u\n", name, app.last_score(),
             app.last_event().fired ? 1 : 0,
             static_cast<unsigned>(app.telemetry().inference_cycles_last));
  nsx_printf("  invoke_cycles is DWT CYCCNT of this binary's Invoke, not hpx profile\n");
}

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
  if (!AudioSourceInit() || !AudioSourceStart()) {
    nsx_printf("ERROR: UART source failed\n");
    for (;;) {
    }
  }

  nsx_printf("kws_uart runtime=%s arena_used=%u\n", kws::ModelRunner::runtime_name(),
             static_cast<unsigned>(kws::ModelRunner::arena_used_bytes()));
  nsx_printf("uart poll-fifo then 1s RAM infer @ %u; PCM on PRINT UART; labels on SWO\n",
             (unsigned)KWS_UART_BAUD);
  nsx_printf("model sha256=%.12s  (host LiteRT on identical PCM: host/expected.json)\n",
             KWS_MODEL_SHA256);

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
}
