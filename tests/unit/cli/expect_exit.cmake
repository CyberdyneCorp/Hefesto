# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Runs ${CLI} ${ARGS} and fails unless it exits with ${EXPECT}. With
# -DSTDIN=<text>, the text is the process's standard input; with
# -DEXPECT_OUT=<regex>, standard output must match it.
#   cmake -DCLI=<exe> -DEXPECT=<code> "-DARGS=a;b" [-DSTDIN=...] -P expect_exit.cmake

set(_input)
if(DEFINED STDIN)
  string(RANDOM LENGTH 12 _tag)
  set(_stdin_file "${CMAKE_CURRENT_BINARY_DIR}/expect_exit_stdin_${_tag}.txt")
  file(WRITE "${_stdin_file}" "${STDIN}")
  set(_input INPUT_FILE "${_stdin_file}")
endif()
execute_process(COMMAND "${CLI}" ${ARGS}
  ${_input}
  RESULT_VARIABLE _status
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err)
if(NOT _status STREQUAL "${EXPECT}")
  message(FATAL_ERROR "expected exit ${EXPECT}, got '${_status}'\nstdout:\n${_out}\nstderr:\n${_err}")
endif()
if(DEFINED EXPECT_OUT AND NOT _out MATCHES "${EXPECT_OUT}")
  message(FATAL_ERROR "stdout does not match '${EXPECT_OUT}':\n${_out}\nstderr:\n${_err}")
endif()
if(DEFINED _stdin_file)
  file(REMOVE "${_stdin_file}")
endif()
