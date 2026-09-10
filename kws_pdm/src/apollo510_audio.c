// SPDX-License-Identifier: Apache-2.0
// Apollo510 PDM capture. See apollo510_audio.h.
#include "apollo510_audio.h"

#include "apollo510_cache.h"
#include "audio/audio_source.h"

#ifdef KWS_TARGET_APOLLO510
#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#endif

#define KWS_PDM_BLOCK_SAMPLES KWS_AUDIO_BLOCK_SAMPLES

#ifndef AM_SHARED_RW
#define AM_SHARED_RW
#endif

AM_SHARED_RW static uint32_t g_dma_a[KWS_PDM_BLOCK_SAMPLES] __attribute__((aligned(32)));
AM_SHARED_RW static uint32_t g_dma_b[KWS_PDM_BLOCK_SAMPLES] __attribute__((aligned(32)));

static AudioSourceStatus g_status;
static AudioSample g_pcm_block[KWS_PDM_BLOCK_SAMPLES];
static volatile const uint32_t* g_completed;

#ifdef KWS_TARGET_APOLLO510
static void* g_pdm_handle;

static am_hal_pdm_config_t g_pdm_config = {
    .eClkDivider = AM_HAL_PDM_MCLKDIV_1,
    .ePDMAClkOutDivder = AM_HAL_PDM_PDMA_CLKO_DIV5,
    .eLeftGain = AM_HAL_PDM_GAIN_0DB,
    .eRightGain = AM_HAL_PDM_GAIN_0DB,
    .eStepSize = AM_HAL_PDM_GAIN_STEP_0_13DB,
    .ui32DecimationRate = KWS_PDM_DECIMATION,
    .bHighPassEnable = AM_HAL_PDM_HIGH_PASS_ENABLE,
    .ui32HighPassCutoff = 10,
    .ePDMClkSpeed = AM_HAL_PDM_CLK_PLL,
    .bDataPacking = 1,
    .ePCMChannels = AM_HAL_PDM_CHANNEL_LEFT,
    .bPDMSampleDelay = AM_HAL_PDM_CLKOUT_PHSDLY_NONE,
    .ui32GainChangeDelay = AM_HAL_PDM_CLKOUT_DELAY_NONE,
    .bSoftMute = 0,
    .bLRSwap = KWS_PDM_LR_SWAP,
};

static am_hal_pdm_transfer_t g_transfer = {
    .ui32TargetAddr = (uint32_t)g_dma_a,
    .ui32TargetAddrReverse = (uint32_t)g_dma_b,
    .ui32TotalCount = KWS_PDM_BLOCK_SAMPLES * sizeof(uint32_t),
};

static const uint32_t kPdmIrqMask =
    AM_HAL_PDM_INT_DCMP | AM_HAL_PDM_INT_DERR | AM_HAL_PDM_INT_UNDFL | AM_HAL_PDM_INT_OVF;
#endif

bool Apollo510AudioInit(void) {
  for (int i = 0; i < 2; ++i) g_status.state[i] = kBufferFree;
  g_status.blocks_received = 0;
  g_status.blocks_processed = 0;
  g_status.overruns = 0;
  g_status.uart_errors = 0;
  g_status.dma_faults = 0;
  g_status.pcm_peak_abs = 0;
  g_status.pcm_dc = 0;
  g_completed = NULL;

#ifdef KWS_TARGET_APOLLO510
  if (am_hal_clkmgr_clock_config(AM_HAL_CLKMGR_CLK_ID_SYSPLL, KWS_PDM_SRC_HZ, NULL) !=
      AM_HAL_STATUS_SUCCESS) {
    return false;
  }
  am_bsp_pdm_pins_enable(KWS_PDM_MODULE);
  if (am_hal_pdm_initialize(KWS_PDM_MODULE, &g_pdm_handle) != AM_HAL_STATUS_SUCCESS) {
    return false;
  }
  if (am_hal_pdm_power_control(g_pdm_handle, AM_HAL_PDM_POWER_ON, false) !=
      AM_HAL_STATUS_SUCCESS) {
    return false;
  }
  if (am_hal_pdm_configure(g_pdm_handle, &g_pdm_config) != AM_HAL_STATUS_SUCCESS) {
    return false;
  }
  am_hal_pdm_fifo_threshold_setup(g_pdm_handle, KWS_PDM_FIFO_THRESHOLD);
  am_hal_pdm_interrupt_clear(g_pdm_handle, kPdmIrqMask);
  am_hal_pdm_interrupt_enable(g_pdm_handle, kPdmIrqMask);
  NVIC_SetPriority(PDM0_IRQn, AM_IRQ_PRIORITY_DEFAULT);
  NVIC_EnableIRQ(PDM0_IRQn);
  if (am_hal_pdm_enable(g_pdm_handle) != AM_HAL_STATUS_SUCCESS) return false;
  (void)am_hal_pdm_fifo_flush(g_pdm_handle);
  return true;
#else
  return true;
#endif
}

