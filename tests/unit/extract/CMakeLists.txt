# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the extract area (openspec/changes/m6-lef-def-oasis/design.md D1/D2; openspec/changes/m7-extraction/design.md D2/D3).
# Owner: INTEGRATION (this file and the shared tests here). Each
# subdirectory is owned by its workstream and builds its own executable once
# it has a *_test.cpp:
#   tech/     EXTTECH  hefesto_extract_tech_tests   (data: tests/data/extract/tech)
#   node/     EXTNODE  hefesto_extract_node_tests   (data: tests/data/extract/node)
#   file/     EXTNODE  hefesto_extract_file_tests   (data: tests/data/extract/file)
#   cap/      EXTCAP   hefesto_extract_cap_tests   (data: tests/data/extract/cap)
#   device/   EXTDEV   hefesto_extract_device_tests   (data: tests/data/extract/device)
#   hier/     EXTHIER  hefesto_extract_hier_tests   (data: tests/data/extract/hier)
#   command/  EXTCMD   hefesto_extract_command_tests   (data: tests/data/extract/command)
#   length/   EXTCMD   hefesto_extract_length_tests   (data: tests/data/extract/length)

hefesto_add_area_tests(extract)

foreach(_extract_sub IN ITEMS tech node file cap device hier command length)
  add_subdirectory(${_extract_sub})
endforeach()
