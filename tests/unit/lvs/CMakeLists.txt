# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the lvs area (openspec/changes/m9-lvs-simulation/design.md D3).
# Owner: SCAFFOLD (this file and the shared tests here). Each subdirectory
# is owned by its workstream and builds its own executable once it has a
# *_test.cpp:
#   net/     LVSREAD   hefesto_lvs_net_tests   (data: tests/data/lvs/net)
#   read/    LVSREAD   hefesto_lvs_read_tests   (data: tests/data/lvs/read)
#   setup/   LVSSETUP  hefesto_lvs_setup_tests   (data: tests/data/lvs/setup)
#   cmp/     LVSALG    hefesto_lvs_cmp_tests   (data: tests/data/lvs/cmp)
#   reduce/  LVSRED    hefesto_lvs_reduce_tests   (data: tests/data/lvs/reduce)
#   prop/    LVSPROP   hefesto_lvs_prop_tests   (data: tests/data/lvs/prop)
#   hier/    LVSHIER   hefesto_lvs_hier_tests   (data: tests/data/lvs/hier)
#   report/  LVSREP    hefesto_lvs_report_tests   (data: tests/data/lvs/report)
#   cmd/     LVSCMD    hefesto_lvs_cmd_tests   (data: tests/data/lvs/cmd)

hefesto_add_area_tests(lvs)

foreach(_lvs_sub IN ITEMS net read setup cmp reduce prop hier report cmd)
  add_subdirectory(${_lvs_sub})
endforeach()
