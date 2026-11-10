// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Interaction checks (drc-engine "Interaction check over an area",
// "Interaction area", "Copying subcell errors"). Ported from Magic 8.3.694
// drc/DRCsubcell.c (DRCInteractionCheck, DRCFindInteractions,
// drcSubcellFunc, drcFindOtherCells, drcSubCheckPaint, drcSubCopyFunc,
// drcSubCopyErrors); Magic is (c) the Regents of the University of
// California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).
//
// Replicated quirks: the sibling pass of DRCFindInteractions expands
// GeoNullRect ({0,0,0,0}) when a use has no sibling nearby, so a square
// round the origin of the cell joins the interaction area when it lies in
// the use's halo area; the subtree-paint pass counts a nested use with paint
// as a second source of its own parent's paint.
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#include <vector>

#include "hefesto/editor/editor_state.hpp"
#include "internal.hpp"

namespace hefesto::drc::detail {

namespace {

const tech::TypeMask& paint_types() {
  static const tech::TypeMask mask = ~tech::TypeMask::of(tech::kSpace);
  return mask;
}

// drcIncludeArea over the paint planes: the bounding box of every
// non-space tile overlapping `area`, at full tile extent.
db::Rect paint_bbox(const db::CellDef& def, const db::Rect& area) {
  db::Rect out = kNullRect;
  for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count(); ++p) {
    def.plane(static_cast<tech::PlaneId>(p)).enumerate(area, paint_types(), [&](const db::TileView& t) {
      out = db::include(out, t.area);
      return true;
    });
  }
  return out;
}

bool has_paint(const db::CellDef& def, const db::Rect& area, const tech::TypeMask& types) {
  for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count(); ++p) {
    const bool stopped = !def.plane(static_cast<tech::PlaneId>(p)).enumerate(area, types, [](const db::TileView&) {
      return false;
    });
    if (stopped) {
      return true;
    }
  }
  return false;
}

// DBTreeSrTiles(..., DBAllButSpaceAndDRCBits, drcAlwaysOne): paint anywhere
// in the subtree of `def` under `area`.
bool subtree_has_paint(const db::CellDef& def, const db::Rect& area) {
  if (has_paint(def, area, paint_types())) {
    return true;
  }
  return !search_elements(def, area, [](const Element& e, const db::Rect& child_area) {
    return subtree_has_paint(*e.use->def, child_area) ? Visit::stop : Visit::next;
  });
}

struct LinkedUse {
  const db::CellUse* use = nullptr;
  db::Rect area;
};

// drcSubcellFunc for every use near `search`; returns whether one was found.
bool subcell_pass(const db::CellDef& def, const db::Rect& search, std::int32_t radius, db::Rect& total,
                  std::vector<LinkedUse>& linked) {
  const std::vector<const db::CellUse*> uses = uses_overlapping(def, search);
  for (const db::CellUse* u : uses) {
    const db::Rect halo_area = clip(db::expand(u->bbox, radius), search);
    db::Rect found = paint_bbox(def, halo_area);
    if (!db::surround(found, halo_area)) {
      linked.insert(linked.begin(), LinkedUse{u, halo_area});  // dsa_dlu is a stack
    }
    if (found.is_null()) {
      continue;
    }
    found = clip(db::expand(found, radius), halo_area);
    total = db::include(total, found);
  }
  return !uses.empty();
}

// The sibling pass over the linked uses (drcFindOtherCells).
void sibling_pass(const db::CellDef& def, std::int32_t radius, const std::vector<LinkedUse>& linked,
                  db::Rect& total) {
  for (const LinkedUse& l : linked) {
    db::Rect others = kNullRect;
    for (const db::CellUse* u : uses_overlapping(def, l.area)) {
      if (u != l.use) {
        others = db::include(others, u->bbox);
      }
    }
    others = clip(db::expand(others, radius), l.area);  // replicated: GeoNullRect expands too
    total = db::include(total, others);
  }
}

// The second pass: at least two sources with paint in `area`.
bool two_sources(const db::CellDef& def, const db::Rect& area) {
  int sources = has_paint(def, area, paint_types()) ? 1 : 0;
  std::function<bool(const db::CellDef&, const db::Rect&)> walk = [&](const db::CellDef& d, const db::Rect& a) {
    return search_elements(d, a, [&](const Element& e, const db::Rect& child_area) {
      if (subtree_has_paint(*e.use->def, child_area) && ++sources >= 2) {
        return Visit::stop;
      }
      return walk(*e.use->def, child_area) ? Visit::next : Visit::stop;
    });
  };
  (void)walk(def, area);
  return sources >= 2;
}

}  // namespace

int find_interactions(Context& ctx, const db::CellDef& def, const db::Rect& area, std::int32_t radius,
                      db::Rect& interaction) {
  (void)ctx;
  const db::Rect search = db::expand(area, radius);
  db::Rect total = kNullRect;
  std::vector<LinkedUse> linked;
  if (!subcell_pass(def, search, radius, total, linked)) {
    return -1;
  }
  sibling_pass(def, radius, linked, total);
  if (total.is_null() || !two_sources(def, total)) {
    return 0;
  }
  interaction = clip(total, area);
  return interaction.is_null() ? 0 : 1;
}

