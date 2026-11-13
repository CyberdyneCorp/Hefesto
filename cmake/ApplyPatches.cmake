# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Applies unified diffs (-p1) to a fetched source tree, once:
#
#   cmake -DSOURCE_DIR=<tree> "-DPATCHES=<a.patch;b.patch>" -P ApplyPatches.cmake
#
# A patch that is already applied (it reverses cleanly) is skipped, so the
# script is safe to run again on a populated or cached tree. A patch that
# neither applies nor reverses stops the configure.

find_program(_hefesto_patch patch REQUIRED)

foreach(_patch IN LISTS PATCHES)
  execute_process(COMMAND "${_hefesto_patch}" -p1 -R -f -s --dry-run -i "${_patch}"
                  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE _applied OUTPUT_QUIET ERROR_QUIET)
  if(_applied EQUAL 0)
    continue()
  endif()
  execute_process(COMMAND "${_hefesto_patch}" -p1 -f -s -i "${_patch}"
                  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE _result)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Hefesto: ${_patch} does not apply to ${SOURCE_DIR}")
  endif()
  message(STATUS "Hefesto: applied ${_patch}")
endforeach()
