# SPDX-License-Identifier: Apache-2.0
# Host-only C++ checks (no NSX, no EVB).
cmake_minimum_required(VERSION 3.16)
project(kws_uart_host_check C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KWS_UART_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(KWS_CORE_ROOT "${KWS_UART_ROOT}/../kws_core")

add_executable(kws_uart_check
  check_uart.cc
  ${KWS_UART_ROOT}/protocol/uart_protocol.cc
  ${KWS_UART_ROOT}/protocol/uart_audio_pump.cc
  ${KWS_UART_ROOT}/audio/uart_clip_buffer.c
  ${KWS_UART_ROOT}/audio/uart_clip_session.cc
  ${KWS_CORE_ROOT}/audio/flash_clip_player.c
)
target_include_directories(kws_uart_check PRIVATE
  ${KWS_UART_ROOT}
  ${KWS_CORE_ROOT}
)

enable_testing()
add_test(NAME kws_uart_check COMMAND kws_uart_check)
set_tests_properties(kws_uart_check PROPERTIES
  ENVIRONMENT
    "KWS_GOLDEN_SYNTHETIC=${KWS_CORE_ROOT}/tests/golden/synthetic.txt;KWS_UART_PY_WIRE=${KWS_UART_ROOT}/host/generated/py_wire.bin"
)
