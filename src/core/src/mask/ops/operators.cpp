// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (openspec/changes/m5-mask-gds-cif/design.md D3): every
// kernel fails with CORE-NOTIMPL-005 until OPS ports it (tasks.md section 4;
// OPS replaces this file with one source file per operator group).
// Owner: OPS.

#include "hefesto/mask/ops/operators.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::mask {

#define HEFESTO_MASK_OP_STUB(name)                                                        \
  diag::Status name(const OpArgs& /*args*/, OperatorContext& /*ctx*/, MaskPlane& /*out*/) { \
    return cmd::not_implemented_status("mask::" #name);                                  \
  }

HEFESTO_MASK_OP_STUB(op_or)
HEFESTO_MASK_OP_STUB(op_and)
HEFESTO_MASK_OP_STUB(op_and_not)
HEFESTO_MASK_OP_STUB(op_grow)
HEFESTO_MASK_OP_STUB(op_shrink)
HEFESTO_MASK_OP_STUB(op_grow_grid)
HEFESTO_MASK_OP_STUB(op_grow_min)
HEFESTO_MASK_OP_STUB(op_squares)
HEFESTO_MASK_OP_STUB(op_squares_grid)
HEFESTO_MASK_OP_STUB(op_slots)
HEFESTO_MASK_OP_STUB(op_bridge)
HEFESTO_MASK_OP_STUB(op_bridge_lim)
HEFESTO_MASK_OP_STUB(op_bloat_or)
HEFESTO_MASK_OP_STUB(op_bloat_min)
HEFESTO_MASK_OP_STUB(op_bloat_max)
HEFESTO_MASK_OP_STUB(op_bloat_all)
HEFESTO_MASK_OP_STUB(op_close)
HEFESTO_MASK_OP_STUB(op_maxrect)
HEFESTO_MASK_OP_STUB(op_orthogonal)
HEFESTO_MASK_OP_STUB(op_not_square)
HEFESTO_MASK_OP_STUB(op_interacting)
HEFESTO_MASK_OP_STUB(op_net)
HEFESTO_MASK_OP_STUB(op_tagged)
HEFESTO_MASK_OP_STUB(op_bbox)
HEFESTO_MASK_OP_STUB(op_boundary)
HEFESTO_MASK_OP_STUB(op_mask_hints)
HEFESTO_MASK_OP_STUB(op_copyup)

#undef HEFESTO_MASK_OP_STUB

}  // namespace hefesto::mask
