# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the def area (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2).
# Owner: INTEGRATION (this file and the shared tests here). Each
# subdirectory is owned by its workstream and builds its own executable once
# it has a *_test.cpp:
#   read/  DEFR       hefesto_def_read_tests   (data: tests/data/def/read)
#   write/ DEFW       hefesto_def_write_tests   (data: tests/data/def/write)

hefesto_add_area_tests(def)

foreach(_def_sub IN ITEMS read write)
  add_subdirectory(${_def_sub})
endforeach()
