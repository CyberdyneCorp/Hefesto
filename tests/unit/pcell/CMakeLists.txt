# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the pcell area (openspec/changes/m4-scripting/design.md D1/D2).
# Owner: INTEGRATION (this file); each subdirectory is owned by PCELL:
#   framework/  hefesto_pcell_framework_tests  (data: tests/data/pcell/framework)
#   native/     hefesto_pcell_native_tests     (data: tests/data/pcell/native)

foreach(_pcell_sub IN ITEMS framework native)
  add_subdirectory(${_pcell_sub})
endforeach()
