// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Substrate capacitance and per-class area and perimeter of node tiles
// (extNodeAreaFunc's capacitance and resistance parts, ExtBasic.c:5466-5872;
// extNodeCornerCap, ExtBasic.c:5341-5441; capabilities extract-capacitance
// "Area capacitance to substrate", "Perimeter capacitance to substrate",
// "Corner fringe capacitance to substrate", "Capacitance option gating";
// extract-resistance-lumped "Per-class area and perimeter of a node").
//
// Owner: EXTCAP.

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "cap_context.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/extract/cap/compat_branches.hpp"

namespace hefesto::extract {

namespace {

using cap::Context;
using cap::Node;

/// The per-tile facts extNodeAreaFunc derives once.
struct TileFacts {
  tech::PlaneId plane = 0;
  db::TileView tile;
  bool right = false;
  tech::TileType type = 0;
  tech::TileType residue = 0;
  tech::TypeMask connect;    // exts_nodeConn[type]
  tech::TypeMask changed;    // exts_typesResistChanged[residue]
  std::int32_t cls = -1;     // exts_typeToResistClass[residue]
  std::size_t node = 0;
  bool corners = false;
};

/// Accumulates into one node.
struct Sink {
  NodeRegion& node;
  void cap(double v) const { node.substrate_cap += v; }
  void perimeter(const TileFacts& f, tech::TileType neighbour_residue, std::int64_t len) const {
    if (f.cls >= 0 && f.changed.test(neighbour_residue)) {
      node.classes[static_cast<std::size_t>(f.cls)].perimeter += len;
    }
  }
};

std::int64_t clipped_area(const Context& ctx, const db::Rect& r) {
  if (!ctx.clip()) {
    return r.width() * r.height();
  }
  if (!db::overlap(r, *ctx.clip())) {
    return 0;
  }
  const db::Rect c = cap::geoclip(r, *ctx.clip());
  return c.width() * c.height();
}

/// (int)sqrt(w*w + h*h) of a split tile (ExtPerim.c:105-117,
/// ExtBasic.c:5574-5577). Magic squares in int, which overflows for a side
/// past 46340 (undefined behaviour); the sum is formed in 64 bits in both
/// modes.
std::int64_t diagonal_length(const db::Rect& r) {
  const std::int64_t w = r.width();
  const std::int64_t h = r.height();
  return static_cast<std::int64_t>(std::sqrt(static_cast<double>(w * w + h * h)));
}

/// The length of the shared side, clipped when a clip area is in effect.
std::int64_t side_length(const Context& ctx, db::Rect seg) {
  if (!ctx.clip()) {
    return seg.x1 - seg.x0 + (seg.y1 - seg.y0);
  }
  seg = cap::geoclip(seg, *ctx.clip());
  // EDGENULL: an inverted segment has length 0.
  if (seg.x1 < seg.x0 || seg.y1 < seg.y0) {
    return 0;
  }
  return seg.x1 - seg.x0 + (seg.y1 - seg.y0);
}

/// The correction at one endpoint of a horizontal segment: +1 convex, -1
/// concave, 0 none.
int corner_sign(const Context& ctx, const TileFacts& f, const db::TileView& tp, bool topside, bool right_end) {
  const db::Coord cy = topside ? f.tile.area.y1 : f.tile.area.y0;
  const db::Coord cx = right_end ? std::min(f.tile.area.x1, tp.area.x1) : std::max(f.tile.area.x0, tp.area.x0);
  if (const auto& clip = ctx.clip(); clip && (cx < clip->x0 || cx >= clip->x1 || cy < clip->y0 || cy >= clip->y1)) {
    return 0;
  }
  const cap::CornerPair pair = cap::corner_pair(ctx.plane(f.plane), f.tile, tp, topside, right_end, cx, cy);
  if (pair.cont.split || pair.diag.split) {
    return 0;
  }
  if (cap::exact_space(pair.cont) && cap::exact_space(pair.diag)) {
    return 1;
  }
  cap::CornerTiles tiles;
  tiles.types_connect = f.connect.test(pair.cont.type) && f.connect.test(pair.diag.type);
  tiles.same_node = ctx.node(f.plane, pair.cont) == Node{f.node} && ctx.node(f.plane, pair.diag) == Node{f.node};
  tiles.quadrant_is_space = cap::exact_space(tp);
  return cap::substrate_corner_concave(ctx.compat(), tiles) ? -1 : 0;
}

/// extNodeCornerCap for one horizontal segment between f.tile and tp.
void corner_cap(const Context& ctx, const TileFacts& f, const db::TileView& tp, bool topside, double capval,
                const Sink& sink) {
  const double mult = static_cast<double>(ctx.mult(f.residue, tech::kSpace));
  if (mult <= 0 || f.tile.split || tp.split) {
    return;
  }
  const double cornercap = capval * (ctx.options().corner_factor / mult);
  for (const bool right_end : {false, true}) {
    const int sign = corner_sign(ctx, f, tp, topside, right_end);
    if (sign != 0) {
      sink.cap(sign > 0 ? cornercap : -cornercap);
    }
  }
}

/// One side of the tile: Magic's Top/Left/Bottom/Right loops.
void walk_side(const Context& ctx, const TileFacts& f, db::Side side, const Sink& sink) {
  std::vector<db::TileView> out;
  const db::Rect& a = f.tile.area;
  const bool vertical = side == db::Side::left || side == db::Side::right;
  ctx.plane(f.plane).neighbors(f.tile, side, vertical ? a.y0 : a.x0, vertical ? a.y1 : a.x1, out);
  // Top walks right to left (RT, BL) and right walks top to bottom (TR,
  // LB); left and bottom walk upward and rightward.
  if (side == db::Side::top || side == db::Side::right) {
    std::reverse(out.begin(), out.end());
  }
  for (const db::TileView& tp : out) {
    db::Rect seg;
    tech::TileType t = 0;
    switch (side) {
      case db::Side::top:
        seg = db::Rect{std::max(a.x0, tp.area.x0), a.y1, std::min(a.x1, tp.area.x1), a.y1};
        t = cap::bottom_type(tp);
        break;
      case db::Side::bottom:
        seg = db::Rect{std::max(a.x0, tp.area.x0), a.y0, std::min(a.x1, tp.area.x1), a.y0};
        t = cap::top_type(tp);
        break;
      case db::Side::left:
        seg = db::Rect{a.x0, std::max(a.y0, tp.area.y0), a.x0, std::min(a.y1, tp.area.y1)};
        t = cap::right_type(tp);
        break;
      case db::Side::right:
        seg = db::Rect{a.x1, std::max(a.y0, tp.area.y0), a.x1, std::min(a.y1, tp.area.y1)};
        t = cap::left_type(tp);
        break;
    }
    const std::int64_t len = side_length(ctx, seg);
    const tech::TileType tres = ctx.residue(t, f.plane);
    if (const double capval = ctx.perim_cap(f.residue, tres); capval != 0.0) {
      sink.cap(capval * static_cast<double>(len));
      const bool horizontal = side == db::Side::top || side == db::Side::bottom;
      if (f.corners && horizontal) {
        corner_cap(ctx, f, tp, side == db::Side::top, capval, sink);
      }
    }
    sink.perimeter(f, tres, len);
  }
}

/// The diagonal of a split tile.
void diagonal(const Context& ctx, const TileFacts& f, const Sink& sink) {
  const std::int64_t len = diagonal_length(f.tile.area);
  const tech::TileType t = f.right ? f.tile.left_type : f.tile.right_type;
  const tech::TileType tres = ctx.residue(t, f.plane);
  if (const double capval = ctx.perim_cap(f.residue, tres); capval != 0.0) {
    sink.cap(capval * static_cast<double>(len));
  }
  sink.perimeter(f, tres, len);
}

/// The sides a tile walks: all four, or the two of a split tile's half.
void walk_sides(const Context& ctx, const TileFacts& f, const Sink& sink) {
  const bool split = f.tile.split;
  const bool owns_top = !split || (f.right == cap::split_direction(f.tile));
  if (owns_top) {
    walk_side(ctx, f, db::Side::top, sink);
  }
  if (!split || !f.right) {
    walk_side(ctx, f, db::Side::left, sink);
  }
  if (!split || !owns_top) {
    walk_side(ctx, f, db::Side::bottom, sink);
  }
  if (!split || f.right) {
    walk_side(ctx, f, db::Side::right, sink);
  }
}

TileFacts facts_of(const Context& ctx, const NodeTile& t, std::size_t node_index) {
  TileFacts f;
  f.plane = t.plane;
  f.tile = t.tile;
  f.right = t.right_side;
  f.type = cap::side_type(t.tile, t.right_side);
  f.residue = ctx.residue(f.type, t.plane);
  const ExtStyle& s = ctx.style();
  f.connect = f.type < s.node_conn.size() ? s.node_conn[f.type] : ctx.technology().connects_to(f.type);
  f.changed = f.residue < s.resist_changed.size() ? s.resist_changed[f.residue] : tech::TypeMask::all();
  f.cls = f.residue < tech::kMaxTileTypes ? s.resist_class[f.residue] : -1;
  if (f.cls >= static_cast<std::int32_t>(s.resist_classes.size())) {
    f.cls = -1;
  }
  f.node = node_index;
  const ExtractOptions& o = ctx.options();
  f.corners = o.corners && o.fringehalo && ctx.halo() > 0;
  return f;
}

}  // namespace

NodeTileAccumulator::NodeTileAccumulator(const CapInputs& in, std::size_t node_count)
    : ctx_(std::make_unique<const Context>(in, nullptr, nullptr, node_count)) {}

NodeTileAccumulator::~NodeTileAccumulator() = default;
NodeTileAccumulator::NodeTileAccumulator(NodeTileAccumulator&&) noexcept = default;
NodeTileAccumulator& NodeTileAccumulator::operator=(NodeTileAccumulator&&) noexcept = default;

void NodeTileAccumulator::add(const NodeTile& tile, NodeRegion& node, std::size_t node_index) const {
  if (!ctx_ || !ctx_->valid()) {
    return;
  }
  const Context& ctx = *ctx_;
  node.classes.resize(ctx.style().resist_classes.size());
  const TileFacts f = facts_of(ctx, tile, node_index);
  const Sink sink{node};
  std::int64_t area = clipped_area(ctx, f.tile.area);
  if (f.tile.split) {
    area /= 2;  // split tiles are half area
  }
  if (f.cls >= 0) {
    node.classes[static_cast<std::size_t>(f.cls)].area += area;
  }
  sink.cap(static_cast<double>(area) * ctx.area_cap(f.residue));
  if (f.tile.split) {
    diagonal(ctx, f, sink);
  }
  walk_sides(ctx, f, sink);
  if (!ctx.options().capacitance) {
    node.substrate_cap = 0.0;
  }
}

diag::Status substrate_caps(NodeRegionList& nodes, const CapInputs& in) {
  const Context ctx(in, &nodes, nullptr);
  if (!ctx.valid()) {
    return cap::missing_inputs("substrate capacitance");
  }
  for (NodeRegion& n : nodes) {
    n.substrate_cap = 0.0;
    n.classes.assign(ctx.style().resist_classes.size(), ClassArea{});
  }
  // Each node's tiles in extNodeAreaFunc's visit order, so the
  // floating-point sums are Magic's (flood_order.cpp).
  const NodeTileAccumulator acc(in, nodes.size());
  const std::vector<std::vector<NodeTile>> order = cap::flood_order(ctx, nodes.size());
  for (std::size_t n = 0; n < order.size(); ++n) {
    for (const NodeTile& t : order[n]) {
      acc.add(t, nodes[n], n);
    }
  }
  if (!cap::class_values_stored(ctx.compat(), ctx.options().lumped)) {
    for (NodeRegion& n : nodes) {
      n.classes.assign(n.classes.size(), ClassArea{});
    }
  }
  return diag::Done{};
}

}  // namespace hefesto::extract
