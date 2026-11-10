// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Layout rescaling for `scalegrid` (capability tech-loading-and-scaling,
// "scalegrid command" steps 6 and 8) and the paint check of "Effect on
// existing layout".
//
// Ported from Magic VLSI 8.3.694 database/DBcellsrch.c (DBScaleValue,
// DBScalePoint, DBScaleEverything, dbScaleCell, dbScalePlane,
// dbTileScaleFunc) and utils/tech.c (checkForPaintFunc), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/tech/commands.hpp"

namespace hefesto::tech {

namespace {

constexpr std::int64_t kScaleLimit = std::int64_t{db::kInfinity} - 2;

// DBScaleValue: scales `v` in place; returns whether the result is not a
// multiple of d (Magic's "inexact" test, applied to the scaled value).
bool scale_in_place(db::Coord& v, std::int64_t n, std::int64_t d) {
  const std::int64_t scaled = scale_value(v, n, d);
  v = static_cast<db::Coord>(scaled);
  return scaled % d != 0;
}

bool scale_point(db::Point& p, std::int64_t n, std::int64_t d) {
  const bool x = scale_in_place(p.x, n, d);
  const bool y = scale_in_place(p.y, n, d);
  return x || y;
}

bool scale_rect(db::Rect& r, std::int64_t n, std::int64_t d) {
  db::Point ll{r.x0, r.y0};
  db::Point ur{r.x1, r.y1};
  const bool a = scale_point(ll, n, d);
  const bool b = scale_point(ur, n, d);
  r = db::Rect{ll.x, ll.y, ur.x, ur.y};
  return a || b;
}

db::PaintRow paint_row(const Technology& tech, PlaneId plane, TileType type) {
  db::PaintRow row{};
  for (std::size_t h = 0; h < kMaxTileTypes; ++h) {
    row[h] = tech.paint_result(plane, static_cast<TileType>(h), type);
  }
  return row;
}

// dbScalePlane: copies every non-space tile at the scaled size.
bool scale_plane(db::Plane& plane, const Technology& tech, PlaneId id, std::int64_t n, std::int64_t d) {
  std::vector<db::TileView> tiles;
  plane.enumerate(db::whole_plane(), ~TypeMask::of(kSpace), [&](const db::TileView& tile) {
    tiles.push_back(tile);
    return true;
  });
  bool inexact = false;
  db::Plane scaled;
  for (db::TileView tile : tiles) {
    inexact = scale_rect(tile.area, n, d) || inexact;
    if (tile.area.x1 == tile.area.x0 || tile.area.y1 == tile.area.y0) {
      continue;  // scaled out of existence
    }
    const db::PaintRow row = paint_row(tech, id, tile.type);
    if (tile.split) {
      (void)scaled.paint_triangle(tile.area, tile.diagonal, tile.right_side, row);
    } else {
      (void)scaled.paint(tile.area, row);
    }
  }
  plane = std::move(scaled);
  return inexact;
}

void scale_uses(db::CellDef& def, std::int64_t n, std::int64_t d) {
  for (const auto& use : def.uses()) {
    (void)scale_rect(use->bbox, n, d);
    (void)scale_in_place(use->transform.c, n, d);
    (void)scale_in_place(use->transform.f, n, d);
    (void)scale_in_place(use->array.xsep, n, d);
    (void)scale_in_place(use->array.ysep, n, d);
  }
}

void scale_labels(db::CellDef& def, std::int64_t n, std::int64_t d) {
  for (auto& label : def.labels()) {
    (void)scale_rect(label.rect, n, d);
    if (label.font >= 0) {
      (void)scale_point(label.offset, n, d);
      (void)scale_in_place(label.size, n, d);
    }
  }
}

// layout-properties "Geometric properties follow the geometry": dimension
// values scale pairwise as points (a trailing odd value is kept), plane
// properties rectangle by rectangle; other types are unchanged.
void scale_properties(db::CellDef& def, std::int64_t n, std::int64_t d) {
  std::vector<std::pair<std::string, db::PropertyValue>> scaled;
  def.properties().for_each([&](std::string_view name, const db::PropertyValue& value) {
    if (const auto* dims = std::get_if<db::DimensionList>(&value)) {
      db::DimensionList out = *dims;
      for (std::size_t i = 0; i + 1 < out.values.size(); i += 2) {
        (void)scale_in_place(out.values[i], n, d);
        (void)scale_in_place(out.values[i + 1], n, d);
      }
      scaled.emplace_back(std::string{name}, std::move(out));
    } else if (const auto* plane = std::get_if<db::PlaneValue>(&value)) {
      std::vector<db::Rect> rects = plane->strips();
      for (db::Rect& r : rects) {
        (void)scale_rect(r, n, d);
      }
      scaled.emplace_back(std::string{name}, db::PlaneValue{std::span<const db::Rect>{rects}});
    }
  });
  for (auto& [name, value] : scaled) {
    def.properties().put(name, std::move(value));
  }
}

bool scale_cell(db::CellDef& def, const Technology& tech, std::int64_t n, std::int64_t d) {
  bool inexact = false;
  if (def.flags().available) {
    def.flags().child_bboxes_changed = true;
    scale_uses(def, n, d);
    for (std::size_t p = 1; p < def.plane_count(); ++p) {
      if (scale_plane(def.plane(static_cast<PlaneId>(p)), tech, static_cast<PlaneId>(p), n, d)) {
        def.flags().modified = true;
        def.flags().needs_new_timestamp = true;
        inexact = true;
      }
    }
    scale_labels(def, n, d);
    scale_properties(def, n, d);
  }
  db::Rect bbox = def.bbox();
  db::Rect extended = def.extended_bbox();
  (void)scale_rect(bbox, n, d);
  (void)scale_rect(extended, n, d);
  def.set_bboxes(bbox, extended);
  return inexact;
}

}  // namespace

std::int64_t scale_value(std::int64_t v, std::int64_t n, std::int64_t d) noexcept {
  if (v >= kScaleLimit || v <= -kScaleLimit) {
    return v;  // an infinity marker
  }
  const std::int64_t product = v * n;
  if (product > 0) {
    return product / d;
  }
  if (product < 0) {
    return ((product + 1) / d) - 1;
  }
  return 0;
}

bool scale_layout(editor::EditorState& editor, std::int64_t n, std::int64_t d) {
  bool inexact = false;
  editor.cells.for_each([&](db::CellDef& def) { inexact = scale_cell(def, *editor.technology, n, d) || inexact; });
  if (editor.box) {
    db::Rect box = *editor.box;
    (void)scale_rect(box, n, d);
    editor.box = box;
  }
  editor.windows.scale_all([n, d](db::Point p) {
    return db::Point{static_cast<db::Coord>(scale_value(p.x, n, d)), static_cast<db::Coord>(scale_value(p.y, n, d))};
  });
  return inexact;
}

bool layout_has_paint(const db::CellLibrary& cells, std::size_t plane_limit) {
  bool found = false;
  cells.for_each([&](db::CellDef& def) {
    if (found || def.flags().internal) {
      return;
    }
    const std::size_t limit = std::min(plane_limit, def.plane_count());
    for (std::size_t p = kPlaneMHint; p < limit && !found; ++p) {
      found = !def.plane(static_cast<PlaneId>(p)).empty();
    }
  });
  return found;
}

}  // namespace hefesto::tech
