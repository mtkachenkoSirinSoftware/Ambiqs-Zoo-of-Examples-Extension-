// SPDX-License-Identifier: Apache-2.0
// nsx-usb CDC bring-up shared by the 0xA51C PCM path and the RPC INFER path.
// DMA buffers in SRAM (Apollo5 USB DMA cannot use TCM). Labels stay on SWO.
#ifndef KWS_UART_USB_H_
#define KWS_UART_USB_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool KwsUartUsbInit(void);
bool KwsUartUsbConnected(void);
uint32_t KwsUartUsbRead(uint8_t* dst, uint32_t max_len);
bool KwsUartUsbSend(const uint8_t* data, uint32_t len);
uint32_t KwsUartUsbPollCount(void);
uint32_t KwsUartUsbRxBytes(void);

#ifdef __cplusplus
}
#endif
#endif  // KWS_UART_USB_H_
