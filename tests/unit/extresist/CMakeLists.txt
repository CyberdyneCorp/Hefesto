# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the extresist area (openspec/changes/m7-extraction/design.md D2/D3).
# Owner: INTEGRATION (this file and the shared tests here). Each
# subdirectory is owned by its workstream and builds its own executable once
# it has a *_test.cpp:
#   network/  RESNET   hefesto_extresist_network_tests   (data: tests/data/extresist/network)
#   command/  RESCMD   hefesto_extresist_command_tests   (data: tests/data/extresist/command)
#   output/   RESCMD   hefesto_extresist_output_tests   (data: tests/data/extresist/output)

hefesto_add_area_tests(extresist)

foreach(_extresist_sub IN ITEMS network command output)
  add_subdirectory(${_extresist_sub})
endforeach()
