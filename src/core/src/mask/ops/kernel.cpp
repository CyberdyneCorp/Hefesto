// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the operator kernels (design D6). See kernel.hpp.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (cifSrTiles,
// cifSrTiles2, SetBoxGrid, SetValueGrid, SetMinBoxGrid, the region walks of
// cifSquaresFillArea and cifInteractingRegions) and cif/CIFmain.c
// (CIFError), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include "mask/ops/kernel.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::mask::detail {

namespace {

std::pair<db::Coord, db::Coord> side_span(const db::TileView& t, db::Side side) noexcept {
  if (side == db::Side::left || side == db::Side::right) {
    return {t.area.y0, t.area.y1};
  }
  return {t.area.x0, t.area.x1};
}

}  // namespace

std::vector<db::TileView> side_tiles(const db::Plane& plane, const db::TileView& t, db::Side side) {
  std::vector<db::TileView> out;
  const auto [lo, hi] = side_span(t, side);
  plane.neighbors(t, side, lo, hi, out);
  return out;
}

std::optional<db::TileView> first_neighbor(const db::Plane& plane, const db::TileView& t, db::Side side) {
  std::vector<db::TileView> n = side_tiles(plane, t, side);
  if (n.empty()) {
    return std::nullopt;
  }
  return n.front();
}

std::optional<db::TileView> last_neighbor(const db::Plane& plane, const db::TileView& t, db::Side side) {
  std::vector<db::TileView> n = side_tiles(plane, t, side);
  if (n.empty()) {
    return std::nullopt;
  }
  return n.back();
}

tech::TileType facing_type(const std::optional<db::TileView>& n, db::Side side) noexcept {
  if (!n) {
    return kBoundaryType;
  }
  switch (side) {
    case db::Side::left:
      return right_type(*n);
    case db::Side::right:
      return left_type(*n);
    case db::Side::bottom:
      return top_type(*n);
    case db::Side::top:
      break;
  }
  return bottom_type(*n);
}

db::Coord clamp_coord(std::int64_t v) noexcept {
  return static_cast<db::Coord>(std::clamp<std::int64_t>(v, db::kMinusInfinity, db::kInfinity));
}

db::Rect scale_rect(const db::Rect& r, std::int32_t s) noexcept {
  if (s == 1) {
    return r;
  }
  return db::Rect{clamp_coord(std::int64_t{r.x0} * s), clamp_coord(std::int64_t{r.y0} * s),
                  clamp_coord(std::int64_t{r.x1} * s), clamp_coord(std::int64_t{r.y1} * s)};
}

db::Rect scale_finite(const db::Rect& r, std::int32_t s) noexcept {
  db::Rect out = r;
  if (r.x0 > db::kPlaneMin) {
    out.x0 = clamp_coord(std::int64_t{r.x0} * s);
  }
  if (r.y0 > db::kPlaneMin) {
    out.y0 = clamp_coord(std::int64_t{r.y0} * s);
  }
  if (r.x1 < db::kPlaneMax) {
    out.x1 = clamp_coord(std::int64_t{r.x1} * s);
  }
  if (r.y1 < db::kPlaneMax) {
    out.y1 = clamp_coord(std::int64_t{r.y1} * s);
  }
  return out;
}

void paint_shape(MaskPlane& p, const db::TileView& t, const db::Rect& area, bool magic_compat) {
  if (t.split) {
    (void)p.paint_triangle(area, t.diagonal, t.right_side, magic_compat);
  } else {
    (void)p.paint(area);
  }
}

void erase_shape(MaskPlane& p, const db::TileView& t, const db::Rect& area, bool magic_compat) {
  if (t.split) {
    (void)p.erase_triangle(area, t.diagonal, t.right_side, magic_compat);
  } else {
    (void)p.erase(area);
  }
}

const tech::TypeMask& solid_mask() {
  static const tech::TypeMask mask = tech::TypeMask::of(kMaskSolid);
  return mask;
}

