# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# The embedded Tcl 8.6.16 (decision X-3; openspec/changes/m4-scripting/
# design.md D3 and D5). Provides
#
#   hefesto::tcl   libtcl, static, threads enabled, deprecated commands kept,
#                  no Tk, no dynamic loading, plus the table of Tcl's own
#                  library scripts (library/: init.tcl, clock.tcl, msgcat,
#                  http, opt, platform, tcltest, msgs/, tzdata/, encoding/)
#                  compiled into the binary, so no Tcl file is ever read from
#                  the host. Its public include directory holds tcl.h; only
#                  src/core/src/script/tcl/ includes it.
#
# The source tarball is fetched by URL with a pinned SHA-256 (offline:
# reconfigure with -DFETCHCONTENT_FULLY_DISCONNECTED=ON once populated).
# Tcl's sources are compiled by CMake with the project toolchain instead of
# Tcl's unix/configure: one recipe for every generator (Ninja, Xcode) and
# preset (debug/release, asan/tsan, fuzz, bench, ci, the ipados cross build),
# with the sanitizer and fuzz instrumentation of the preset. The platform
# facts configure would probe are written down in cmake/tcl/tclConfig.h.
#
# Owner: TCL (openspec/changes/m4-scripting/design.md D2); written by the
# M4 scaffold (tasks.md 2.3). The notice is in THIRD_PARTY_NOTICES.md
# ("Tcl"); scripts/check_third_party_notices.py checks the pin.

include(FetchContent)

# --- Tcl 8.6.16 (Tcl/Tk license, BSD-style) --------------------------------
# Patches carried on top of the release (third_party/patches/tcl/, listed in
# THIRD_PARTY_NOTICES.md):
#   0001-objsetvar2-hold-value-across-lookup.patch
#     Heap use-after-free in Tcl_ObjSetVar2 when `catch body var` cannot set
#     `var` (found by fuzz_tcl_sandbox). Upstream 8.6.18 and 9.x still have
#     the bug; drop the patch when the pinned release contains a fix.
# ApplyPatches.cmake skips a patch that is already applied, and runs again
# after population so a deps cache filled before a patch was added is
# patched too.
cmake_path(SET _tcl_patches NORMALIZE
           "${CMAKE_CURRENT_LIST_DIR}/../third_party/patches/tcl/0001-objsetvar2-hold-value-across-lookup.patch")
set(_tcl_apply_patches "${CMAKE_COMMAND}" "-DPATCHES=${_tcl_patches}")
set(_tcl_apply_script -P "${CMAKE_CURRENT_LIST_DIR}/ApplyPatches.cmake")
FetchContent_Declare(hefesto_tcl
  URL https://downloads.sourceforge.net/project/tcl/Tcl/8.6.16/tcl8.6.16-src.tar.gz
      https://prdownloads.sourceforge.net/tcl/tcl8.6.16-src.tar.gz
  URL_HASH SHA256=91cb8fa61771c63c262efb553059b7c7ad6757afa5857af6265e4b0bdc2a14a5
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  PATCH_COMMAND ${_tcl_apply_patches} -DSOURCE_DIR=<SOURCE_DIR> ${_tcl_apply_script}
  SOURCE_SUBDIR hefesto-sources-only)
FetchContent_MakeAvailable(hefesto_tcl)
execute_process(COMMAND ${_tcl_apply_patches} "-DSOURCE_DIR=${hefesto_tcl_SOURCE_DIR}" ${_tcl_apply_script}
                COMMAND_ERROR_IS_FATAL ANY)

set(_tcl_src "${hefesto_tcl_SOURCE_DIR}")
set(_tcl_gen "${CMAKE_BINARY_DIR}/generated/hefesto_tcl")

# --- libtcl -------------------------------------------------------------------
# The object lists of unix/Makefile.in (GENERIC_OBJS, OO_OBJS, TOMMATH_OBJS,
# UNIX_OBJS, NOTIFY_OBJS, MAC_OSX_OBJS), with generic/tclLoadNone.c as the
# loader (no dlopen: `load` of a binary extension always fails, design D5).
set(_tcl_generic
  regcomp regexec regfree regerror tclAlloc tclAssembly tclAsync tclBasic tclBinary tclCkalloc
  tclClock tclCmdAH tclCmdIL tclCmdMZ tclCompCmds tclCompCmdsGR tclCompCmdsSZ tclCompExpr
  tclCompile tclConfig tclDate tclDictObj tclDisassemble tclEncoding tclEnsemble tclEnv tclEvent
  tclExecute tclFCmd tclFileName tclGet tclHash tclHistory tclIndexObj tclInterp tclIO tclIOCmd
  tclIORChan tclIORTrans tclIOGT tclIOSock tclIOUtil tclLink tclListObj tclLiteral tclLoad
  tclMain tclNamesp tclNotify tclObj tclOptimize tclPanic tclParse tclPathObj tclPipe tclPkg
  tclPkgConfig tclPosixStr tclPreserve tclProc tclRegexp tclResolve tclResult tclScan
  tclStringObj tclStrToD tclThread tclThreadAlloc tclThreadJoin tclThreadStorage tclStubInit
  tclTimer tclTrace tclUtf tclUtil tclVar tclZlib tclTomMathInterface
  tclOO tclOOBasic tclOOCall tclOODefineCmds tclOOInfo tclOOMethod tclOOStubInit
  tclLoadNone)
