# SPDX-License-Identifier: Apache-2.0
#
# kws_uart — host WAV over FIFO-poll UART through kws_core, labels on SWO.
#
# After `nsx configure --app-dir .` this file is the firmware app.
# Before that, `cmake -S .` registers host Python tests only.
cmake_minimum_required(VERSION 3.16)

set(KWS_UART_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(KWS_CORE_ROOT "${KWS_UART_ROOT}/../kws_core")
set(KWS_ZOO_ROOT "${KWS_UART_ROOT}/..")
set(NSX_BOOTSTRAP "${KWS_UART_ROOT}/cmake/nsx/nsx_app_bootstrap.cmake")

if(NOT EXISTS "${NSX_BOOTSTRAP}")
  project(kws_uart_host C CXX)
  enable_testing()
  add_test(
    NAME kws_uart_identity
    COMMAND python3 "${KWS_UART_ROOT}/host/test_uart_identity.py")
  add_test(
    NAME kws_uart_stream_wav
    COMMAND python3 "${KWS_UART_ROOT}/host/test_stream_wav.py")
  add_test(
    NAME kws_uart_rpc_wire
    COMMAND python3 "${KWS_UART_ROOT}/host/test_rpc_wire.py")
  return()
endif()

project(kws_uart LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NSX_BOARD "apollo510_evb" CACHE STRING "Target board")
set(NSX_HELIA_RT_BACKEND "helia" CACHE STRING "heliaRT kernels: helia | cmsis_nn | reference")
set(NSX_CMSIS_NN_ENABLE_F32 ON CACHE BOOL "" FORCE)
set(NSX_CMSIS_NN_ENABLE_F16 ON CACHE BOOL "" FORCE)
set(HELIA_RT_VARIANT "release-with-logs" CACHE STRING "debug | release-with-logs | release")

option(KWS_UART_USB_CDC
    "PCM on nsx-usb CDC (0xA51C). DTR=True. Labels stay on SWO." OFF)
option(KWS_UART_USB_RPC
    "usb_rpc INFER of 16000 int16 through KwsApp. DTR=True. Not the 5-class toy." OFF)
if(KWS_UART_USB_CDC AND KWS_UART_USB_RPC)
  message(FATAL_ERROR "KWS_UART_USB_CDC and KWS_UART_USB_RPC are mutually exclusive")
endif()

include(${KWS_UART_ROOT}/cmake/nsx/modules.cmake)
include(${NSX_BOOTSTRAP})

nsx_bootstrap_app(
    APP_ROOT "${KWS_UART_ROOT}"
    BOARD "${NSX_BOARD}"
    MODULES ${NSX_APP_MODULES}
)

include(${KWS_CORE_ROOT}/cmake/kws_core.cmake)

set(_model_cc "${KWS_UART_ROOT}/model/generated/kws_model_data.cc")
set(_embed_model "${KWS_ZOO_ROOT}/tools/embed_model.py")
set(_tflite "${KWS_ZOO_ROOT}/assets/depgraph_r060_kd_int8.tflite")

if(NOT EXISTS "${_model_cc}")
  execute_process(
    COMMAND python3 "${_embed_model}" "${_tflite}" --out-dir "${KWS_UART_ROOT}/model/generated"
    RESULT_VARIABLE _rv)
  if(NOT _rv EQUAL 0)
    message(FATAL_ERROR "embed_model.py failed (${_rv}). Need ${_tflite}.")
  endif()
endif()

add_executable(kws_uart
    src/main.cc
    src/kws_profile_target.c
    ${KWS_CORE_SOURCES}
    ${_model_cc}
)
if(KWS_UART_USB_RPC)
  target_sources(kws_uart PRIVATE
      src/kws_uart_usb.c
      src/kws_uart_rpc.cc
      rpc/nsx_rpc.pb.c
  )
elseif(KWS_UART_USB_CDC)
  target_sources(kws_uart PRIVATE
      src/kws_uart_usb.c
      src/apollo510_usb.cc
      protocol/uart_protocol.cc
      protocol/uart_audio_pump.cc
      audio/uart_clip_buffer.c
      audio/uart_clip_session.cc
  )
else()
  target_sources(kws_uart PRIVATE
      src/apollo510_uart.cc
      protocol/uart_protocol.cc
      protocol/uart_audio_pump.cc
      audio/uart_clip_buffer.c
      audio/uart_clip_session.cc
  )
endif()

target_include_directories(kws_uart PRIVATE
    ${KWS_UART_ROOT}
    ${KWS_UART_ROOT}/src
    ${KWS_CORE_ROOT}
)
if(KWS_UART_USB_RPC)
  target_include_directories(kws_uart PRIVATE ${KWS_UART_ROOT}/rpc)
endif()

target_compile_definitions(kws_uart PRIVATE
    KWS_TARGET_APOLLO510=1
    KWS_USE_NSX_MEM=1
    KWS_RUNTIME_HELIA_RT=1
)
if(NOT KWS_UART_USB_RPC)
  target_compile_definitions(kws_uart PRIVATE KWS_AUDIO_SOURCE_UART=1)
endif()
if(KWS_UART_USB_CDC)
  target_compile_definitions(kws_uart PRIVATE KWS_UART_USB_CDC=1)
endif()
if(KWS_UART_USB_RPC)
  target_compile_definitions(kws_uart PRIVATE KWS_UART_USB_RPC=1)
endif()

target_link_libraries(kws_uart PRIVATE
    nsx::board
    nsx::runtime_core
    nsx::power
    nsx::helia_rt
)
if(KWS_UART_USB_CDC OR KWS_UART_USB_RPC)
  target_link_libraries(kws_uart PRIVATE nsx::usb)
endif()
if(KWS_UART_USB_RPC)
  target_link_libraries(kws_uart PRIVATE nsx::nanopb)
endif()

nsx_finalize_app(kws_uart)
