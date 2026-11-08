# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests (Catch2 v3). Shared file owned by the scaffolding workstream;
# each area adds tests in its own subdirectory (design.md D2):
#   core/    Core & CLI workstream          (always)
#   cli/     Core & CLI workstream          (when hefesto-cli is built)
#   render/  Desktop shell workstream       (desktop builds, if present)
#   shell/   Desktop shell workstream       (desktop builds, if present)
#
# Tests name the capability and requirement they cover in Catch2 tags, for the
# traceability report (capability hefesto-testing-strategy):
#   TEST_CASE("bucket of short key",
#             "[cap:hefesto-output-determinism][req:Magic string hash table]")

include(Catch)

# hefesto_add_unit_test(<name> SOURCES <files...> [LIBS <targets...>])
#   Creates a Catch2 test executable linked to Catch2WithMain and LIBS, applies
#   Hefesto warnings, and registers each TEST_CASE with CTest (label "unit").
#   Discovery happens when ctest runs, so building never executes tests.
function(hefesto_add_unit_test name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;LIBS")
  if(NOT ARG_SOURCES)
    message(FATAL_ERROR "hefesto_add_unit_test(${name}): SOURCES is required")
  endif()
  add_executable(${name} ${ARG_SOURCES})
  target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain ${ARG_LIBS})
  hefesto_set_warnings(${name})
  catch_discover_tests(${name}
    TEST_PREFIX "${name}: "
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS unit)
endfunction()

add_subdirectory(core)

if(TARGET hefesto-cli)
  add_subdirectory(cli)
endif()

if(HEFESTO_BUILD_DESKTOP)
  foreach(_area IN ITEMS render shell)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_area}/CMakeLists.txt")
      add_subdirectory(${_area})
    endif()
  endforeach()
endif()
