// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Operator group 1, boolean part (capability mask-output-operators "or,
// and, and-not"; design D6): CIFOP_OR, CIFOP_AND and CIFOP_ANDNOT.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (CIFGenLayer,
// cifPaintFunc), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include "mask/ops/kernel.hpp"

namespace hefesto::mask {

namespace {

/// cifPaintFunc over every tile of the operator's layers: paints (or
/// erases) each tile, scaled, into `into`.
void apply_op_tiles(const OpArgs& args, OperatorContext& ctx, MaskPlane& into, bool erase) {
  detail::search_op_tiles(args, args.area, [&](const db::TileView& t, std::int32_t scale) {
    const db::Rect r = detail::scale_rect(t.area, scale);
    if (erase) {
      detail::erase_shape(into, t, r, ctx.compat.magic_compat);
    } else {
      detail::paint_shape(into, t, r, ctx.compat.magic_compat);
    }
    detail::count_tile(ctx);
    return true;
  });
}

}  // namespace

diag::Status op_or(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  apply_op_tiles(args, ctx, out, false);
  return diag::Done{};
}

diag::Status op_and_not(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  apply_op_tiles(args, ctx, out, true);
  return diag::Done{};
}

diag::Status op_and(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  // Collect the layers into a scratch plane, then erase from `out` every
  // space tile (and space triangle) of it.
  ScratchPlane other{*ctx.pool};
  apply_op_tiles(args, ctx, *other, false);
  other->plane().enumerate(db::whole_plane(), detail::space_mask(), [&](const db::TileView& t) {
    detail::erase_shape(out, t, t.area, ctx.compat.magic_compat);
    detail::count_tile(ctx);
    return true;
  });
  return diag::Done{};
}

}  // namespace hefesto::mask
