# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the view3d area (the 3D view: layer stack, mesh, camera, the
# wind3d client, picking, cross-sections, export and overlays), included by
# src/core/CMakeLists.txt. Everything here is backend-free: no GPU, UI or
# Raven header (scripts/check_core_includes.py; design D2, D11).
# Owner: SCAFFOLD (openspec/changes/m10-3d-view/design.md D2/D3): this file
# lists the shared files and includes one sources.cmake per subdirectory,
# each owned by its workstream:
#   stack/    TECH
#   mesh/     MESH
#   camera/   CAMERA
#   client/   WIND3D
#   pick/     INSPECT
#   section/  INSPECT (P2)
#   export/   EXPORT (P2)
#   overlay/  OVERLAY (P3)
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp)

foreach(_view3d_sub IN ITEMS stack mesh camera client pick section export overlay)
  include("${CMAKE_CURRENT_LIST_DIR}/${_view3d_sub}/sources.cmake")
endforeach()
