# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# hefesto_set_warnings(<target>)
#   Applies Hefesto's warning set to a first-party target. Third-party targets
#   are never given these flags. HEFESTO_WARNINGS_AS_ERRORS adds -Werror (/WX).

function(hefesto_set_warnings target)
  if(MSVC)
    set(_warnings /W4 /permissive-)
    if(HEFESTO_WARNINGS_AS_ERRORS)
      list(APPEND _warnings /WX)
    endif()
  else()
    set(_warnings
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wconversion
      -Wsign-conversion
      -Wold-style-cast
      -Wnon-virtual-dtor
      -Woverloaded-virtual
      -Wcast-align
      -Wnull-dereference
      -Wdouble-promotion
      -Wformat=2
      -Wimplicit-fallthrough)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      list(APPEND _warnings
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast)
    endif()
    if(HEFESTO_WARNINGS_AS_ERRORS)
      list(APPEND _warnings -Werror)
    endif()
  endif()

  get_target_property(_type ${target} TYPE)
  if(_type STREQUAL "INTERFACE_LIBRARY")
    target_compile_options(${target} INTERFACE $<$<COMPILE_LANGUAGE:CXX>:${_warnings}>)
  else()
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${_warnings}>)
  endif()
endfunction()
