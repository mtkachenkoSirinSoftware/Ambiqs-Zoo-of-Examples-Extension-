// SPDX-License-Identifier: Apache-2.0
// kws_pdm — EVB PDM MEMS through kws_core. Labels on SWO (`nsx view`).
//
// Live pred is not GATE 3 and not comparable to LiteRT on a GSC WAV.
// Bring up AmbiqSuite pdm_rtt_stream / pdm_fft on GPIO 50/51 first.
#include "app/kws_app.h"
#include "audio/audio_source.h"
#include "config/kws_config.h"
#include "model/model_runner.h"
#include "profiling/kws_profile.h"
#include "apollo510_audio.h"

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

const char* LabelName(int lab) {
  return (lab >= 0 && lab < KWS_NUM_CLASSES) ? kws::kLabels[lab] : "-";
}

void PrintPred(const kws::KwsApp& app, const char* kind) {
  nsx_printf("%s pred=%s score=%.3f fired=%d invoke_cycles=%u\n", kind,
             LabelName(app.last_label()), app.last_score(),
             app.last_event().fired ? 1 : 0,
             static_cast<unsigned>(app.telemetry().inference_cycles_last));
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
    nsx_printf("ERROR: PDM source failed\n");
    for (;;) {
    }
  }

  nsx_printf("kws_pdm runtime=%s arena_used=%u\n", kws::ModelRunner::runtime_name(),
             static_cast<unsigned>(kws::ModelRunner::arena_used_bytes()));
  nsx_printf("pdm clk_out=%u fs=%u osr=%u gpio clk=%u data=%u lr_swap=%d\n",
             (unsigned)KWS_PDM_CLK_OUT_HZ, (unsigned)KWS_PDM_FS_HZ, (unsigned)KWS_PDM_OSR,
             (unsigned)KWS_PDM_CLK_GPIO, (unsigned)KWS_PDM_DATA_GPIO, (int)KWS_PDM_LR_SWAP);
  nsx_printf("model sha256=%.12s\n", KWS_MODEL_SHA256);
  nsx_printf("pred is live PDM, not GATE 3, not a GSC clip, not hpx profile\n");

  uint32_t now_ms = 0;
  uint32_t last_stats_ms = 0;
  for (;;) {
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
        const bool fired = app.RunInference(now_ms);
        if (fired) {
          nsx_printf("event label=%s score=%.3f\n", LabelName(app.last_event().label),
                     app.last_event().score);
        }
      }
    }

    if (app.InferenceDue()) {
      const bool fired = app.RunInference(now_ms);
      if (fired) {
        nsx_printf("event label=%s score=%.3f\n", LabelName(app.last_event().label),
                   app.last_event().score);
      }
    }

    if (now_ms - last_stats_ms >= 1000u) {
      last_stats_ms = now_ms;
      const AudioSourceStatus* st = AudioSourceGetStatus();
      nsx_printf("pdm peak=%d dc=%d overruns=%u dma_faults=%u derr=%u\n",
                 (int)st->pcm_peak_abs, (int)st->pcm_dc, (unsigned)st->overruns,
                 (unsigned)st->dma_faults, (unsigned)st->uart_errors);
      PrintPred(app, "live");
      nsx_printf("  invoke_cycles is DWT CYCCNT of this binary's Invoke, not hpx profile\n");
    }
  }
}
