# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of view3d/mesh. Owner: MESH (openspec/changes/m10-3d-view/design.md
# D2/D3). Every *_test.cpp here is compiled into hefesto_view3d_mesh_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(view3d_mesh DATA_DIR view3d/mesh)
if(TARGET hefesto_view3d_mesh_tests)
  # The build steps of src/core/src/view3d/mesh/internal.hpp (chunk plan).
  target_include_directories(hefesto_view3d_mesh_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/core/src")
endif()
