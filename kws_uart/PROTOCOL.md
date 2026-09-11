# SPDX-License-Identifier: Apache-2.0
# UART framing for kws_uart

Firmware: `protocol/uart_protocol.{h,cc}`. Host: `host/stream_wav.py`.
Independent implementations; `tests/check_uart.cc` decodes a blob the Python
encoder actually wrote.

## Frame layout

All multi-byte fields little-endian.

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | `MAGIC` = `0xA51C` (wire bytes `1C A5`) |
| 2 | 1 | `VERSION` = 1 |
| 3 | 1 | `TYPE` |
| 4 | 2 | `SEQ` |
| 6 | 2 | `PAYLOAD_LEN` (<= 1024) |
| 8 | 2 | `HEADER_CRC` — CRC-16/CCITT-FALSE over bytes 0..7 |
| 10 | n | payload |
| 10+n | 2 | `PAYLOAD_CRC` — CRC-16/CCITT-FALSE over the payload |

`Crc16Ccitt("123456789")` = `0x29B1`.

A header CRC exists so a corrupt `PAYLOAD_LEN` cannot stall the decoder.

## Message types (host → MCU)

| Value | Name | Payload |
|---|---|---|
| `0x01` | `START_STREAM` | none |
| `0x02` | `AUDIO_BLOCK` | 320 × int16 LE = 640 B (one 20 ms hop) |
| `0x03` | `END_STREAM` | none |
| `0x04` | `RESET` | none (session rings; not helia `AllocateTensors`) |

MCU does **not** TX `PREDICTION` on this UART. Labels are SWO (`nsx view`).

## Transport

Main-loop FIFO poll (`am_hal_uart_fifo_read`). NVIC UART IRQ off.
`am_hal_uart_dma_abort` so leftover `RXDMAE` cannot steal bytes.

Do not use `am_hal_uart_stream_*` / RX-DMA on this path.

`stream_wav.py --dtr auto`: **high** on NSX CDC `0xCafe`/`0x4011` (`usb_serial`:
without DTR the device ignores RX), **low** on J-Link VCP (otherwise the MCU
resets). Baud 921600 8N1 on PRINT UART; CDC baud is a host-side number only.

Optional `-DKWS_UART_USB_CDC=ON` carries the same `0xA51C` frames over
`nsx-usb`. Labels stay on SWO.

## USB RPC (optional, `-DKWS_UART_USB_RPC=ON`)

Same 4-byte LE length prefix + nanopb `NsxRpcMessage` as
`neuralspotx/examples/usb_rpc`. `INFER.input` is **32000** bytes (16000 ×
int16 LE). The handler is `KwsApp`. Stock `usb_rpc` maps INFER to five stub
classes — do not compare that image to SWO `pred=`.
