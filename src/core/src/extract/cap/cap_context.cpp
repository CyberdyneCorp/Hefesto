// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The run context and tile helpers of extract/cap (cap_context.hpp).
// Owner: EXTCAP.

#include "cap_context.hpp"

#include <algorithm>
#include <string>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::extract::cap {

Context::Context(const CapInputs& in, NodeRegionList* nodes, CouplingTable* table, std::size_t node_count)
    : cell_(in.cell),
      style_(in.style),
      tech_(in.technology),
      clip_(in.clip),
      search_(in.clip.value_or(db::whole_plane())),
      node_of_(in.node_of),
      nodes_(nodes),
      table_(table) {
  if (in.compat != nullptr) {
    compat_ = *in.compat;
  }
  if (in.options != nullptr) {
    options_ = *in.options;
  }
  valid_ = cell_ != nullptr && style_ != nullptr && tech_ != nullptr;
  if (!valid_) {
    return;
  }
  halo_ = in.side_halo.value_or(style_->side_halo);
  plane_end_ = std::min(cell_->plane_count(), tech_->plane_count());
  if (!node_of_) {
    const db::CellDef* cell = cell_;
    const std::size_t count = node_count;
    node_of_ = [cell, count](tech::PlaneId plane, const db::TileView& tile, bool /*right_side*/) {
      return client_node(cell->plane(plane), tile, count);
    };
  }
}

diag::Status missing_inputs(std::string_view what) {
  return diag::make("EXTRACT-CAP-002", std::string{what} + " needs a cell, an extraction style and a technology");
}

Node Context::node(tech::PlaneId plane, const db::TileView& tile, bool right_side) const {
  if (tile.tile == nullptr) {
    return std::nullopt;
  }
  return node_of_(plane, tile, right_side);
}

tech::TileType Context::residue(tech::TileType type, tech::PlaneId plane) const {
  return tech_->is_contact(type) ? tech_->residue_on(type, plane) : type;
}

std::int32_t Context::plane_order(tech::PlaneId plane) const {
  const auto& order = style_->plane_order;
  return plane < order.size() ? order[plane] : -1;
}

double Context::area_cap(tech::TileType t) const {
  return t < tech::kMaxTileTypes ? tables().area[t] : 0.0;
}

double Context::perim_cap(tech::TileType a, tech::TileType b) const {
  const auto it = tables().perimeter.find({a, b});
  return it == tables().perimeter.end() ? 0.0 : it->second;
}

double Context::overlap_cap(tech::TileType a, tech::TileType b) const {
  const auto it = tables().overlap.find({a, b});
  return it == tables().overlap.end() ? 0.0 : it->second;
}

float Context::mult(tech::TileType a, tech::TileType b) const {
  const auto it = tables().fringe_mult.find({a, b});
  return it == tables().fringe_mult.end() ? 0.0F : it->second;
}

void Context::subtract_cap(Node node, double value, bool clamp) const {
  // Magic writes through a CLIENTDEFAULT region pointer here when the
  // inside tile has no node (undefined behaviour); fixed in both modes.
  if (!node || nodes_ == nullptr || *node >= nodes_->size()) {
    return;
  }
  double& cap = (*nodes_)[*node].substrate_cap;
  cap -= value;
  if (clamp && cap > -0.001 && cap < 0.001) {
    cap = 0;
  }
}

void Context::add_coupling(std::size_t a, std::size_t b, double value) const {
  if (table_ != nullptr) {
    table_->add(a, b, value);
  }
}

// --- Tile sides ---

tech::TileType side_type(const db::TileView& t, bool right_side) noexcept {
  if (!t.split) {
    return t.type;
  }
  return right_side ? t.right_type : t.left_type;
}

tech::TileType left_type(const db::TileView& t) noexcept {
  return t.split ? t.left_type : t.type;
}

tech::TileType right_type(const db::TileView& t) noexcept {
  return t.split ? t.right_type : t.type;
}

bool split_direction(const db::TileView& t) noexcept {
  return t.split && t.diagonal == db::Diagonal::nw_se;
}

tech::TileType top_type(const db::TileView& t) noexcept {
  if (!t.split) {
    return t.type;
  }
  return split_direction(t) ? t.right_type : t.left_type;
}

tech::TileType bottom_type(const db::TileView& t) noexcept {
  if (!t.split) {
    return t.type;
  }
  return split_direction(t) ? t.left_type : t.right_type;
}

tech::TileType stored_type(const db::TileView& t) noexcept {
  return left_type(t);
}

bool exact_space(const db::TileView& t) noexcept {
  return !t.split && t.type == tech::kSpace;
}

namespace {

db::TileView beyond(Stitch s) {
  using db::kInfinity;
  using db::kMinusInfinity;
  db::TileView v;
  switch (s) {
    case Stitch::rt:
      v.area = db::Rect{kMinusInfinity, kInfinity, kInfinity, kInfinity};
      break;
    case Stitch::tr:
      v.area = db::Rect{kInfinity, kMinusInfinity, kInfinity, kInfinity};
      break;
    case Stitch::lb:
      v.area = db::Rect{kMinusInfinity, kMinusInfinity, kInfinity, kMinusInfinity};
      break;
    case Stitch::bl:
      v.area = db::Rect{kMinusInfinity, kMinusInfinity, kMinusInfinity, kInfinity};
      break;
  }
  return v;
}

// Inside the infinity tiles: Magic's stitches reach the boundary tiles at
// +-INFINITY, which no walk enters.
bool in_plane(db::Point p) noexcept {
  return p.x >= db::kMinusInfinity && p.x < db::kInfinity && p.y >= db::kMinusInfinity && p.y < db::kInfinity;
}

}  // namespace

