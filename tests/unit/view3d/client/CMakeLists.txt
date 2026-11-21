# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of view3d/client. Owner: WIND3D (openspec/changes/m10-3d-view/design.md
# D2/D3). Every *_test.cpp here is compiled into hefesto_view3d_client_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(view3d_client DATA_DIR view3d/client)

# The WindowDisplay test double drives render::Mesh3dRenderer through its
# header-only, Vulkan-free interface (design D11), so the tests build
# without the desktop renderer.
if(TARGET hefesto_view3d_client_tests)
  target_include_directories(hefesto_view3d_client_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/render/vulkan/include")
endif()
