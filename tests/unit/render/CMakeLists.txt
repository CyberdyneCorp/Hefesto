# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Vulkan canvas renderer tests (desktop builds). The GPU test SKIPs when no
# Vulkan device can be created (e.g. a CI runner without a driver).
# Owner: workstream "Desktop shell & canvas spike" (design.md D2).

hefesto_add_unit_test(hefesto_render_tests
  SOURCES
    camera_test.cpp
    canvas_index_test.cpp
    headless_gpu_test.cpp
    scene_test.cpp
    synthetic_layout_test.cpp
  LIBS
    hefesto::render_vk)
