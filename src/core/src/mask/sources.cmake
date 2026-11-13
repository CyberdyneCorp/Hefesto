# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the mask area (mask generation: planes, output and input styles, operators, generation),
# included by src/core/CMakeLists.txt. The area builds with -ffp-contract=off
# (set PUBLIC on hefesto_core; openspec/changes/m5-mask-gds-cif/design.md D3).
# Owner: INTEGRATION (openspec/changes/m5-mask-gds-cif/design.md D1/D2): this
# file lists the shared files and includes one sources.cmake per
# subdirectory, each owned by its workstream:
#   plane/    OPS
#   style/    STYLES
#   input/    INPUT
#   ops/      OPS
#   gen/      GEN
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp)

foreach(_mask_sub IN ITEMS plane style input ops gen)
  include("${CMAKE_CURRENT_LIST_DIR}/${_mask_sub}/sources.cmake")
endforeach()