set(_tcl_tommath
  bn_s_mp_reverse bn_s_mp_mul_digs_fast bn_s_mp_sqr_fast bn_mp_add bn_mp_and bn_mp_add_d
  bn_mp_clamp bn_mp_clear bn_mp_clear_multi bn_mp_cmp bn_mp_cmp_d bn_mp_cmp_mag bn_mp_cnt_lsb
  bn_mp_copy bn_mp_count_bits bn_mp_div bn_mp_div_d bn_mp_div_2 bn_mp_div_2d bn_s_mp_div_3
  bn_mp_exch bn_mp_expt_n bn_mp_grow bn_mp_init bn_mp_init_copy bn_mp_init_multi
  bn_mp_init_set bn_mp_init_size bn_s_mp_karatsuba_mul bn_s_mp_karatsuba_sqr
  bn_s_mp_balance_mul bn_mp_lshd bn_mp_mod bn_mp_mod_2d bn_mp_mul bn_mp_mul_2 bn_mp_mul_2d
  bn_mp_mul_d bn_mp_neg bn_mp_or bn_mp_pack bn_mp_pack_count bn_mp_radix_size
  bn_mp_radix_smap bn_mp_read_radix bn_mp_rshd bn_mp_shrink bn_mp_set bn_mp_sqr bn_mp_sqrt
  bn_mp_sub bn_mp_sub_d bn_mp_signed_rsh bn_mp_to_ubin bn_mp_unpack bn_s_mp_toom_mul
  bn_s_mp_toom_sqr bn_mp_to_radix bn_mp_ubin_size bn_mp_xor bn_mp_zero bn_s_mp_add
  bn_s_mp_mul_digs bn_s_mp_sqr bn_s_mp_sub)
set(_tcl_unix
  tclUnixChan tclUnixEvent tclUnixFCmd tclUnixFile tclUnixPipe tclUnixSock tclUnixTime
  tclUnixInit tclUnixThrd tclUnixCompat tclUnixNotfy)
set(_tcl_macosx tclMacOSXBundle tclMacOSXFCmd tclMacOSXNotify)

list(TRANSFORM _tcl_generic PREPEND "${_tcl_src}/generic/")
list(TRANSFORM _tcl_tommath PREPEND "${_tcl_src}/libtommath/")
list(TRANSFORM _tcl_unix PREPEND "${_tcl_src}/unix/")
list(TRANSFORM _tcl_macosx PREPEND "${_tcl_src}/macosx/")
set(_tcl_sources ${_tcl_generic} ${_tcl_tommath} ${_tcl_unix})
if(APPLE)
  list(APPEND _tcl_sources ${_tcl_macosx})
endif()
list(TRANSFORM _tcl_sources APPEND ".c")

add_library(hefesto_tcl STATIC ${_tcl_sources})
set_target_properties(hefesto_tcl PROPERTIES
  C_STANDARD 99
  C_EXTENSIONS ON
  POSITION_INDEPENDENT_CODE ON)
target_include_directories(hefesto_tcl PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/tcl" "${_tcl_gen}" "${_tcl_src}/unix" "${_tcl_src}/generic"
  "${_tcl_src}/libtommath")  # unix/Makefile.in order: generic/tommath.h first
# configure's DEFS (-DHAVE_TCL_CONFIG_H -imacros tclConfig.h) and CC_SWITCHES.
target_compile_definitions(hefesto_tcl PRIVATE
  HAVE_TCL_CONFIG_H BUILD_tcl MP_FIXED_CUTOFFS MP_NO_STDINT)
target_compile_options(hefesto_tcl PRIVATE
  "SHELL:-imacros ${CMAKE_CURRENT_LIST_DIR}/tcl/tclConfig.h"
  # Third-party code: no warnings. Same floating-point rule as the core
  # (expr results and %g formatting must not depend on FMA contraction).
  -w -ffp-contract=off)
if(HEFESTO_SANITIZERS)
  # Sanitizer builds: plain malloc for Tcl objects (PURIFY: the per-thread
  # allocator would hide heap errors), and AddressSanitizer/ThreadSanitizer
  # but not UBSan on Tcl itself, whose findings are not Hefesto's to fix.
  target_compile_definitions(hefesto_tcl PRIVATE PURIFY)
  target_compile_options(hefesto_tcl PRIVATE -fno-sanitize=undefined)
endif()
target_link_libraries(hefesto_tcl PRIVATE hefesto::zlib)
find_package(Threads REQUIRED)
target_link_libraries(hefesto_tcl PUBLIC Threads::Threads)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(hefesto_tcl PUBLIC m)
endif()

