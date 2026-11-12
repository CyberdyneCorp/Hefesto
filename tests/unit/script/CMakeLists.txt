# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the script area (openspec/changes/m4-scripting/design.md D1/D2).
# Owner: INTEGRATION (this file and the shared tests here, which cover the
# shared types of src/core/include/hefesto/script/{types,interp,events,project}.hpp).
# Each subdirectory is owned by its workstream and builds its own executable:
#   tcl/      TCL      hefesto_script_tcl_tests      (data: tests/data/script/tcl)
#   binding/  BINDING  hefesto_script_binding_tests  (data: tests/data/script/binding)
#   pdk/      PDKINT   hefesto_script_pdk_tests      (data: tests/data/script/pdk)

hefesto_add_area_tests(script)

foreach(_script_sub IN ITEMS tcl binding pdk)
  add_subdirectory(${_script_sub})
endforeach()
