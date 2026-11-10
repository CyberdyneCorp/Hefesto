# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the drc area (openspec/changes/m3-drc/design.md D1/D2).
# Owner: INTEGRATION (this file and the shared tests here, which cover the
# shared types of src/core/include/hefesto/drc/{types,session,events}.hpp).
# Each subdirectory is owned by its workstream and builds its own
# executable:
#   rules/     RULES     hefesto_drc_rules_tests     (data: tests/data/drc/rules)
#   check/     CHECK     hefesto_drc_check_tests     (data: tests/data/drc/check)
#   engine/    ENGINE    hefesto_drc_engine_tests    (data: tests/data/drc/engine)
#   commands/  COMMANDS  hefesto_drc_commands_tests  (data: tests/data/drc/commands)

hefesto_add_area_tests(drc)

foreach(_drc_sub IN ITEMS rules check engine commands)
  add_subdirectory(${_drc_sub})
endforeach()
