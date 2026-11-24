# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Schematic benchmark (openspec/changes/m8-schematic task 14.1): load, save and
# netlist time of the largest PDK example schematics and of a generated
# 10k-instance hierarchy (scripts/gen_schem_hierarchy.py), through the public
# schematic API. bench/schem/schem_bench.py drives it and compares the report
# with bench/schem/baseline.json.
#   build/<preset>/bench/schem/hefesto-bench-schem --schematic FILE --sharedir DIR --out-dir DIR
# Owner: PERF (design D3).

add_executable(hefesto-bench-schem main.cpp)
target_link_libraries(hefesto-bench-schem
  PRIVATE
    hefesto::core
    nlohmann_json::nlohmann_json)
hefesto_set_warnings(hefesto-bench-schem)
