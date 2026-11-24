# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# libngspice-hefesto: ngspice-47 (Q-SIM-7) with the Hefesto patch series,
# built by this Hefesto-written CMake wrapper (the ngspice-47 tag has no CMake
# build). openspec/changes/m9-lvs-simulation design D2 and D9, task 5.1;
# capability simulation-engine-embedding, "ngspice embedded as a library" and
# "ngspice build configuration and license hygiene".
#
# Two ways in:
#   - from Hefesto's top level, with -DHEFESTO_BUILD_NGSPICE=ON (default OFF);
#   - on its own: cmake -S third_party/ngspice -B build/ngspice
#     (.github/workflows/ngspice.yml, tools/golden/container/build_ngspice.sh).
#
# Targets:
#   ngspice-hefesto                 shared library libngspice-hefesto (the engine)
#   ngspice                         the executable (HEFESTO_NGSPICE_EXECUTABLE=ON;
#                                   golden capture image only)
#   ngspice_hefesto_manifest        ngspice-manifest.json (built with ALL)
#   ngspice_hefesto_source_archive  ngspice-hefesto-src.tar.gz: every built source
#                                   file, the whole OSDI tree, the patches
#
# Warnings: ngspice's own sources are compiled with -w. They do not build
# cleanly under Hefesto's warning set (-Wall -Wextra -Wconversion ... -Werror)
# and their findings are not Hefesto's to fix; the Hefesto glue in src/ is
# compiled with warnings as errors.

cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(HefestoNgspice LANGUAGES C CXX)
endif()

option(HEFESTO_NGSPICE_EXECUTABLE "Also build the ngspice executable (golden capture image)" OFF)

include(FetchContent)
include(GNUInstallDirs)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)

set(_ng_dir "${CMAKE_CURRENT_SOURCE_DIR}")
set(_ng_bin "${CMAKE_CURRENT_BINARY_DIR}")

# --- Pin (pin.json) -----------------------------------------------------------
file(READ "${_ng_dir}/pin.json" _ng_pin)
string(JSON _ng_release GET "${_ng_pin}" release)
string(JSON _ng_level GET "${_ng_pin}" hefesto_level)
string(JSON _ng_sha256 GET "${_ng_pin}" archive sha256)
string(JSON _ng_url_count LENGTH "${_ng_pin}" archive urls)
set(_ng_urls "")
math(EXPR _ng_last "${_ng_url_count} - 1")
foreach(_i RANGE ${_ng_last})
  string(JSON _url GET "${_ng_pin}" archive urls ${_i})
  list(APPEND _ng_urls "${_url}")
endforeach()
set(NGSPICE_VERSION "${_ng_release}-hefesto${_ng_level}")
# Every patch, not only the series: editing one patch re-runs the configure, which
# reverses the old series and applies the new one (cmake/ApplyPatchSeries.cmake).
file(GLOB _ng_patch_files CONFIGURE_DEPENDS "${_ng_dir}/patches/*.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ng_dir}/pin.json" "${_ng_dir}/patches/series"
             ${_ng_patch_files})

# --- Source: the release archive, patched ---------------------------------------
set(_ng_apply "${CMAKE_COMMAND}" "-DSERIES=${_ng_dir}/patches/series"
              -P "${_ng_dir}/cmake/ApplyPatchSeries.cmake")
FetchContent_Declare(hefesto_ngspice
  URL ${_ng_urls}
  URL_HASH SHA256=${_ng_sha256}
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  PATCH_COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=<SOURCE_DIR> "-DSERIES=${_ng_dir}/patches/series"
                -P "${_ng_dir}/cmake/ApplyPatchSeries.cmake"
  SOURCE_SUBDIR hefesto-sources-only)
FetchContent_MakeAvailable(hefesto_ngspice)
# Again after population, so a cached tree from before a patch was added is patched too.
execute_process(COMMAND ${CMAKE_COMMAND} "-DSOURCE_DIR=${hefesto_ngspice_SOURCE_DIR}"
                        "-DSERIES=${_ng_dir}/patches/series" -P "${_ng_dir}/cmake/ApplyPatchSeries.cmake"
                COMMAND_ERROR_IS_FATAL ANY)
set(NGSPICE_SRC "${hefesto_ngspice_SOURCE_DIR}")

# --- config.h, one per variant -------------------------------------------------
include("${_ng_dir}/cmake/NgspiceProbes.cmake")
set(NGSPICE_BINDIR "${CMAKE_INSTALL_FULL_BINDIR}")
set(NGSPICE_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}/ngspice")
set(NGSPICE_LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}/ngspice")
if(APPLE)
  set(NGSPICE_OS_COMPILED 7)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(NGSPICE_OS_COMPILED 6)
