# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Fails when src/core includes a windowing, GPU, OS-UI, Raven or
# process-spawning header (capability hefesto-architecture, requirement
# "Platform-independent core"). Run by CTest:
#   cmake -DCORE_DIR=<src/core> -P check_core_includes.cmake
# scripts/check_core_includes.py (CI lint job) is the authoritative check;
# this keeps the rule enforced in every local `ctest` run.

if(NOT CORE_DIR)
  message(FATAL_ERROR "CORE_DIR is required")
endif()

set(_forbidden
  "vulkan/" "raven" "GL/" "GLES" "OpenGL/" "Metal/" "MetalKit/" "Cocoa/" "AppKit/" "UIKit/"
  "Foundation/" "CoreGraphics/" "QuartzCore/" "X11/" "xcb/" "wayland-" "windows.h" "SDL" "GLFW/"
  "Qt" "gtk/" "spawn.h" "unistd.h" "sys/wait.h" "sys/types.h" "process.h" "boost/process")

file(GLOB_RECURSE _sources "${CORE_DIR}/*.hpp" "${CORE_DIR}/*.cpp" "${CORE_DIR}/*.h" "${CORE_DIR}/*.in")
set(_violations "")
foreach(_file IN LISTS _sources)
  file(STRINGS "${_file}" _includes REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]")
  foreach(_line IN LISTS _includes)
    string(REGEX REPLACE "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]*)[>\"].*$" "\\1" _header "${_line}")
    foreach(_pattern IN LISTS _forbidden)
      string(FIND "${_header}" "${_pattern}" _pos)
      if(_pos EQUAL 0 OR (_pattern STREQUAL "raven" AND NOT _pos EQUAL -1))
        list(APPEND _violations "${_file}: #include <${_header}>")
      endif()
    endforeach()
  endforeach()
endforeach()

if(_violations)
  list(JOIN _violations "\n  " _report)
  message(FATAL_ERROR "forbidden includes in the core:\n  ${_report}")
endif()
list(LENGTH _sources _count)
message(STATUS "core include check: ${_count} files clean")
