// SPDX-License-Identifier: Apache-2.0
// Per-layer PMU profiler for TFLM (same shape as neuralspotx/examples/kws_infer).
// Wired only into kws_clip Invoke. Not hpx profile, not always-on hop time.
#ifndef NSX_PMU_PROFILER_H
#define NSX_PMU_PROFILER_H

#include "tensorflow/lite/micro/micro_profiler_interface.h"
#include "nsx_pmu_utils.h"

class NsxPmuProfiler : public tflite::MicroProfilerInterface {
 public:
  static constexpr int kMaxLayers = 128;
  static constexpr int kNumEvents = 4;

  NsxPmuProfiler() = default;
  ~NsxPmuProfiler() override = default;

  void Init(nsx_pmu_preset_e preset = NSX_PMU_PRESET_ML_DEFAULT);
  uint32_t BeginEvent(const char* tag) override;
  void EndEvent(uint32_t event_handle) override;
  void ClearEvents();
  void PrintCsv() const;
  int num_events() const { return num_events_; }

 private:
  struct LayerRecord {
    const char* tag;
    uint32_t counters[kNumEvents];
  };

  nsx_pmu_config_t pmu_cfg_ = {};
  LayerRecord layers_[kMaxLayers] = {};
  int num_events_ = 0;
  bool initialized_ = false;
};

#endif
