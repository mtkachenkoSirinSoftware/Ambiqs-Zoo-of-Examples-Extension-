// SPDX-License-Identifier: Apache-2.0
// kws_pdm — EVB PDM MEMS through kws_core. Labels on SWO (`nsx view`).
// Live pred is the microphone, not the flash/UART clip.
// Preflight: neuralspotx audio_capture and/or AmbiqSuite pdm_rtt_stream.
#include "app/kws_app.h"
#include "audio/audio_source.h"
#include "config/kws_config.h"
#include "model/model_runner.h"
#include "profiling/kws_profile.h"
#include "profiling/kws_swo_perf.h"
#include "apollo510_audio.h"

#include "nsx_core.h"
#include "nsx_system.h"

#if defined(KWS_PDM_RTT_PCM)
#include "kws_pdm_rtt.h"
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

const char* LabelName(int lab) {
  return (lab >= 0 && lab < KWS_NUM_CLASSES) ? kws::kLabels[lab] : "-";
}

void PrintPred(const kws::KwsApp& app, const char* kind) {
  nsx_printf("%s pred=%s score=%.3f fired=%d", kind, LabelName(app.last_label()),
             app.last_score(), app.last_event().fired ? 1 : 0);
  KwsPrintInvokeTail(app.telemetry().inference_cycles_last);
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

#if defined(KWS_PDM_RTT_PCM)
  KwsPdmRttInit();
#endif

  nsx_printf("kws_pdm runtime=%s arena_used=%u\n", kws::ModelRunner::runtime_name(),
             static_cast<unsigned>(kws::ModelRunner::arena_used_bytes()));
  KwsPrintPerfBanner(kCfg.perf_mode);
#if defined(KWS_PDM_USE_NSX_AUDIO)
  nsx_printf("pdm backend=nsx-audio hop=%u fs=16000 clk=HFRC2_ADJ (not pdm_fft PLL)\n",
             (unsigned)KWS_AUDIO_BLOCK_SAMPLES);
  nsx_printf("gpio clk=%u data=%u\n",
             (unsigned)KWS_PDM_CLK_GPIO, (unsigned)KWS_PDM_DATA_GPIO);
#else
  nsx_printf("pdm backend=hal clk_out=%u fs=%u osr=%u gpio clk=%u data=%u lr_swap=%d\n",
             (unsigned)KWS_PDM_CLK_OUT_HZ, (unsigned)KWS_PDM_FS_HZ, (unsigned)KWS_PDM_OSR,
             (unsigned)KWS_PDM_CLK_GPIO, (unsigned)KWS_PDM_DATA_GPIO, (int)KWS_PDM_LR_SWAP);
#endif
  nsx_printf("model sha256=%.12s\n", KWS_MODEL_SHA256);
  nsx_printf("pred is live PDM (microphone, not the flash/UART clip)\n");
#if defined(KWS_PDM_RTT_PCM)
  nsx_printf("rtt ch1=PCM 16kHz int16 hop=%u (not labels; labels on SWO)\n",
             (unsigned)KWS_AUDIO_BLOCK_SAMPLES);
#endif

  uint32_t now_ms = 0;
  uint32_t last_stats_ms = 0;
  for (;;) {
    unsigned drained = 0;
    while (AudioSourceBlockReady() && drained < 8u) {
      uint32_t count = 0;
      const AudioSample* block = AudioSourceAcquireBlock(&count);
      if (block != nullptr) {
#if defined(KWS_PDM_RTT_PCM)
        KwsPdmRttWriteHop(block, count);
#endif
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
      // audio_capture shape: "Frame N  peak=..." — hop count, plus dc.
      nsx_printf("Frame %lu  peak=%d dc=%d overruns=%u dma_faults=%u derr=%u\n",
                 (unsigned long)st->blocks_received, (int)st->pcm_peak_abs, (int)st->pcm_dc,
                 (unsigned)st->overruns, (unsigned)st->dma_faults,
                 (unsigned)st->uart_errors);
      PrintPred(app, "live");
    }
  }
}
