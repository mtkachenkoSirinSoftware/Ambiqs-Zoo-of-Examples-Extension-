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
| `0x04` | `RESET` | none (session rings; **not** helia `AllocateTensors`) |

MCU does **not** TX `PREDICTION` on this UART. Labels are SWO (`nsx view`).

## Transport

Main-loop FIFO poll (`am_hal_uart_fifo_read`). NVIC UART IRQ off.
`am_hal_uart_dma_abort` so leftover `RXDMAE` cannot steal bytes.

Do not use `am_hal_uart_stream_*` / RX-DMA on this path.

Host `stream_wav.py` holds DTR/RTS low. Baud 921600 8N1. `--realtime` is
optional: the MCU stores 1 s then infers.
