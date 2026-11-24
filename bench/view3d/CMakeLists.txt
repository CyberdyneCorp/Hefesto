# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# 3D mesh benchmark (openspec/changes/m10-3d-view design D6, D17, task 13.1;
# capability hefesto-performance "Interactive frame rate"). Core only: builds
# the layout-layer mesh of a synthetic 1M-rectangle region or of a sky130
# reference layout and prints a JSON report (build time, model memory, the
# GPU size estimate) and, when the Vulkan renderer is built
# (HEFESTO_BUILD_DESKTOP=ON), the frame time of the mesh drawn by
# render::VulkanMesh3dRenderer on the headless device (task 13.2, frame.cpp;
# recorded, not gated). bench/view3d/view3d_gate.py gates the report.
#   build/<preset>/bench/view3d/hefesto-bench-view3d --workload synthetic --json FILE
# Owner: PERF (design D3); frame.cpp: RENDER3D (task 13.2).

add_executable(hefesto-bench-view3d
  frame.cpp
  main.cpp
  workloads.cpp)
target_link_libraries(hefesto-bench-view3d
  PRIVATE
    hefesto::core
    nlohmann_json::nlohmann_json)
hefesto_set_warnings(hefesto-bench-view3d)
if(TARGET hefesto::render_vk)
  target_link_libraries(hefesto-bench-view3d PRIVATE hefesto::render_vk)
  target_compile_definitions(hefesto-bench-view3d PRIVATE HEFESTO_VIEW3D_BENCH_FRAME=1)
endif()
