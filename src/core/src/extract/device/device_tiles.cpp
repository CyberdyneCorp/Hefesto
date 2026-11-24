// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tile walks of the device extractor: the perimeter enumeration
// (extEnumTilePerim), the region walk (ExtFindNeighbors) and the device
// regions (ExtFindRegions with extTransFirst / extTransEach). Capability
// extract-devices "Device regions"; openspec/changes/m7-extraction/design.md
// D6, D10.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtPerim.c:82-210,
// ExtNghbors.c:74-385, ExtRegion.c:142-224 and ExtBasic.c:4908-5050,
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTDEV.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "device_internal.hpp"
#include "hefesto/db/cell.hpp"

namespace hefesto::extract {

using device_detail::bottom_type;
using device_detail::Boundary;
using device_detail::key_of;
using device_detail::left_type;
using device_detail::PlaneTile;
using device_detail::right_type;
using device_detail::split_direction;
using device_detail::TileSet;
using device_detail::top_type;

namespace device_detail {

db::TileView side_view(const db::TileView& t, bool right_side) {
  db::TileView v = t;
  if (t.split) {
    v.right_side = right_side;
    v.type = right_side ? t.right_type : t.left_type;
  }
  return v;
}

// extEnumTilePerim's `sides`: left or right by the side, bottom or top by
// side and direction.
SkippedSides skipped_sides(const db::TileView& t) {
  SkippedSides s;
  if (!t.split) {
    return s;
  }
  (t.right_side ? s.left : s.right) = true;
  ((t.right_side == split_direction(t)) ? s.bottom : s.top) = true;
  return s;
}

namespace {

std::vector<db::TileView> neighbors_of(const db::Plane& plane, const db::TileView& t, db::Side side, bool reverse) {
  std::vector<db::TileView> out;
  const bool vertical = side == db::Side::left || side == db::Side::right;
  plane.neighbors(t, side, vertical ? t.area.y0 : t.area.x0, vertical ? t.area.y1 : t.area.x1, out);
  if (reverse) {
    std::reverse(out.begin(), out.end());
  }
  return out;
}

BoundaryDir diagonal_dir(const db::TileView& t) {
  if (split_direction(t)) {
    return t.right_side ? BoundaryDir::ne : BoundaryDir::sw;
  }
  return t.right_side ? BoundaryDir::se : BoundaryDir::nw;
}

std::int64_t diagonal_boundary(const db::TileView& tile, tech::PlaneId pnum, const tech::TypeMask& mask,
                               const std::function<void(const Boundary&)>& fn) {
  const tech::TileType other = tile.right_side ? tile.left_type : tile.right_type;
  std::int64_t correction = 0;
  if (mask.test(other)) {
    const double w = static_cast<double>(tile.area.width());
    const double h = static_cast<double>(tile.area.height());
    correction = static_cast<std::int64_t>(std::sqrt((w * w) + (h * h)));
  }
  fn(Boundary{tile, tile, tile.area, diagonal_dir(tile), pnum});
  return correction;
}

void side_boundaries(const db::Plane& plane, tech::PlaneId pnum, const db::TileView& tile, db::Side side,
                     const tech::TypeMask& mask, const std::function<void(const Boundary&)>& fn) {
  const bool reverse = side == db::Side::top || side == db::Side::right;
  const db::Rect& a = tile.area;
  for (const db::TileView& out : neighbors_of(plane, tile, side, reverse)) {
    Boundary b{tile, out, {}, BoundaryDir::left, pnum};
    tech::TileType facing = tech::kSpace;
    switch (side) {
      case db::Side::top:
        facing = bottom_type(out);
        b.dir = BoundaryDir::top;
        b.segment = {std::max(a.x0, out.area.x0), a.y1, std::min(a.x1, out.area.x1), a.y1};
        break;
      case db::Side::bottom:
        facing = top_type(out);
        b.dir = BoundaryDir::bottom;
        b.segment = {std::max(a.x0, out.area.x0), a.y0, std::min(a.x1, out.area.x1), a.y0};
        break;
      case db::Side::left:
        facing = right_type(out);
        b.dir = BoundaryDir::left;
        b.segment = {a.x0, std::max(a.y0, out.area.y0), a.x0, std::min(a.y1, out.area.y1)};
        break;
      case db::Side::right:
        facing = left_type(out);
        b.dir = BoundaryDir::right;
        b.segment = {a.x1, std::max(a.y0, out.area.y0), a.x1, std::min(a.y1, out.area.y1)};
        break;
    }
    if (mask.test(facing)) {
      fn(b);
    }
  }
}

}  // namespace

std::int64_t enum_tile_perim(const db::Plane& plane, tech::PlaneId pnum, const db::TileView& tile,
                             const tech::TypeMask& mask, const std::function<void(const Boundary&)>& fn) {
  std::int64_t correction = 0;
  if (tile.split) {
    correction = diagonal_boundary(tile, pnum, mask, fn);
  }
  const SkippedSides skip = skipped_sides(tile);
  if (!skip.top) {
    side_boundaries(plane, pnum, tile, db::Side::top, mask, fn);
  }
  if (!skip.bottom) {
    side_boundaries(plane, pnum, tile, db::Side::bottom, mask, fn);
  }
  if (!skip.left) {
    side_boundaries(plane, pnum, tile, db::Side::left, mask, fn);
  }
  if (!skip.right) {
    side_boundaries(plane, pnum, tile, db::Side::right, mask, fn);
  }
  return correction;
}

namespace {

using Accept = std::function<bool(tech::PlaneId, const db::TileView&)>;

struct Walk {
  const db::CellDef& cell;
  const tech::Technology& tech;
  const std::vector<tech::TypeMask>& conn;
  TileSet& visited;
  const Accept& accept;
  std::vector<PlaneTile> stack;

