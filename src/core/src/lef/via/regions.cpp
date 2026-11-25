// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Contact region identification for DEF write (defCountVias and the
// boundary search of defCountViaFunc, lef/defWrite.c:1661-1858; capability
// def-write "Contact region identification", Q-LEFDEF-111). Magic is (c)
// the Regents of the University of California, BSD-style licence (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: LEFVIA (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include "hefesto/lef/via/regions.hpp"

#include <algorithm>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::lef {

namespace {

/// The type of a tile's side facing a neighbour (TiGetLeftType, ...).
tech::TileType side_type(const db::TileView& t, db::Side side) {
  if (!t.split) {
    return t.type;
  }
  const bool nw_se = t.diagonal == db::Diagonal::nw_se;  // TT_DIRECTION set
  switch (side) {
    case db::Side::left:
      return t.left_type;
    case db::Side::right:
      return t.right_type;
    case db::Side::top:
      return nw_se ? t.right_type : t.left_type;
    case db::Side::bottom:
      return nw_se ? t.left_type : t.right_type;
  }
  return t.type;
}

/// One plane's search: the contact type `ctype` a tile stands for, and
/// whether a type is (or stacks) it.
class PlaneSearch {
 public:
  PlaneSearch(const db::CellDef& cell, const tech::Technology& tech, tech::PlaneId plane)
      : plane_(cell.plane(plane)), tech_(tech), plane_id_(plane) {}

  [[nodiscard]] const db::Plane& plane() const { return plane_; }
  [[nodiscard]] tech::PlaneId id() const { return plane_id_; }

  /// The types searched on this plane: the user contacts whose home plane
  /// it is, and every stacked contact.
  [[nodiscard]] tech::TypeMask mask() const {
    tech::TypeMask mask;
    for (const tech::ContactInfo& c : tech_.contacts()) {
      if (c.stacked || tech_.home_plane(c.type) == plane_id_) {
        mask.set(c.type);
      }
    }
    return mask;
  }

  /// The contact a tile of `type` stands for: itself, or a stacked type's
  /// first residue (type order) homed here; nullopt when none is.
  [[nodiscard]] std::optional<tech::TileType> contact_of(tech::TileType type) const {
    const tech::ContactInfo* info = tech_.contact(type);
    if (info == nullptr || !info->stacked) {
      return type;
    }
    for (std::size_t t = tech::kFirstUserType; t < tech_.user_type_end(); ++t) {
      const auto r = static_cast<tech::TileType>(t);
      if (info->residues.test(r) && tech_.home_plane(r) == plane_id_) {
        return r;
      }
    }
    return std::nullopt;
  }

  /// `type` is `ctype`, or a stacked type with `ctype` among its residues.
  [[nodiscard]] bool matches(tech::TileType type, tech::TileType ctype) const {
    if (type == ctype) {
      return true;
    }
    const tech::ContactInfo* info = tech_.contact(type);
    return info != nullptr && info->stacked && info->residues.test(ctype);
  }

  /// Some neighbour across `side` of `t` faces it with `ctype`.
  [[nodiscard]] bool neighbour_matches(const db::TileView& t, db::Side side, db::Side facing,
                                       tech::TileType ctype) const {
    const bool vertical = side == db::Side::left || side == db::Side::right;
    plane_.neighbors(t, side, vertical ? t.area.y0 : t.area.x0, vertical ? t.area.y1 : t.area.x1, scratch_);
    return std::any_of(scratch_.begin(), scratch_.end(),
                       [&](const db::TileView& n) { return matches(side_type(n, facing), ctype); });
  }

  /// The one neighbour at the corner stitch RT (side top) or TR (right).
  [[nodiscard]] std::optional<db::TileView> corner(const db::TileView& t, db::Side side) const {
    const bool top = side == db::Side::top;
    const db::Coord hi = top ? t.area.x1 : t.area.y1;
    plane_.neighbors(t, side, hi - 1, hi, scratch_);
    if (scratch_.empty()) {
      return std::nullopt;
    }
    return scratch_.front();
  }

  /// `n`, or either side of a split `n`, matches `ctype`.
  [[nodiscard]] bool any_side_matches(const db::TileView& n, tech::TileType ctype) const {
    if (!n.split) {
      return matches(n.type, ctype);
    }
    return matches(n.left_type, ctype) || matches(n.right_type, ctype);
  }

  /// The neighbours of `t` across every side that match `ctype`.
  [[nodiscard]] std::vector<db::TileView> matching_neighbours(const db::TileView& t, tech::TileType ctype) const {
    std::vector<db::TileView> out;
    for (const db::Side side : {db::Side::left, db::Side::right, db::Side::bottom, db::Side::top}) {
      const bool vertical = side == db::Side::left || side == db::Side::right;
      plane_.neighbors(t, side, vertical ? t.area.y0 : t.area.x0, vertical ? t.area.y1 : t.area.x1, scratch_);
      for (const db::TileView& n : scratch_) {
        if (any_side_matches(n, ctype)) {
          out.push_back(n);
        }
      }
    }
    return out;
  }

