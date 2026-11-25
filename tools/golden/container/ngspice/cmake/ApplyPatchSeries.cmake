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
# instead: the same stamp means nothing to do. The applied patches are copied
# into the tree (.hefesto-applied/), so when the stamp differs (a patch or the
# series was edited) the old series is reversed, last patch first, and the new
# one applied; a tree patched before those copies existed must be unpacked
# again. Kept apart from
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
set(_ng_applied "${SOURCE_DIR}/.hefesto-applied")

# Reverses the series recorded in .hefesto-applied/order, last patch first.
function(_ng_reverse_applied)
  set(_unpack "delete ${SOURCE_DIR} so the pinned archive is unpacked and patched again")
  if(NOT EXISTS "${_ng_applied}/order")
    message(FATAL_ERROR "ngspice: ${SOURCE_DIR} carries another version of the patch series "
                        "and no copy of it to reverse; ${_unpack}")
  endif()
  file(STRINGS "${_ng_applied}/order" _old_patches)
  list(REVERSE _old_patches)
  foreach(_name IN LISTS _old_patches)
    execute_process(COMMAND "${_ng_patch}" -R -p1 -f -s -i "${_ng_applied}/${_name}"
                    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE _result)
    if(NOT _result EQUAL 0)
      message(FATAL_ERROR "ngspice: the applied ${_name} does not reverse in ${SOURCE_DIR}; ${_unpack}")
    endif()
    message(STATUS "ngspice: reversed ${_name}")
  endforeach()
  file(REMOVE_RECURSE "${_ng_applied}")
  file(REMOVE "${_ng_stamp_file}")
endfunction()

if(EXISTS "${_ng_stamp_file}")
  file(READ "${_ng_stamp_file}" _old)
  if(_old STREQUAL _ng_stamp)
    return()
  endif()
  _ng_reverse_applied()
endif()

file(MAKE_DIRECTORY "${_ng_applied}")
foreach(_name IN LISTS _ng_patches)
  execute_process(COMMAND "${_ng_patch}" -p1 -f -s -i "${_ng_dir}/${_name}"
                  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE _result)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "ngspice: ${_name} does not apply to ${SOURCE_DIR}")
  endif()
  file(COPY "${_ng_dir}/${_name}" DESTINATION "${_ng_applied}")
  file(APPEND "${_ng_applied}/order" "${_name}\n")
  message(STATUS "ngspice: applied ${_name}")
endforeach()
file(WRITE "${_ng_stamp_file}" "${_ng_stamp}")
