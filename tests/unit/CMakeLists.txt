# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests (Catch2 v3). Shared file owned by INTEGRATION
# (openspec/changes/m1-core-viewer/design.md D2); each area adds tests in its
# own subdirectory and never edits this file:
#   core/    INTEGRATION (M0 core)          (always)
#   cli/     PDK (M1 CLI commands)          (when hefesto-cli is built)
#   net/     INTEGRATION (HTTPS Fetcher)    (when hefesto::platform_net is built)
#   tech/    TECH                           (always)
#   db/      MODEL                          (always)
#   cell/    CELL                           (always)
#   display/ view/  VIEWER                  (always)
#   pdk/ app/       PDK                     (always)
#   undo/    UNDO (M2)                      (always)
#   select/ connect/  SELECT (M2)           (always)
#   ui/      DISPATCH (M2)                  (always)
#   wiring/  WIRE (M2)                      (always)
#   drc/     INTEGRATION (M3 shared tests); drc/rules/ RULES, drc/check/
#            CHECK, drc/engine/ ENGINE, drc/commands/ COMMANDS   (always)
#   hints/   HINTS (M3)                     (always)
#   script/  INTEGRATION (M4 shared tests); script/tcl/ TCL, script/binding/
#            BINDING, script/pdk/ PDKINT    (always)
#   pcell/   pcell/framework/, pcell/native/ PCELL (M4)   (always)
#   tools/   TOOLS (M4)                     (always)
#   gui/     GUIMGR (M4)                    (always)
#   mask/ gds/ cif/  INTEGRATION (M5 shared tests); each subdirectory is
#            owned by its workstream (openspec/changes/m5-mask-gds-cif
#            design.md D1/D2)              (always)
#   render/  VIEWER                         (desktop builds, if present)
#   shell/   SHELL                          (desktop builds, if present)
#   common/  INTEGRATION: header-only helpers shared by the area tests
# Each M1 area directory builds one executable hefesto_<area>_tests from every
# *_test.cpp in it (glob), so adding a test file needs no CMake edit.
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

# hefesto_add_area_tests(<area> [DATA_DIR <dir>])
#   One Catch2 executable hefesto_<area>_tests from the *_test.cpp files of
#   the current directory, linked to hefesto::core, with HEFESTO_SOURCE_DIR
#   and HEFESTO_TEST_DATA_DIR (tests/data/<dir>, default tests/data/<area>)
#   defined for fixtures. The M3 drc subdirectories use e.g.
#   hefesto_add_area_tests(drc_rules DATA_DIR drc/rules). A directory without
#   tests yet builds nothing (the M5 scaffold creates the M5 test directories
#   before their workstreams add tests; openspec/changes/m5-mask-gds-cif D1).
function(hefesto_add_area_tests area)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "DATA_DIR" "")
  if(NOT ARG_DATA_DIR)
    set(ARG_DATA_DIR "${area}")
  endif()
  file(GLOB _sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp")
  if(NOT _sources)
    return()
  endif()
  hefesto_add_unit_test(hefesto_${area}_tests SOURCES ${_sources} LIBS hefesto::core)
  target_compile_definitions(hefesto_${area}_tests PRIVATE
    HEFESTO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    HEFESTO_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data/${ARG_DATA_DIR}")
  # common/: shared test helpers (reference_data.hpp: pinned PDK and Magic data).
  target_include_directories(hefesto_${area}_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests/unit/common")
endfunction()

foreach(_area IN ITEMS tech db cell display view pdk app undo select connect ui wiring drc hints
                      script pcell tools gui mask gds cif)
  add_subdirectory(${_area})
endforeach()

if(TARGET hefesto-cli)
  add_subdirectory(cli)
endif()

if(TARGET hefesto_platform_net)
  add_subdirectory(net)  # INTEGRATION: the HTTPS Fetcher (URLSession, libcurl)
endif()

if(HEFESTO_BUILD_DESKTOP)
  foreach(_area IN ITEMS render shell)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_area}/CMakeLists.txt")
      add_subdirectory(${_area})
    endif()
  endforeach()
endif()
