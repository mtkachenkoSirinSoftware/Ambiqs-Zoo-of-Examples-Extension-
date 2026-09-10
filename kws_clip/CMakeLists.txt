# SPDX-License-Identifier: Apache-2.0
#
# kws_clip — 1 s flash PCM through kws_core, labels on SWO (`nsx view`).
#
# After `nsx configure --app-dir .` this file is the firmware app (cmake/nsx/
# is materialised by the tool). Before that, `cmake -S .` runs host identity
# tests only (no Arm toolchain, no EVB).
cmake_minimum_required(VERSION 3.16)

set(KWS_CLIP_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(KWS_CORE_ROOT "${KWS_CLIP_ROOT}/../kws_core")
set(KWS_ZOO_ROOT "${KWS_CLIP_ROOT}/..")
set(NSX_BOOTSTRAP "${KWS_CLIP_ROOT}/cmake/nsx/nsx_app_bootstrap.cmake")

if(NOT EXISTS "${NSX_BOOTSTRAP}")
  project(kws_clip_host C CXX)
  enable_testing()
  add_test(
    NAME kws_clip_identity
    COMMAND python3 "${KWS_CLIP_ROOT}/host/test_clip_identity.py")
  return()
endif()

project(kws_clip LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NSX_BOARD "apollo510_evb" CACHE STRING "Target board")
set(NSX_HELIA_RT_BACKEND "helia" CACHE STRING "heliaRT kernels: helia | cmsis_nn | reference")
set(NSX_CMSIS_NN_ENABLE_F32 ON CACHE BOOL "" FORCE)
set(NSX_CMSIS_NN_ENABLE_F16 ON CACHE BOOL "" FORCE)
set(HELIA_RT_VARIANT "release-with-logs" CACHE STRING "debug | release-with-logs | release")

include(${KWS_CLIP_ROOT}/cmake/nsx/modules.cmake)
include(${NSX_BOOTSTRAP})

nsx_bootstrap_app(
    APP_ROOT "${KWS_CLIP_ROOT}"
    BOARD "${NSX_BOARD}"
    MODULES ${NSX_APP_MODULES}
)

include(${KWS_CORE_ROOT}/cmake/kws_core.cmake)

set(_model_cc "${KWS_CLIP_ROOT}/model/generated/kws_model_data.cc")
set(_clip_cc "${KWS_CLIP_ROOT}/audio/generated/kws_clip_data.cc")
set(_embed_model "${KWS_ZOO_ROOT}/tools/embed_model.py")
set(_embed_clip "${KWS_ZOO_ROOT}/tools/embed_clip.py")
set(_tflite "${KWS_ZOO_ROOT}/assets/depgraph_r060_kd_int8.tflite")
set(_golden "${KWS_CORE_ROOT}/tests/golden/synthetic.txt")

if(NOT EXISTS "${_model_cc}")
  execute_process(
    COMMAND python3 "${_embed_model}" "${_tflite}" --out-dir "${KWS_CLIP_ROOT}/model/generated"
    RESULT_VARIABLE _rv)
  if(NOT _rv EQUAL 0)
    message(FATAL_ERROR "embed_model.py failed (${_rv}). Need ${_tflite}.")
  endif()
endif()
if(NOT EXISTS "${_clip_cc}")
  execute_process(
    COMMAND python3 "${_embed_clip}" --golden "${_golden}"
            --out-dir "${KWS_CLIP_ROOT}/audio/generated"
    RESULT_VARIABLE _rv)
  if(NOT _rv EQUAL 0)
    message(FATAL_ERROR "embed_clip.py failed (${_rv}). Need ${_golden}.")
  endif()
endif()

add_executable(kws_clip
    src/main.cc
    src/flash_clip_source.c
    src/kws_profile_target.c
    ${KWS_CORE_SOURCES}
    ${_model_cc}
    ${_clip_cc}
)

target_include_directories(kws_clip PRIVATE
    ${KWS_CLIP_ROOT}
    ${KWS_CORE_ROOT}
)

target_compile_definitions(kws_clip PRIVATE
    KWS_TARGET_APOLLO510=1
    KWS_USE_NSX_MEM=1
    KWS_RUNTIME_HELIA_RT=1
    KWS_AUDIO_SOURCE_CLIP=1
)

target_link_libraries(kws_clip PRIVATE
    nsx::board
    nsx::runtime_core
    nsx::power
    nsx::helia_rt
)

nsx_finalize_app(kws_clip)
