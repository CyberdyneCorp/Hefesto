# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the extract area (openspec/changes/m6-lef-def-oasis/design.md D1/D2).
# Owner: INTEGRATION (this file and the shared tests here). Each
# subdirectory is owned by its workstream and builds its own executable once
# it has a *_test.cpp:
#   tech/ EXTSUB     hefesto_extract_tech_tests   (data: tests/data/extract/tech)

hefesto_add_area_tests(extract)

foreach(_extract_sub IN ITEMS tech)
  add_subdirectory(${_extract_sub})
endforeach()