# Empty compiled-in locations: Tcl never derives a host path for its library,
# packages or configuration (tclUnixInit.c, tclPkgConfig.c).
set_source_files_properties("${_tcl_src}/unix/tclUnixInit.c" PROPERTIES
  COMPILE_DEFINITIONS [[TCL_LIBRARY="";TCL_PACKAGE_PATH=""]])
set_source_files_properties("${_tcl_src}/generic/tclPkgConfig.c" PROPERTIES
  COMPILE_DEFINITIONS [[CFG_INSTALL_LIBDIR="";CFG_INSTALL_BINDIR="";CFG_INSTALL_SCRDIR="";CFG_INSTALL_INCDIR="";CFG_INSTALL_DOCDIR="";CFG_RUNTIME_LIBDIR="";CFG_RUNTIME_BINDIR="";CFG_RUNTIME_SCRDIR="";CFG_RUNTIME_INCDIR="";CFG_RUNTIME_DOCDIR="";CFG_RUNTIME_DLLFILE=""]])

# tclUuid.h (unix/Makefile.in rule): the source's manifest identifier.
file(READ "${_tcl_src}/manifest.uuid" _tcl_uuid)
string(STRIP "${_tcl_uuid}" _tcl_uuid)
file(CONFIGURE OUTPUT "${_tcl_gen}/tclUuid.h"
  CONTENT "#define TCL_VERSION_UUID \\\n${_tcl_uuid}\n" @ONLY)

# --- Embedded library scripts --------------------------------------------------
# hefesto_tcl_embed_library(<library dir> <output .cpp>)
#   Writes the table behind hefesto/script/tcl/embedded_library.hpp: one raw
#   string literal per file of Tcl's library/ (the Windows-only dde/ and reg/
#   packages excluded), keyed by its path relative to library/. The file is
#   rewritten only when its content changes.
function(hefesto_tcl_embed_library library_dir out_cpp)
  file(GLOB_RECURSE _files RELATIVE "${library_dir}" "${library_dir}/*")
  list(FILTER _files EXCLUDE REGEX "^(dde|reg)/")
  list(SORT _files)
  set(_arrays "")
  set(_rows "")
  set(_index 0)
  foreach(_file IN LISTS _files)
    file(READ "${library_dir}/${_file}" _text)
    string(FIND "${_text}" ")HEFESTOTCL\"" _clash)
    if(NOT _clash EQUAL -1)
      message(FATAL_ERROR "Tcl library file ${_file} contains the raw-string delimiter")
    endif()
    string(APPEND _arrays "constexpr char kFile${_index}[] = R\"HEFESTOTCL(${_text})HEFESTOTCL\";\n")
    string(APPEND _rows "    EmbeddedFile{\"${_file}\", std::string_view{kFile${_index}, sizeof(kFile${_index}) - 1}},\n")
    math(EXPR _index "${_index} + 1")
  endforeach()
  set(_content "// Generated by cmake/Tcl.cmake from Tcl ${hefesto_tcl_version} library/. Do not edit.\n\n")
  string(APPEND _content "#include \"hefesto/script/tcl/embedded_library.hpp\"\n\n")
  string(APPEND _content "namespace hefesto::script {\n\nnamespace {\n\n${_arrays}\n")
  string(APPEND _content "constexpr EmbeddedFile kFiles[] = {\n${_rows}};\n\n}  // namespace\n\n")
  string(APPEND _content "std::span<const EmbeddedFile> embedded_library() noexcept {\n  return kFiles;\n}\n\n")
  string(APPEND _content "}  // namespace hefesto::script\n")
  file(WRITE "${out_cpp}.new" "${_content}")
  file(COPY_FILE "${out_cpp}.new" "${out_cpp}" ONLY_IF_DIFFERENT)
  file(REMOVE "${out_cpp}.new")
endfunction()

set(hefesto_tcl_version 8.6.16)
hefesto_tcl_embed_library("${_tcl_src}/library" "${_tcl_gen}/tcl_library.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_tcl_src}/manifest.uuid")

add_library(hefesto_tcl_library STATIC "${_tcl_gen}/tcl_library.cpp")
target_include_directories(hefesto_tcl_library PRIVATE "${PROJECT_SOURCE_DIR}/src/core/include")
target_compile_features(hefesto_tcl_library PRIVATE cxx_std_20)
# Generated data: megabytes of raw string literals, no warnings.
target_compile_options(hefesto_tcl_library PRIVATE -w)
set_target_properties(hefesto_tcl_library PROPERTIES POSITION_INDEPENDENT_CODE ON)

# hefesto::tcl: what the core links. tcl.h comes from a system include
# directory (Tcl's macros are not held to Hefesto's warning set); it is a
# separate interface target because CMake folds a directory that is both a
# private (-I, libtcl's own build order) and a system (-isystem) include of
# one target into the latter.
add_library(hefesto_tcl_interface INTERFACE)
add_library(hefesto::tcl ALIAS hefesto_tcl_interface)
target_include_directories(hefesto_tcl_interface SYSTEM INTERFACE "$<BUILD_INTERFACE:${_tcl_src}/generic>")
target_link_libraries(hefesto_tcl_interface INTERFACE hefesto_tcl hefesto_tcl_library)
