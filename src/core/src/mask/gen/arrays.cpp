// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Array interactions (capability mask-hierarchical-generation "Array
// interaction geometry parameters", "Array interaction regions", "Array
// interaction correction", "Array correction replication"; design D8):
// CIFGenArrays.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFhier.c (CIFGenArrays,
// cifHierArrayFunc, cifHierElementFuncLow/High, cifHierElementFunc,
// cifHierPaintArrayFunc), Copyright (C) 1985, 1990 Regents of the University
// of California (see THIRD_PARTY_NOTICES.md).
//
// Compat splits:
// - Q-MASK-406: the element filter uses Magic's signed index differences
//   with magic-compat ON, the absolute distance from the corner with it OFF.
// - Magic's low-phase replication reuses its A and B rectangles: the core
//   windows A'' and B'' overwrite them while the first layer is copied, so
//   every later layer's left-edge and bottom-edge copies use the core
//   windows. magic-compat ON replicates this; OFF copies every layer from
//   the windows the spec names (open item for a decision).
// - An array whose pitch is zero along an axis it repeats on makes Magic
//   divide by zero; it gets no array correction here, in both modes.
//
// Owner: GEN.

#include <cstdlib>

#include "mask/gen/hier_internal.hpp"

namespace hefesto::mask {

namespace gen_detail {

namespace {

// "Array interaction geometry parameters".
struct ArrayGeometry {
  db::Rect bb;  // the whole array (U.bbox)
  std::int64_t xsep = 0;
  std::int64_t ysep = 0;
  std::int64_t xsize = 0;
  std::int64_t ysize = 0;
  std::int64_t nx = 0;
  std::int64_t ny = 0;
  std::int64_t r = 0;

  [[nodiscard]] bool x_close() const noexcept { return xsep < xsize + r; }
  [[nodiscard]] bool y_close() const noexcept { return ysep < ysize + r; }
};

std::optional<ArrayGeometry> array_geometry(const db::CellUse& use, std::int64_t radius) {
  const db::Rect child = use.def->bbox();
  const db::ArrayInfo& a = use.array;
  const std::int64_t x = a.xlo == a.xhi ? radius + child.width() : std::int64_t{a.xsep};
  const std::int64_t y = a.ylo == a.yhi ? radius + child.height() : std::int64_t{a.ysep};
  const db::Rect pitch = db::apply(use.transform, db::Rect{0, 0, clamp_coord(x), clamp_coord(y)});
  const db::Rect size = db::apply(use.transform, child);
  ArrayGeometry g{use.bbox, pitch.width(), pitch.height(), size.width(), size.height(), 0, 0, radius};
  if (g.xsep <= 0 || g.ysep <= 0) {
    return std::nullopt;  // Magic divides by zero
  }
  g.nx = (g.bb.width() - g.xsize) / g.xsep + 1;
  g.ny = (g.bb.height() - g.ysize) / g.ysep + 1;
  return g;
}

db::Rect rect64(std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1) {
  return db::Rect{clamp_coord(x0), clamp_coord(y0), clamp_coord(x1), clamp_coord(y1)};
}

// "Array interaction regions".
db::Rect region_a(const ArrayGeometry& g) {
  return rect64(std::int64_t{g.bb.x0} - g.r, std::int64_t{g.bb.y0} + g.ysep - g.r, g.bb.x0 + g.xsize + g.r,
                g.bb.y0 + g.ysize + g.r);
}
db::Rect region_b(const ArrayGeometry& g) {
  return rect64(g.bb.x0 + g.xsep - g.r, std::int64_t{g.bb.y0} - g.r, g.bb.x0 + g.xsize + g.r,
                g.bb.y0 + g.ysep - g.r);
}
db::Rect region_c(const ArrayGeometry& g) {
  return rect64(g.bb.x1 - g.xsize - g.r, g.bb.y1 - g.ysize - g.r, g.bb.x1 - g.xsep + g.r, std::int64_t{g.bb.y1} + g.r);
}
db::Rect region_d(const ArrayGeometry& g) {
  return rect64(g.bb.x1 - g.xsep + g.r, g.bb.y1 - g.ysize - g.r, std::int64_t{g.bb.x1} + g.r,
                g.bb.y1 - g.ysep + g.r);
}

enum class Phase : std::uint8_t { low, high };

// cifHierElementFuncLow/High (Q-MASK-406).
bool element_selected(const db::CellUse& use, std::int32_t x, std::int32_t y, Phase phase, bool magic_compat) {
  const db::ArrayInfo& a = use.array;
  std::int64_t dx = phase == Phase::low ? std::int64_t{x} - a.xlo : std::int64_t{a.xhi} - x;
  std::int64_t dy = phase == Phase::low ? std::int64_t{y} - a.ylo : std::int64_t{a.yhi} - y;
  if (!magic_compat) {
    dx = std::llabs(dx);
    dy = std::llabs(dy);
  }
  return dx < 2 && dy < 2;
}

// The scratch state of one phase.
struct PhaseState {
  PhaseState(HierPass& p, const db::CellUse& u)
      : pass(p),
        use(u),
        total_def(make_scratch("__CIF__", *p.source.technology)),
        comp_def(make_scratch("__CIF2__", *p.source.technology)),
        total(p.ctx.pool, p.style.layers.size()),
        comp(p.ctx.pool, p.style.layers.size()) {}

