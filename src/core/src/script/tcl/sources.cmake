# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of script/tcl (the embedded Tcl and its sandbox), included by
# src/core/src/script/sources.cmake. Only the files of this directory
# include tcl.h (hefesto::tcl is linked privately by hefesto_core).
# Owner: TCL (openspec/changes/m4-scripting/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/embedded_library.cpp
  ${CMAKE_CURRENT_LIST_DIR}/sandbox.cpp
  ${CMAKE_CURRENT_LIST_DIR}/sandbox_commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tcl_filesystem.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tcl_interpreter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/value_conversion.cpp)
