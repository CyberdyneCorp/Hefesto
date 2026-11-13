# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the mask area (openspec/changes/m5-mask-gds-cif/design.md D1/D2).
# Owner: INTEGRATION (this file and the shared tests here). Each
# subdirectory is owned by its workstream and builds its own executable once
# it has a *_test.cpp:
#   plane/    OPS     hefesto_mask_plane_tests   (data: tests/data/mask/plane)
#   style/    STYLES  hefesto_mask_style_tests   (data: tests/data/mask/style)
#   input/    INPUT   hefesto_mask_input_tests   (data: tests/data/mask/input)
#   ops/      OPS     hefesto_mask_ops_tests   (data: tests/data/mask/ops)
#   gen/      GEN     hefesto_mask_gen_tests   (data: tests/data/mask/gen)

hefesto_add_area_tests(mask)

foreach(_mask_sub IN ITEMS plane style input ops gen)
  add_subdirectory(${_mask_sub})
endforeach()
