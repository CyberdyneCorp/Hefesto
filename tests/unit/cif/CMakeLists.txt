# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the cif area (openspec/changes/m5-mask-gds-cif/design.md D1/D2).
# Owner: INTEGRATION (this file and the shared tests here). Each
# subdirectory is owned by its workstream and builds its own executable once
# it has a *_test.cpp:
#   read/     CIF     hefesto_cif_read_tests   (data: tests/data/cif/read)
#   write/    CIF     hefesto_cif_write_tests   (data: tests/data/cif/write)
#   command/  CIF     hefesto_cif_command_tests   (data: tests/data/cif/command)

hefesto_add_area_tests(cif)

foreach(_cif_sub IN ITEMS read write command)
  add_subdirectory(${_cif_sub})
endforeach()