const tech::TypeMask& space_mask() {
  static const tech::TypeMask mask = tech::TypeMask::of(tech::kSpace);
  return mask;
}

std::vector<Source> layout_sources(const LayerInputs& inputs, const tech::TypeMask& types) {
  std::vector<Source> out;
  if (inputs.layout == nullptr || types.empty()) {
    return out;
  }
  for (std::size_t i = tech::kPlaneDrcCheck; i < inputs.layout->plane_count(); ++i) {
    const auto id = static_cast<tech::PlaneId>(i);
    const bool meets = inputs.technology == nullptr || inputs.technology->plane_types(id).intersects(types);
    if (meets) {
      out.push_back(Source{&inputs.layout->plane(id), true, id});
    }
  }
  return out;
}

std::vector<const MaskPlane*> layer_sources(const LayerInputs& inputs, const std::vector<std::size_t>& cif_layers) {
  std::vector<std::size_t> sorted = cif_layers;
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  std::vector<const MaskPlane*> out;
  for (const std::size_t i : sorted) {
    if (i < inputs.layers.size() && inputs.layers[i] != nullptr) {
      out.push_back(inputs.layers[i]);
    }
  }
  return out;
}

bool search_op_tiles(const OpArgs& args, const db::Rect& area, ScaledVisitor visit) {
  const std::int32_t scale = scale_of(args);
  for (const Source& s : layout_sources(args.inputs, args.op.types)) {
    const bool done = !s.plane->enumerate(area, args.op.types, [&](const db::TileView& t) { return visit(t, scale); });
    if (done) {
      return false;
    }
  }
  for (const MaskPlane* m : layer_sources(args.inputs, args.op.cif_layers)) {
    const bool done =
        !m->plane().enumerate(db::whole_plane(), solid_mask(), [&](const db::TileView& t) { return visit(t, 1); });
    if (done) {
      return false;
    }
  }
  return true;
}

bool search_op_tiles2(const OpArgs& args, const OperatorContext& ctx, const db::Rect& mask_area,
                      const MaskPlane* current, FoundVisitor visit) {
  const db::Rect internal = q_mask_21_to_internal(mask_area, scale_of(args), ctx.compat);
  for (const Source& s : layout_sources(args.inputs, args.op.types)) {
    bool found = false;
    s.plane->enumerate(internal, args.op.types, [&](const db::TileView& t) {
      found = visit(t, s);
      return !found;
    });
    if (found) {
      return true;
    }
  }
  std::vector<const MaskPlane*> layers;
  if (args.op.types.empty() && args.op.cif_layers.empty()) {
    if (current != nullptr) {
      layers.push_back(current);
    }
  } else {
    layers = layer_sources(args.inputs, args.op.cif_layers);
  }
  for (const MaskPlane* m : layers) {
    bool found = false;
    const Source src{&m->plane(), false, 0};
    m->plane().enumerate(mask_area, solid_mask(), [&](const db::TileView& t) {
      found = visit(t, src);
      return !found;
    });
    if (found) {
      return true;
    }
  }
  return false;
}

void set_box_grid(db::Rect& r, std::int32_t limit) noexcept {
  if (limit <= 1) {
    return;
  }
  const auto low = [limit](db::Coord& v) {
    const std::int32_t delta = std::abs(v) % limit;
    if (delta > 0) {
      v = v < 0 ? v + delta - limit : v - delta;
    }
  };
  const auto high = [limit](db::Coord& v) {
    const std::int32_t delta = std::abs(v) % limit;
    if (delta > 0) {
      v = v < 0 ? v + delta : v - delta + limit;
    }
  };
  low(r.x0);
  high(r.x1);
  low(r.y0);
  high(r.y1);
}

std::int32_t set_value_grid(std::int32_t v, std::int32_t limit) noexcept {
  if (limit > 1) {
    const std::int32_t delta = v % limit;
    if (delta > 0) {
      v += limit - delta;
    }
  }
  return v;
}