db::TileView step(const db::Plane& plane, const db::TileView& t, Stitch s) {
  db::Point p;
  switch (s) {
    case Stitch::rt:
      p = db::Point{t.area.x1 - 1, t.area.y1};
      break;
    case Stitch::tr:
      p = db::Point{t.area.x1, t.area.y1 - 1};
      break;
    case Stitch::lb:
      p = db::Point{t.area.x0, t.area.y0 - 1};
      break;
    case Stitch::bl:
      p = db::Point{t.area.x0 - 1, t.area.y0};
      break;
  }
  if (t.tile == nullptr || !in_plane(p)) {
    return beyond(s);
  }
  // Nothing is ever painted outside the usable plane, so the tile at a
  // point out there is the tile at the nearest usable point.
  p.x = std::clamp(p.x, db::kPlaneMin, db::kPlaneMax - 1);
  p.y = std::clamp(p.y, db::kPlaneMin, db::kPlaneMax - 1);
  db::TileView next = plane.tile_at(p, &t);
  // A tile at the edge of the usable plane (its view clipped there) has no
  // neighbour beyond it but the infinity tile.
  if (next.tile == t.tile || next.tile == nullptr) {
    return beyond(s);
  }
  return next;
}

db::Rect geoclip(db::Rect r, const db::Rect& c) noexcept {
  r.x0 = std::max(r.x0, c.x0);
  r.y0 = std::max(r.y0, c.y0);
  r.x1 = std::min(r.x1, c.x1);
  r.y1 = std::min(r.y1, c.y1);
  return r;
}

std::optional<tech::PlaneId> first_plane(tech::PlaneMask planes, std::size_t end) noexcept {
  for (std::size_t p = tech::kFirstUserPlane; p < end && p < tech::kMaxPlanes; ++p) {
    if ((planes & tech::plane_bit(static_cast<tech::PlaneId>(p))) != 0) {
      return static_cast<tech::PlaneId>(p);
    }
  }
  return std::nullopt;
}

void shield_pieces(const Context& ctx, tech::PlaneMask planes, const tech::TypeMask& shield, const db::Rect& area,
                   ShieldVisit on) {
  const std::optional<tech::PlaneId> p = first_plane(planes, ctx.plane_end());
  if (!p) {
    return;
  }
  const tech::PlaneMask rest = planes & ~tech::plane_bit(*p);
  if (rest == 0) {
    ctx.plane(*p).enumerate(area, shield, [&](const db::TileView& t) {
      on(t, geoclip(t.area, area));
      return true;
    });
    return;
  }
  ctx.plane(*p).enumerate(area, tech::TypeMask::all(), [&](const db::TileView& t) {
    const db::Rect piece = geoclip(t.area, area);
    if (piece.x1 <= piece.x0 || piece.y1 <= piece.y0) {
      return true;
    }
    if (shield.test(side_type(t, t.right_side))) {
      on(t, piece);
    } else {
      shield_pieces(ctx, rest, shield, piece, on);
    }
    return true;
  });
}

namespace {

// Right endpoint: the tile beyond x = cx on the inside row, and the tile
// diagonally beyond it on the outside row (ExtBasic.c:5378-5401).
CornerPair right_pair(const db::Plane& plane, const db::TileView& inside, const db::TileView& outside, bool topside,
                      db::Coord cx, db::Coord cy) {
  CornerPair out;
  if (inside.area.x1 > cx) {
    out.cont = inside;
  } else if (topside) {
    out.cont = step(plane, inside, Stitch::tr);
  } else {
    db::TileView t2 = step(plane, inside, Stitch::tr);
    while (t2.area.y0 > cy) {
      t2 = step(plane, t2, Stitch::lb);
    }
    out.cont = t2;
  }
  if (outside.area.x1 > cx) {
    out.diag = outside;
  } else if (topside) {
    db::TileView t2 = step(plane, outside, Stitch::tr);
    while (t2.area.y0 > cy) {
      t2 = step(plane, t2, Stitch::lb);
    }
    out.diag = t2;
  } else {
    out.diag = step(plane, outside, Stitch::tr);
  }
  return out;
}

// Left endpoint (ExtBasic.c:5403-5426).
CornerPair left_pair(const db::Plane& plane, const db::TileView& inside, const db::TileView& outside, bool topside,
                     db::Coord cx, db::Coord cy) {
  CornerPair out;
  if (inside.area.x0 < cx) {
    out.cont = inside;
  } else if (topside) {
    db::TileView t2 = step(plane, inside, Stitch::bl);
    while (t2.area.y1 < cy) {
      t2 = step(plane, t2, Stitch::rt);
    }
    out.cont = t2;
  } else {
    out.cont = step(plane, inside, Stitch::bl);
  }
  if (outside.area.x0 < cx) {
    out.diag = outside;
  } else if (topside) {
    out.diag = step(plane, outside, Stitch::bl);
  } else {
    db::TileView t2 = step(plane, outside, Stitch::bl);
    while (t2.area.y1 < cy) {
      t2 = step(plane, t2, Stitch::rt);
    }
    out.diag = t2;
  }
  return out;
}

}  // namespace

CornerPair corner_pair(const db::Plane& plane, const db::TileView& inside, const db::TileView& outside, bool topside,
                       bool right_end, db::Coord cx, db::Coord cy) {
  return right_end ? right_pair(plane, inside, outside, topside, cx, cy)
                   : left_pair(plane, inside, outside, topside, cx, cy);
}

}  // namespace hefesto::extract::cap