  void offer(tech::PlaneId plane, const db::TileView& view, tech::TileType facing, const tech::TypeMask& mask) {
    if (mask.test(facing) && !visited.contains(key_of(plane, view)) && accept(plane, view)) {
      stack.emplace_back(plane, view);
    }
  }

  // The neighbours of one side: for a split neighbour, the side facing us.
  void side(const PlaneTile& at, db::Side side, const tech::TypeMask& mask) {
    const bool reverse = side == db::Side::top || side == db::Side::right;
    for (const db::TileView& tp : neighbors_of(cell.plane(at.first), at.second, side, reverse)) {
      switch (side) {
        case db::Side::top:
          offer(at.first, side_view(tp, !split_direction(tp)), bottom_type(tp), mask);
          break;
        case db::Side::left:
          offer(at.first, side_view(tp, true), right_type(tp), mask);
          break;
        case db::Side::bottom:
          offer(at.first, side_view(tp, split_direction(tp)), top_type(tp), mask);
          break;
        case db::Side::right:
          offer(at.first, side_view(tp, false), left_type(tp), mask);
          break;
      }
    }
  }

  void sides(const PlaneTile& at, const tech::TypeMask& mask) {
    const db::TileView& t = at.second;
    const bool rs = t.split && t.right_side;
    const bool dir = t.split && split_direction(t);
    if (!t.split || !(rs ^ dir)) {
      side(at, db::Side::top, mask);
    }
    if (!t.split || !rs) {
      side(at, db::Side::left, mask);
    }
    if (!t.split || (rs ^ dir)) {
      side(at, db::Side::bottom, mask);
    }
    if (!t.split || rs) {
      side(at, db::Side::right, mask);
    }
  }

  // A contact's images on its other planes (the tile at its lower-left).
  void contact_images(const PlaneTile& at, const tech::TypeMask& mask) {
    const tech::PlaneMask planes = tech.contact_planes(at.second.type) & ~tech::plane_bit(at.first);
    for (std::size_t p = tech::kFirstUserPlane; p < cell.plane_count(); ++p) {
      if ((planes & tech::plane_bit(static_cast<tech::PlaneId>(p))) == 0) {
        continue;
      }
      const auto pnum = static_cast<tech::PlaneId>(p);
      const db::TileView tp = cell.plane(pnum).tile_at({at.second.area.x0, at.second.area.y0});
      if (tp.split) {
        offer(pnum, side_view(tp, true), tp.right_type, mask);
        offer(pnum, side_view(tp, false), tp.left_type, mask);
      } else {
        offer(pnum, tp, tp.type, mask);
      }
    }
  }

