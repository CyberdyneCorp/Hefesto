# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# hefesto-extcheck: Magic's standalone extcheck (capability netlist-extcheck;
# openspec/changes/m7-extraction/design.md D2, D15; Open Question 6 (a)).
# A thin main() over netlist::extcheck::run. Desktop hosts only (Linux and
# macOS, like hefesto-cli). Owner: SIM (the target); INTEGRATION (install
# and packaging, design D20).

add_executable(hefesto-extcheck
  main.cpp)
target_link_libraries(hefesto-extcheck PRIVATE hefesto::core)
hefesto_set_warnings(hefesto-extcheck)
