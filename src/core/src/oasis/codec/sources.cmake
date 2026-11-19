# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of oasis/codec (the OASIS codec: primitives, records, modal state,
# repetitions, CBLOCK, validation and the encoder),
# included by src/core/src/oasis/sources.cmake.
# Owner: OASFMT (openspec/changes/m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/decoder.cpp
  ${CMAKE_CURRENT_LIST_DIR}/decoder_elements.cpp
  ${CMAKE_CURRENT_LIST_DIR}/encoder.cpp
  ${CMAKE_CURRENT_LIST_DIR}/encoder_elements.cpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry.cpp
  ${CMAKE_CURRENT_LIST_DIR}/input.cpp
  ${CMAKE_CURRENT_LIST_DIR}/lists.cpp
  ${CMAKE_CURRENT_LIST_DIR}/primitives.cpp
  ${CMAKE_CURRENT_LIST_DIR}/property.cpp
  ${CMAKE_CURRENT_LIST_DIR}/strict.cpp
  ${CMAKE_CURRENT_LIST_DIR}/validate.cpp)
