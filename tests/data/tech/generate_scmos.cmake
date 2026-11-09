# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Generates Magic's scmos.tech from the reference tree (build/ref/magic, not
# committed) for the TECH unit tests, the way scmos/Makefile.in does, but
# with the C compiler's preprocessor in traditional mode instead of
# Magic's configure-selected one (Magic's own preprocessor script is not run).
#   cmake -DMAGIC=<magic tree> -DOUT=<dir> -DCXX=<compiler> -P generate_scmos.cmake
# Produces <dir>/scmos.tech; the file is test input only.

set(_tpl "${MAGIC}/scmos/cif_template")
file(MAKE_DIRECTORY "${OUT}/cif_template/objs")

function(_preprocess input output)
  execute_process(
    COMMAND "${CXX}" -E -P -x c -traditional-cpp -I${_tpl} -I${OUT} -I${MAGIC}/scmos/extract_template
            -I${MAGIC}/scmos ${ARGN} "${input}"
    OUTPUT_VARIABLE _text
    ERROR_QUIET
    RESULT_VARIABLE _rc)
  if(NOT _rc EQUAL 0)
    message(WARNING "scmos.tech generation failed on ${input}")
    return()
  endif()
  # cif_template/Makefile.in SED_CMD: drop directive lines, normalize "(gen )" etc.
  string(REGEX REPLACE "\n#[^\n]*" "" _text "${_text}")
  foreach(_word gen nowell nwell pwell)
    string(REPLACE "(${_word} )" "(${_word})" _text "${_text}")
  endforeach()
  file(WRITE "${output}" "${_text}")
endfunction()

_preprocess("${_tpl}/cifout.c" "${OUT}/cif_template/objs/CIFout" -DSTANDARD)
_preprocess("${_tpl}/cifin.c" "${OUT}/cif_template/objs/CIFin" -DSTANDARD)
_preprocess("${MAGIC}/scmos/scmos.tech.in" "${OUT}/scmos.tech" -DV5 -DSTANDARD)
