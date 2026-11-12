// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Flattening into the DRC yank cell (drc-engine "Flattening for
// interaction checks"): the paint of a hierarchy painted with the style's
// DRC paint table, and the mask hints of its descendants. Ported from Magic
// 8.3.694 database/DBcellcopy.c (DBCellCopyAllPaint, dbCopyAllPaint,
// DBFlatCopyMaskHints, dbFlatCopyMaskHintsFunc) and database/DBcellsrch.c
// (DBTreeSrTiles, DBTreeSrCells); Magic is (c) the Regents of the
// University of California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).
//
// Differences kept as open items (engine.hpp): a split tile that crosses
// the copy area is painted whole and the part outside the area erased,
// where Magic paints GrClipTriangle's polygon; the off-grid (why 5) report
// of DBCellCheckCopyAllPaint is not produced.
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#include <vector>

#include "hefesto/hints/hints.hpp"
#include "internal.hpp"

namespace hefesto::drc::detail {

namespace {

struct CopyTile {
  db::TileView tile;
  tech::PlaneId plane = 0;
};

// The paint tiles of one cell overlapping `area`, plane by plane (the DRC
// planes carry no paint the basic check reads).
std::vector<CopyTile> paint_tiles(const db::CellDef& def, const db::Rect& area) {
  std::vector<CopyTile> out;
  const tech::TypeMask paint = ~tech::TypeMask::of(tech::kSpace);
  for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count(); ++p) {
    const auto id = static_cast<tech::PlaneId>(p);
    def.plane(id).enumerate(area, paint, [&](const db::TileView& t) {
      if (!t.split || t.type != tech::kSpace) {
        out.push_back(CopyTile{t, id});
      }
      return true;
    });
  }
  return out;
}

// Erases everything of `plane` in `r` (used to trim a whole triangle).
void erase_outside(db::Plane& plane, const db::Rect& bbox, const db::Rect& keep) {
  const db::PaintRow space = identity_paint_row(tech::kSpace);
  const db::Rect parts[] = {
      db::Rect{bbox.x0, bbox.y0, bbox.x1, keep.y0},  // below
      db::Rect{bbox.x0, keep.y1, bbox.x1, bbox.y1},  // above
      db::Rect{bbox.x0, keep.y0, keep.x0, keep.y1},  // left
      db::Rect{keep.x1, keep.y0, bbox.x1, keep.y1},  // right
  };
  for (const db::Rect& part : parts) {
    const db::Rect r = clip(part, bbox);
    if (!r.is_null()) {
      (void)plane.paint(r, space);
    }
  }
}

// dbCopyAllPaint for one tile.
void copy_tile(Context& ctx, const CopyTile& c, const db::Transform& t, const db::Rect& target_area) {
  db::Plane& plane = ctx.yank.plane(c.plane);
  const db::PaintRow& row = ctx.rules->paint_row(c.plane, c.tile.type);
  const db::Rect r = db::apply(t, c.tile.area);
  if (!c.tile.split) {
    const db::Rect clipped = clip(r, target_area);
    if (!clipped.is_null()) {
      (void)plane.paint(clipped, row);
    }
    return;
  }
  const db::TriangleSide side = db::transform_triangle(t, c.tile.diagonal, c.tile.right_side);
  if (!db::overlap(r, target_area)) {
    return;
  }
  bool off_grid = false;
  (void)plane.paint_triangle(r, side.diagonal, side.right_side, row, ctx.input.compat.magic_compat, &off_grid);
  if (off_grid && ctx.flag_off_grid) {
    ctx.report(clip(r, target_area), engine_rule(why::kOffGrid));  // DRCOffGridError
  }
  if (!db::surround(target_area, r)) {
    erase_outside(plane, r, target_area);
  }
}

void yank_cell_paint(Context& ctx, const db::CellDef& def, const db::Rect& area, const db::Transform& t,
                     const db::Rect& target_area) {
  if (ctx.cancel_requested()) {
    return;
  }
  for (const CopyTile& c : paint_tiles(def, area)) {
    copy_tile(ctx, c, t, target_area);
  }
  (void)search_elements(def, area, [&](const Element& e, const db::Rect& child_area) {
    yank_cell_paint(ctx, *e.use->def, child_area, db::compose(t, e.to_parent), target_area);
    return ctx.cancelled ? Visit::stop : Visit::next;
  });
}

void yank_child_hints(hints::HintBatch& batch, const db::CellDef& def, const db::Rect& area,
                      const db::Transform& t) {
  (void)search_elements(def, area, [&](const Element& e, const db::Rect& child_area) {
    const db::Transform to_yank = db::compose(t, e.to_parent);
    batch.add(*e.use->def, to_yank, db::apply(to_yank, child_area));
    yank_child_hints(batch, *e.use->def, child_area, to_yank);
    return Visit::next;
  });
}

}  // namespace

void yank_paint(Context& ctx, const db::CellDef& def, const db::Rect& area, const db::Transform& to_yank) {
  yank_cell_paint(ctx, def, area, to_yank, db::apply(to_yank, area));
}

void yank_hints(Context& ctx, const db::CellDef& def, const db::Rect& area, const db::Transform& to_yank) {
  // One union per hint for the whole yank (dbCopyMaskHintsFunc paints in
  // place), not one rebuild of the yank's region per element.
  hints::HintBatch batch;
  yank_child_hints(batch, def, area, to_yank);
  batch.commit(ctx.yank);
}

}  // namespace hefesto::drc::detail
