# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# hefesto_shell_schematic_models: the desktop schematic editor's view models
# (canvas, symbol library, panels, key bindings, canvas input, symbol
# resolution, the clipboard link) built without Raven and without Vulkan,
# for builds with HEFESTO_BUILD_DESKTOP=OFF: their tests
# (tests/unit/shell/schematic_models) then build and run on any machine,
# under ThreadSanitizer too (the linux-tsan CI job). A desktop build
# compiles the same sources into hefesto_shell_desktop instead
# (../CMakeLists.txt), so this target is never linked next to it.
# The few Raven-free shell sources the models use come along; the render
# headers they include (camera, scene) are Vulkan-free, and the camera's
# source is plain geometry.
# Owner: SHELL (openspec/changes/m8-schematic task 13.2).

set(_shell "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(_render "${PROJECT_SOURCE_DIR}/src/render/vulkan")
add_library(hefesto_shell_schematic_models STATIC
  ${_shell}/src/schematic_bindings.cpp
  ${_shell}/src/schematic_canvas.cpp
  ${_shell}/src/schematic_clipboard.cpp
  ${_shell}/src/schematic_input.cpp
  ${_shell}/src/schematic_library.cpp
  ${_shell}/src/schematic_panels.cpp
  ${_shell}/src/schematic_symbols.cpp
  # Raven-free support the models use.
  ${_shell}/src/canvas_controller.cpp
  ${_shell}/src/input_router.cpp
  ${_shell}/src/key_bindings_sheet.cpp
  ${_shell}/src/manager_views.cpp
  ${_shell}/src/script_consent_model.cpp
  ${_shell}/src/assets.cpp
  ${_shell}/src/gui_script.cpp
  ${_shell}/src/screen_models.cpp
  ${_shell}/src/view_models.cpp
  ${_render}/src/camera.cpp)
add_library(hefesto::shell_schematic_models ALIAS hefesto_shell_schematic_models)
target_include_directories(hefesto_shell_schematic_models
  PUBLIC "$<BUILD_INTERFACE:${_shell}/include>" "$<BUILD_INTERFACE:${_render}/include>")
target_link_libraries(hefesto_shell_schematic_models PUBLIC hefesto::core)
hefesto_set_warnings(hefesto_shell_schematic_models)