else()
  set(NGSPICE_OS_COMPILED 0)
endif()

set(_ng_variants shared)
if(HEFESTO_NGSPICE_EXECUTABLE)
  list(APPEND _ng_variants exe)
endif()
foreach(NGSPICE_VARIANT IN LISTS _ng_variants)
  set(SHARED_MODULE OFF)
  if(NGSPICE_VARIANT STREQUAL "shared")
    set(SHARED_MODULE ON)
  endif()
  configure_file("${_ng_dir}/cmake/config.h.in"
                 "${_ng_bin}/${NGSPICE_VARIANT}/include/ngspice/config.h" @ONLY)
endforeach()

# Flags of every ngspice object: C11 with GNU extensions (configure's -std=gnu11),
# no FMA contraction on any platform (Q-SIM-10), no OpenMP (Q-SIM-12), hidden
# symbols except the ngSpice_* API (sharedspice.h marks it), no warnings.
function(_ngspice_object_flags target)
  set_target_properties(${target} PROPERTIES
    C_STANDARD 11 C_EXTENSIONS ON CXX_STANDARD 11 CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
  target_compile_definitions(${target} PRIVATE HAVE_CONFIG_H)
  target_compile_options(${target} PRIVATE -w -ffp-contract=off)
endfunction()

# --- The XSPICE code models (patch 0002) -------------------------------------------
# cmpp, ngspice's code-model preprocessor, runs on the build host.
file(GLOB _ng_cmpp_sources "${NGSPICE_SRC}/src/xspice/cmpp/*.c")
add_executable(ngspice_cmpp ${_ng_cmpp_sources})
target_include_directories(ngspice_cmpp PRIVATE "${NGSPICE_SRC}/src/xspice/cmpp")
set_target_properties(ngspice_cmpp PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_compile_options(ngspice_cmpp PRIVATE -w)

include("${_ng_dir}/cmake/CodeModels.cmake")
ngspice_code_models("${_ng_pin}")  # sets NGSPICE_CM_SOURCES and NGSPICE_CM_FUNCTIONS

# The registries: code models (generated from the model list) and OSDI modules
# (none yet; task 5.2 adds the IHP modules).
include("${_ng_dir}/cmake/Registries.cmake")
ngspice_write_registries("${_ng_bin}")

# --- The compiled ngspice files (cmake/sources.cmake, generated) -----------------
set(_ng_groups_shared "")
set(_ng_groups_exe "")
function(ngspice_group variant name lang)
  if(NOT variant IN_LIST _ng_variants)
    return()
  endif()
  cmake_parse_arguments(PARSE_ARGV 3 G "" "" "DEFINES;INCLUDES;SOURCES")
  list(TRANSFORM G_SOURCES PREPEND "${NGSPICE_SRC}/")
  list(TRANSFORM G_INCLUDES REPLACE "^SRC/" "${NGSPICE_SRC}/")
  list(TRANSFORM G_INCLUDES REPLACE "^GEN/" "${_ng_bin}/${variant}/")
  set(target ngspice_${name})
  add_library(${target} OBJECT ${G_SOURCES})
  _ngspice_object_flags(${target})
  target_include_directories(${target} PRIVATE ${G_INCLUDES} "${_ng_dir}/src")
  target_compile_definitions(${target} PRIVATE ${G_DEFINES})
  set(_ng_groups_${variant} ${_ng_groups_${variant}} ${target} PARENT_SCOPE)
endfunction()
include("${_ng_dir}/cmake/sources.cmake")

# Code models, registries and the Hefesto glue, per variant.
foreach(_variant IN LISTS _ng_variants)
  add_library(ngspice_${_variant}_cm OBJECT ${NGSPICE_CM_SOURCES})
  _ngspice_object_flags(ngspice_${_variant}_cm)
  target_include_directories(ngspice_${_variant}_cm PRIVATE
    "${_ng_bin}/${_variant}/include" "${NGSPICE_SRC}/src/include" "${_ng_dir}/src")

  add_library(ngspice_${_variant}_glue OBJECT
    "${_ng_dir}/src/hefesto_static.c" "${_ng_bin}/hefesto_cm_registry.c" "${_ng_bin}/hefesto_osdi_builtins.c")
  set_target_properties(ngspice_${_variant}_glue PROPERTIES
    C_STANDARD 11 C_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
  target_include_directories(ngspice_${_variant}_glue PRIVATE
    "${_ng_bin}/${_variant}/include" "${NGSPICE_SRC}/src/include" "${_ng_dir}/src")
  target_compile_definitions(ngspice_${_variant}_glue PRIVATE HAVE_CONFIG_H)
  target_compile_options(ngspice_${_variant}_glue PRIVATE -Wall -Wextra -Werror -ffp-contract=off)
  list(APPEND _ng_groups_${_variant} ngspice_${_variant}_cm ngspice_${_variant}_glue)
endforeach()

function(_ngspice_link target)
  target_link_libraries(${target} PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
  if(NOT APPLE)
    target_link_libraries(${target} PRIVATE m)
  endif()
endfunction()

# --- libngspice-hefesto ------------------------------------------------------------
set(_ng_objects "")
foreach(_g IN LISTS _ng_groups_shared)
  list(APPEND _ng_objects $<TARGET_OBJECTS:${_g}>)
endforeach()
add_library(ngspice-hefesto SHARED ${_ng_objects})
set_target_properties(ngspice-hefesto PROPERTIES
  LINKER_LANGUAGE CXX
  OUTPUT_NAME ngspice-hefesto
  VERSION "${_ng_release}.${_ng_level}"
  SOVERSION "${_ng_release}"
  BUILD_RPATH_USE_ORIGIN ON)
target_include_directories(ngspice-hefesto INTERFACE "$<BUILD_INTERFACE:${NGSPICE_SRC}/src/include>")
_ngspice_link(ngspice-hefesto)
add_library(hefesto::ngspice ALIAS ngspice-hefesto)
install(TARGETS ngspice-hefesto LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES "${NGSPICE_SRC}/src/include/ngspice/sharedspice.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ngspice)

# --- ngspice executable (golden capture image) ---------------------------------------
if(HEFESTO_NGSPICE_EXECUTABLE)
  set(_ng_exe_objects "")
  foreach(_g IN LISTS _ng_groups_exe)
    list(APPEND _ng_exe_objects $<TARGET_OBJECTS:${_g}>)
  endforeach()
  add_executable(ngspice ${_ng_exe_objects})
  set_target_properties(ngspice PROPERTIES LINKER_LANGUAGE CXX)
  _ngspice_link(ngspice)
  # spinit with the code-model lines commented out (they are registered
  # statically; patch 0002) and the stock OSDI section, which spinit disables.
  file(READ "${NGSPICE_SRC}/src/spinit.in" _spinit)
  string(REPLACE "@XSPICEINIT@" "*" _spinit "${_spinit}")
  string(REPLACE "@pkglibdir@" "${NGSPICE_LIBDIR}" _spinit "${_spinit}")
  file(WRITE "${_ng_bin}/spinit" "${_spinit}")
  install(TARGETS ngspice RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES "${_ng_bin}/spinit" DESTINATION ${CMAKE_INSTALL_DATADIR}/ngspice/scripts)
endif()

# --- Manifest and source archive ---------------------------------------------------
set(_ng_tool "${_ng_dir}/tools/ngspice_manifest.py")
set(_ng_manifest "${_ng_bin}/ngspice-manifest.json")
set(_ng_tool_args --pin "${_ng_dir}/pin.json" --series "${_ng_dir}/patches/series"
                  --sources-cmake "${_ng_dir}/cmake/sources.cmake" --src "${NGSPICE_SRC}"
                  --code-models "${_ng_bin}/code_models.txt")
add_custom_command(OUTPUT "${_ng_manifest}"
  COMMAND Python3::Interpreter -I "${_ng_tool}" manifest ${_ng_tool_args} --out "${_ng_manifest}"
  DEPENDS "${_ng_tool}" "${_ng_dir}/pin.json" "${_ng_dir}/patches/series" "${_ng_dir}/cmake/sources.cmake"
          "${_ng_bin}/code_models.txt"
  COMMENT "ngspice: writing ngspice-manifest.json" VERBATIM)
add_custom_target(ngspice_hefesto_manifest ALL DEPENDS "${_ng_manifest}")
install(FILES "${_ng_manifest}" DESTINATION ${CMAKE_INSTALL_DATADIR}/ngspice)

set(_ng_archive "${_ng_bin}/ngspice-hefesto-${NGSPICE_VERSION}-src.tar.gz")
add_custom_target(ngspice_hefesto_source_archive
  COMMAND Python3::Interpreter -I "${_ng_tool}" archive ${_ng_tool_args} --wrapper "${_ng_dir}"
          --out "${_ng_archive}"
  BYPRODUCTS "${_ng_archive}"
  COMMENT "ngspice: writing the source archive" VERBATIM)

message(STATUS "ngspice: libngspice-hefesto ${NGSPICE_VERSION} (variants: ${_ng_variants})")
