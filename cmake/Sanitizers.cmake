# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Applies HEFESTO_SANITIZERS (a comma-separated -fsanitize= list, e.g.
# "address,undefined" or "address,undefined,fuzzer-no-link") to every target
# defined after this point, dependencies included, so that instrumentation is
# consistent across the whole program.

if(HEFESTO_SANITIZERS)
  if(MSVC)
    message(FATAL_ERROR "HEFESTO_SANITIZERS is supported with GCC and Clang only")
  endif()
  set(_hefesto_san_flags -fsanitize=${HEFESTO_SANITIZERS} -fno-omit-frame-pointer)
  if(HEFESTO_SANITIZERS MATCHES "undefined")
    list(APPEND _hefesto_san_flags -fno-sanitize-recover=undefined)
  endif()
  add_compile_options(${_hefesto_san_flags})
  # fuzzer-no-link is a compile-time-only instrumentation flag.
  string(REPLACE "fuzzer-no-link" "" _hefesto_san_link "${HEFESTO_SANITIZERS}")
  string(REGEX REPLACE ",+" "," _hefesto_san_link "${_hefesto_san_link}")
  string(REGEX REPLACE "^,|,$" "" _hefesto_san_link "${_hefesto_san_link}")
  if(_hefesto_san_link)
    add_link_options(-fsanitize=${_hefesto_san_link})
  endif()
  message(STATUS "Hefesto: sanitizers enabled: ${HEFESTO_SANITIZERS}")
endif()
