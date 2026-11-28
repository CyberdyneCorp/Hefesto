// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The order in which extNodeAreaFunc visits the tiles of each node
// (ExtBasic.c:5466-5872): a stack flood from the node's seed that pushes,
// from each popped tile, its unvisited connected neighbours along the top
// (right to left), left (bottom to top), bottom (left to right) and right
// (top to bottom) sides, then the images of a contact on its other planes
// (:5760-5800) and the connected tiles of the one-unit halo on the planes
// the type crosses (extNbrPushFunc). Magic sums nreg_cap tile by tile in
// that order, so the node capacitance (and the near-zero residue left when
// shielding cancels it, kept or clamped to 0 by Magic) is reproduced only
// by adding the tiles in the same order (capability extract-capacitance
// "Area capacitance to substrate", "Perimeter capacitance to substrate").
//
// The seed of a node is its first tile in extFindNodes' scan (planes in
// order, DBSrPaintArea over the style's active types). A node with tiles
// the flood does not reach from one seed (the substrate node, seeded from
// all its tiles at once) gets more seeds from a final scan over every
// non-space tile, so every tile is visited exactly once. Nodes come from
// the region finder's slots (Context::node), so the replay never changes
// which tiles belong to a node, only their order.
//
// Owner: EXTCAP.

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "cap_context.hpp"

namespace hefesto::extract::cap {

namespace {

/// A tile with two non-space sides keeps one visit state per side.
bool two_sided(const db::TileView& v) noexcept {
  return v.split && v.left_type != tech::kSpace && v.right_type != tech::kSpace;
}

struct Item {
  tech::PlaneId plane = 0;
  db::TileView view;
  bool right = false;
};

/// The pushed-or-visited (`pending`) and visited sets of extNodeAreaFunc's
/// tile marks, keyed by tile and side.
class Marks {
 public:
  [[nodiscard]] bool marked(const db::TileView& v, bool right) const { return pending_.count(key(v, right)) != 0; }
  [[nodiscard]] bool visited(const Item& it) const { return visited_.count(key(it.view, it.right)) != 0; }
  void push(const Item& it) { pending_.insert(key(it.view, it.right)); }
  void visit(const Item& it) { visited_.insert(key(it.view, it.right)); }

 private:
  static std::uintptr_t key(const db::TileView& v, bool right) noexcept {
    // Tiles are at least 2-aligned, so the low bit is free for the side.
    return reinterpret_cast<std::uintptr_t>(v.tile) | (two_sided(v) && right ? 1U : 0U);
  }
  std::unordered_set<std::uintptr_t> pending_;
  std::unordered_set<std::uintptr_t> visited_;
};

class Replay {
 public:
  explicit Replay(const Context& ctx) : ctx_(ctx) {}

  [[nodiscard]] bool visited(const Item& it) const { return marks_.visited(it); }

  /// Floods from `seed`, calling `on(tile)` for each tile in visit order.
  void run(const Item& seed, std::vector<NodeTile>& out) {
    push(seed);
    while (!stack_.empty()) {
      const Item it = stack_.back();
      stack_.pop_back();
      if (marks_.visited(it)) {
        continue;
      }
      marks_.visit(it);
      const tech::TileType type = side_type(it.view, it.right);
      out.push_back(NodeTile{it.plane, it.view, it.right});
      sides(it, type);
      contact_images(it, type);
      halo(it, type);
    }
  }

 private:
  void push(const Item& it) {
    marks_.push(it);
    stack_.push_back(it);
  }

  [[nodiscard]] const tech::TypeMask& conn(tech::TileType t) const {
    static const tech::TypeMask kEmpty;
    const auto& c = ctx_.style().node_conn;
    return t < c.size() ? c[t] : kEmpty;
  }

  void neighbour(const Item& it, tech::TileType type, const db::TileView& nb, bool nb_right) {
    const bool right = nb.split && nb_right;
    if (!marks_.marked(nb, right) && conn(type).test(side_type(nb, right))) {
      push(Item{it.plane, nb, right});
    }
  }

  /// One side's neighbours; `backward` walks them in reverse (RT then BL,
  /// TR then LB). `side_of` picks the neighbour's side facing the tile.
  template <typename SideOf>
  void walk(const Item& it, tech::TileType type, db::Side side, bool backward, SideOf side_of) {
    const db::Rect& a = it.view.area;
    const bool vertical = side == db::Side::left || side == db::Side::right;
    ctx_.plane(it.plane).neighbors(it.view, side, vertical ? a.y0 : a.x0, vertical ? a.y1 : a.x1, nbrs_);
    if (backward) {
      std::reverse(nbrs_.begin(), nbrs_.end());
    }
    for (const db::TileView& n : nbrs_) {
      neighbour(it, type, n, side_of(n));
    }
  }

