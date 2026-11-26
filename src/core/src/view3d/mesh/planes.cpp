// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The mask-layer source (view-3d "Rendered geometry", mask-layer clause;
// hefesto-3d-visualization "Mesh generation from layout", "Region model";
// openspec/changes/m10-3d-view/design.md D6, D15). W3DCIFredisplay
// (Magic 8.3.694 graphics/W3Dmain.c:1726-1784) flattens the rendered cell
// over the clip area grown by the style's radius, generates every layer of
// the current output style over the clip area (CIFGen clips the result to
// it, in CIF units) and draws each visible layer's plane. Here
// mask::generate_flat does the flatten and the generation; each generated
// plane becomes the flat plane of its stack layer, and the faces, chunks
// and versions are the layout builder's (mesh.cpp, faces.cpp).
//
// An incremental rebuild regenerates only the chunks a change can reach:
// a layout change of area d changes mask geometry within d grown by the
// generation radius (the halo), so that area, in mask units, marks the
// chunks; each of their areas (grown by 1, rebuild_flat) is generated
// again, in internal units rounded outward, and the pieces are united
// into one plane per layer.
//
// Owner: MASKSRC (design D3, round 5).

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <utility>

#include "hefesto/mask/ops/evaluate.hpp"
#include "internal.hpp"

