# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of view3d/export. Owner: EXPORT (P2) (openspec/changes/m10-3d-view/design.md
# D2/D3). Every *_test.cpp here is compiled into hefesto_view3d_export_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(view3d_export DATA_DIR view3d/export)

# The tests read exported glTF files back with cgltf_parse and cgltf_validate
# (compiled once into hefesto_core, view3d/export/cgltf_impl.cpp).
if(TARGET hefesto_view3d_export_tests)
  target_link_libraries(hefesto_view3d_export_tests PRIVATE hefesto::cgltf)
  # The size checks of src/core/src/view3d/export/model.hpp (GLB, STL).
  target_include_directories(hefesto_view3d_export_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/core/src")
endif()