  // Types connected on other planes that are not contacts (a well and its
  // diffusion): everything overlapping or sharing a side within the tile
  // grown by 1 (extNbrPushFunc).
  void cross_planes(const PlaneTile& at, const tech::TypeMask& mask) {
    const tech::PlaneMask planes = tech.cross_planes(at.second.type);
    const db::Rect area = at.second.area;
    const db::Rect bigger = db::expand(area, 1);
    for (std::size_t p = tech::kFirstUserPlane; p < cell.plane_count(); ++p) {
      const auto pnum = static_cast<tech::PlaneId>(p);
      if (pnum == at.first || (planes & tech::plane_bit(pnum)) == 0) {
        continue;
      }
      (void)cell.plane(pnum).enumerate(bigger, mask, [&](const db::TileView& tp) {
        if (!db::overlap(tp.area, area)) {
          const db::Rect c{std::max(tp.area.x0, area.x0), std::max(tp.area.y0, area.y0),
                           std::min(tp.area.x1, area.x1), std::min(tp.area.y1, area.y1)};
          if (c.x0 >= c.x1 && c.y0 >= c.y1) {
            return true;
          }
        }
        offer(pnum, tp, tp.type, mask);
        return true;
      });
    }
  }
};

}  // namespace

std::vector<PlaneTile> walk_region(const db::CellDef& cell, const tech::Technology& technology,
                                   const std::vector<tech::TypeMask>& conn, const PlaneTile& start,
                                   TileSet& visited, const Accept& accept) {
  Walk w{cell, technology, conn, visited, accept, {start}};
  std::vector<PlaneTile> out;
  while (!w.stack.empty()) {
    const PlaneTile at = w.stack.back();
    w.stack.pop_back();
    if (!visited.insert(key_of(at.first, at.second)).second) {
      continue;
    }
    const tech::TileType type = at.second.type;
    static const tech::TypeMask kNone;
    const tech::TypeMask& mask = type < conn.size() ? conn[type] : kNone;
    w.sides(at, mask);
    out.push_back(at);
    if (technology.is_contact(type)) {
      w.contact_images(at, mask);
    }
    w.cross_planes(at, mask);
  }
  return out;
}

}  // namespace device_detail

namespace {

using device_detail::walk_region;

// extTransEach's reference point and area (extSetNodeNum).
void add_region_tile(DeviceRegionTiles& r, tech::PlaneId pnum, const db::TileView& t, bool& first) {
  std::int64_t area = t.area.width() * t.area.height();
  if (t.split) {
    area /= 2;
  }
  DeviceRegion& reg = r.region;
  if (first || pnum < reg.plane) {
    reg.area = 0;
    reg.type = t.type;
    reg.plane = pnum;
    reg.reference = {t.area.x0, t.area.y0};
    first = false;
  } else if (pnum == reg.plane) {
    if (t.area.x0 < reg.reference.x) {
      reg.reference = {t.area.x0, t.area.y0};
      reg.type = t.type;
    } else if (t.area.x0 == reg.reference.x && t.area.y0 < reg.reference.y) {
      reg.reference.y = t.area.y0;
      reg.type = t.type;
    }
  }
  if (pnum == reg.plane) {
    reg.area += area;
  }
}

}  // namespace

std::vector<DeviceRegionTiles> find_device_regions(const db::CellDef& cell, const tech::Technology& technology,
                                                   const ExtStyle& style) {
  std::vector<DeviceRegionTiles> found;
  device_detail::TileSet visited;
  const auto accept = [](tech::PlaneId, const db::TileView&) { return true; };
  for (std::size_t p = tech::kFirstUserPlane; p < cell.plane_count(); ++p) {
    const auto pnum = static_cast<tech::PlaneId>(p);
    (void)cell.plane(pnum).enumerate(db::whole_plane(), style.device_types, [&](const db::TileView& t) {
      if (visited.contains(key_of(pnum, t))) {
        return true;
      }
      DeviceRegionTiles r;
      r.first = t;
      r.first_plane = pnum;
      r.tiles = walk_region(cell, technology, style.device_conn, {pnum, t}, visited, accept);
      bool first = true;
      for (const auto& [tp, tv] : r.tiles) {
        add_region_tile(r, tp, tv, first);
      }
      found.push_back(std::move(r));
      return true;
    });
  }
  std::reverse(found.begin(), found.end());  // prepended to the region list
  return found;
}

DeviceNodeLookup client_slot_lookup(const db::CellDef& cell) {
  return [&cell](tech::PlaneId plane, const db::TileView& t) -> std::optional<std::size_t> {
    const std::uintptr_t v = cell.plane(plane).client(t);
    if (v == db::kClientDefault) {
      return std::nullopt;
    }
    return v;  // std::uintptr_t is std::size_t on every target
  };
}

DeviceNodeLookup region_slot_lookup(const RegionFinder& finder, const db::CellDef& cell, std::size_t list_size) {
  return [&finder, &cell, list_size](tech::PlaneId plane, const db::TileView& t) {
    return finder.node_of(cell.plane(plane), t, list_size);
  };
}

}  // namespace hefesto::extract
