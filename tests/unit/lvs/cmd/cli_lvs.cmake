# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Runs hefesto-cli with `lvs layout.spice schematic.spice setup.tcl comp.out
# -json` in a fresh folder and compares comp.out and comp.json with
# ${DATA}/expected (capability lvs-command "Headless LVS", "lvs command
# syntax"; lvs-reports "JSON file"). Owner: LVSCMD.
#
#   cmake -DCLI=<hefesto-cli> -DDATA=<tests/data/lvs/cmd> -DWORK=<folder> -P cli_lvs.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${DATA}/layout.spice" "${DATA}/schematic.spice" DESTINATION "${WORK}")

execute_process(
  COMMAND "${CLI}" --no-log -c "lvs layout.spice schematic.spice setup.tcl comp.out -json; quit"
  WORKING_DIRECTORY "${WORK}"
  RESULT_VARIABLE status
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "hefesto-cli exited ${status}\nstdout:\n${out}\nstderr:\n${err}")
endif()

foreach(name comp.out comp.json)
  if(NOT EXISTS "${WORK}/${name}")
    message(FATAL_ERROR "${name} was not written\nstdout:\n${out}\nstderr:\n${err}")
  endif()
  file(READ "${WORK}/${name}" actual)
  file(READ "${DATA}/expected/${name}" expected)
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "${name} differs from ${DATA}/expected/${name}:\n${actual}")
  endif()
endforeach()

string(FIND "${out}${err}" "LVS Done." done)
if(done EQUAL -1)
  message(FATAL_ERROR "no `LVS Done.` on the console\nstdout:\n${out}\nstderr:\n${err}")
endif()
