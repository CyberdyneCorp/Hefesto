# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of script/pdk (startup sequence, consent and trust, the
# <tech>.magicrc step, helper origins, widget interception, curated packs),
# included by src/core/src/script/sources.cmake.
# Owner: PDKINT (openspec/changes/m4-scripting/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/consent.cpp
  ${CMAKE_CURRENT_LIST_DIR}/curated.cpp
  ${CMAKE_CURRENT_LIST_DIR}/helper_origins.cpp
  ${CMAKE_CURRENT_LIST_DIR}/inventory.cpp
  ${CMAKE_CURRENT_LIST_DIR}/startup.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tech_rc.cpp
  ${CMAKE_CURRENT_LIST_DIR}/trust.cpp
  ${CMAKE_CURRENT_LIST_DIR}/widgets.cpp)
