# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# The simulation worker and the engine adapter plugin (openspec/changes/
# m9-lvs-simulation design D2, D9, Area contract SIMD; task 5.4).
# Owner: SIMD.
#
# Targets:
#   hefesto-simd             the worker executable (Linux, macOS): IPC server,
#                            heartbeat thread, confinement; loads the plugin at
#                            start-up (no ngspice symbol of its own)
#   hefesto-ngspice-adapter  the plugin, a shared module exporting
#                            hefesto_engine_v1 (HEFESTO_BUILD_NGSPICE=ON only;
#                            the only code that includes sharedspice.h)
#   hefesto_simd_devhost     the in-process developer host (Q-SIM-22 (a));
#                            HEFESTO_DEVELOPER_BUILD=ON only
#
# Install layout: bin/hefesto-simd, lib/hefesto-ngspice-adapter.{so,dylib},
# lib/libngspice-hefesto.*; hefesto-simd looks for the plugin in
# <bin>/../lib, then next to itself (the build tree), and the plugin finds
# libngspice-hefesto through its runpath ($ORIGIN, @loader_path).

find_package(Threads REQUIRED)

if(APPLE)
  set(_simd_sandbox sandbox/sandbox_macos.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(_simd_sandbox sandbox/sandbox_linux.cpp)
else()
  message(STATUS "Hefesto: hefesto-simd needs Linux or macOS; not built")
  return()
endif()

add_executable(hefesto-simd
  main.cpp
  ipc/frame_io.cpp
  server/plugin.cpp
  server/server.cpp
  ${_simd_sandbox})
target_include_directories(hefesto-simd PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/src/core/include")  # header-only: hefesto/sim/engine/frames.hpp
target_link_libraries(hefesto-simd PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
hefesto_set_warnings(hefesto-simd)
install(TARGETS hefesto-simd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# macOS: hefesto-simd, and only it, may load a library signed by someone else
# (a user-rebuilt libngspice-hefesto; design D2 "Replaceability"). The build
# signs it ad hoc with the entitlement; release packaging re-signs it with the
# hardened runtime and the same entitlements file.
if(APPLE)
  find_program(HEFESTO_CODESIGN codesign)
  if(HEFESTO_CODESIGN)
    add_custom_command(TARGET hefesto-simd POST_BUILD
      COMMAND "${HEFESTO_CODESIGN}" --force --sign -
              --entitlements "${CMAKE_CURRENT_SOURCE_DIR}/hefesto-simd.entitlements"
              "$<TARGET_FILE:hefesto-simd>"
      VERBATIM)
  endif()
endif()

if(TARGET ngspice-hefesto)
  file(READ "${PROJECT_SOURCE_DIR}/third_party/ngspice/pin.json" _simd_pin)
  string(JSON _simd_release GET "${_simd_pin}" release)
  string(JSON _simd_level GET "${_simd_pin}" hefesto_level)
  add_library(hefesto-ngspice-adapter MODULE adapter/ngspice_adapter.cpp)
  set_target_properties(hefesto-ngspice-adapter PROPERTIES
    PREFIX ""
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    BUILD_RPATH_USE_ORIGIN ON)
  if(APPLE)
    set_target_properties(hefesto-ngspice-adapter PROPERTIES INSTALL_RPATH "@loader_path")
  else()
    set_target_properties(hefesto-ngspice-adapter PROPERTIES INSTALL_RPATH "$ORIGIN")
  endif()
  target_include_directories(hefesto-ngspice-adapter PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_definitions(hefesto-ngspice-adapter PRIVATE
    NGSPICE_HEFESTO_VERSION="${_simd_release}-hefesto${_simd_level}")
  target_link_libraries(hefesto-ngspice-adapter PRIVATE hefesto::ngspice Threads::Threads)
  hefesto_set_warnings(hefesto-ngspice-adapter)
  install(TARGETS hefesto-ngspice-adapter LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

# The in-process developer host (Q-SIM-22 (a)): developer builds only. It
# loads the same plugin; release builds never compile it.
if(HEFESTO_DEVELOPER_BUILD)
  add_library(hefesto_simd_devhost STATIC devhost/in_process_engine.cpp server/plugin.cpp)
  target_include_directories(hefesto_simd_devhost PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(hefesto_simd_devhost PUBLIC hefesto::core PRIVATE ${CMAKE_DL_LIBS})
  hefesto_set_warnings(hefesto_simd_devhost)
endif()
