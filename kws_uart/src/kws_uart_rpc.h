// SPDX-License-Identifier: Apache-2.0
// usb_rpc framing on nsx-usb. INFER is 16000 int16 → KwsApp, not the 5-class toy.
#ifndef KWS_UART_RPC_H_
#define KWS_UART_RPC_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool KwsUartRpcInit(void);

#ifdef __cplusplus
}

namespace kws {
class KwsApp;
}

// Drain CDC, dispatch one complete RPC frame. Returns true if INFER ran.
bool KwsUartRpcPoll(kws::KwsApp* app, uint32_t now_ms);
#endif

#endif  // KWS_UART_RPC_H_
