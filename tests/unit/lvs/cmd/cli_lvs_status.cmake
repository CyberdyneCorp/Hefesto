# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Runs hefesto-cli with `lvs layout.spice <SECOND> nosetup nolog` in a fresh
# folder and checks the exit status (capability lvs-command "Headless LVS":
# 0 when the circuits match, 1 when they do not, 2 when an input cannot be
# read). Owner: LVSCMD.
#
#   cmake -DCLI=<hefesto-cli> -DDATA=<tests/data/lvs/cmd> -DWORK=<folder>
#         -DSECOND=<file> -DEXPECT=<status> -P cli_lvs_status.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${DATA}/layout.spice" "${DATA}/schematic.spice" "${DATA}/mismatch.spice" DESTINATION "${WORK}")

execute_process(
  COMMAND "${CLI}" --no-log -c "lvs layout.spice ${SECOND} nosetup nolog; quit"
  WORKING_DIRECTORY "${WORK}"
  RESULT_VARIABLE status
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)
if(NOT status EQUAL EXPECT)
  message(FATAL_ERROR "hefesto-cli exited ${status}, expected ${EXPECT}\nstdout:\n${out}\nstderr:\n${err}")
endif()
