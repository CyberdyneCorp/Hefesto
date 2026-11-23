# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the lef area (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2).
# Owner: INTEGRATION (this file and the shared tests here). Each
# subdirectory is owned by its workstream and builds its own executable once
# it has a *_test.cpp:
#   lex/     LEFR       hefesto_lef_lex_tests   (data: tests/data/lef/lex)
#   tech/    LEFTECH    hefesto_lef_tech_tests   (data: tests/data/lef/tech)
#   via/     LEFVIA     hefesto_lef_via_tests   (data: tests/data/lef/via)
#   read/    LEFR       hefesto_lef_read_tests   (data: tests/data/lef/read)
#   write/   LEFW       hefesto_lef_write_tests   (data: tests/data/lef/write)
#   command/ LEFR, LEFW hefesto_lef_command_tests   (data: tests/data/lef/command)

hefesto_add_area_tests(lef)

foreach(_lef_sub IN ITEMS lex tech via read write command)
  add_subdirectory(${_lef_sub})
endforeach()
