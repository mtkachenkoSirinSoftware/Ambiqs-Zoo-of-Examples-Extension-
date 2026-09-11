// SPDX-License-Identifier: Apache-2.0
// Copied from neuralspotx/examples/kws_infer (same PMU hooks). Used only on
// kws_clip. Preserves DWT CYCCNT so invoke_cycles= stays the app Invoke.
#include "nsx_pmu_profiler.h"

#include "am_mcu_apollo.h"
#include "nsx_core.h"
#include "nsx_pmu_map.h"

void NsxPmuProfiler::Init(nsx_pmu_preset_e preset) {
  nsx_pmu_reset_config(&pmu_cfg_);
  pmu_cfg_.api = &nsx_pmu_V1_0_0;
  nsx_pmu_apply_preset(&pmu_cfg_, preset);
  nsx_pmu_init(&pmu_cfg_);
  num_events_ = 0;
  initialized_ = true;
  layer_hooks_ = true;
}

void NsxPmuProfiler::SetLayerHooks(bool on) { layer_hooks_ = on; }

uint32_t NsxPmuProfiler::BeginEvent(const char* tag) {
  if (!initialized_ || !layer_hooks_ || num_events_ >= kMaxLayers) {
    return 0;
  }
  int idx = num_events_++;
  layers_[idx].tag = tag;
  uint32_t saved_cyccnt = DWT->CYCCNT;
  nsx_pmu_reset_counters();
  DWT->CYCCNT = saved_cyccnt;
  return static_cast<uint32_t>(idx);
}

void NsxPmuProfiler::EndEvent(uint32_t event_handle) {
  if (!initialized_ || !layer_hooks_ ||
      event_handle >= static_cast<uint32_t>(num_events_)) {
    return;
  }
  uint32_t saved_cyccnt = DWT->CYCCNT;
  nsx_pmu_get_counters(&pmu_cfg_);
  DWT->CYCCNT = saved_cyccnt;
  LayerRecord& rec = layers_[event_handle];
  for (int i = 0; i < kNumEvents; i++) {
    rec.counters[i] = pmu_cfg_.counter[i].counterValue;
  }
}

void NsxPmuProfiler::ClearEvents() { num_events_ = 0; }

void NsxPmuProfiler::BeginWholeInvokeMeasure() {
  SetLayerHooks(false);
  nsx_pmu_reset_config(&pmu_cfg_);
  pmu_cfg_.api = &nsx_pmu_V1_0_0;
  nsx_pmu_event_create(&pmu_cfg_.events[0], ARM_PMU_CPU_CYCLES, NSX_PMU_EVENT_COUNTER_SIZE_32);
  nsx_pmu_event_create(&pmu_cfg_.events[1], ARM_PMU_INST_RETIRED, NSX_PMU_EVENT_COUNTER_SIZE_32);
  nsx_pmu_init(&pmu_cfg_);
  nsx_pmu_reset_counters();
  dwt_start_ = DWT->CYCCNT;
}

void NsxPmuProfiler::EndWholeInvokeMeasure() {
  dwt_cycles_ = DWT->CYCCNT - dwt_start_;
  nsx_pmu_get_counters(&pmu_cfg_);
  pmu_cycles_ = pmu_cfg_.counter[0].counterValue;
  inst_retired_ = pmu_cfg_.counter[1].counterValue;
  measured_ = true;
  Init(NSX_PMU_PRESET_ML_DEFAULT);
}

void NsxPmuProfiler::PrintCsv() const {
  if (!initialized_ || num_events_ == 0) {
    return;
  }
  nsx_printf("\"Layer\",\"Op\"");
  for (int e = 0; e < kNumEvents; e++) {
    if (!pmu_cfg_.events[e].enabled) break;
    uint32_t map_idx = pmu_cfg_.counter[e].mapIndex;
    if (map_idx < NSX_PMU_MAP_SIZE) {
      nsx_printf(",\"%s\"", nsx_pmu_map[map_idx].regname);
    } else {
      nsx_printf(",\"event_%d\"", e);
    }
  }
  nsx_printf("\n");
  for (int i = 0; i < num_events_; i++) {
    const LayerRecord& rec = layers_[i];
    nsx_printf("%d,%s", i, rec.tag ? rec.tag : "?");
    for (int e = 0; e < kNumEvents; e++) {
      if (!pmu_cfg_.events[e].enabled) break;
      nsx_printf(",%lu", (unsigned long)rec.counters[e]);
    }
    nsx_printf("\n");
  }
}
