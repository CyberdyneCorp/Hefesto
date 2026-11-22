# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the system symbol library assets/schematic/xschem_library/devices
# (openspec/changes/m8-schematic/design.md D8, tasks 7.5 and 7.6). Owner: DEVLIB
# (design.md D3). Every *_test.cpp here is compiled into hefesto_schem_devlib_tests.

hefesto_add_area_tests(schem_devlib DATA_DIR schem/devlib)
if(TARGET hefesto_schem_devlib_tests)
  # devices_interface.json and inventory.json are read with nlohmann/json.
  target_link_libraries(hefesto_schem_devlib_tests PRIVATE nlohmann_json::nlohmann_json)
endif()
