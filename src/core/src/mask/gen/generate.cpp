// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Area and flat generation (capability mask-hierarchical-generation "Halo
// radius", "Own-paint generation area", "Flat region generation"; design
// D8): CIFGen over one cell's own paint and over a flattened area.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (CIFGen,
// cifGenClip), cif/CIFtech.c (cifComputeRadii, cifComputeHalo) and
// cif/CIFsee.c (the flattening of CIFSeeLayer, CIFPaintLayer and
// CIFCoverageLayer), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: GEN.

#include "hefesto/mask/gen/generate.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/ops/evaluate.hpp"
#include "mask/gen/internal.hpp"

namespace hefesto::mask {

using gen_detail::grow;

// --- LayerPlanes -------------------------------------------------------------

LayerPlanes::LayerPlanes(LayerPlanes&& other) noexcept
    : pool_(std::exchange(other.pool_, nullptr)), planes(std::move(other.planes)) {
  other.planes.clear();
}

LayerPlanes& LayerPlanes::operator=(LayerPlanes&& other) noexcept {
  if (this != &other) {
    reset();
    pool_ = std::exchange(other.pool_, nullptr);
    planes = std::move(other.planes);
    other.planes.clear();
  }
  return *this;
}

void LayerPlanes::reset() noexcept {
  if (pool_ != nullptr) {
    for (MaskPlane* plane : planes) {
      if (plane != nullptr) {
        pool_->release(*plane);
      }
    }
  }
  planes.clear();
}

// --- Sources -------------------------------------------------------------------

GenerationSource generation_source(const editor::EditorState& editor) {
  GenerationSource source{editor.technology.get(), editor.drc.compat_settings(), {}, editor.mask.cif.unfracture};
  source.find_interaction = [&editor](const db::CellDef& cell, const db::Rect& area,
                                      std::int64_t radius) -> std::optional<db::Rect> {
    const auto r = static_cast<std::int32_t>(std::clamp<std::int64_t>(radius, 0, db::kInfinity));
    diag::Outcome<std::optional<db::Rect>> found = drc::interaction_area(editor, cell, area, r);
    return found.ok() ? found.value() : std::nullopt;
  };
  return source;
}

// --- The radius ------------------------------------------------------------------

namespace {

struct Radii {
  std::int64_t grow = 0;
  std::int64_t shrink = 0;
};

std::int64_t largest_positive(const BloatTable& table) noexcept {
  std::int64_t out = 0;
  for (const std::int32_t d : table.distance) {
    out = std::max<std::int64_t>(out, d);
  }
  return out;
}

// bloat-or: the largest positive distance grows; a negative one shrinks
// only when it did not first exceed the running positive maximum.
void add_bloat_or(const BloatTable& table, Radii& r) noexcept {
  std::int64_t cur_grow = 0;
  std::int64_t cur_shrink = 0;
  for (const std::int32_t d : table.distance) {
    if (d > cur_grow) {
      cur_grow = d;
    } else if (-std::int64_t{d} > cur_shrink) {
      cur_shrink = -std::int64_t{d};
    }
  }
  r.grow += cur_grow;
  r.shrink += cur_shrink;
}

// Q-MASK-404 OFF: bloat-max and bloat-min distances, bridge widths.
void add_off_reach(const LayerOp& op, Radii& r) noexcept {
  if ((op.kind == OpKind::bloat_max || op.kind == OpKind::bloat_min) && op.bloat) {
    r.grow += largest_positive(*op.bloat);
  } else if ((op.kind == OpKind::bridge || op.kind == OpKind::bridge_lim) && !op.values.empty()) {
    r.grow += std::max<std::int64_t>(0, op.values[bridge_value::width]);
  }
}

void add_op_reach(const LayerOp& op, bool magic_compat, Radii& r) noexcept {
  if (op.kind == OpKind::grow || op.kind == OpKind::grow_min || op.kind == OpKind::grow_grid) {
    r.grow += op.distance;
  } else if (op.kind == OpKind::shrink) {
    r.shrink += op.distance;
  } else if (op.kind == OpKind::bloat_or && op.bloat) {
    add_bloat_or(*op.bloat, r);
  } else if (!magic_compat) {
    add_off_reach(op, r);
  }
}

bool ends_radius_walk(OpKind kind) noexcept {
  return kind == OpKind::bbox || kind == OpKind::net || kind == OpKind::tagged || kind == OpKind::mask_hints;
}

Radii layer_radii(const OutputLayer& layer, const std::vector<Radii>& earlier, bool magic_compat) {
  Radii r;
  for (const LayerOp& op : layer.ops) {
    if (ends_radius_walk(op.kind)) {
      break;
    }
    for (const std::size_t i : op.cif_layers) {
      if (i < earlier.size()) {
        r.grow = std::max(r.grow, earlier[i].grow);
        r.shrink = std::max(r.shrink, earlier[i].shrink);
      }
    }
    add_op_reach(op, magic_compat, r);
  }
  return r;
}

}  // namespace

std::int64_t generation_radius(const OutputStyle& style, const compat::CompatSettings& compat) {
  if (compat.magic_compat) {
    return std::clamp<std::int64_t>(style.halo(), 0, db::kInfinity);
  }
  std::vector<Radii> radii;
  radii.reserve(style.layers.size());
  std::int64_t max_grow = 0;
  std::int64_t max_shrink = 0;
  for (const OutputLayer& layer : style.layers) {
    radii.push_back(layer_radii(layer, radii, false));
    max_grow = std::max(max_grow, radii.back().grow);
    max_shrink = std::max(max_shrink, radii.back().shrink);
  }
  std::int64_t d = max_grow > max_shrink ? 2 * max_grow : 2 * max_shrink;
  d = style.scale.scale_factor > 0 ? d / style.scale.scale_factor + 1 : d + 1;
  return std::clamp<std::int64_t>(d, 0, db::kInfinity);
}

// --- CIFGen ----------------------------------------------------------------------

namespace {

bool requested(const GenerateOptions& options, std::size_t layer) noexcept {
  return options.layers.empty() || (layer < options.layers.size() && options.layers[layer]);
}

// A finite side of the search area times the scale factor; a side at the
// plane's edge stays (as cifGenClip leaves it).
db::Coord scale_side(db::Coord v, std::int32_t scale) noexcept {
  return v <= db::kPlaneMin || v >= db::kPlaneMax ? v : gen_detail::clamp_coord(std::int64_t{v} * scale);
}

}  // namespace

void unfracture_layer(MaskPlane& plane, const GenerationClip& clip, std::int32_t scale,
                      const compat::CompatSettings& compat) {
  const db::Rect& e = clip.expanded;
  if (compat.magic_compat) {
    plane.plane().merge_split_tiles(e);  // Magic's internal-unit area on a mask-unit plane
    return;
  }
  const std::int32_t s = std::max(scale, 1);
  plane.plane().merge_split_tiles(
      db::Rect{scale_side(e.x0, s), scale_side(e.y0, s), scale_side(e.x1, s), scale_side(e.y1, s)});
}

LayerPlanes generate_area(const GenerationSource& source, const db::CellDef& layout, const db::CellDef* origin,
                          const db::Rect& area, const OutputStyle& style, GenerationContext& ctx,
                          const GenerateOptions& options) {
  const std::int64_t radius = generation_radius(style, source.compat);
  const GenerationClip clip = generation_clip(area, gen_detail::scale_of(style), radius);
  LayerPlanes out(ctx.pool, style.layers.size());
  OperatorContext octx = ctx.operator_context(origin, source.compat);
  octx.hierarchical = options.hierarchical;
  if (!options.report_errors) {
    octx.sink = nullptr;  // CIFErrorDef = NULL
  }
  const LayerInputs inputs{&layout, std::span<MaskPlane* const>{out.planes}, source.technology};
  const bool unfracture = options.unfracture || source.unfracture;
  for (std::size_t i = 0; i < style.layers.size(); ++i) {
    if (requested(options, i)) {
      out.planes[i] = &evaluate_layer(style, i, inputs, clip.expanded, octx);
      if (unfracture) {
        unfracture_layer(*out.planes[i], clip, gen_detail::scale_of(style), source.compat);
      }
    } else if (options.all_planes) {
      out.planes[i] = &ctx.pool.acquire();
    }
  }
  for (MaskPlane* plane : out.planes) {
    if (plane != nullptr) {
      plane->clip(clip.clip);
    }
  }
  return out;
}

void generate_area_into(const GenerationSource& source, const db::CellDef& layout, const db::CellDef* origin,
                        const db::Rect& area, const OutputStyle& style, GenerationContext& ctx,
                        const GenerateOptions& options, LayerPlanes& into) {
  LayerPlanes fresh = generate_area(source, layout, origin, area, style, ctx, options);
  if (into.planes.size() < fresh.planes.size()) {
    into.planes.resize(fresh.planes.size(), nullptr);
  }
  for (std::size_t i = 0; i < fresh.planes.size(); ++i) {
    MaskPlane* plane = fresh.planes[i];
    if (plane == nullptr) {
      continue;
    }
    if (into.planes[i] == nullptr) {
      into.planes[i] = std::exchange(fresh.planes[i], nullptr);  // both release into ctx.pool
    } else {
      (void)into.planes[i]->unite(*plane);
    }
  }
}

// --- Flat generation ----------------------------------------------------------------

LayerPlanes generate_flat(const GenerationSource& source, const db::CellDef& cell, const db::Rect& area,
                          const OutputStyle& style, GenerationContext& ctx, const GenerateOptions& options) {
  if (source.technology == nullptr) {
    return {};
  }
  const db::Rect search = grow(area, generation_radius(style, source.compat));
  std::unique_ptr<db::CellDef> scratch = gen_detail::make_scratch("__CIF2__", *source.technology);
  gen_detail::Flattener flattener(*source.technology, source.compat.magic_compat);
  if (!flattener.copy(cell, search, db::kIdentity, gen_detail::all_but_space_and_drc(), *scratch) &&
      ctx.sink != nullptr) {
    ctx.sink->emit(diag::make_native(gen_detail::kTooDeep, "The hierarchy under " + cell.name() + " is deeper than " +
                                                              std::to_string(gen_detail::kMaxDepth) +
                                                              " levels; the deeper levels were not generated."));
  }
  return generate_area(source, *scratch, &cell, area, style, ctx, options);
}

LayerPlanes generate_flat(editor::EditorState& editor, const db::CellDef& cell, const db::Rect& area,
                          const OutputStyle& style, GenerationContext& ctx) {
  return generate_flat(generation_source(editor), cell, area, style, ctx);
}

}  // namespace hefesto::mask
