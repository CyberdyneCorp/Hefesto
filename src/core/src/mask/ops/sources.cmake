# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of mask/ops (the operator kernels and evaluate_layer),
# included by src/core/src/mask/sources.cmake.
# Owner: OPS (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/evaluate.cpp
  ${CMAKE_CURRENT_LIST_DIR}/operators.cpp)