  HierPass& pass;
  const db::CellUse& use;
  std::unique_ptr<db::CellDef> total_def;
  std::unique_ptr<db::CellDef> comp_def;
  LayerPlanes total;
  LayerPlanes comp;
};

// cifHierElementFunc.
void add_element(PhaseState& s, const ElementRef& e, const db::Rect& region, const std::vector<bool>& gen) {
  HierPass& pass = s.pass;
  const db::Rect def_area = db::apply(db::invert(e.to_parent), region);
  const db::Rect yank_area = grow(def_area, pass.radius);
  const LayerClasses& classes = *pass.classes;
  (void)pass.flattener.copy(*s.use.def, yank_area, e.to_parent, classes.yank, *s.total_def);
  clear_scratch(*s.comp_def);
  (void)pass.flattener.copy(*s.use.def, yank_area, e.to_parent, classes.yank, *s.comp_def);
  GenerateOptions options;
  options.layers = gen;
  options.hierarchical = true;
  options.report_errors = false;
  options.unfracture = pass.unfracture;
  generate_area_into(pass.source, *s.comp_def, s.use.def, region, pass.style, pass.ctx, options, s.comp);
}

// One region: DBArraySr over it grown by r, then the total generated.
void process_region(PhaseState& s, const db::Rect& region, Phase phase) {
  HierPass& pass = s.pass;
  const std::vector<bool>& gen = pass.classes->hier;
  array_elements(s.use, grow(region, pass.radius), [&](const ElementRef& e) {
    if (!pass.ctx.cancelled() && element_selected(s.use, e.x, e.y, phase, pass.source.compat.magic_compat)) {
      add_element(s, e, region, gen);
    }
  });
  GenerateOptions options;
  options.layers = gen;
  options.hierarchical = true;
  options.unfracture = pass.unfracture;
  generate_area_into(pass.source, *s.total_def, s.use.parent, region, pass.style, pass.ctx, options, s.total);
}

// cifHierPaintArrayFunc over the residual tiles of `plane` overlapping
// `window` (mask units), copied cx by cy times at offsets (dx, dy).
struct Copies {
  db::Rect window;
  std::int64_t dx = 0;
  std::int64_t dy = 0;
  std::int64_t cx = 1;
  std::int64_t cy = 1;
};

void replicate(HierPass& pass, const MaskPlane& plane, std::size_t layer, const Copies& c, MaskPlane& out) {
  const bool slivers = pass.style.has_option(style_option::grow_slivers);
  std::vector<db::TileView> tiles;
  plane.plane().enumerate(c.window, tech::TypeMask::of(kMaskSolid), [&](const db::TileView& t) {
    tiles.push_back(t);
    return true;
  });
  const std::int32_t min_width = pass.style.layers[layer].min_width;
  for (const db::TileView& t : tiles) {
    const db::Rect base =
        slivers ? grow_sliver(plane.plane(), t, min_width, pass.source.compat.magic_compat) : t.area;
    for (std::int64_t j = 0; j < c.cy; ++j) {
      for (std::int64_t i = 0; i < c.cx; ++i) {
        const db::Rect area = rect64(base.x0 + i * c.dx, base.y0 + j * c.dy, base.x1 + i * c.dx, base.y1 + j * c.dy);
        paint_piece(out, t, area);
        ++pass.ctx.statistics.flat_tiles;
        ++pass.ctx.statistics.hier_rects;
      }
    }
  }
}

// The low phase's windows (A, B and the core windows A'', B'').
struct LowWindows {
  db::Rect a;
  db::Rect b;
};

db::Rect core_a(const ArrayGeometry& g) {
  const std::int64_t e = 2 * g.r > g.ysep ? (g.ysep >> 1) : g.r;
  return rect64(g.bb.x0, g.bb.y0 + g.ysep - e, g.bb.x0 + g.xsep, g.bb.y0 + g.ysize + e);
}

db::Rect core_b(const ArrayGeometry& g, const db::Rect& a2) {
  const std::int64_t f = 2 * g.r > g.xsep ? (g.xsep >> 1) : g.r;
  const std::int64_t ytop = std::min<std::int64_t>(g.bb.y0 + g.ysep, a2.y0);
  return rect64(g.bb.x0 + g.xsep - f, g.bb.y0, g.bb.x0 + g.xsize + f, ytop);
}

// The low-phase copies of one layer; `w` holds Magic's A and B, which the
// core windows overwrite (Magic reuses the rectangles across layers).
void replicate_low(HierPass& pass, const ArrayGeometry& g, const MaskPlane& plane, std::size_t layer, LowWindows& w,
                   MaskPlane& out) {
  const std::int64_t s = scale_of(pass.style);
  if (g.ny > 1 && g.y_close()) {
    replicate(pass, plane, layer, Copies{scaled(w.a, s), 0, g.ysep * s, 1, g.ny - 1}, out);
  }
  if (!(g.nx > 1 && g.ny > 1 && g.x_close() && g.y_close())) {
    return;
  }
  replicate(pass, plane, layer, Copies{scaled(w.b, s), g.xsep * s, 0, g.nx - 1, 1}, out);
  const db::Rect a2 = core_a(g);
  replicate(pass, plane, layer, Copies{scaled(a2, s), g.xsep * s, g.ysep * s, g.nx - 1, g.ny - 1}, out);
  w.a = a2;
  if (2 * g.r < g.ysep) {
    const db::Rect b2 = core_b(g, a2);
    replicate(pass, plane, layer, Copies{scaled(b2, s), g.xsep * s, g.ysep * s, g.nx - 1, g.ny - 1}, out);
    w.b = b2;
  }
}

void replicate_high(HierPass& pass, const ArrayGeometry& g, const MaskPlane& plane, std::size_t layer,
                    const db::Rect& c, const db::Rect& d, MaskPlane& out) {
  const std::int64_t s = scale_of(pass.style);
  if (g.nx > 1 && g.x_close()) {
    replicate(pass, plane, layer, Copies{scaled(c, s), -g.xsep * s, 0, g.nx - 1, 1}, out);
  }
  if (g.nx > 1 && g.ny > 1 && g.x_close() && g.y_close()) {
    replicate(pass, plane, layer, Copies{scaled(d, s), 0, -g.ysep * s, 1, g.ny - 1}, out);
  }
}

MaskPlane& output_plane(HierPass& pass, LayerPlanes& out, std::size_t layer) {
  if (out.planes[layer] == nullptr) {
    out.planes[layer] = &pass.ctx.pool.acquire();
  }
  return *out.planes[layer];
}

void low_phase(HierPass& pass, const db::CellUse& use, const ArrayGeometry& g, LayerPlanes& out) {
  if (!g.y_close()) {
    return;  // A is not used, so neither is B
  }
  PhaseState s(pass, use);
  LowWindows w{region_a(g), region_b(g)};
  process_region(s, w.a, Phase::low);
  if (g.x_close()) {
    process_region(s, w.b, Phase::low);
  }
  pass.error_cell = use.parent;
  check_and_erase(pass, s.total, s.comp);
  const LowWindows original = w;
  for (std::size_t i = 0; i < pass.style.layers.size(); ++i) {
    if (s.total[i] != nullptr && keeps_residual(pass, i)) {
      replicate_low(pass, g, *s.total[i], i, w, output_plane(pass, out, i));
    }
    if (!pass.source.compat.magic_compat) {
      w = original;  // OFF: the named windows for every layer
    }
  }
}

void high_phase(HierPass& pass, const db::CellUse& use, const ArrayGeometry& g, LayerPlanes& out) {
  if (!g.x_close()) {
    return;  // C is not used, so neither is D
  }
  PhaseState s(pass, use);
  const db::Rect c = region_c(g);
  const db::Rect d = region_d(g);
  process_region(s, c, Phase::high);
  if (g.y_close()) {
    process_region(s, d, Phase::high);
  }
  pass.error_cell = use.parent;
  check_and_erase(pass, s.total, s.comp);
  for (std::size_t i = 0; i < pass.style.layers.size(); ++i) {
    if (s.total[i] != nullptr && keeps_residual(pass, i)) {
      replicate_high(pass, g, *s.total[i], i, c, d, output_plane(pass, out, i));
    }
  }
}

// cifHierArrayFunc.
void array_correction(HierPass& pass, const db::CellUse& use, LayerPlanes& out) {
  const db::ArrayInfo& a = use.array;
  if (a.xlo == a.xhi && a.ylo == a.yhi) {
    return;
  }
  const std::optional<ArrayGeometry> g = array_geometry(use, pass.radius);
  if (!g) {
    return;
  }
  low_phase(pass, use, *g, out);
  if (!pass.ctx.cancelled()) {
    high_phase(pass, use, *g, out);
  }
}

}  // namespace

}  // namespace gen_detail

void generate_arrays(const GenerationSource& source, const db::CellDef& cell, const db::Rect& area,
                     const OutputStyle& style, GenerationContext& ctx, LayerPlanes& into) {
  if (source.technology == nullptr) {
    return;
  }
  gen_detail::HierPass pass(source, style, ctx, cell);
  const LayerClasses classes = classify_layers(style, source.compat);
  pass.classes = &classes;
  const std::uint64_t old_tiles = ctx.statistics.flat_tiles;
  (void)gen_detail::search_elements(cell, area, [&](const gen_detail::ElementRef& e, const db::Rect&) {
    gen_detail::array_correction(pass, *e.use, into);
    return ctx.cancelled() ? gen_detail::Visit::stop : gen_detail::Visit::skip_array;
  });
  ctx.statistics.hier_tiles += ctx.statistics.flat_tiles - old_tiles;  // CIFHierTileOps
}

}  // namespace hefesto::mask
