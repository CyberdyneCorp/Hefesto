# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lef/lex (the LEF/DEF tokenizer, message reporting and error
# limits),
# included by src/core/src/lef/sources.cmake.
# Owner: LEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/lexer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/statements.cpp)
