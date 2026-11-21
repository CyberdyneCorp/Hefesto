# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Vulkan canvas renderer tests (desktop builds). The GPU test SKIPs when no
# Vulkan device can be created (e.g. a CI runner without a driver).
# Owner: VIEWER (openspec/changes/m1-core-viewer/design.md D2);
# mesh3d_target_test.cpp: RENDER3D (openspec/changes/m10-3d-view).

hefesto_add_unit_test(hefesto_render_tests
  SOURCES
    camera_test.cpp
    canvas_index_test.cpp
    headless_gpu_test.cpp
    layout_scene_test.cpp
    mesh3d_target_test.cpp
    polygon_fill_test.cpp
    scene_test.cpp
    synthetic_layout_test.cpp
  LIBS
    hefesto::render_vk)
