// SPDX-License-Identifier: Apache-2.0
// UART AudioSource: classic HAL + RX FIFO poll in the main loop.
// No am_uartN_isr, no am_hal_uart_stream_*, no RX DMA.
#include "apollo510_uart.h"

#include "audio/audio_source.h"
#include "audio/uart_clip_session.h"

#ifdef KWS_TARGET_APOLLO510
#include "am_bsp.h"
#include "am_mcu_apollo.h"
#endif

static kws::UartClipSession g_session;
static uint32_t g_poll_n;
static uint32_t g_rx_bytes;

uint32_t Apollo510UartIsrCount(void) { return 0; }
uint32_t Apollo510UartCbCount(void) { return g_poll_n; }
uint32_t Apollo510UartRxBytes(void) { return g_rx_bytes; }
uint32_t Apollo510UartClipDropped(void) { return g_session.clip_dropped(); }

#ifdef KWS_TARGET_APOLLO510
static void* g_uart_handle;

static void CountOverruns(void) {
  uint32_t st = 0;
  if (am_hal_uart_interrupt_status_get(g_uart_handle, &st, false) != AM_HAL_STATUS_SUCCESS) {
    return;
  }
  if ((st & AM_HAL_UART_INT_OVER_RUN) != 0u) {
    g_session.AddUartErrors(1);
    (void)am_hal_uart_interrupt_clear(g_uart_handle, AM_HAL_UART_INT_OVER_RUN);
  }
}

static void PollFifo(void) {
  uint8_t tmp[32];
  uint32_t n = 0;
  const uint32_t st = am_hal_uart_fifo_read(g_uart_handle, tmp, sizeof(tmp), &n);
  if (n != 0u) {
    ++g_poll_n;
    g_rx_bytes += n;
    g_session.PushBytes(tmp, n);
  }
  if (st != AM_HAL_STATUS_SUCCESS) {
    g_session.AddUartErrors(1);
  }
  CountOverruns();
}
#endif

static void DrainRxQueue(void) {
#ifdef KWS_TARGET_APOLLO510
  if (g_uart_handle == nullptr) {
    return;
  }
  while (!UARTn(AM_BSP_UART_PRINT_INST)->FR_b.RXFE) {
    PollFifo();
  }
  CountOverruns();
#endif
}

bool Apollo510UartInit(void) {
  g_session.Init();
  g_poll_n = 0;
  g_rx_bytes = 0;

#ifdef KWS_TARGET_APOLLO510
  g_uart_handle = nullptr;
  if (am_hal_uart_initialize(AM_BSP_UART_PRINT_INST, &g_uart_handle) != AM_HAL_STATUS_SUCCESS) {
    return false;
  }
  if (am_hal_uart_power_control(g_uart_handle, AM_HAL_SYSCTRL_WAKE, false) !=
      AM_HAL_STATUS_SUCCESS) {
    return false;
  }

  am_hal_gpio_pinconfig(AM_BSP_GPIO_COM_UART_TX, g_AM_BSP_GPIO_COM_UART_TX);
  am_hal_gpio_pinconfig(AM_BSP_GPIO_COM_UART_RX, g_AM_BSP_GPIO_COM_UART_RX);

  am_hal_uart_config_t cfg = {};
  cfg.ui32BaudRate = KWS_UART_BAUD;
  cfg.eDataBits = AM_HAL_UART_DATA_BITS_8;
  cfg.eParity = AM_HAL_UART_PARITY_NONE;
  cfg.eStopBits = AM_HAL_UART_ONE_STOP_BIT;
  cfg.eFlowControl = AM_HAL_UART_FLOW_CTRL_NONE;
  cfg.eTXFifoLevel = AM_HAL_UART_FIFO_LEVEL_16;
  cfg.eRXFifoLevel = AM_HAL_UART_FIFO_LEVEL_16;
  cfg.eClockSrc = AM_HAL_UART_CLOCK_SRC_HFRC;
  if (am_hal_uart_configure(g_uart_handle, &cfg) != AM_HAL_STATUS_SUCCESS) {
    return false;
  }

  am_hal_uart_dma_abort(g_uart_handle);
  (void)am_hal_uart_interrupt_disable(g_uart_handle, AM_HAL_UART_INT_ALL);
  (void)am_hal_uart_interrupt_clear(g_uart_handle, AM_HAL_UART_INT_ALL);
  NVIC_DisableIRQ(static_cast<IRQn_Type>(UART0_IRQn + AM_BSP_UART_PRINT_INST));
  NVIC_ClearPendingIRQ(static_cast<IRQn_Type>(UART0_IRQn + AM_BSP_UART_PRINT_INST));
  (void)am_hal_uart_rx_fifo_drain(g_uart_handle);
#endif
  return true;
}

bool Apollo510UartStart(void) { return true; }

bool Apollo510UartStop(void) {
#ifdef KWS_TARGET_APOLLO510
  if (g_uart_handle == nullptr) {
    return true;
  }
  am_hal_uart_dma_abort(g_uart_handle);
  (void)am_hal_uart_interrupt_disable(g_uart_handle, AM_HAL_UART_INT_ALL);
  NVIC_DisableIRQ(static_cast<IRQn_Type>(UART0_IRQn + AM_BSP_UART_PRINT_INST));
#endif
  return true;
}

bool AudioSourceInit(void) { return Apollo510UartInit(); }
bool AudioSourceStart(void) { return Apollo510UartStart(); }
bool AudioSourceStop(void) { return Apollo510UartStop(); }

bool AudioSourceBlockReady(void) {
  DrainRxQueue();
  return g_session.BlockReady();
}

const AudioSample* AudioSourceAcquireBlock(uint32_t* out_samples) {
  DrainRxQueue();
  return g_session.AcquireBlock(out_samples);
}

void AudioSourceReleaseBlock(void) { g_session.ReleaseBlock(); }

const AudioSourceStatus* AudioSourceGetStatus(void) { return g_session.status(); }

bool AudioSourceTakeStreamBegin(void) {
  DrainRxQueue();
  return g_session.TakeStreamBegin();
}
bool AudioSourceTakeStreamEnd(void) {
  DrainRxQueue();
  return g_session.TakeStreamEnd();
}
bool AudioSourceTakeReset(void) {
  DrainRxQueue();
  return g_session.TakeReset();
}
