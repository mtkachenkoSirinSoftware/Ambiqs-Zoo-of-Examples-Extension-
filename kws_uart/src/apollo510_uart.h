// SPDX-License-Identifier: Apache-2.0
// Apollo510 UART AudioSource: poll the RX FIFO in the main loop (no IRQ, no
// RX-DMA). Silicon: am_hal_uart_interrupt_stream_service + RX DMA hung the
// UART ISR (`while (!RXFE)` never emptied), so 1 Hz stats froze on the first
// host byte.
//
// HAL (am_hal_uart.h — not am_hal_uart_stream.h):
//   am_hal_uart_initialize / _power_control / _configure
//   am_hal_uart_fifo_read          // DrainRxQueue, main loop
//   am_hal_uart_interrupt_disable  // AM_HAL_UART_INT_ALL; NVIC stays off
//   am_hal_uart_dma_abort          // DCR=0 so leftover RXDMAE cannot steal bytes
//
// PCM on AM_BSP_UART_PRINT_INST @ 921600. Labels on SWO (`nsx view`), not UART.
#ifndef APOLLO510_UART_H_
#define APOLLO510_UART_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef KWS_UART_BAUD
#define KWS_UART_BAUD 921600u
#endif

bool Apollo510UartInit(void);
bool Apollo510UartStart(void);
bool Apollo510UartStop(void);

uint32_t Apollo510UartIsrCount(void);
uint32_t Apollo510UartCbCount(void);
uint32_t Apollo510UartRxBytes(void);
uint32_t Apollo510UartClipDropped(void);

#ifdef __cplusplus
}
#endif
#endif  // APOLLO510_UART_H_
