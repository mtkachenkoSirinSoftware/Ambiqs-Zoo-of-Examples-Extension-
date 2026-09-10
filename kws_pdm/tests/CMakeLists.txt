# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)
project(kws_pdm_host_check C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KWS_PDM_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(KWS_CORE_ROOT "${KWS_PDM_ROOT}/../kws_core")

add_executable(kws_pdm_check check_pdm.cc)
target_include_directories(kws_pdm_check PRIVATE ${KWS_PDM_ROOT} ${KWS_CORE_ROOT})

enable_testing()
add_test(NAME kws_pdm_check COMMAND kws_pdm_check)
