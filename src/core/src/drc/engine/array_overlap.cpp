// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Array checks and the exact-overlap check (drc-engine "Array checks",
// "Exact-overlap check"). Ported from Magic 8.3.694 drc/DRCarray.c
// (drcArrayFunc, drcArrayYankFunc, drcArrayOverlapFunc) and
// drc/DRCsubcell.c (drcExactOverlapTile, drcExactOverlapCheck) over
// database/DBcellsrch.c (DBTreeSrTiles, DBTreeSrUniqueTiles); Magic is (c)
// the Regents of the University of California, distributed under its
// permissive license (THIRD_PARTY_NOTICES.md).
//
// Replicated quirks: drcArrayFunc clips the caller's dCD_clip rectangle in
// place (`*arg->dCD_clip = *area` copies a rectangle onto itself), so each
// checked array region narrows the area the later regions, uses and strips
// of the same square see; region C reuses region A's y extent after A's
// clip, region D region B's x extent.
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#include <algorithm>
#include <vector>

#include "internal.hpp"

namespace hefesto::drc::detail {

namespace {

// DBResidueMask: a stacked type's component contacts, a contact's residues.
tech::TypeMask residue_mask(const tech::Technology& tech, tech::TileType type) {
  const tech::ContactInfo* c = tech.contact(type);
  if (c == nullptr) {
    return {};
  }
  if (c->stacked) {
    tech::TypeMask m = tech::TypeMask::of(c->component_a);
    m.set(c->component_b);
    return m;
  }
  return c->residues;
}

struct OverlapMasks {
  tech::TypeMask type;     // typeMask: the type (and the stacked types containing it)
  tech::TypeMask inverse;  // invMask
};

OverlapMasks overlap_masks(const tech::Technology& tech, tech::TileType type) {
  OverlapMasks m{tech::TypeMask::of(type), {}};
  if (type < tech.user_type_end()) {
    for (std::size_t t = tech.user_type_end(); t < tech.type_count(); ++t) {
      const auto st = static_cast<tech::TileType>(t);
      if (residue_mask(tech, st).test(type)) {
        m.type.set(st);
      }
    }
    m.inverse = ~m.type;
  } else {
    m.inverse = ~(m.type | residue_mask(tech, type));
  }
  return m;
}

bool plane_has(const db::Plane& plane, const db::Rect& area, const tech::TypeMask& types) {
  return !plane.enumerate(area, types, [](const db::TileView&) { return false; });
}

// The tiles touching `tile`'s bottom, right, top and left sides in the
// stitch order of drcExactOverlapTile (LB..TR, TR..LB, RT..BL, BL..RT).
std::vector<db::TileView> neighbors(const db::Plane& plane, const db::Rect& r) {
  std::vector<db::TileView> out;
  const tech::TypeMask all = tech::TypeMask::all();
  auto side = [&](const db::Rect& strip, auto order) {
    std::vector<db::TileView> found;
    plane.enumerate(strip, all, [&](const db::TileView& t) {
      if (!found.empty() && found.back().area == t.area) {
        return true;  // the second side of a split tile
      }
      found.push_back(t);
      if (t.split) {
        found.back().type = t.left_type;  // TiGetType of a split tile
      }
      return true;
    });
    std::sort(found.begin(), found.end(), order);
    out.insert(out.end(), found.begin(), found.end());
  };
  side(db::Rect{r.x0, r.y0 - 1, r.x1, r.y0}, [](const db::TileView& a, const db::TileView& b) { return a.area.x0 < b.area.x0; });
  side(db::Rect{r.x1, r.y0, r.x1 + 1, r.y1}, [](const db::TileView& a, const db::TileView& b) { return a.area.y1 > b.area.y1; });
  side(db::Rect{r.x0, r.y1, r.x1, r.y1 + 1}, [](const db::TileView& a, const db::TileView& b) { return a.area.x1 > b.area.x1; });
  side(db::Rect{r.x0 - 1, r.y0, r.x0, r.y1}, [](const db::TileView& a, const db::TileView& b) { return a.area.y0 < b.area.y0; });
  return out;
}

// drcExactOverlapCheck over `area` of one yank plane.
void report_tiles(Context& ctx, const db::Plane& plane, const db::Rect& area, const tech::TypeMask& types,
                  const db::Rect& clip_area, const Rule& rule) {
  plane.enumerate(area, types, [&](const db::TileView& t) {
    const db::Rect r = clip(t.area, clip_area);
    if (!r.is_null()) {
      ctx.report(r, rule);
    }
    return true;
  });
}

}  // namespace

void exact_overlap_tile(Context& ctx, const db::CellDef& owner, tech::PlaneId plane, const db::TileView& tile,
                        const db::Transform& to_yank, const db::Rect& clip_area, const Rule& rule) {
  const db::Rect r2 = db::apply(to_yank, tile.area);
  const db::Rect rex = clip(db::expand(r2, 1), clip_area);
  const OverlapMasks m = overlap_masks(ctx.tech(), tile.type);
  std::vector<db::TileView> around;
  bool have_neighbors = false;
  for (std::size_t p = tech::kFirstUserPlane; p < ctx.yank.plane_count(); ++p) {
    const db::Plane& yp = ctx.yank.plane(static_cast<tech::PlaneId>(p));
    if (!plane_has(yp, rex, m.type)) {
      continue;
    }
    report_tiles(ctx, yp, r2, m.inverse, clip_area, rule);
    if (!have_neighbors) {
      around = neighbors(owner.plane(plane), tile.area);
      have_neighbors = true;
    }
    for (const db::TileView& nb : around) {
      if (!m.inverse.test(nb.type)) {
        continue;
      }
      const db::Rect r3 = clip(db::apply(to_yank, nb.area), rex);
      if (!r3.is_null()) {
        report_tiles(ctx, yp, r3, m.type, clip_area, rule);
      }
    }
  }
}

namespace {

// DBTreeSrTiles (`unique` false) or DBTreeSrUniqueTiles (`unique` true) of
// the exact-overlap types under `area` of `def`, calling drcExactOverlapTile.
// The exact-overlap types searched on plane `id`: all of them, or with
// `unique` (DBTreeSrUniqueTiles) only those whose home plane is `id`.
tech::TypeMask overlap_mask(const Context& ctx, tech::PlaneId id, bool unique) {
  const tech::TypeMask& types = ctx.rules->exact_overlap_types();
  if (!unique) {
    return types;
  }
  tech::TypeMask home;
  for (std::size_t t = 1; t < ctx.tech().type_count(); ++t) {
    const auto tt = static_cast<tech::TileType>(t);
    if (types.test(tt) && ctx.tech().home_plane(tt) == id) {
      home.set(tt);
    }
  }
  return home;
}

void overlap_tree(Context& ctx, const db::CellDef& def, const db::Rect& area, const db::Transform& to_yank,
                  const db::Rect& clip_area, const Rule& rule, bool unique) {
  for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count() && !ctx.cancel_requested(); ++p) {
    const auto id = static_cast<tech::PlaneId>(p);
    const tech::TypeMask mask = overlap_mask(ctx, id, unique);
    if (mask.empty()) {
      continue;
    }
    std::vector<db::TileView> tiles;
    def.plane(id).enumerate(area, mask, [&](const db::TileView& t) {
      tiles.push_back(t);
      return true;
    });
    for (const db::TileView& t : tiles) {
      exact_overlap_tile(ctx, def, id, t, to_yank, clip_area, rule);
    }
  }
  (void)search_elements(def, area, [&](const Element& e, const db::Rect& child_area) {
    overlap_tree(ctx, *e.use->def, child_area, db::compose(to_yank, e.to_parent), clip_area, rule, unique);
    return ctx.cancelled ? Visit::stop : Visit::next;
  });
}

// One array region (A, B, C or D): yank, basic check, exact overlaps.
void array_region(Context& ctx, const db::CellUse& use, db::Rect& error_area, db::Rect& clip_area) {
  error_area = clip(error_area, clip_area);
  if (error_area.is_null() || ctx.cancelled) {
    return;
  }
  const db::Rect yank_area = db::expand(error_area, ctx.halo);
  ctx.clear_yank();
  array_elements(use, yank_area, [&](const Element& e) {
    const db::Rect child_area = db::apply(db::invert(e.to_parent), yank_area);
    yank_paint(ctx, *use.def, child_area, e.to_parent);
    yank_hints(ctx, *use.def, child_area, e.to_parent);
  });
  if (!ctx.basic(ctx.yank, yank_area, error_area)) {
    return;
  }
  clip_area = clip(clip_area, yank_area);  // replicated: dCD_clip narrowed in place
  const Rule& rule = engine_rule(why::kArrayOverlap);
  array_elements(use, error_area, [&](const Element& e) {
    overlap_tree(ctx, *use.def, db::apply(db::invert(e.to_parent), clip_area), e.to_parent, clip_area, rule, false);
  });
}

struct ArrayGeometry {
  db::Coord xsep = 0;
  db::Coord ysep = 0;
  db::Coord xsize = 0;
  db::Coord ysize = 0;
};

ArrayGeometry array_geometry(const db::CellUse& use, std::int32_t halo) {
  const db::ArrayInfo& a = use.array;
  const db::Rect& child = use.def->bbox();
  const db::Rect pitch{0, 0, a.xlo == a.xhi ? halo + child.x1 - child.x0 : a.xsep,
                       a.ylo == a.yhi ? halo + child.y1 - child.y0 : a.ysep};
  const db::Rect sep = db::apply(use.transform, pitch);
  const db::Rect size = db::apply(use.transform, child);
  return ArrayGeometry{sep.x1 - sep.x0, sep.y1 - sep.y0, size.x1 - size.x0, size.y1 - size.y0};
}

void array_use(Context& ctx, const db::CellUse& use, db::Rect& clip_area) {
  const std::uint64_t old_tiles = ctx.stats->tiles;
  const std::int32_t h = ctx.halo;
  const ArrayGeometry g = array_geometry(use, h);
  const db::Rect& bb = use.bbox;
  db::Rect err;
  if (g.ysep < g.ysize + h) {
    err = db::Rect{bb.x0, bb.y0 + g.ysep - h, bb.x0 + g.xsize + h, bb.y0 + g.ysize + h};  // A
    array_region(ctx, use, err, clip_area);
    err.x1 = bb.x1;  // C keeps A's clipped y extent
    err.x0 = bb.x1 - h;
    array_region(ctx, use, err, clip_area);
  }
  if (g.xsep < g.xsize + h) {
    err = db::Rect{bb.x0 + g.xsep - h, bb.y0, bb.x0 + g.xsize + h, bb.y0 + g.ysep - h};  // B
    array_region(ctx, use, err, clip_area);
    err.y1 = bb.y1;  // D keeps B's clipped x extent
    err.y0 = bb.y1 - h;
    array_region(ctx, use, err, clip_area);
  }
  ctx.stats->array_tiles += ctx.stats->tiles - old_tiles;
}

}  // namespace

void array_checks(Context& ctx, const db::CellDef& def, const db::Rect& search, db::Rect& clip_area) {
  for (const db::CellUse* u : uses_overlapping(def, search)) {
    if (ctx.cancel_requested()) {
      return;
    }
    const IndexRange r = array_overlap(*u, search);
    if (!u->array.is_array() || r.xlo > r.xhi || r.ylo > r.yhi) {
      continue;  // drcArrayFunc returns at once (or is never called)
    }
    array_use(ctx, *u, clip_area);
  }
}

void exact_overlap_check(Context& ctx, const db::CellDef& def, const db::Rect& area, const Rule& rule) {
  if (ctx.rules->exact_overlap_types().empty()) {
    return;
  }
  overlap_tree(ctx, def, area, db::kIdentity, area, rule, true);
}

}  // namespace hefesto::drc::detail
