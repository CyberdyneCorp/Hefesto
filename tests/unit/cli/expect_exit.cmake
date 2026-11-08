# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Runs ${CLI} ${ARGS} and fails unless it exits with ${EXPECT}.
#   cmake -DCLI=<exe> -DEXPECT=<code> "-DARGS=a;b" -P expect_exit.cmake

execute_process(COMMAND "${CLI}" ${ARGS}
  RESULT_VARIABLE _status
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err)
if(NOT _status STREQUAL "${EXPECT}")
  message(FATAL_ERROR "expected exit ${EXPECT}, got '${_status}'\nstdout:\n${_out}\nstderr:\n${_err}")
endif()
