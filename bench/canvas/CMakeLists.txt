# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Canvas benchmark for the M0 Raven go/no-go spike (capability
# hefesto-performance, requirement "Interactive frame rate"; design.md D9).
# Desktop only; runs headless (offscreen) and prints a JSON report.
#   build/<preset>/bench/canvas/hefesto-bench-canvas [--rects N] [--output FILE]
# Owner: workstream "Desktop shell & canvas spike" (design.md D2).

add_executable(hefesto-bench-canvas
  main.cpp)
target_link_libraries(hefesto-bench-canvas
  PRIVATE
    hefesto::render_vk
    nlohmann_json::nlohmann_json)
hefesto_set_warnings(hefesto-bench-canvas)
