// SPDX-License-Identifier: Apache-2.0
// USB CDC AudioSource: same 0xA51C pump as PRINT UART, bytes from nsx-usb.
// Labels stay on SWO. Host must assert DTR (usb_serial). Not RX-DMA UART.
#include "apollo510_uart.h"

#include "audio/audio_source.h"
#include "audio/uart_clip_session.h"
#include "kws_uart_usb.h"

static kws::UartClipSession g_session;

uint32_t Apollo510UartIsrCount(void) { return 0; }
uint32_t Apollo510UartCbCount(void) { return KwsUartUsbPollCount(); }
uint32_t Apollo510UartRxBytes(void) { return KwsUartUsbRxBytes(); }
uint32_t Apollo510UartClipDropped(void) { return g_session.clip_dropped(); }

static void DrainRxQueue(void) {
  if (!KwsUartUsbConnected()) {
    return;
  }
  uint8_t tmp[64];
  for (int i = 0; i < 32; ++i) {
    const uint32_t n = KwsUartUsbRead(tmp, sizeof(tmp));
    if (n == 0u) {
      break;
    }
    g_session.PushBytes(tmp, n);
  }
}

bool Apollo510UartInit(void) {
  g_session.Init();
  return KwsUartUsbInit();
}

bool Apollo510UartStart(void) { return true; }
bool Apollo510UartStop(void) { return true; }

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
