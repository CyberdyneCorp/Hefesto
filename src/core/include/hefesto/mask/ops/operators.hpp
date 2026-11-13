// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The operator kernels (capability mask-output-operators;
// openspec/changes/m5-mask-gds-cif/design.md D6), one per operator, each a
// function of the operator's operands, the input planes, the area, the
// context and the output plane (ported from CIFgen.c in four groups:
// boolean and grow; contact cuts; bridge and bloat; topology). Each compat
// decision is a small helper `q_mask_<n>_*` taking const CompatSettings&,
// declared next to the kernel that calls it.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); OPS fills in
// the implementation in src/core/src/mask/ops/ (design D2).

#pragma once

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/mask/ops/evaluate.hpp"

namespace hefesto::mask {

/// The operands of one operator call.
struct OpArgs {
  const LayerOp& op;
  const OutputStyle& style;
  const LayerInputs& inputs;
  db::Rect area;  // the area being generated, internal units
};

// Group 1: boolean and grow.
[[nodiscard]] diag::Status op_or(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_and(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_and_not(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_grow(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_shrink(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_grow_grid(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_grow_min(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);

// Group 2: contact cuts.
[[nodiscard]] diag::Status op_squares(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_squares_grid(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_slots(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);

// Group 3: bridge and bloat.
[[nodiscard]] diag::Status op_bridge(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_bridge_lim(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_bloat_or(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_bloat_min(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_bloat_max(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_bloat_all(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_close(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);

// Group 4: topology.
[[nodiscard]] diag::Status op_maxrect(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_orthogonal(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_not_square(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
/// interacting, noninteracting, overlapping and nonoverlapping (args.op.interact).
[[nodiscard]] diag::Status op_interacting(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_net(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_tagged(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_bbox(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_boundary(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);
[[nodiscard]] diag::Status op_mask_hints(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);

// cifinput only.
[[nodiscard]] diag::Status op_copyup(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);

}  // namespace hefesto::mask
