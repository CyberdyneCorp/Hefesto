# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Coverage-guided fuzz targets (libFuzzer), one per reader of external input
# (capability hefesto-testing-strategy, requirement "Parser fuzzing").
# Use the linux-fuzz preset (Clang, HEFESTO_SANITIZERS=address,undefined,fuzzer-no-link).
# Owner: workstream "Core & CLI" (design.md D2).

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

# hefesto_add_fuzz_target(<name> SOURCES <files...> [LIBS <targets...>])
#   Creates a libFuzzer executable. CI runs each target for a bounded time.
function(hefesto_add_fuzz_target name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;LIBS")
  add_executable(${name} ${ARG_SOURCES})
  target_link_libraries(${name} PRIVATE hefesto::core ${ARG_LIBS})
  target_compile_options(${name} PRIVATE -fsanitize=fuzzer)
  target_link_options(${name} PRIVATE -fsanitize=fuzzer)
  hefesto_set_warnings(${name})
endfunction()

# Seed corpora live in corpus/<target>/ (e.g. corpus/fuzz_command_script/).
# CI runs, per target, from a copy of its seeds:
#   <target> <copy of corpus/<target>> -max_total_time=60

# The M0 command-script reader (design D6).
hefesto_add_fuzz_target(fuzz_command_script SOURCES fuzz_command_script.cpp)