namespace hefesto::view3d::detail {

namespace {

// Floor and ceiling of v / f (f > 0), in 64 bits.
[[nodiscard]] std::int64_t floor_div(std::int64_t v, std::int64_t f) noexcept {
  const std::int64_t q = v / f;
  return (v % f != 0 && v < 0) ? q - 1 : q;
}

[[nodiscard]] std::int64_t ceil_div(std::int64_t v, std::int64_t f) noexcept {
  const std::int64_t q = v / f;
  return (v % f != 0 && v > 0) ? q + 1 : q;
}

[[nodiscard]] db::Coord clamp_coord(std::int64_t v) noexcept {
  return static_cast<db::Coord>(std::clamp<std::int64_t>(v, db::kPlaneMin, db::kPlaneMax));
}

[[nodiscard]] std::int32_t scale_of(const mask::OutputStyle& style) noexcept {
  return style.scale.scale_factor > 0 ? style.scale.scale_factor : 1;
}

// Builds `out` from the stack's mask layers; `plane_of(i)` gives the flat
// plane of mask layer i (only called for a layer that has a plane).
void layout_from_planes(const LayerStack& stack, const mask::LayerPlanes& planes,
                        const std::function<db::Plane(std::size_t)>& plane_of, FlatLayout& out) {
  out = FlatLayout{};
  for (const StackLayer& layer : stack.layers()) {
    if (!layer.mask_layer || planes[*layer.mask_layer] == nullptr) {
      out.of_layer.push_back(-1);
      continue;
    }
    out.of_layer.push_back(static_cast<std::ptrdiff_t>(out.types.size()));
    out.types.push_back(FlatType{mask::kMaskSolid, plane_of(*layer.mask_layer), {}});
  }
}

// ORs each plane of `from` into `into` (an entry of `into` that is null
// takes `from`'s plane).
void unite_into(mask::LayerPlanes& into, mask::LayerPlanes& from) {
  if (into.empty()) {
    into = std::move(from);
    return;
  }
  for (std::size_t i = 0; i < from.size() && i < into.size(); ++i) {
    if (from.planes[i] == nullptr) {
      continue;
    }
    if (into.planes[i] == nullptr) {
      std::swap(into.planes[i], from.planes[i]);  // both release into the same pool
    } else {
      static_cast<void>(into.planes[i]->unite(*from.planes[i]).ok());
    }
  }
}

// generate_flat over each of `areas` (internal units), united. The
// generation errors go to `gen.sink` when it is set (never as feedback:
// gen.problems is not read).
bool generate_areas(const db::CellDef& cell, const MaskSource& source, std::span<const db::Rect> areas,
                    mask::GenerationContext& gen, mask::LayerPlanes& out) {
  mask::GenerateOptions options;
  options.report_errors = gen.sink != nullptr;
  for (const db::Rect& area : areas) {
    if (gen.cancelled()) {
      return false;
    }
    if (area.is_null()) {
      continue;
    }
    mask::LayerPlanes fresh = mask::generate_flat(source.generation, cell, area, *source.style, gen, options);
    unite_into(out, fresh);
  }
  return !gen.cancelled();
}

// The generation of `areas` under ctx.lock held shared when it is set
// (generate_flat reads the database while it flattens the cell), then the
// flat layers of `stack`. The generation errors go to `report` (null: not
// reported). False when cancelled.
bool generate_snapshot(const LayerStack& stack, const db::CellDef& cell, const MaskSource& source,
                       std::span<const db::Rect> areas, const BuildContext& ctx, diag::DiagnosticSink* report,
                       FlatLayout& out) {
  mask::GenerationContext gen;
  gen.progress = &ctx.progress;
  gen.sink = report;
  mask::LayerPlanes planes;
  bool done = false;
  if (ctx.lock == nullptr) {
    done = generate_areas(cell, source, areas, gen, planes);
  } else {
    wait_for_writers(*ctx.lock, ctx.progress);
    const db::DatabaseLock::Shared guard(*ctx.lock);
    done = generate_areas(cell, source, areas, gen, planes);
  }
  if (done) {
    take_planes(stack, planes, out);
  }
  return done;
}

}  // namespace

void copy_planes(const LayerStack& stack, const mask::LayerPlanes& planes, FlatLayout& out) {
  layout_from_planes(stack, planes, [&planes](std::size_t i) { return planes[i]->plane().clone(); }, out);
}

void take_planes(const LayerStack& stack, mask::LayerPlanes& planes, FlatLayout& out) {
  layout_from_planes(
      stack, planes,
      [&planes](std::size_t i) {
        db::Plane taken;
        std::swap(taken, planes[i]->plane());
        return taken;
      },
      out);
}

std::vector<db::Rect> mask_dirty(std::span<const db::Rect> dirty, std::int32_t scale_factor, std::int64_t halo) {
  std::vector<db::Rect> out;
  out.reserve(dirty.size());
  for (const db::Rect& d : dirty) {
    out.push_back(mask::generation_clip(d, scale_factor > 0 ? scale_factor : 1, halo).clip);
    const std::int64_t grow = halo * std::int64_t{scale_factor > 0 ? scale_factor : 1};
    db::Rect& r = out.back();
    r = db::Rect{clamp_coord(std::int64_t{r.x0} - grow), clamp_coord(std::int64_t{r.y0} - grow),
                 clamp_coord(std::int64_t{r.x1} + grow), clamp_coord(std::int64_t{r.y1} + grow)};
  }
  return out;
}

db::Rect internal_area(const db::Rect& area, std::int32_t scale_factor, const db::Rect& limit) {
  const std::int64_t f = scale_factor > 0 ? scale_factor : 1;
  const db::Rect r{clamp_coord(floor_div(area.x0, f)), clamp_coord(floor_div(area.y0, f)),
                   clamp_coord(ceil_div(area.x1, f)), clamp_coord(ceil_div(area.y1, f))};
  return intersect(r, limit);
}

std::optional<MeshModel> build_mask(const LayerStack& stack, const db::CellDef& cell, const MaskSource& source,
                                    const ClipArea& clip, BuildContext& ctx, std::size_t max_tiles) {
  if (!source.style) {
    return std::nullopt;
  }
  const std::array<db::Rect, 1> areas{source.area};
  // R5 item 20 (approved 2026-11-26): with magic-compat ON the errors of a
  // full build are reported once, to the build's sink (W3DCIFredisplay adds
  // them as feedback on every redraw); OFF reports none.
  diag::DiagnosticSink* report = source.generation.compat.magic_compat ? ctx.sink : nullptr;
  const FlattenFn generate = [&](std::span<const db::Rect> /*mask_areas*/, FlatLayout& out) {
    return generate_snapshot(stack, cell, source, areas, ctx, report, out);
  };
  std::optional<MeshModel> model = build_flat(stack, generate, clip, ctx, max_tiles);
  if (model) {
    model->units_per_root = scale_of(*source.style);  // mask units
  }
  return model;
}

std::optional<MeshModel> rebuild_mask(const MeshModel& previous, const LayerStack& stack, const db::CellDef& cell,
                                      const MaskSource& source, std::span<const db::Rect> dirty, BuildContext& ctx,
                                      std::size_t max_tiles) {
  if (!source.style) {
    return std::nullopt;
  }
  const std::int32_t f = scale_of(*source.style);
  const std::int64_t halo = mask::generation_radius(*source.style, source.generation.compat);
  const FlattenFn generate = [&](std::span<const db::Rect> mask_areas, FlatLayout& out) {
    std::vector<db::Rect> areas;
    areas.reserve(mask_areas.size());
    for (const db::Rect& a : mask_areas) {
      areas.push_back(internal_area(a, f, source.area));
    }
    return generate_snapshot(stack, cell, source, areas, ctx, nullptr, out);  // reported by the full build
  };
  return rebuild_flat(previous, stack, generate, mask_dirty(dirty, f, halo), ctx, max_tiles);
}

}  // namespace hefesto::view3d::detail

namespace hefesto::view3d {

ClipArea mask_clip(const MaskSource& source, bool cut_box) {
  if (!source.style) {
    return ClipArea{source.area, cut_box, 0};
  }
  const std::int64_t halo = mask::generation_radius(*source.style, source.generation.compat);
  const mask::GenerationClip clip = mask::generation_clip(source.area, detail::scale_of(*source.style), halo);
  return ClipArea{clip.clip, cut_box, static_cast<db::Coord>(halo)};
}

}  // namespace hefesto::view3d
