// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Operator group 1, grow part (capability mask-output-operators "grow and
// shrink (default, square metric)", "Euclidean grow and shrink",
// "grow-grid", "grow-min"; design D6): CIFOP_GROW, CIFOP_SHRINK,
// CIFOP_GROW_G and CIFOP_GROWMIN.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (cifGrowFunc,
// cifGrowEuclideanFunc, cifGrowGridFunc, cifGrowMinFunc), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include "mask/ops/grow.hpp"

#include <algorithm>
#include <array>

namespace hefesto::mask {

namespace detail {

namespace {

/// The paint or erase table of a grow (grow paints, shrink erases).
struct Painter {
  MaskPlane& plane;
  bool erase = false;
  bool magic_compat = true;

  void rect(const db::Rect& r) const {
    if (erase) {
      detail::erase(plane, r);
    } else {
      detail::paint(plane, r);
    }
  }
  void shape(const db::TileView& t, const db::Rect& r) const {
    if (erase) {
      erase_shape(plane, t, r, magic_compat);
    } else {
      paint_shape(plane, t, r, magic_compat);
    }
  }
};

/// TT_SIDE >> 1 == TT_DIRECTION: the triangle's horizontal leg is on top.
bool leg_on_top(const db::TileView& t) noexcept { return t.right_side == direction(t); }

void grow_split_square(const Painter& p, const db::TileView& t, const db::Rect& a, db::Coord d) {
  db::Rect r;
  r.y0 = a.y0 - d;
  r.y1 = a.y1 + d;
  if (t.right_side) {
    r.x0 = a.x1 - d;
    r.x1 = a.x1 + d;
  } else {
    r.x0 = a.x0 - d;
    r.x1 = a.x0 + d;
  }
  p.rect(r);
  r.x0 = a.x0 - d;
  r.x1 = a.x1 + d;
  const bool top = leg_on_top(t);
  r.y0 = top ? a.y1 - d : a.y0 - d;
  r.y1 = top ? a.y1 + d : a.y0 + d;
  p.rect(r);
  const db::Coord sx = t.right_side ? -d : d;
  const db::Coord sy = top ? -d : d;
  p.shape(t, db::Rect{a.x0 + sx, a.y0 + sy, a.x1 + sx, a.y1 + sy});
}

}  // namespace

void grow_tile_square(MaskPlane& into, const db::TileView& t, db::Coord d, bool erase, bool magic_compat) {
  const Painter p{into, erase, magic_compat};
  const db::Rect a = t.area;
  if (t.split) {
    grow_split_square(p, t, a, d);
    return;
  }
  db::Rect r = a;
  if (r.x0 > db::kPlaneMin) {
    r.x0 -= d;
  }
  if (r.y0 > db::kPlaneMin) {
    r.y0 -= d;
  }
  if (r.x1 < db::kPlaneMax) {
    r.x1 += d;
  }
  if (r.y1 < db::kPlaneMax) {
    r.y1 += d;
  }
  p.rect(r);
}

namespace {

enum GrowDir : std::uint8_t { kNorth = 0x1, kSouth = 0x2, kEast = 0x4, kWest = 0x8 };
enum Stop : std::uint8_t {
  kStopNW = 0x1,
  kStopSW = 0x2,
  kStopNE = 0x4,
  kStopSE = 0x8,
  kStopWN = 0x10,
  kStopWS = 0x20,
  kStopEN = 0x40,
  kStopES = 0x80
};

std::uint8_t finite_dirs(const db::Rect& a) noexcept {
  std::uint8_t dirs = kNorth | kSouth | kEast | kWest;
  if (a.x0 <= db::kPlaneMin) {
    dirs &= ~kWest;
  }
  if (a.y0 <= db::kPlaneMin) {
    dirs &= ~kSouth;
  }
  if (a.x1 >= db::kPlaneMax) {
    dirs &= ~kEast;
  }
  if (a.y1 >= db::kPlaneMax) {
    dirs &= ~kNorth;
  }
  return dirs;
}

/// cifGrowEuclideanFunc's diagonal part: the two leg rectangles and the
/// moved triangle. Returns the reference type for the corner tests.
tech::TileType grow_split_euclidean(const Painter& p, const db::TileView& t, const db::Rect& a, db::Coord d,
                                    std::int32_t limit, std::uint8_t& dirs) {
  dirs &= t.right_side ? ~kWest : ~kEast;
  dirs &= leg_on_top(t) ? ~kSouth : ~kNorth;
  const EuclideanOffsets off =
      euclidean_offsets(d, static_cast<std::int32_t>(a.width()), static_cast<std::int32_t>(a.height()));
  const db::Coord gx = set_value_grid(off.dx, limit);
  const db::Coord gy = set_value_grid(off.dy, limit);
  const bool east = (dirs & kEast) != 0;
  const bool west = (dirs & kWest) != 0;
  const bool south = (dirs & kSouth) != 0;
  const bool north = (dirs & kNorth) != 0;
  db::Rect r = a;
  if (!east) {
    r.x1 = r.x0 + gx;
  }
  if (!west) {
    r.x0 = r.x1 - gx;
  }
  r.y0 = south ? r.y0 : r.y0 - d;
  r.y1 = north ? r.y1 : r.y1 + d;
  p.rect(r);
  r = a;
  r.x1 = east ? r.x1 : r.x1 + d;
  r.x0 = west ? r.x0 : r.x0 - d;
  if (!south) {
    r.y0 = r.y1 - gy;
  }
  if (!north) {
    r.y1 = r.y0 + gy;
  }
  p.rect(r);
  r = a;
  r.y1 = north ? r.y1 - gy : r.y1 + d;
  r.y0 = south ? r.y0 + gy : r.y0 - d;
  r.x1 = east ? r.x1 - gx : r.x1 + d;
  r.x0 = west ? r.x0 + gx : r.x0 - d;
  p.shape(t, r);
  return east ? t.right_type : t.left_type;
}

std::uint8_t corner_stops(const db::Plane& plane, const db::TileView& t, tech::TileType ref) {
  std::uint8_t stops = 0;
  const auto test = [&](const std::vector<db::TileView>& n, bool first, db::Side side, std::uint8_t flag) {
    if (n.empty()) {
      return;
    }
    if (facing_type(first ? n.front() : n.back(), side) == ref) {
      stops |= flag;
    }
  };
  const std::vector<db::TileView> right = side_tiles(plane, t, db::Side::right);
  test(right, false, db::Side::right, kStopNE);
  test(right, true, db::Side::right, kStopSE);
  const std::vector<db::TileView> top = side_tiles(plane, t, db::Side::top);
  test(top, false, db::Side::top, kStopEN);
  test(top, true, db::Side::top, kStopWN);
  const std::vector<db::TileView> left = side_tiles(plane, t, db::Side::left);
  test(left, true, db::Side::left, kStopSW);
  test(left, false, db::Side::left, kStopNW);
  const std::vector<db::TileView> bottom = side_tiles(plane, t, db::Side::bottom);
  test(bottom, true, db::Side::bottom, kStopWS);
  test(bottom, false, db::Side::bottom, kStopES);
  return stops;
}

/// The four edge strips of cifGrowEuclideanFunc, each extended over a
/// corner unless a neighbour of the reference type stops it.
/// One edge strip: the side it grows from, and for each of its two ends the
/// direction that must be grown and the corner stops that prevent the
/// extension. `high` is the end at the larger coordinate.
struct StripSpec {
  std::uint8_t side;
  std::uint8_t high_dir;
  std::uint8_t high_stops;
  std::uint8_t low_dir;
  std::uint8_t low_stops;
};

/// The rectangle of an edge strip before its corner extensions.
db::Rect strip_base(const db::Rect& a, db::Coord d, std::uint8_t side) {
  switch (side) {
    case kNorth:
      return db::Rect{a.x0, a.y1, a.x1, a.y1 + d};
    case kEast:
      return db::Rect{a.x1, a.y0, a.x1 + d, a.y1};
    case kSouth:
      return db::Rect{a.x0, a.y0 - d, a.x1, a.y0};
    default:
      return db::Rect{a.x0 - d, a.y0, a.x0, a.y1};
  }
}

/// The four edge strips of cifGrowEuclideanFunc (in Magic's order N, E, S,
/// W), each extended over a corner unless a neighbour of the reference type
/// stops it.
void edge_strips(const Painter& p, const db::Rect& a, db::Coord d, std::uint8_t dirs, std::uint8_t stops) {
  static constexpr std::array<StripSpec, 4> kStrips{{
      {kNorth, kEast, kStopEN | kStopNE, kWest, kStopWN | kStopNW},
      {kEast, kNorth, kStopEN | kStopNE, kSouth, kStopSE | kStopES},
      {kSouth, kEast, kStopSE | kStopES, kWest, kStopSW | kStopWS},
      {kWest, kNorth, kStopNW | kStopWN, kSouth, kStopSW | kStopWS},
  }};
  const auto extends = [&](std::uint8_t dir, std::uint8_t mask) { return (stops & mask) == 0 && (dirs & dir) != 0; };
  for (const StripSpec& s : kStrips) {
    if ((dirs & s.side) == 0) {
      continue;
    }
    db::Rect r = strip_base(a, d, s.side);
    const bool vertical = s.side == kEast || s.side == kWest;
    db::Coord& hi = vertical ? r.y1 : r.x1;
    db::Coord& lo = vertical ? r.y0 : r.x0;
    hi += extends(s.high_dir, s.high_stops) ? d : 0;
    lo -= extends(s.low_dir, s.low_stops) ? d : 0;
    p.rect(r);
  }
}

}  // namespace

void grow_tile_euclidean(MaskPlane& into, const db::Plane& from, const db::TileView& t, db::Coord d, bool erase,
                         std::int32_t grid_limit, bool magic_compat, std::int32_t scale) {
  const Painter p{into, erase, magic_compat};
  std::uint8_t dirs = finite_dirs(t.area);
  const db::Rect a = scale_finite(t.area, scale);
  tech::TileType ref = t.type;
  if (t.split) {
    ref = grow_split_euclidean(p, t, a, d, grid_limit, dirs);
  } else {
    p.rect(a);
  }
  edge_strips(p, a, d, dirs, corner_stops(from, t, ref));
}

}  // namespace detail

namespace {

/// Grow (`shrink` false) or shrink by `d`: the square or Euclidean per-tile
/// construction over the solid (grow) or space (shrink) tiles of `out`,
/// into a new plane that replaces it.
diag::Status grow_or_shrink(const OpArgs& args, OperatorContext& ctx, MaskPlane& out, bool shrink) {
  const db::Coord d = detail::value32(args.op, 0);
  ScratchPlane next{*ctx.pool};
  if (shrink) {
    detail::paint(*next, db::whole_plane());
  }
  const bool euclid = detail::euclidean(args);
  const db::Plane& from = out.plane();
  from.enumerate(db::whole_plane(), shrink ? detail::space_mask() : detail::solid_mask(), [&](const db::TileView& t) {
    if (euclid) {
      detail::grow_tile_euclidean(*next, from, t, d, shrink, detail::grid_limit_of(args), ctx.compat.magic_compat);
    } else {
      detail::grow_tile_square(*next, t, d, shrink, ctx.compat.magic_compat);
    }
    detail::count_tile(ctx);
    return true;
  });
  out.swap(*next);
  return diag::Done{};
}

/// cifGrowMinFunc's free-side tests: is there a neighbour on the bottom
/// (top) whose facing type equals the tile's bottom (top) type?
bool side_matches(const std::vector<db::TileView>& n, tech::TileType own, bool bottom_side) {
  return std::any_of(n.begin(), n.end(), [&](const db::TileView& v) {
    return (bottom_side ? detail::top_type(v) : detail::bottom_type(v)) == own;
  });
}

/// The first rectangle of grow-min: the tile widened (and heightened) to g.
db::Rect grow_min_rect(const db::TileView& t, std::int32_t g, const std::vector<db::TileView>& below,
                       const std::vector<db::TileView>& above, std::int32_t limit) {
  db::Rect a = t.area;
  const auto width = static_cast<std::int32_t>(a.width());
  if (width >= g) {
    return a;
  }
  const std::int32_t loc = (g - width) / 2;
  a.x0 -= loc;
  a.x1 += loc;
  const auto height = static_cast<std::int32_t>(a.height());
  if (height >= g) {
    return a;
  }
  const bool free_bot = !side_matches(below, detail::bottom_type(t), true);
  const bool free_top = !side_matches(above, detail::top_type(t), false);
  if (free_top && !free_bot) {
    a.y0 -= grow_min_height(g, width);
  } else if (!free_top && free_bot) {
    a.y1 += grow_min_height(g, width);
  } else {
    const std::int32_t k = (g - height) / 2;
    a.y0 -= k;
    a.y1 += k;
  }
  detail::set_min_box_grid(a, g, limit);
  return a;
}

/// The x-range shared by bottom neighbour `p` and top neighbour `q` within
/// the tile (cifGrowMinFunc's overlap cases); nullopt when neither case.
std::optional<std::pair<db::Coord, db::Coord>> shared_span(const db::TileView& t, const db::TileView& p,
                                                           const db::TileView& q) {
  if (q.area.x0 <= p.area.x1 && q.area.x0 >= p.area.x0) {
    return std::pair{std::max(q.area.x0, t.area.x0), std::min(p.area.x1, t.area.x1)};
  }
  if (q.area.x1 >= p.area.x0 && q.area.x1 <= p.area.x1) {
    return std::pair{std::max(p.area.x0, t.area.x0), std::min(q.area.x1, t.area.x1)};
  }
  return std::nullopt;
}

void grow_min_columns(MaskPlane& into, const db::TileView& t, std::int32_t g, const std::vector<db::TileView>& below,
                      const std::vector<db::TileView>& above, std::int32_t limit) {
  for (const db::TileView& p : below) {
    const tech::TileType type = detail::top_type(p);
    for (auto q = above.rbegin(); q != above.rend(); ++q) {
      if (detail::bottom_type(*q) != type) {
        continue;
      }
      const auto span = shared_span(t, p, *q);
      if (!span) {
        continue;
      }
      const db::Rect a{span->first, t.area.y0, span->second, t.area.y1};
      const auto width = static_cast<std::int32_t>(a.width());
      const auto height = static_cast<std::int32_t>(a.height());
      if (width >= g && height >= g) {
        continue;
      }
      db::Rect b = a;
      if (width < g) {
        b.x0 = a.x0 - (g - width) / 2;
        b.x1 = a.x1 + (g - width) / 2;
      }
      if (height < g) {
        b.y0 = a.y0 - (g - height) / 2;
        b.y1 = a.y1 + (g - height) / 2;
      }
      detail::set_min_box_grid(b, g, limit);
      detail::paint(into, b);
    }
  }
}

}  // namespace

diag::Status op_grow(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  return grow_or_shrink(args, ctx, out, false);
}

diag::Status op_shrink(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  return grow_or_shrink(args, ctx, out, true);
}

diag::Status op_grow_grid(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  const std::int32_t g = detail::value32(args.op, 0);
  if (g <= 0) {
    return diag::Done{};
  }
  ScratchPlane next{*ctx.pool};
  out.plane().enumerate(db::whole_plane(), detail::solid_mask(), [&](const db::TileView& t) {
    detail::paint(*next, q_mask_1_grow_grid(t.area, g, ctx.compat));
    detail::count_tile(ctx);
    return true;
  });
  out.swap(*next);
  return diag::Done{};
}

diag::Status op_grow_min(const OpArgs& args, OperatorContext& ctx, MaskPlane& out) {
  const std::int32_t g = detail::value32(args.op, 0);
  const std::int32_t limit = detail::grid_limit_of(args);
  ScratchPlane next{*ctx.pool};
  const db::Plane& from = out.plane();
  from.enumerate(db::whole_plane(), detail::solid_mask(), [&](const db::TileView& t) {
    const std::vector<db::TileView> below = detail::side_tiles(from, t, db::Side::bottom);
    const std::vector<db::TileView> above = detail::side_tiles(from, t, db::Side::top);
    detail::paint(*next, grow_min_rect(t, g, below, above, limit));
    grow_min_columns(*next, t, g, below, above, limit);
    detail::count_tile(ctx);
    return true;
  });
  out.swap(*next);
  return diag::Done{};
}

}  // namespace hefesto::mask
