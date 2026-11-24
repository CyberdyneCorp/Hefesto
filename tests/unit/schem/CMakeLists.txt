# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the schem area (openspec/changes/m8-schematic/design.md D3).
# Owner: SCAFFOLD (this file and the shared tests here). Each subdirectory
# is owned by its workstream and builds its own executable once it has a
# *_test.cpp:
#   file/   FORMAT   hefesto_schem_file_tests   (data: tests/data/schem/file)
#   sym/    SYMLIB   hefesto_schem_sym_tests   (data: tests/data/schem/sym)
#   lib/    SYMLIB   hefesto_schem_lib_tests   (data: tests/data/schem/lib)
#   tcl/    SYMLIB   hefesto_schem_tcl_tests   (data: tests/data/schem/tcl)
#   net/    NETLIST  hefesto_schem_net_tests   (data: tests/data/schem/net)
#   edit/   EDIT     hefesto_schem_edit_tests   (data: tests/data/schem/edit)
#   cmd/    CMD      hefesto_schem_cmd_tests   (data: tests/data/schem/cmd)
#   sim/    SIMINT   hefesto_schem_sim_tests   (data: tests/data/schem/sim)

hefesto_add_area_tests(schem)

foreach(_schem_sub IN ITEMS file sym lib tcl net edit cmd sim devlib security)
  add_subdirectory(${_schem_sub})
endforeach()
