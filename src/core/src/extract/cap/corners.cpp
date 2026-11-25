// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Corner fringe coupling (ExtCouple.c extCornerFrac,
// extSubtractCornerOverlap{,2}, extCornerOverlap, extAddCornerCouple
// :1838-2447; capability extract-capacitance "Corner fringe coupling").
// The shield fractions follow Q-EXTRACT-17's resolution, as the edge
// fringe does (reported for confirmation).
//
// Owner: EXTCAP.

#include <algorithm>
#include <cstdint>

#include "edge_pass.hpp"
#include "hefesto/extract/cap/compat_branches.hpp"
#include "util/cr_atan.hpp"

namespace hefesto::extract::cap {

namespace {

struct Corner {
  db::Point at;
  int quadrant = kNorthEast;
  int sign = 1;
  db::Rect area;  // the h x h quadrant
};

bool empty_rect(const db::Rect& r) noexcept {
  return r.x1 <= r.x0 || r.y1 <= r.y0;
}

/// extCornerOverlap's per-rule shield fractions (extSubtractCornerOverlap).
struct CornerShield {
  double coup = 0.0;
  double sub = 0.0;
};

CornerShield corner_shield(const EdgeState& es, const Corner& c, tech::TileType ta, tech::TileType tb,
                           const EdgeRule& e, const db::Rect& clip) {
  const Context& ctx = *es.ctx;
  CornerShield out;
  const auto it = ctx.tables().side_overlap_shield_planes.find({ta, tb});
  if (it == ctx.tables().side_overlap_shield_planes.end() || it->second == 0) {
    return out;
  }
  const tech::TileType stored = stored_type(es.bp.inside);
  const tech::TileType inside = shield_inside_type(ctx.compat(), stored, ctx.residue(stored, es.plane));
  shield_pieces(ctx, it->second, e.far_types, clip, [&](const db::TileView& /*t*/, const db::Rect& r) {
    if (empty_rect(r)) {
      return;
    }
    const double m0 = static_cast<double>(ctx.mult(inside, tech::kSpace));
    if (m0 > 0) {
      out.sub += corner_frac(r, c.at, c.quadrant, m0);
    }
    const double mb = static_cast<double>(ctx.mult(inside, tb));
    if (mb > 0) {
      out.coup += corner_frac(r, c.at, c.quadrant, mb);
    }
  });
  return out;
}

/// A coupling tile in a corner quadrant, as extCornerOverlap sees it.
struct CornerTarget {
  tech::PlaneId plane = 0;
  db::Rect clip;  // the tile clipped to the quadrant
  tech::TileType ta = 0;
  tech::TileType tb = 0;
  Node rtp;
  Node rbp;
};

struct CornerSums {
  double cap = 0.0;
  double subfrac = 0.0;
};

/// The corner coupling and the shielded substrate fraction over the rules
/// of the edge (shield fractions per Q-EXTRACT-17).
CornerSums corner_rules(const EdgeState& es, const Corner& c, const CornerTarget& t) {
  const Context& ctx = *es.ctx;
  const double k = ctx.options().corner_factor;
  const bool distinct = t.rtp != t.rbp;
  const bool per_rule = shield_counted_per_rule(ctx.compat());
  const bool needs_distinct = shield_needs_distinct_nodes(ctx.compat());
  CornerSums sums;
  bool counted = false;
  for (const EdgeRule& e : *es.overlap_list) {
    const double mult = static_cast<double>(ctx.mult(t.ta, t.tb));
    if ((e.planes & tech::plane_bit(t.plane)) == 0 || !e.near_types.test(t.tb) || mult <= 0) {
      continue;
    }
    const CornerShield sh = corner_shield(es, c, t.ta, t.tb, e, t.clip);
    const double cfrac = std::max(corner_frac(t.clip, c.at, c.quadrant, mult) - sh.coup, 0.0);
    if (distinct) {
      sums.cap += e.cap * (k / mult) * cfrac;
    }
    if (per_rule ? (distinct || !needs_distinct) : !counted) {
      sums.subfrac += sh.sub;
      counted = true;
    }
  }
  return sums;
}

/// The corner substrate fringe the tile blocks (convex: removed; concave:
/// one of the two edges' subtractions given back).
void corner_substrate(const EdgeState& es, const Corner& c, const CornerTarget& t, double subfrac) {
  const Context& ctx = *es.ctx;
  if (!t.rbp || ctx.plane_order(es.plane) <= ctx.plane_order(t.plane)) {
    return;
  }
  const double m0 = static_cast<double>(ctx.mult(t.ta, tech::kSpace));
  if (m0 <= 0) {
    return;
  }
  const double sfrac = std::max(corner_frac(t.clip, c.at, c.quadrant, m0) - subfrac, 0.0);
  const double k = ctx.options().corner_factor;
  const double subcap = static_cast<double>(c.sign) * ctx.perim_cap(t.ta, tech::kSpace) * (k / m0) * sfrac;
  ctx.subtract_cap(t.rbp, subcap, true);
}

/// extCornerOverlap: tile `u` (plane q) in the quadrant of corner `c`.
void corner_overlap(const EdgeState& es, const Corner& c, tech::PlaneId q, const db::TileView& u) {
  const Context& ctx = *es.ctx;
  const tech::TileType raw = side_type(u, u.right_side);
  CornerTarget t;
  t.clip = geoclip(u.area, c.area);
  if (raw == tech::kSpace || empty_rect(t.clip)) {
    return;
  }
  t.plane = q;
  t.ta = ctx.residue(stored_type(es.bp.inside), es.plane);
  t.tb = ctx.residue(raw, q);
  t.rtp = ctx.node(q, u, u.right_side);
  t.rbp = inside_node(es);
  const CornerSums sums = corner_rules(es, c, t);
  corner_substrate(es, c, t, sums.subfrac);
  if (sums.cap == 0 || t.rtp == t.rbp || !t.rtp || !t.rbp) {
    return;
  }
  ctx.add_coupling(*t.rtp, *t.rbp, sums.cap * static_cast<double>(c.sign));
}

db::Rect quadrant_area(int quadrant, db::Point at, std::int32_t h) {
  switch (quadrant) {
    case kNorthEast:
      return {at.x, at.y, at.x + h, at.y + h};
    case kNorthWest:
      return {at.x - h, at.y, at.x, at.y + h};
    case kSouthEast:
      return {at.x, at.y - h, at.x + h, at.y};
    default:
      break;
  }
  return {at.x - h, at.y - h, at.x, at.y};
}

/// The quadrant of a corner: diagonally outward for a convex corner, on the
/// segment side of the endpoint for a concave one.
int quadrant_of(bool top, bool right_end, bool convex) {
  const bool east = convex ? right_end : !right_end;
  if (top) {
    return east ? kNorthEast : kNorthWest;
  }
  return east ? kSouthEast : kSouthWest;
}

/// Classifies one endpoint; false when it is no corner.
bool classify(const EdgeState& es, bool right_end, db::Coord cx, db::Coord cy, Corner& c) {
  const Context& ctx = *es.ctx;
  const Boundary& b = es.bp;
  const bool top = b.dir == Dir::top;
  const CornerPair pair = corner_pair(ctx.plane(es.plane), b.inside, b.outside, top, right_end, cx, cy);
  if (pair.cont.split || pair.diag.split) {
    return false;
  }
  const Node rbp = inside_node(es);
  const bool contsame = ctx.node(es.plane, pair.cont) == rbp;
  const bool diagsame = ctx.node(es.plane, pair.diag) == rbp;
  if (contsame != diagsame) {
    return false;  // a straight continuation, or shapes meeting at a point
  }
  const bool convex = !contsame;
  const bool open = convex ? exact_space(pair.cont) && exact_space(pair.diag) : exact_space(b.outside);
  if (!open) {
    return false;
  }
  c.sign = convex ? 1 : -1;
  c.quadrant = quadrant_of(top, right_end, convex);
  return true;
}

/// The tiles of the edge's sideoverlap targets in a corner's quadrant.
void search_quadrant(const EdgeState& es, const Corner& c, tech::PlaneMask planes, const tech::TypeMask& mask) {
  const Context& ctx = *es.ctx;
  for (std::size_t q = tech::kFirstUserPlane; q < ctx.plane_end(); ++q) {
    const auto plane = static_cast<tech::PlaneId>(q);
    if ((planes & tech::plane_bit(plane)) == 0) {
      continue;
    }
    ctx.plane(plane).enumerate(c.area, mask, [&](const db::TileView& u) {
      corner_overlap(es, c, plane, u);
      return true;
    });
  }
}

}  // namespace

double corner_frac(const db::Rect& r, db::Point corner, int quadrant, double mult) {
  const bool east = quadrant == kNorthEast || quadrant == kSouthEast;
  const bool north = quadrant == kNorthEast || quadrant == kNorthWest;
  std::int64_t dxnear = east ? std::int64_t{r.x0} - corner.x : std::int64_t{corner.x} - r.x1;
  const std::int64_t dxfar = east ? std::int64_t{r.x1} - corner.x : std::int64_t{corner.x} - r.x0;
  std::int64_t dynear = north ? std::int64_t{r.y0} - corner.y : std::int64_t{corner.y} - r.y1;
  const std::int64_t dyfar = north ? std::int64_t{r.y1} - corner.y : std::int64_t{corner.y} - r.y0;
  if (dxnear < 0) {
    dxnear = 0;
  }
  if (dynear < 0) {
    dynear = 0;
  }
  const double fx = 0.6366 * (util::cr_atan(mult * static_cast<double>(dxfar)) -
                              util::cr_atan(mult * static_cast<double>(dxnear)));
  const double fy = 0.6366 * (util::cr_atan(mult * static_cast<double>(dyfar)) -
                              util::cr_atan(mult * static_cast<double>(dynear)));
  return fx * fy;
}

void add_corner_couple(EdgeState& es) {
  const Context& ctx = *es.ctx;
  const CapTables& t = ctx.tables();
  const auto planes = t.side_overlap_other_planes.find({es.tin, es.tout});
  if (planes == t.side_overlap_other_planes.end() || planes->second == 0 || es.overlap_list == nullptr) {
    return;
  }
  const Boundary& b = es.bp;
  if (b.inside.split || b.outside.split) {
    return;
  }
  const auto types = t.side_overlap_other_types.find({es.tin, es.tout});
  const tech::TypeMask mask = types == t.side_overlap_other_types.end() ? tech::TypeMask{} : types->second;
  const db::Coord cy = b.dir == Dir::top ? b.segment.y1 : b.segment.y0;
  for (int end = 0; end < 2; ++end) {
    const bool right_end = end == 1;
    const db::Coord cx = right_end ? b.segment.x1 : b.segment.x0;
    Corner c;
    if (!classify(es, right_end, cx, cy, c)) {
      continue;
    }
    c.at = db::Point{cx, cy};
    c.area = quadrant_area(c.quadrant, c.at, ctx.halo());
    search_quadrant(es, c, planes->second, mask);
  }
}

}  // namespace hefesto::extract::cap
