// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Layer evaluation (capability mask-output-operators "Layer evaluation
// pipeline for an area", "Hierarchical-mode truncation"; design D4, D6):
// evaluate_layer (CIFGenLayer), evaluate_area (CIFGen's per-area part) and
// the clip areas (cifGenClip).
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (CIFGenLayer,
// CIFGen, cifGenClip), Copyright (C) 1985, 1990 Regents of the University
// of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include "hefesto/mask/ops/evaluate.hpp"

#include <array>

#include "mask/ops/kernel.hpp"

namespace hefesto::mask {

namespace {

using Kernel = diag::Status (*)(const OpArgs&, OperatorContext&, MaskPlane&);

/// The kernel of each OpKind, in the enum's order.
constexpr std::array<Kernel, 27> kKernels = {
    op_and,           // op_and
    op_or,            // op_or
    op_and_not,       // and_not
    op_grow,          // grow
    op_grow_min,      // grow_min
    op_grow_grid,     // grow_grid
    op_shrink,        // shrink
    op_bloat_or,      // bloat_or
    op_bloat_min,     // bloat_min
    op_bloat_max,     // bloat_max
    op_bloat_all,     // bloat_all
    op_squares,       // squares
    op_squares_grid,  // squares_grid
    op_slots,         // slots
    op_bbox,          // bbox
    op_boundary,      // boundary
    op_net,           // net
    op_maxrect,       // maxrect
    op_interacting,   // interacting
    op_copyup,        // copyup
    op_close,         // close
    op_orthogonal,    // orthogonal
    op_bridge,        // bridge
    op_bridge_lim,    // bridge_lim
    op_mask_hints,    // mask_hints
    op_not_square,    // not_square
    op_tagged,        // tagged
};
static_assert(static_cast<std::size_t>(OpKind::tagged) + 1 == kKernels.size(), "one kernel per OpKind");

bool stops_unless_grow(OpKind kind) noexcept {
  return kind == OpKind::squares || kind == OpKind::squares_grid || kind == OpKind::slots;
}

bool always_stops(OpKind kind) noexcept {
  return kind == OpKind::net || kind == OpKind::tagged || kind == OpKind::boundary || kind == OpKind::bbox;
}

bool cancelled(const OperatorContext& ctx) noexcept { return ctx.progress != nullptr && ctx.progress->cancelled(); }

}  // namespace

std::optional<std::size_t> hierarchical_stop(std::span<const LayerOp> ops) noexcept {
  for (std::size_t i = 0; i < ops.size(); ++i) {
    const OpKind kind = ops[i].kind;
    if (always_stops(kind)) {
      return i;
    }
    if (stops_unless_grow(kind) && (i + 1 >= ops.size() || ops[i + 1].kind != OpKind::grow)) {
      return i;
    }
  }
  return std::nullopt;
}

MaskPlane& evaluate_layer(const OutputStyle& style, std::size_t layer, const LayerInputs& inputs, const db::Rect& area,
                          OperatorContext& ctx) {
  MaskPlane& cur = ctx.pool->acquire();
  if (layer >= style.layers.size()) {
    return cur;
  }
  const std::vector<LayerOp>& ops = style.layers[layer].ops;
  const std::optional<std::size_t> stop = ctx.hierarchical ? hierarchical_stop(ops) : std::nullopt;
  const std::size_t count = stop.value_or(ops.size());
  for (std::size_t i = 0; i < count && !cancelled(ctx); ++i) {
    const OpArgs args{ops[i], style, inputs, area, layer};
    const diag::Status status = kKernels.at(static_cast<std::size_t>(ops[i].kind))(args, ctx, cur);
    if (!status.ok() && ctx.sink != nullptr) {
      ctx.sink->emit(status.error());
    }
  }
  return cur;
}

GenerationClip generation_clip(const db::Rect& area, std::int32_t scale_factor, std::int64_t radius) noexcept {
  GenerationClip out{area, area};
  const auto grow = [](db::Coord v, std::int64_t by) { return detail::clamp_coord(std::int64_t{v} + by); };
  const auto scale = [scale_factor](db::Coord v) { return detail::clamp_coord(std::int64_t{v} * scale_factor); };
  if (area.x0 > db::kPlaneMin) {
    out.clip.x0 = scale(area.x0);
    out.expanded.x0 = grow(area.x0, -radius);
  }
  if (area.y0 > db::kPlaneMin) {
    out.clip.y0 = scale(area.y0);
    out.expanded.y0 = grow(area.y0, -radius);
  }
  if (area.x1 < db::kPlaneMax) {
    out.clip.x1 = scale(area.x1);
    out.expanded.x1 = grow(area.x1, radius);
  }
  if (area.y1 < db::kPlaneMax) {
    out.clip.y1 = scale(area.y1);
    out.expanded.y1 = grow(area.y1, radius);
  }
  return out;
}

std::vector<MaskPlane*> evaluate_area(const OutputStyle& style, const std::vector<bool>& requested,
                                      const LayerInputs& inputs, const db::Rect& area, OperatorContext& ctx,
                                      const AreaOptions& options) {
  const std::int32_t scale = style.scale.scale_factor > 0 ? style.scale.scale_factor : 1;
  const GenerationClip clip = generation_clip(area, scale, options.radius.value_or(style.halo()));
  std::vector<MaskPlane*> planes(style.layers.size(), nullptr);
  const LayerInputs layer_inputs{inputs.layout, std::span<MaskPlane* const>{planes}, inputs.technology};
  for (std::size_t i = 0; i < style.layers.size(); ++i) {
    if (i < requested.size() && requested[i]) {
      planes[i] = &evaluate_layer(style, i, layer_inputs, clip.expanded, ctx);
    } else if (options.all_planes) {
      planes[i] = &ctx.pool->acquire();
    }
  }
  for (MaskPlane* plane : planes) {
    if (plane != nullptr) {
      plane->clip(clip.clip);
    }
  }
  return planes;
}

}  // namespace hefesto::mask
