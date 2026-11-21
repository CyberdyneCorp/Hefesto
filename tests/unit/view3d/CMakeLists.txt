# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the view3d area (openspec/changes/m10-3d-view/design.md D2/D3).
# Owner: SCAFFOLD (this file and the shared tests here). Each subdirectory is
# owned by its workstream and builds its own executable once it has a
# *_test.cpp:
#   stack/    TECH          hefesto_view3d_stack_tests   (data: tests/data/view3d/stack)
#   mesh/     MESH          hefesto_view3d_mesh_tests   (data: tests/data/view3d/mesh)
#   camera/   CAMERA        hefesto_view3d_camera_tests   (data: tests/data/view3d/camera)
#   client/   WIND3D        hefesto_view3d_client_tests   (data: tests/data/view3d/client)
#   pick/     INSPECT       hefesto_view3d_pick_tests   (data: tests/data/view3d/pick)
#   section/  INSPECT (P2)  hefesto_view3d_section_tests   (data: tests/data/view3d/section)
#   export/   EXPORT (P2)   hefesto_view3d_export_tests   (data: tests/data/view3d/export)
#   overlay/  OVERLAY (P3)  hefesto_view3d_overlay_tests   (data: tests/data/view3d/overlay)

hefesto_add_area_tests(view3d)

foreach(_view3d_sub IN ITEMS stack mesh camera client pick section export overlay)
  add_subdirectory(${_view3d_sub})
endforeach()