 private:
  const db::Plane& plane_;
  const tech::Technology& tech_;
  tech::PlaneId plane_id_;
  mutable std::vector<db::TileView> scratch_;
};

db::Rect bound(const db::Rect& a, const db::Rect& b) {
  return db::Rect{std::min(a.x0, b.x0), std::min(a.y0, b.y0), std::max(a.x1, b.x1), std::max(a.y1, b.y1)};
}

/// Grows `r` along the corner stitches from `t` (RT upward, TR rightward)
/// while each tile matches `ctype` (defWrite.c:1810-1855).
db::Rect grow_chain(const PlaneSearch& s, const db::TileView& t, db::Side side, tech::TileType ctype, db::Rect r) {
  const db::Side facing = side == db::Side::top ? db::Side::bottom : db::Side::left;
  for (auto tp = s.corner(t, side); tp && s.matches(side_type(*tp, facing), ctype); tp = s.corner(*tp, side)) {
    r = bound(r, tp->area);
  }
  return r;
}

/// The union of `rects` within the band [y0, y1) as maximal x spans,
/// appended to `out`.
void band_strips(const std::vector<db::Rect>& rects, db::Coord y0, db::Coord y1, std::vector<db::Rect>& out) {
  std::vector<std::pair<db::Coord, db::Coord>> spans;
  for (const db::Rect& r : rects) {
    if (r.y0 <= y0 && r.y1 >= y1) {
      spans.emplace_back(r.x0, r.x1);
    }
  }
  std::sort(spans.begin(), spans.end());
  const std::size_t first = out.size();
  for (const auto& [x0, x1] : spans) {
    if (out.size() > first && out.back().x1 >= x0) {
      out.back().x1 = std::max(out.back().x1, x1);
    } else {
      out.push_back(db::Rect{x0, y0, x1, y1});
    }
  }
}

/// The maximal horizontal strips of the union of `rects`, merged vertically
/// when their x extents are equal (Q-LEFDEF-111 OFF).
std::vector<db::Rect> strips(const std::vector<db::Rect>& rects) {
  std::set<db::Coord> ys;
  for (const db::Rect& r : rects) {
    ys.insert(r.y0);
    ys.insert(r.y1);
  }
  std::vector<db::Rect> out;
  for (auto it = ys.begin(); it != ys.end() && std::next(it) != ys.end(); ++it) {
    band_strips(rects, *it, *std::next(it), out);
  }
  std::vector<db::Rect> merged;
  for (const db::Rect& r : out) {
    auto above = std::find_if(merged.begin(), merged.end(),
                              [&](const db::Rect& m) { return m.y1 == r.y0 && m.x0 == r.x0 && m.x1 == r.x1; });
    if (above != merged.end()) {
      above->y1 = r.y1;
    } else {
      merged.push_back(r);
    }
  }
  return merged;
}

/// The tiles of the chain of `seed`: every tile reached through neighbours
/// that match `ctype`.
std::vector<db::Rect> chain_tiles(const PlaneSearch& s, const db::TileView& seed, tech::TileType ctype,
                                  std::set<std::tuple<db::Coord, db::Coord, db::Coord, db::Coord>>& seen) {
  std::vector<db::Rect> rects;
  std::vector<db::TileView> todo{seed};
  while (!todo.empty()) {
    const db::TileView t = todo.back();
    todo.pop_back();
    if (!seen.insert({t.area.x0, t.area.y0, t.area.x1, t.area.y1}).second) {
      continue;
    }
    rects.push_back(t.area);
    for (const db::TileView& n : s.matching_neighbours(t, ctype)) {
      todo.push_back(n);
    }
  }
  return rects;
}

/// The regions of one plane, in Magic's search order (defCountViaFunc).
void plane_regions(const PlaneSearch& s, const compat::CompatSettings& compat, std::vector<ContactRegion>& regions) {
  std::set<std::tuple<db::Coord, db::Coord, db::Coord, db::Coord>> seen;
  std::vector<db::TileView> seeds;
  (void)s.plane().enumerate(db::whole_plane(), s.mask(), [&](const db::TileView& t) {
    seeds.push_back(t);
    return true;
  });
  for (const db::TileView& t : seeds) {
    const auto ctype = s.contact_of(t.type);
    if (!ctype || s.neighbour_matches(t, db::Side::left, db::Side::right, *ctype) ||
        s.neighbour_matches(t, db::Side::bottom, db::Side::top, *ctype)) {
      continue;  // only the bottom-most, left-most tile of a region
    }
    if (compat.magic_compat) {
      const db::Rect up = grow_chain(s, t, db::Side::top, *ctype, t.area);
      regions.push_back(ContactRegion{*ctype, s.id(), grow_chain(s, t, db::Side::right, *ctype, up)});
      continue;
    }
    for (const db::Rect& r : strips(chain_tiles(s, t, *ctype, seen))) {
      regions.push_back(ContactRegion{*ctype, s.id(), r});
    }
  }
}

}  // namespace

std::vector<ContactRegion> contact_regions(const db::CellDef& cell, const tech::Technology& tech,
                                           const compat::CompatSettings& compat) {
  std::vector<ContactRegion> regions;
  for (std::size_t p = tech::kFirstUserPlane; p < std::min(cell.plane_count(), tech.plane_count()); ++p) {
    plane_regions(PlaneSearch{cell, tech, static_cast<tech::PlaneId>(p)}, compat, regions);
  }
  return regions;
}

}  // namespace hefesto::lef
