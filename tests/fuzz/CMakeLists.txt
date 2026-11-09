# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Coverage-guided fuzz targets (libFuzzer), one per reader of external input
# (capability hefesto-testing-strategy, requirement "Parser fuzzing").
#
# Two modes:
#   HEFESTO_BUILD_FUZZ=ON   libFuzzer executables (preset linux-fuzz: Clang,
#                           HEFESTO_SANITIZERS=address,undefined,fuzzer-no-link).
#                           CI runs every fuzz_* target for a bounded time.
#   otherwise (tests on)    the same targets linked with standalone_main.cpp,
#                           which replays the seed corpus once; each is the
#                           CTest fuzz-seeds.<target> (label "fuzz-seeds"), so
#                           every target compiles and its seeds run on every leg.
#
# Seed corpora live in corpus/<target>/; every target must have one.

set(HEFESTO_FUZZ_TARGETS
  fuzz_command_script   # M0 command-script reader (m0 design D6)
  fuzz_tech_file        # technology files (tech-file-format)
  fuzz_mag_reader       # .mag cell files (cell-file-format)
  fuzz_dstyle           # display styles (display-style-files)
  fuzz_cmap             # color maps (display-style-files)
  fuzz_glyphs           # cursor glyphs (display-style-files)
  fuzz_font             # outline fonts (font-labels)
  fuzz_project_file     # hefesto.project (hefesto-settings)
  fuzz_pdk_manifest     # PDK catalog and manifests (hefesto-pdk-manager)
  fuzz_tar_reader       # PDK tar unpacking (hefesto-pdk-manager)
)

if(HEFESTO_BUILD_FUZZ)
  include(CheckCXXSourceCompiles)
  set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer")
  set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=fuzzer")
  check_cxx_source_compiles([=[
    #include <cstddef>
    #include <cstdint>
    extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t*, std::size_t) { return 0; }
  ]=] HEFESTO_HAVE_LIBFUZZER)
  unset(CMAKE_REQUIRED_FLAGS)
  unset(CMAKE_REQUIRED_LINK_OPTIONS)
  if(NOT HEFESTO_HAVE_LIBFUZZER)
    message(WARNING "Hefesto: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} has no libFuzzer; "
                    "fuzz targets are skipped (use Clang on Linux, preset linux-fuzz)")
    return()
  endif()
endif()

# hefesto_add_fuzz_target(<name>)
#   Builds <name>.cpp as a libFuzzer executable (HEFESTO_BUILD_FUZZ) or as a
#   seed-replay executable plus its CTest.
function(hefesto_add_fuzz_target name)
  if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name}")
    message(FATAL_ERROR "fuzz target ${name} has no seed corpus in tests/fuzz/corpus/${name}/")
  endif()
  if(HEFESTO_BUILD_FUZZ)
    add_executable(${name} ${name}.cpp)
    target_compile_options(${name} PRIVATE -fsanitize=fuzzer)
    target_link_options(${name} PRIVATE -fsanitize=fuzzer)
  else()
    add_executable(${name} ${name}.cpp standalone_main.cpp)
    add_test(NAME fuzz-seeds.${name}
             COMMAND ${name} "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name}")
    set_tests_properties(fuzz-seeds.${name} PROPERTIES LABELS "unit;fuzz-seeds" TIMEOUT 120)
  endif()
  target_link_libraries(${name} PRIVATE hefesto::core)
  target_compile_definitions(${name} PRIVATE HEFESTO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  hefesto_set_warnings(${name})
endfunction()

foreach(target IN LISTS HEFESTO_FUZZ_TARGETS)
  hefesto_add_fuzz_target(${target})
endforeach()