bool Apollo510AudioStart(void) {
#ifdef KWS_TARGET_APOLLO510
  g_status.state[0] = kBufferDmaOwned;
  g_status.state[1] = kBufferDmaOwned;
  am_util_delay_ms(KWS_PDM_SETTLE_MS);
  return am_hal_pdm_dma_start(g_pdm_handle, &g_transfer) == AM_HAL_STATUS_SUCCESS;
#else
  return true;
#endif
}

bool Apollo510AudioStop(void) {
#ifdef KWS_TARGET_APOLLO510
  return am_hal_pdm_dma_stop(g_pdm_handle) == AM_HAL_STATUS_SUCCESS;
#else
  return true;
#endif
}

#ifdef KWS_TARGET_APOLLO510
void am_pdm0_isr(void) {
  uint32_t status = 0;
  am_hal_pdm_interrupt_status_get(g_pdm_handle, &status, true);
  am_hal_pdm_interrupt_clear(g_pdm_handle, status);
  am_hal_pdm_interrupt_service(g_pdm_handle, status, &g_transfer);

  if (status & AM_HAL_PDM_INT_DCMP) {
    const uint32_t idx = g_status.blocks_received & 1u;
    if (g_status.state[idx] == kBufferCpuProcessing) {
      ++g_status.overruns;
    }
    g_completed = (const uint32_t*)am_hal_pdm_dma_get_buffer(g_pdm_handle);
    g_status.state[idx] = kBufferReadyForCpu;
    ++g_status.blocks_received;
  }
  if (status & AM_HAL_PDM_INT_DERR) {
    ++g_status.uart_errors;
  }
  if (status & (AM_HAL_PDM_INT_OVF | AM_HAL_PDM_INT_UNDFL)) {
    ++g_status.dma_faults;
  }
}
#endif

bool AudioSourceInit(void) { return Apollo510AudioInit(); }
bool AudioSourceStart(void) { return Apollo510AudioStart(); }
bool AudioSourceStop(void) { return Apollo510AudioStop(); }

bool AudioSourceBlockReady(void) {
  return g_status.blocks_received != g_status.blocks_processed;
}

const AudioSample* AudioSourceAcquireBlock(uint32_t* out_samples) {
  if (!AudioSourceBlockReady()) return NULL;
  const uint32_t idx = g_status.blocks_processed & 1u;
  g_status.state[idx] = kBufferCpuProcessing;

  const uint32_t* src = (const uint32_t*)g_completed;
  if (src != NULL) {
    Apollo510DmaInvalidate(src, KWS_PDM_BLOCK_SAMPLES * sizeof(uint32_t));
    int32_t peak = 0;
    int32_t dc = 0;
    KwsPdmBlockToPcm16(src, g_pcm_block, KWS_PDM_BLOCK_SAMPLES, &peak, &dc);
    g_status.pcm_peak_abs = peak;
    g_status.pcm_dc = dc;
  }
  if (out_samples != NULL) *out_samples = KWS_PDM_BLOCK_SAMPLES;
  return g_pcm_block;
}

void AudioSourceReleaseBlock(void) {
  const uint32_t idx = g_status.blocks_processed & 1u;
  g_status.state[idx] = kBufferFree;
  ++g_status.blocks_processed;
}

const AudioSourceStatus* AudioSourceGetStatus(void) { return &g_status; }

bool AudioSourceTakeStreamBegin(void) { return false; }
bool AudioSourceTakeStreamEnd(void) { return false; }
bool AudioSourceTakeReset(void) { return false; }