  /// The four sides, skipping those a split tile's side does not have.
  void sides(const Item& it, tech::TileType type) {
    const bool split = it.view.split;
    const bool slant = it.right != split_direction(it.view);
    if (!split || !slant) {
      walk(it, type, db::Side::top, true, [](const db::TileView& n) { return !split_direction(n); });
    }
    if (!split || !it.right) {
      walk(it, type, db::Side::left, false, [](const db::TileView&) { return true; });
    }
    if (!split || slant) {
      walk(it, type, db::Side::bottom, false, [](const db::TileView& n) { return split_direction(n); });
    }
    if (!split || it.right) {
      walk(it, type, db::Side::right, true, [](const db::TileView&) { return false; });
    }
  }

  /// The image of a contact on `plane`: the tile at its lower-left corner.
  void contact_image(const Item& it, tech::TileType type, tech::PlaneId plane) {
    const db::TileView tp = ctx_.plane(plane).tile_at({it.view.area.x0, it.view.area.y0});
    if (marks_.marked(tp, false) || marks_.marked(tp, true)) {
      return;
    }
    const auto try_side = [&](bool right) {
      if (conn(type).test(side_type(tp, right))) {
        push(Item{plane, tp, tp.split && right});
      }
    };
    if (it.view.split && tp.split) {
      try_side(it.right);
    } else if (tp.split) {
      try_side(true);
      try_side(false);
    } else {
      try_side(false);
    }
  }

  void contact_images(const Item& it, tech::TileType type) {
    const tech::Technology& t = ctx_.technology();
    if (!t.is_contact(type)) {
      return;
    }
    const tech::PlaneMask planes = t.contact_planes(type) & ~tech::plane_bit(it.plane);
    for (std::size_t p = tech::kFirstUserPlane; p < ctx_.plane_end(); ++p) {
      if ((planes & tech::plane_bit(static_cast<tech::PlaneId>(p))) != 0) {
        contact_image(it, type, static_cast<tech::PlaneId>(p));
      }
    }
  }

  /// extNbrPushFunc over the one-unit halo of the tile on `plane`.
  void halo_on(const Item& it, tech::TileType type, tech::PlaneId plane) {
    const db::Rect& area = it.view.area;
    const auto visit = [&](const db::TileView& v) {
      const bool right = v.split && v.right_side;
      if (marks_.marked(v, right)) {
        return true;
      }
      const db::Rect c{std::max(v.area.x0, area.x0), std::max(v.area.y0, area.y0), std::min(v.area.x1, area.x1),
                       std::min(v.area.y1, area.y1)};
      if (db::overlap(v.area, area) || c.x0 < c.x1 || c.y0 < c.y1) {
        push(Item{plane, v, right});
      }
      return true;
    };
    const db::Rect bigger = db::expand(area, 1);
    if (it.view.split) {
      (void)ctx_.plane(plane).enumerate_triangle(bigger, it.view.diagonal, it.right, conn(type), visit);
    } else {
      (void)ctx_.plane(plane).enumerate(bigger, conn(type), visit);
    }
  }

  void halo(const Item& it, tech::TileType type) {
    const tech::PlaneMask planes = ctx_.technology().cross_planes(type);
    for (std::size_t p = tech::kFirstUserPlane; p < ctx_.plane_end() && planes != 0; ++p) {
      const auto plane = static_cast<tech::PlaneId>(p);
      if (plane != it.plane && (planes & tech::plane_bit(plane)) != 0) {
        halo_on(it, type, plane);
      }
    }
  }

  const Context& ctx_;
  Marks marks_;
  std::vector<Item> stack_;
  std::vector<db::TileView> nbrs_;
};

/// Seeds a flood at every tile of `types` whose node is set and that no
/// earlier flood visited; `first_only` seeds each node once.
void seed_scan(const Context& ctx, Replay& replay, const tech::TypeMask& types, bool first_only,
               std::vector<std::vector<NodeTile>>& out) {
  for (std::size_t p = tech::kFirstUserPlane; p < ctx.plane_end(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    ctx.plane(plane).enumerate(db::whole_plane(), types, [&](const db::TileView& v) {
      const Item it{plane, v, v.split && v.right_side};
      const Node n = ctx.node(plane, v, v.right_side);
      const bool skip = !n || *n >= out.size() || replay.visited(it) || (first_only && !out[*n].empty());
      if (!skip) {
        replay.run(it, out[*n]);
      }
      return true;
    });
  }
}

}  // namespace

std::vector<std::vector<NodeTile>> flood_order(const Context& ctx, std::size_t node_count) {
  std::vector<std::vector<NodeTile>> out(node_count);
  Replay replay(ctx);
  seed_scan(ctx, replay, ctx.style().active, true, out);
  tech::TypeMask material = tech::TypeMask::all();
  material.clear_type(tech::kSpace);
  seed_scan(ctx, replay, material, false, out);
  return out;
}

}  // namespace hefesto::extract::cap
