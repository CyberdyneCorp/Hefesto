// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hints::HintRegion (mask-hints "Mask-hint property data model"): the
// region painted into a db::Plane of one type, whose tiles are Magic's
// maximal horizontal strips.
//
// Ported from Magic VLSI 8.3.694 database/DBprop.c (plane properties),
// database/DBcellsrch.c (DBScaleValue, dbScalePlane, dbTileScaleFunc) and
// cif/CIFgen.c (CIFOP_ANDNOT), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md). The CIF paint and
// erase tables live in mask/plane (mask::cif_paint_table, M5 design D6).
//
// Owner: HINTS (openspec/changes/m3-drc/design.md D2).

#include "hefesto/hints/region.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/db/plane.hpp"
#include "hefesto/mask/plane/plane.hpp"

namespace hefesto::hints {

namespace {

constexpr tech::TileType kHintType = mask::kMaskSolid;

// DBScaleValue: v * n / d rounded down (also for negative values).
db::Coord scale_coord(db::Coord v, std::int64_t num, std::int64_t den) {
  std::int64_t x = std::int64_t{v} * num;
  if (x > 0) {
    x /= den;
  } else if (x < 0) {
    x = ((x + 1) / den) - 1;
  }
  return static_cast<db::Coord>(x);
}

}  // namespace

HintRegion::HintRegion(std::initializer_list<db::Rect> rects) {
  rebuild(std::span<const db::Rect>{rects.begin(), rects.size()});
}

HintRegion::HintRegion(std::span<const db::Rect> rects) {
  rebuild(rects);
}

void HintRegion::paint(const db::Rect& area) {
  rebuild(std::span<const db::Rect>{&area, 1});
}

void HintRegion::paint(std::span<const db::Rect> areas) {
  rebuild(areas);
}

void HintRegion::paint(const HintRegion& other) {
  rebuild(other.strips_);
}

db::Rect HintRegion::bounding_box() const noexcept {
  if (strips_.empty()) {
    return db::Rect{};
  }
  db::Rect box = strips_.front();
  for (const db::Rect& r : strips_) {
    box = db::Rect{std::min(box.x0, r.x0), std::min(box.y0, r.y0), std::max(box.x1, r.x1), std::max(box.y1, r.y1)};
  }
  return box;
}

HintRegion HintRegion::transformed(const db::Transform& transform) const {
  std::vector<db::Rect> out;
  out.reserve(strips_.size());
  for (const db::Rect& r : strips_) {
    out.push_back(db::apply(transform, r));
  }
  return HintRegion{std::span<const db::Rect>{out}};
}

HintRegion HintRegion::translated(db::Coord dx, db::Coord dy) const {
  return transformed(db::translation(dx, dy));
}

HintRegion HintRegion::scaled(std::int64_t num, std::int64_t den) const {
  std::vector<db::Rect> out;
  out.reserve(strips_.size());
  for (const db::Rect& r : strips_) {
    out.push_back(db::Rect{scale_coord(r.x0, num, den), scale_coord(r.y0, num, den), scale_coord(r.x1, num, den),
                           scale_coord(r.y1, num, den)});
  }
  return HintRegion{std::span<const db::Rect>{out}};
}

HintRegion HintRegion::clipped(const db::Rect& clip) const {
  std::vector<db::Rect> out;
  for (const db::Rect& r : strips_) {
    const db::Rect c{std::max(r.x0, clip.x0), std::max(r.y0, clip.y0), std::min(r.x1, clip.x1),
                     std::min(r.y1, clip.y1)};
    if (!c.is_null()) {
      out.push_back(c);
    }
  }
  return HintRegion{std::span<const db::Rect>{out}};
}

HintRegion HintRegion::minus(const HintRegion& other) const {
  HintRegion out = *this;
  out.rebuild({}, other.strips_);
  return out;
}

void HintRegion::rebuild(std::span<const db::Rect> extra, std::span<const db::Rect> erase) {
  auto plane = std::make_shared<db::Plane>();
  const db::PaintRow& paint_row = mask::cif_paint_table();
  const db::PaintRow& erase_row = mask::cif_erase_table();
  const auto apply_row = [&](const db::PaintRow& row) {
    return [&plane, &row](const db::Rect& r) {
      if (!r.is_null()) {
        (void)plane->paint(r, row);  // geometry beyond the usable plane is rejected and contributes nothing
      }
    };
  };
  std::for_each(strips_.begin(), strips_.end(), apply_row(paint_row));
  std::for_each(extra.begin(), extra.end(), apply_row(paint_row));
  std::for_each(erase.begin(), erase.end(), apply_row(erase_row));
  std::vector<db::Rect> strips;
  plane->enumerate(db::whole_plane(), tech::TypeMask::of(kHintType), [&](const db::TileView& t) {
    strips.push_back(t.area);
    return true;
  });
  strips_ = std::move(strips);
  if (strips_.empty()) {
    plane_.reset();
  } else {
    plane_ = std::move(plane);
  }
}

}  // namespace hefesto::hints
