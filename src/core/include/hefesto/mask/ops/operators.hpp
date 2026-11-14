// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The operator kernels (capability mask-output-operators;
// openspec/changes/m5-mask-gds-cif/design.md D6), one per operator, each a
// function of the operator's operands, the input planes, the area, the
// context and the layer's current plane (ported from CIFgen.c in four
// groups: boolean and grow; contact cuts; bridge and bloat; topology). Each
// compat decision is a small helper `q_mask_<n>_*` taking const
// CompatSettings& (mask/ops/compat.hpp).
//
// `out` is the layer's current plane (CIFGenLayer's curPlane): a kernel
// reads it as its input and leaves its result in it. Kernels that build a
// new plane take scratch planes from ctx.pool and swap.
//
// Operands (LayerOp; Magic's CIFOp and its co_client record, as the style
// reader fills them). Distances are mask units. `types` and `cif_layers` are
// co_paintMask and co_cifMask.
//   or, and, and-not, interacting  types / cif_layers: the layers read
//   grow, shrink, grow-grid,       distance (co_distance)
//   grow-min
//   close                          distance: the area threshold as
//                                  normalised linearly (Magic, Q-MASK-2 ON;
//                                  0 = every enclosed area); optional
//                                  values {declared, D, L}: the threshold as
//                                  declared, the cumulative multiply D and
//                                  the cumulative reduce L applied since
//                                  (Q-MASK-2 OFF; absent = distance, 1, 1)
//   bridge                         distance; values[bridge_value::width]
//   bridge-lim                     as bridge; types / cif_layers: the
//                                  limiting layers
//   squares                        values: squares_value {border, size, sep}
//   squares-grid                   values: {border, size, sep, gridx, gridy}
//   slots                          values: slots_value {sborder, ssize, ssep,
//                                  lborder, lsize, lsep, offset, start}
//                                  (missing trailing values are 0)
//   bloat-or, bloat-min,           types: the bloated types (co_paintMask);
//   bloat-max                      bloat: plane (bl_plane, a tech::PlaneId)
//                                  and distance[t] (bl_distance)
//   bloat-all                      types / cif_layers: the trigger layers;
//                                  distance: the clip distance (0 = none);
//                                  bloat: plane (bl_plane), or -1 when the
//                                  bloated layers are mask layers;
//                                  distance[i] non-zero for each bloated
//                                  type i (plane >= 0) or mask layer i
//                                  (plane = -1)
//   net, tagged                    text: the name; types / cif_layers: the
//                                  optional layer list
//   mask-hints                     text: the hint name
//   bbox                           option: `top` (only for top-level cells)
//   maxrect                        option: `external`
//   orthogonal                     option: `fill` (else remove)
//   boundary, not-square, copyup   no operands
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); OPS fills in
// the implementation in src/core/src/mask/ops/ (design D2).

#pragma once

#include <cstddef>

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
  /// The layer being generated (CIFErrorLayer), for error messages; the
  /// layer's next operator (op + 1) when the kernel needs it.
  std::size_t layer = 0;
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

/// cifinput only: the read engine (INPUT) keeps the copy itself; in output
/// generation the operator does nothing, as CIFGenLayer skips it.
[[nodiscard]] diag::Status op_copyup(const OpArgs& args, OperatorContext& ctx, MaskPlane& out);

}  // namespace hefesto::mask
