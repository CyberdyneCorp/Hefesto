// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Sideoverlap (fringe) coupling: extFindOverlap, the legacy model
// extSideOverlap, the fringe-halo model extSideOverlapHalo with its shield
// fractions extSubtractSideOverlap{,2}, and extRemoveSubcap (ExtCouple.c
// :672-828, :1256-1836; capability extract-capacitance "Legacy sideoverlap
// (fringe directly below the edge)" with Q-EXTRACT-18, "Fringe-halo
// sideoverlap" with Q-EXTRACT-17, "Nearest-neighbor shadowing along an
// edge" for the blocked substrate fringe).
//
// Owner: EXTCAP.

#include <algorithm>
#include <cstdint>
#include <vector>

#include "edge_pass.hpp"
#include "hefesto/extract/cap/compat_branches.hpp"

namespace hefesto::extract::cap {

namespace {

bool vertical(const db::Rect& seg) noexcept {
  return seg.x1 == seg.x0;
}

std::int64_t along(const db::Rect& seg, const db::Rect& r) noexcept {
  return vertical(seg) ? std::int64_t{r.y1} - r.y0 : std::int64_t{r.x1} - r.x0;
}

/// The distances from the boundary line to the near and far sides of `r`
/// (the near one clamped to 0: "don't count underlap").
struct Distances {
  std::int64_t dnear = 0;
  std::int64_t dfar = 0;
};

Distances distances(Dir d, const db::Rect& seg, const db::Rect& r) noexcept {
  Distances out;
  switch (d) {
    case Dir::left:
      out = {std::int64_t{seg.x0} - r.x1, std::int64_t{seg.x0} - r.x0};
      break;
    case Dir::right:
      out = {std::int64_t{r.x0} - seg.x1, std::int64_t{r.x1} - seg.x1};
      break;
    case Dir::bottom:
      out = {std::int64_t{seg.y0} - r.y1, std::int64_t{seg.y0} - r.y0};
      break;
    case Dir::top:
      out = {std::int64_t{r.y0} - seg.y1, std::int64_t{r.y1} - seg.y1};
      break;
  }
  out.dnear = std::max<std::int64_t>(out.dnear, 0);
  return out;
}

/// sfar - snear for one multiplier.
double fringe_span(double mult, const Distances& d) {
  return fringe_fraction(mult, static_cast<double>(d.dfar)) - fringe_fraction(mult, static_cast<double>(d.dnear));
}

const tech::PlaneMask* shield_planes(const Context& ctx, tech::TileType a, tech::TileType b) {
  const auto it = ctx.tables().side_overlap_shield_planes.find({a, b});
  return it == ctx.tables().side_overlap_shield_planes.end() || it->second == 0 ? nullptr : &it->second;
}

/// One overlapped tile U on plane q.
struct Target {
  tech::PlaneId plane = 0;
  db::TileView tile;
  tech::TileType type = 0;  // residue on q
  Node node;
};

/// The rules of the edge that apply to U (plane and target type).
template <typename Fn>
void for_matching_rules(const EdgeState& es, const Target& u, Fn&& fn) {
  if (es.overlap_list == nullptr) {
    return;
  }
  for (const EdgeRule& e : *es.overlap_list) {
    if ((e.planes & tech::plane_bit(u.plane)) != 0 && e.near_types.test(u.type)) {
      fn(e);
    }
  }
}

/// The substrate subtraction of either model, then the coupling entry.
void finish(const EdgeState& es, const Target& u, Node rbp, double subcap, double cap) {
  const Context& ctx = *es.ctx;
  if (ctx.plane_order(es.plane) > ctx.plane_order(u.plane)) {
    ctx.subtract_cap(rbp, subcap, true);
  }
  if (u.node == rbp || !u.node || !rbp) {
    return;
  }
  ctx.add_coupling(*u.node, *rbp, cap);
}

// --- Legacy model ---

/// extSideOverlap (Q-EXTRACT-18 per mode).
void legacy(const EdgeState& es, const Target& u, tech::TileType ta) {
  const Context& ctx = *es.ctx;
  const db::Rect& seg = es.bp.segment;
  const db::Rect& tr = u.tile.area;
  const std::int64_t whole = vertical(seg) ? std::int64_t{std::min(seg.y1, tr.y1)} - std::max(seg.y0, tr.y0)
                                           : std::int64_t{std::min(seg.x1, tr.x1)} - std::max(seg.x0, tr.x0);
  const db::Rect clip = geoclip(tr, es.area);
  const std::int64_t total = clip.width() * clip.height();
  const Node rbp = inside_node(es);
  double cap = 0;
  std::vector<std::int64_t> accounted;
  for_matching_rules(es, u, [&](const EdgeRule& e) {
    std::int64_t o = total;
    if (const tech::PlaneMask* planes = shield_planes(ctx, ta, u.type)) {
      shield_pieces(ctx, *planes, e.far_types, clip, [&](const db::TileView& t, const db::Rect& piece) {
        std::int64_t a = piece.width() * piece.height();
        if (t.split) {
          a /= 2;
        }
        if (a > 0) {
          o -= a;
        }
      });
    }
    if (u.node != rbp) {
      cap += e.cap * static_cast<double>(o);
    }
    accounted.push_back(o);
  });
  const tech::TileType outtype = ctx.residue(stored_type(es.bp.outside), es.plane);
  const std::int64_t sub = along(seg, geoclip(tr, es.area));
  const std::int64_t length = legacy_cap_length(ctx.compat(), whole, sub);
  const std::int64_t acc = legacy_accounted_area(ctx.compat(), accounted);
  const double subcap = ctx.perim_cap(ta, outtype) * static_cast<double>(std::min(acc, length));
  finish(es, u, rbp, subcap, cap);
}

// --- Fringe-halo model ---

/// The shielded coupling and substrate fractions of one rule.
struct Shielded {
  double coup = 0.0;
  double sub = 0.0;
};

/// extSubtractSideOverlap{,2} over the shield planes of (ta, tb).
Shielded side_shield(const EdgeState& es, const Target& u, tech::TileType ta, const EdgeRule& e,
                     const db::Rect& clip, std::int64_t length) {
  const Context& ctx = *es.ctx;
  Shielded out;
  const tech::PlaneMask* planes = shield_planes(ctx, ta, u.type);
  if (planes == nullptr) {
    return out;
  }
  const tech::TileType stored = stored_type(es.bp.inside);
  const tech::TileType inside = shield_inside_type(ctx.compat(), stored, ctx.residue(stored, es.plane));
  shield_pieces(ctx, *planes, e.far_types, clip, [&](const db::TileView& /*t*/, const db::Rect& r) {
    if (r.width() * r.height() <= 0) {
      return;
    }
    const Distances d = distances(es.bp.dir, es.bp.segment, r);
    const double share = static_cast<double>(along(es.bp.segment, r)) / static_cast<double>(length);
    double mult = static_cast<double>(ctx.mult(inside, tech::kSpace));
    double span = fringe_span(mult, d);
    out.sub += span * share;
    if (static_cast<double>(ctx.mult(inside, u.type)) != mult) {
      mult = static_cast<double>(ctx.mult(inside, u.type));
      span = fringe_span(mult, d);
    }
    out.coup += span * share;
  });
  return out;
}

/// extSideOverlapHalo (Q-EXTRACT-17 per mode).
void halo(const EdgeState& es, const Target& u, tech::TileType ta) {
  const Context& ctx = *es.ctx;
  const db::Rect clip = geoclip(u.tile.area, es.area);
  const std::int64_t length = along(es.bp.segment, clip);
  const Distances d = distances(es.bp.dir, es.bp.segment, clip);
  double mult = static_cast<double>(ctx.mult(ta, u.type));
  double span = fringe_span(mult, d);
  const double cfrac = span;
  if (static_cast<double>(ctx.mult(ta, tech::kSpace)) != mult) {
    mult = static_cast<double>(ctx.mult(ta, tech::kSpace));
    span = fringe_span(mult, d);
  }
  const double sfrac = span;
  const Node rbp = inside_node(es);
  const bool distinct = u.node != rbp;
  const bool per_rule = shield_counted_per_rule(ctx.compat());
  const bool needs_distinct = shield_needs_distinct_nodes(ctx.compat());
  double cap = 0;
  double subfrac = 0.0;
  bool counted = false;
  for_matching_rules(es, u, [&](const EdgeRule& e) {
    const Shielded sh = side_shield(es, u, ta, e, clip, length);
    if (distinct) {
      cap += e.cap * ((cfrac - sh.coup) * static_cast<double>(length));
    }
    if (per_rule ? (distinct || !needs_distinct) : !counted) {
      subfrac += sh.sub;
      counted = true;
    }
  });
  const double subcap = ctx.perim_cap(ta, tech::kSpace) * ((sfrac - subfrac) * static_cast<double>(length));
  finish(es, u, rbp, subcap, cap);
}

}  // namespace

namespace {

/// The legacy model looks one unit deep (esws->area) along the sub-segment.
db::Rect fringe_search(const EdgeState& es, const db::Rect& area) {
  if (es.fringe_halo) {
    return area;
  }
  db::Rect search = es.area;
  if (es.bp.dir == Dir::top || es.bp.dir == Dir::bottom) {
    search.x0 = area.x0;
    search.x1 = area.x1;
  } else {
    search.y0 = area.y0;
    search.y1 = area.y1;
  }
  return search;
}

/// The overlapped tiles of one target plane.
void search_plane(EdgeState& es, tech::PlaneId plane, const tech::TypeMask& types, tech::TileType ta) {
  const Context& ctx = *es.ctx;
  ctx.plane(plane).enumerate(es.area, types, [&](const db::TileView& tile) {
    const tech::TileType tb = side_type(tile, tile.right_side);
    if (tb == tech::kSpace) {
      return true;
    }
    const Target u{plane, tile, ctx.residue(tb, plane), ctx.node(plane, tile, tile.right_side)};
    if (es.fringe_halo) {
      halo(es, u, ta);
    } else {
      legacy(es, u, ta);
    }
    return true;
  });
}

}  // namespace

void find_overlap(EdgeState& es, const db::Rect& area) {
  if (!es.kinds.sideoverlap) {
    return;
  }
  const Context& ctx = *es.ctx;
  const CapTables& t = ctx.tables();
  const tech::TileType tin = ctx.residue(stored_type(es.bp.inside), es.plane);
  const tech::TileType tout = ctx.residue(stored_type(es.bp.outside), es.plane);
  const auto planes = t.side_overlap_other_planes.find({tin, tout});
  const auto types = t.side_overlap_other_types.find({tin, tout});
  if (planes == t.side_overlap_other_planes.end() || types == t.side_overlap_other_types.end()) {
    return;
  }
  const db::Rect saved = es.area;
  es.area = fringe_search(es, area);
  for (std::size_t q = tech::kFirstUserPlane; q < ctx.plane_end(); ++q) {
    const auto plane = static_cast<tech::PlaneId>(q);
    if ((planes->second & tech::plane_bit(plane)) != 0) {
      search_plane(es, plane, types->second, tin);
    }
  }
  es.area = saved;
}

void remove_subcap(const EdgeState& es, const Boundary& bloc, const db::Rect& clip) {
  if (!es.kinds.sideoverlap || !es.fringe_halo) {
    return;
  }
  const Context& ctx = *es.ctx;
  const auto [ta, tb] = boundary_types(bloc);
  const std::int64_t length = along(bloc.segment, bloc.segment);
  std::int64_t dnear = 0;
  switch (bloc.dir) {
    case Dir::left:
      dnear = std::int64_t{bloc.segment.x0} - clip.x0;
      break;
    case Dir::right:
      dnear = std::int64_t{clip.x1} - bloc.segment.x1;
      break;
    case Dir::bottom:
      dnear = std::int64_t{bloc.segment.y0} - clip.y0;
      break;
    case Dir::top:
      dnear = std::int64_t{clip.y1} - bloc.segment.y1;
      break;
  }
  dnear = std::max<std::int64_t>(dnear, 0);
  const float mult = ctx.mult(ta, tech::kSpace);
  const double snear = fringe_fraction(static_cast<double>(mult), static_cast<double>(dnear));
  const double subcap = ctx.perim_cap(ta, tb) * (1.0 - snear) * static_cast<double>(length);
  ctx.subtract_cap(inside_node(es), subcap, false);
}

}  // namespace hefesto::extract::cap
