# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# The XSPICE code models compiled into libngspice-hefesto (Hefesto patch 0002;
# feature table "XSPICE code models"). For each library of pin.json
# "code_models", every model of its modpath.lst except those of
# "code_models_excluded" is preprocessed by cmpp (ifspec.ifs -> ifspec.c,
# cfunc.mod -> cfunc.c), exactly as ngspice's icm/GNUmakefile does for a .cm
# file. dlmain.c is added once, compiled with HEFESTO_STATIC_CODEMODELS.
#
# ngspice_code_models(<pin json>) sets in the caller:
#   NGSPICE_CM_SOURCES    generated sources plus dlmain.c
#   NGSPICE_CM_FUNCTIONS  the models' C function names (cm_<name>), in
#                         registration order
# and writes <binary dir>/code_models.txt ("<library>/<model> <C function>"
# per line) for the manifest.

function(_ngspice_cm_function ifs out_var)
  file(READ "${ifs}" _text)
  if(NOT _text MATCHES "C_Function_Name:[ \t]*([A-Za-z0-9_]+)")
    message(FATAL_ERROR "ngspice: no C_Function_Name in ${ifs}")
  endif()
  set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(_ngspice_cm_model lib model)
  set(idir "${NGSPICE_SRC}/src/xspice/icm/${lib}/${model}")
  set(odir "${CMAKE_CURRENT_BINARY_DIR}/cm/${lib}/${model}")
  file(MAKE_DIRECTORY "${odir}")
  set(env ${CMAKE_COMMAND} -E env "CMPP_IDIR=${idir}" "CMPP_ODIR=${odir}")
  add_custom_command(OUTPUT "${odir}/ifspec.c"
    COMMAND ${env} $<TARGET_FILE:ngspice_cmpp> -ifs
    DEPENDS "${idir}/ifspec.ifs" ngspice_cmpp VERBATIM)
  add_custom_command(OUTPUT "${odir}/cfunc.c"
    COMMAND ${env} $<TARGET_FILE:ngspice_cmpp> -mod
    DEPENDS "${idir}/cfunc.mod" "${idir}/ifspec.ifs" ngspice_cmpp VERBATIM)
  set_source_files_properties("${odir}/ifspec.c" "${odir}/cfunc.c" PROPERTIES
    GENERATED TRUE
    INCLUDE_DIRECTORIES "${idir};${NGSPICE_SRC}/src/xspice/icm/${lib}")
  _ngspice_cm_function("${idir}/ifspec.ifs" cfunc)
  set(_cm_sources "${odir}/ifspec.c" "${odir}/cfunc.c" PARENT_SCOPE)
  set(_cm_function "${cfunc}" PARENT_SCOPE)
endfunction()

function(ngspice_code_models pin)
  string(JSON excluded GET "${pin}" code_models_excluded)
  string(JSON count LENGTH "${pin}" code_models)
  math(EXPR last "${count} - 1")
  set(sources "${NGSPICE_SRC}/src/xspice/icm/dlmain.c")
  set(functions "")
  set(listing "")
  foreach(i RANGE ${last})
    string(JSON lib GET "${pin}" code_models ${i})
    set(udn "${NGSPICE_SRC}/src/xspice/icm/${lib}/udnpath.lst")
    if(EXISTS "${udn}")
      file(STRINGS "${udn}" udns REGEX "[^ \t]")
      if(udns)
        message(FATAL_ERROR "ngspice: code-model library ${lib} has user-defined nodes; "
                            "cmake/Registries.cmake does not register them yet")
      endif()
    endif()
    file(STRINGS "${NGSPICE_SRC}/src/xspice/icm/${lib}/modpath.lst" models REGEX "[^ \t]")
    foreach(model IN LISTS models)
      string(STRIP "${model}" model)
      # A model listed in code_models_excluded is skipped (the lookup succeeds).
      string(JSON reason ERROR_VARIABLE lookup_error GET "${excluded}" "${lib}/${model}")
      if(lookup_error STREQUAL "NOTFOUND")
        continue()
      endif()
      _ngspice_cm_model("${lib}" "${model}")
      list(APPEND sources ${_cm_sources})
      list(APPEND functions "${_cm_function}")
      string(APPEND listing "${lib}/${model} ${_cm_function}\n")
    endforeach()
  endforeach()
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/code_models.txt.new" "${listing}")
  file(COPY_FILE "${CMAKE_CURRENT_BINARY_DIR}/code_models.txt.new" "${CMAKE_CURRENT_BINARY_DIR}/code_models.txt"
       ONLY_IF_DIFFERENT)
  set(NGSPICE_CM_SOURCES "${sources}" PARENT_SCOPE)
  set(NGSPICE_CM_FUNCTIONS "${functions}" PARENT_SCOPE)
endfunction()
