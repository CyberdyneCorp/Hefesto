# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the gds area (openspec/changes/m5-mask-gds-cif/design.md D1/D2).
# Owner: INTEGRATION (this file and the shared tests here). Each
# subdirectory is owned by its workstream and builds its own executable once
# it has a *_test.cpp:
#   stream/   GDSR    hefesto_gds_stream_tests   (data: tests/data/gds/stream)
#   read/     GDSR    hefesto_gds_read_tests   (data: tests/data/gds/read)
#   write/    GDSW    hefesto_gds_write_tests   (data: tests/data/gds/write)
#   command/  GDSW    hefesto_gds_command_tests   (data: tests/data/gds/command)

hefesto_add_area_tests(gds)

foreach(_gds_sub IN ITEMS stream read write command)
  add_subdirectory(${_gds_sub})
endforeach()
