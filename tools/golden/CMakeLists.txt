# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Golden compatibility tests (capability hefesto-testing-strategy, requirement
# "Golden compatibility suite"). Registers CTest tests that replay captured
# cases in tests/golden/<AREA>/cases/ through hefesto-cli (label "golden") and
# the comparator/pipeline unit tests (label "golden-tools").
# Owner: workstream "Golden capture" (design.md D2). Workflow: README.md.

# 3.11: the tools read TOML with the standard-library tomllib.
find_package(Python3 3.11 COMPONENTS Interpreter)
if(NOT Python3_FOUND)
  message(STATUS "Hefesto: Python 3.11+ not found; golden tests are not registered")
  return()
endif()

set(_golden_env "PYTHONDONTWRITEBYTECODE=1")

add_test(NAME golden-tools.unit
  COMMAND Python3::Interpreter -m unittest discover -s tests -t . -v
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(golden-tools.unit PROPERTIES LABELS golden-tools ENVIRONMENT "${_golden_env}")

# Every case manifest must validate, captured or not.
file(GLOB _golden_cases CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/golden/*/cases/*/case.toml)
list(TRANSFORM _golden_cases REPLACE "/case.toml$" "")
if(_golden_cases)
  add_test(NAME golden-tools.validate-cases
    COMMAND Python3::Interpreter capture.py --check ${_golden_cases}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(golden-tools.validate-cases PROPERTIES LABELS golden-tools ENVIRONMENT "${_golden_env}")
endif()

if(NOT TARGET hefesto-cli)
  message(STATUS "Hefesto: hefesto-cli not built; golden replay tests are not registered")
  return()
endif()

# One test per captured case. Re-run CMake after adding a case.
file(GLOB _golden_manifests CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/tests/golden/*/cases/*/expected/provenance.json)
foreach(_manifest IN LISTS _golden_manifests)
  get_filename_component(_case_dir "${_manifest}" DIRECTORY)
  get_filename_component(_case_dir "${_case_dir}" DIRECTORY)
  get_filename_component(_case_id "${_case_dir}" NAME)
  add_test(NAME golden.${_case_id}
    COMMAND Python3::Interpreter replay.py --hefesto-cli $<TARGET_FILE:hefesto-cli>
            --skip-code 77 ${_case_dir}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  # 77 (reported as Skipped): the case needs a command this hefesto-cli lacks, or it is a documented
  # deviation (tests/golden/deviations.toml, goldenlib/deviations.py).
  # A manifest with `slow = true` (standard-cell corpus, m3-drc design D14) is labelled
  # golden-slow instead: it runs in the golden job, not in the per-leg unit run.
  file(STRINGS "${_case_dir}/case.toml" _golden_slow REGEX "^slow = true$")
  set(_golden_label golden)
  if(_golden_slow)
    set(_golden_label golden-slow)
  endif()
  set_tests_properties(golden.${_case_id} PROPERTIES
    LABELS ${_golden_label} SKIP_RETURN_CODE 77 ENVIRONMENT "${_golden_env}")
endforeach()
