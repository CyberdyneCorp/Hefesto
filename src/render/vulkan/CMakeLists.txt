# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# hefesto_render_vk: Hefesto's own Vulkan canvas renderer (desktop only;
# MoltenVK on macOS). Knows Vulkan and plain geometry, never Raven.
# Public headers are Vulkan-free (handles cross as opaque pointers); only the
# sources under src/ include <vulkan/vulkan.h>.
# Owner: VIEWER (openspec/changes/m1-core-viewer/design.md D2).

add_library(hefesto_render_vk STATIC
  src/camera.cpp
  src/canvas_index.cpp
  src/headless.cpp
  src/layout_scene.cpp
  src/polygon_fill.cpp
  src/renderer.cpp
  src/scene.cpp
  src/synthetic_layout.cpp
  src/vk_util.cpp)
add_library(hefesto::render_vk ALIAS hefesto_render_vk)

target_include_directories(hefesto_render_vk
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(hefesto_render_vk
  PUBLIC
    hefesto::core
    Vulkan::Vulkan)
hefesto_set_warnings(hefesto_render_vk)

# The SPIR-V of the shaders is committed (src/generated/rect_spv.hpp), so a
# shader compiler is never needed to build. After editing shaders/*.vert|frag,
# rebuild the header with:  cmake --build <dir> --target hefesto_render_shaders
find_program(HEFESTO_GLSLANG glslangValidator)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(HEFESTO_GLSLANG AND Python3_FOUND)
  add_custom_target(hefesto_render_shaders
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile_shaders.py"
            --glslang "${HEFESTO_GLSLANG}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Compiling canvas shaders to src/generated/rect_spv.hpp"
    VERBATIM)
endif()
