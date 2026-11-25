# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lef/command (the `lef` and `def` commands: one dispatcher, as
# Magic's CmdLef), included by src/core/src/lef/sources.cmake.
# Owner: INTEGRATION, the dispatcher (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md
# D1/D2, D14); LEFR, LEFW, DEFR and DEFW own their argument parsers in their
# own areas (lef/read/args.cpp, lef/write/args.cpp, def/read/args.cpp,
# def/write/args.cpp). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/command.cpp)
