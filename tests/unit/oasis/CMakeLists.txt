# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the oasis area (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2).
# Owner: INTEGRATION (this file and the shared tests here). Each
# subdirectory is owned by its workstream and builds its own executable once
# it has a *_test.cpp:
#   codec/   OASFMT     hefesto_oasis_codec_tests   (data: tests/data/oasis/codec)
#   read/    OASR       hefesto_oasis_read_tests   (data: tests/data/oasis/read)
#   write/   OASW       hefesto_oasis_write_tests   (data: tests/data/oasis/write)
#   command/ OASCMD     hefesto_oasis_command_tests   (data: tests/data/oasis/command)

hefesto_add_area_tests(oasis)

foreach(_oasis_sub IN ITEMS codec read write command)
  add_subdirectory(${_oasis_sub})
endforeach()
