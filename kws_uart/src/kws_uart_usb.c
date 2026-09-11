// SPDX-License-Identifier: Apache-2.0
// nsx-usb CDC. VID/PID default 0xCafe/0x4011 (usb_serial). DTR must be True.
#include "kws_uart_usb.h"

#include <stddef.h>
#include <stdint.h>

#include "nsx_mem.h"
#include "nsx_usb.h"

#define KWS_USB_TX_BYTES 2048u
#define KWS_USB_RX_BYTES 4096u

static NSX_MEM_SRAM_BSS uint8_t g_usb_tx[KWS_USB_TX_BYTES];
static NSX_MEM_SRAM_BSS uint8_t g_usb_rx[KWS_USB_RX_BYTES];
static nsx_usb_config_t g_usb;
static uint32_t g_poll_n;
static uint32_t g_rx_bytes;
static bool g_inited;

bool KwsUartUsbInit(void) {
  g_poll_n = 0;
  g_rx_bytes = 0;
  g_usb = (nsx_usb_config_t){
      .tx_buffer = g_usb_tx,
      .tx_buffer_len = sizeof(g_usb_tx),
      .rx_buffer = g_usb_rx,
      .rx_buffer_len = sizeof(g_usb_rx),
      .poll_interval_us = NSX_USB_DEFAULT_POLL_US,
      .timeout_ms = NSX_USB_DEFAULT_TIMEOUT_MS,
      .rx_cb = NULL,
      .vendor_rx_cb = NULL,
      .device_desc = NULL,
      .user_ctx = NULL,
  };
  g_inited = (nsx_usb_init(&g_usb) == 0);
  return g_inited;
}

bool KwsUartUsbConnected(void) {
  return g_inited && nsx_usb_connected(&g_usb);
}

uint32_t KwsUartUsbRead(uint8_t* dst, uint32_t max_len) {
  if (!g_inited || dst == NULL || max_len == 0u) {
    return 0;
  }
  uint32_t n = 0;
  nsx_usb_read_nb(&g_usb, dst, max_len, &n);
  if (n != 0u) {
    ++g_poll_n;
    g_rx_bytes += n;
  }
  return n;
}

bool KwsUartUsbSend(const uint8_t* data, uint32_t len) {
  if (!g_inited || data == NULL || len == 0u) {
    return false;
  }
  uint32_t sent = 0;
  return nsx_usb_send(&g_usb, data, len, &sent) == 0 && sent == len;
}

uint32_t KwsUartUsbPollCount(void) { return g_poll_n; }
uint32_t KwsUartUsbRxBytes(void) { return g_rx_bytes; }
