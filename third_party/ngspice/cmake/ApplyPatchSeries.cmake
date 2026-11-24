# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Applies the Hefesto patch series to an unpacked ngspice tree, in the order of
# the series file (comment and blank lines skipped):
#
#   cmake -DSOURCE_DIR=<tree> -DSERIES=<patches/series> -P ApplyPatchSeries.cmake
#
# The patches touch some files twice (0003 and 0004 both edit randnumb.c), so
# "does this patch reverse cleanly" cannot tell whether the series is applied.
# A stamp file in the tree records the SHA-256 of the series and of every patch
# instead: the same stamp means nothing to do; another stamp means the tree was
# patched by a different series and must be unpacked again. Kept apart from
# cmake/ApplyPatches.cmake so that third_party/ngspice also builds on its own
# (the capture image).

find_program(_ng_patch patch REQUIRED)
get_filename_component(_ng_dir "${SERIES}" DIRECTORY)
file(STRINGS "${SERIES}" _ng_lines)
set(_ng_patches "")
file(SHA256 "${SERIES}" _ng_stamp)
foreach(_line IN LISTS _ng_lines)
  string(STRIP "${_line}" _line)
  if(_line STREQUAL "" OR _line MATCHES "^#")
    continue()
  endif()
  list(APPEND _ng_patches "${_line}")
  file(SHA256 "${_ng_dir}/${_line}" _hash)
  string(APPEND _ng_stamp "\n${_hash} ${_line}")
endforeach()

set(_ng_stamp_file "${SOURCE_DIR}/.hefesto-patch-series")
if(EXISTS "${_ng_stamp_file}")
  file(READ "${_ng_stamp_file}" _old)
  if(_old STREQUAL _ng_stamp)
    return()
  endif()
  message(FATAL_ERROR "ngspice: ${SOURCE_DIR} carries another version of the patch series; "
                      "delete that directory so the pinned archive is unpacked and patched again")
endif()

foreach(_name IN LISTS _ng_patches)
  execute_process(COMMAND "${_ng_patch}" -p1 -f -s -i "${_ng_dir}/${_name}"
                  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE _result)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "ngspice: ${_name} does not apply to ${SOURCE_DIR}")
  endif()
  message(STATUS "ngspice: applied ${_name}")
endforeach()
file(WRITE "${_ng_stamp_file}" "${_ng_stamp}")