void set_min_box_grid(db::Rect& r, std::int32_t width, std::int32_t limit) noexcept {
  if (r.x1 - r.x0 < width) {
    const std::int32_t total = r.x1 + r.x0;
    r.x0 = (total - width) / 2;
    r.x1 = (total + width) / 2;
  }
  if (r.y1 - r.y0 < width) {
    const std::int32_t total = r.y1 + r.y0;
    r.y0 = (total - width) / 2;
    r.y1 = (total + width) / 2;
  }
  set_box_grid(r, limit);
}

void cif_error(const OpArgs& args, const OperatorContext& ctx, const db::Rect& mask_area, std::string_view code,
               std::string_view message) {
  constexpr std::uint32_t kNoErrors = 0x20;  // CWF_NO_ERRORS
  if (ctx.sink == nullptr || ctx.cell == nullptr || (args.style.options & kNoErrors) != 0) {
    return;
  }
  const std::string layer = args.layer < args.style.layers.size() ? args.style.layers[args.layer].name : std::string{};
  const std::int32_t s = scale_of(args);
  diag::Diagnostic d =
      diag::make(code, "CIF error in cell " + ctx.cell->name() + ", layer " + layer + ": " + std::string{message},
                 diag::LayoutLocation{ctx.cell->name(), mask_area.x0 / s, mask_area.y0 / s});
  d.native_only = true;  // Magic adds feedback; it prints nothing
  ctx.sink->emit(d);
}

namespace {

/// Pushes the solid neighbours of `t` in Magic's order (top right to left,
/// left bottom to top, bottom left to right, right top to bottom), marking
/// unprocessed ones pending. Returns whether any solid neighbour exists.
bool push_solid_neighbors(const db::Plane& plane, const db::TileView& t, Marks& marks,
                          std::vector<db::TileView>& stack) {
  bool any = false;
  const auto push = [&](const db::TileView& n, tech::TileType facing) {
    if (facing != kMaskSolid) {
      return;
    }
    any = true;
    if (marks.get(n) == Mark::unprocessed) {
      marks.set(n, Mark::pending);
      stack.push_back(n);
    }
  };
  std::vector<db::TileView> n = side_tiles(plane, t, db::Side::top);
  std::for_each(n.rbegin(), n.rend(), [&](const db::TileView& v) { push(v, bottom_type(v)); });
  n = side_tiles(plane, t, db::Side::left);
  std::for_each(n.begin(), n.end(), [&](const db::TileView& v) { push(v, right_type(v)); });
  n = side_tiles(plane, t, db::Side::bottom);
  std::for_each(n.begin(), n.end(), [&](const db::TileView& v) { push(v, top_type(v)); });
  n = side_tiles(plane, t, db::Side::right);
  std::for_each(n.rbegin(), n.rend(), [&](const db::TileView& v) { push(v, left_type(v)); });
  return any;
}

}  // namespace

Region collect_region(const db::Plane& plane, const db::TileView& start, Marks& marks,
                      util::FunctionRef<void(const db::TileView&)> on_tile) {
  Region region;
  region.bbox = start.area;
  std::vector<db::TileView> stack;
  if (marks.get(start) == Mark::unprocessed) {
    marks.set(start, Mark::pending);
    stack.push_back(start);
  }
  while (!stack.empty()) {
    const db::TileView t = stack.back();
    stack.pop_back();
    if (marks.get(t) != Mark::pending) {
      continue;
    }
    marks.set(t, Mark::processed);
    region.bbox = db::include(region.bbox, t.area);
    region.tiles.push_back(t);
    on_tile(t);
    if (t.split || push_solid_neighbors(plane, t, marks, stack)) {
      region.simple = false;
    }
  }
  return region;
}

void for_each_region_start(const db::Plane& plane, const db::Rect& area, Marks& marks,
                           util::FunctionRef<void(const db::TileView&)> fn) {
  plane.enumerate(area, solid_mask(), [&](const db::TileView& t) {
    if (marks.get(t) == Mark::unprocessed) {
      fn(t);
    }
    return true;
  });
}

}  // namespace hefesto::mask::detail
