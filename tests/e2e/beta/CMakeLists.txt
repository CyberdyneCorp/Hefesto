# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop Beta end-to-end tests (Q-SIM-27; openspec/changes/m9-lvs-simulation task 20.2), per PDK:
# - layout: beta_inv drawn with its own transistors, zero DRC errors with the PDK's Magic tech file,
#   written to GDS;
# - schematic: beta_inv.sch and the FO4 testbench beta_tb.sch drawn with the PDK's xschem symbols,
#   saved and netlisted.
# Each is skipped (exit 77) when its PDK data set is not installed (scripts/fetch_pdk.py).
# Included from tests/unit/CMakeLists.txt.

find_package(Python3 3.10 COMPONENTS Interpreter)
if(NOT Python3_FOUND OR NOT TARGET hefesto-cli)
  return()
endif()

foreach(_beta_pdk IN ITEMS sky130 gf180mcu ihp)
  string(TOUPPER "${_beta_pdk}" _beta_id)
  add_test(NAME e2e.beta.${_beta_pdk}.layout
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_beta.py
            --cli $<TARGET_FILE:hefesto-cli> --source-dir ${PROJECT_SOURCE_DIR} --pdk ${_beta_pdk}
            --work ${CMAKE_CURRENT_BINARY_DIR}/${_beta_pdk})
  set_tests_properties(e2e.beta.${_beta_pdk}.layout PROPERTIES
    LABELS "e2e;beta;BETA-E2E-${_beta_id}"
    SKIP_RETURN_CODE 77 TIMEOUT 600)
  add_test(NAME e2e.beta.${_beta_pdk}.schematic
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_beta.py
            --cli $<TARGET_FILE:hefesto-cli> --source-dir ${PROJECT_SOURCE_DIR} --pdk ${_beta_pdk}
            --work ${CMAKE_CURRENT_BINARY_DIR}/${_beta_pdk}-schematic --step schematic)
  set_tests_properties(e2e.beta.${_beta_pdk}.schematic PROPERTIES
    LABELS "e2e;beta;BETA-E2E-${_beta_id}"
    SKIP_RETURN_CODE 77 TIMEOUT 600)
endforeach()
