# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of mask/ops (the operator kernels and evaluate_layer),
# included by src/core/src/mask/sources.cmake.
# Owner: OPS (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/bloat.cpp
  ${CMAKE_CURRENT_LIST_DIR}/bloat_all.cpp
  ${CMAKE_CURRENT_LIST_DIR}/boolean.cpp
  ${CMAKE_CURRENT_LIST_DIR}/bridge.cpp
  ${CMAKE_CURRENT_LIST_DIR}/bridge_lim.cpp
  ${CMAKE_CURRENT_LIST_DIR}/cell_ops.cpp
  ${CMAKE_CURRENT_LIST_DIR}/close.cpp
  ${CMAKE_CURRENT_LIST_DIR}/compat.cpp
  ${CMAKE_CURRENT_LIST_DIR}/evaluate.cpp
  ${CMAKE_CURRENT_LIST_DIR}/grow.cpp
  ${CMAKE_CURRENT_LIST_DIR}/kernel.cpp
  ${CMAKE_CURRENT_LIST_DIR}/squares.cpp
  ${CMAKE_CURRENT_LIST_DIR}/topology.cpp)