void copy_child_errors(Context& ctx, const db::CellDef& def, const db::Rect& strip) {
  tech::TypeMask errors;
  errors.set(tech::kErrorP);
  errors.set(tech::kErrorS);
  errors.set(tech::kErrorPS);
  const Rule& rule = engine_rule(why::kInSubcell);
  (void)search_elements(def, strip, [&](const Element& e, const db::Rect& child_area) {
    e.use->def->plane(tech::kPlaneDrcError).enumerate(child_area, errors, [&](const db::TileView& t) {
      ctx.report(db::apply(e.to_parent, clip(t.area, child_area)), rule);
      return !ctx.cancel_requested();
    });
    return ctx.cancelled ? Visit::stop : Visit::next;
  });
}

namespace {

// One non-interaction strip: basic check, copied child errors, array checks.
void plain_strip(Context& ctx, const db::CellDef& def, db::Rect& strip) {
  if (!ctx.basic(def, db::expand(strip, ctx.halo), strip)) {
    return;
  }
  const db::Rect search = strip;  // scx_area is a copy; dCD_clip is `strip` itself
  copy_child_errors(ctx, def, search);
  array_checks(ctx, def, search, strip);
}

// The strips of `erase_clip` outside `inter`, in Magic's order, through the
// one subArea rectangle Magic reuses (array checks may narrow it).
void strips_outside(Context& ctx, const db::CellDef& def, const db::Rect& erase_clip, const db::Rect& inter) {
  db::Rect sub = erase_clip;
  if (inter.y1 < erase_clip.y1) {  // above
    sub.y0 = inter.y1;
    plain_strip(ctx, def, sub);
  }
  if (inter.y0 > erase_clip.y0) {  // below
    sub.y0 = erase_clip.y0;
    sub.y1 = inter.y0;
    plain_strip(ctx, def, sub);
  }
  sub.y1 = inter.y1;
  sub.y0 = inter.y0;
  if (inter.x1 < erase_clip.x1) {  // right
    sub.x0 = inter.x1;
    plain_strip(ctx, def, sub);
  }
  if (inter.x0 > erase_clip.x0) {  // left
    sub.x1 = inter.x0;
    sub.x0 = erase_clip.x0;
    plain_strip(ctx, def, sub);
  }
}

// The flattened check of the interaction area.
void flattened_check(Context& ctx, const db::CellDef& def, const db::Rect& erase_box, db::Rect inter) {
  inter = clip(inter, db::expand(erase_box, ctx.halo));
  ++ctx.stats->interactions;
  const db::Rect yank_area = db::expand(inter, ctx.halo);
  ctx.clear_yank();
  yank_paint(ctx, def, yank_area, db::kIdentity);
  yank_hints(ctx, def, yank_area, db::kIdentity);
  if (!ctx.basic(ctx.yank, yank_area, inter)) {
    return;
  }
  exact_overlap_check(ctx, def, inter, engine_rule(why::kSubcellOverlap));
}

void check_square(Context& ctx, const db::CellDef& def, const db::Rect& clip_area, const db::Rect& erase_box) {
  db::Rect inter;
  const tech::TileType saved = ctx.error_type;
  if (find_interactions(ctx, def, clip_area, ctx.halo, inter) <= 0) {
    db::Rect sub = clip(erase_box, clip_area);
    if (sub.is_null()) {
      return;
    }
    ctx.error_type = tech::kErrorP;  // the basic check is always error_p
    plain_strip(ctx, def, sub);
    ctx.error_type = saved;
    return;
  }
  ctx.error_type = tech::kErrorP;
  strips_outside(ctx, def, clip(erase_box, clip_area), inter);
  ctx.error_type = saved;
  flattened_check(ctx, def, erase_box, inter);
}

}  // namespace

void interaction_check(Context& ctx, const db::CellDef& def, const db::Rect& area, const db::Rect& erase_box) {
  const db::Coord s = ctx.step;
  const std::uint64_t old_tiles = ctx.stats->tiles;
  db::Coord x = (area.x0 / s) * s - s / 2;
  if (x > area.x0) {
    x -= s;
  }
  db::Coord y = (area.y0 / s) * s - s / 2;
  if (y > area.y0) {
    y -= s;
  }
  for (db::Coord sx = x; sx < area.x1 && !ctx.cancelled; sx += s) {
    for (db::Coord sy = y; sy < area.y1 && !ctx.cancel_requested(); sy += s) {
      const db::Rect square{sx, sy, sx + s, sy + s};
      check_square(ctx, def, clip(square, area), erase_box);
    }
  }
  ctx.stats->interaction_tiles += ctx.stats->tiles - old_tiles;
}

}  // namespace hefesto::drc::detail
