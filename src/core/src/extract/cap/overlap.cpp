// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Overlap coupling with shielding (ExtCouple.c extBasicOverlap,
// extAddOverlap, extSubtractOverlap{,2}: :336-584; capability
// extract-capacitance "Overlap coupling with shielding").
//
// Owner: EXTCAP.

#include <cstdint>

#include "cap_context.hpp"

namespace hefesto::extract::cap {

namespace {

struct Above {
  tech::PlaneId plane = 0;
  db::TileView tile;
  bool right = false;
};

/// The overlap area left after the shield planes (Magic keeps it in an
/// int, which overflows past 2^31 square units; 64 bits in both modes).
std::int64_t unshielded_area(const Context& ctx, tech::TileType ta, tech::TileType tb, const db::Rect& clip,
                             std::int64_t area) {
  const auto planes = ctx.tables().overlap_shield_planes.find({ta, tb});
  if (planes == ctx.tables().overlap_shield_planes.end() || planes->second == 0) {
    return area;
  }
  const auto types = ctx.tables().overlap_shield_types.find({ta, tb});
  const tech::TypeMask shield = types == ctx.tables().overlap_shield_types.end() ? tech::TypeMask{} : types->second;
  shield_pieces(ctx, planes->second, shield, clip, [&](const db::TileView& t, const db::Rect& piece) {
    std::int64_t a = piece.width() * piece.height();
    if (t.split) {
      a /= 2;
    }
    if (a > 0) {
      area -= a;
    }
  });
  return area;
}

/// extAddOverlap: `below` (on plane q) is overlapped by `above`.
void add_overlap(const Context& ctx, const Above& above, tech::PlaneId q, const db::TileView& below) {
  const Node rabove = ctx.node(above.plane, above.tile, above.right);
  const Node rbelow = ctx.node(q, below, below.right_side);
  if (!rabove || !rbelow) {
    return;
  }
  db::Rect clip{std::max(below.area.x0, above.tile.area.x0), std::max(below.area.y0, above.tile.area.y0),
                std::min(below.area.x1, above.tile.area.x1), std::min(below.area.y1, above.tile.area.y1)};
  if (ctx.clip()) {
    clip = geoclip(clip, *ctx.clip());
  }
  std::int64_t area = (std::int64_t{clip.y1} - clip.y0) * (std::int64_t{clip.x1} - clip.x0);
  const tech::TileType ta = ctx.residue(side_type(above.tile, above.right), above.plane);
  const tech::TileType tb = ctx.residue(side_type(below, below.right_side), q);
  area = unshielded_area(ctx, ta, tb, clip, area);
  if (area <= 0) {
    return;
  }
  if (ctx.plane_order(above.plane) > ctx.plane_order(q)) {
    ctx.subtract_cap(rabove, ctx.area_cap(ta) * static_cast<double>(area), false);
  }
  if (rabove == rbelow) {
    return;
  }
  ctx.add_coupling(*rabove, *rbelow, ctx.overlap_cap(ta, tb) * static_cast<double>(area));
}

}  // namespace

void overlap_plane(const Context& ctx, tech::PlaneId plane) {
  const CapTables& t = ctx.tables();
  if ((t.overlap_planes & tech::plane_bit(plane)) == 0) {
    return;
  }
  ctx.plane(plane).enumerate(ctx.search_area(), t.overlap_types[plane], [&](const db::TileView& tile) {
    const tech::TileType type = ctx.residue(side_type(tile, tile.right_side), plane);
    const tech::PlaneMask planes = t.overlap_other_planes[type];
    const tech::TypeMask& types = t.overlap_other_types[type];
    db::Rect r = tile.area;
    if (ctx.clip()) {
      r = geoclip(r, *ctx.clip());
    }
    const Above above{plane, tile, tile.right_side};
    for (std::size_t q = tech::kFirstUserPlane; q < ctx.plane_end(); ++q) {
      const auto other = static_cast<tech::PlaneId>(q);
      if (other == plane || (planes & tech::plane_bit(other)) == 0) {
        continue;
      }
      ctx.plane(other).enumerate(r, types, [&](const db::TileView& below) {
        add_overlap(ctx, above, other, below);
        return true;
      });
    }
    return true;
  });
}

}  // namespace hefesto::extract::cap
