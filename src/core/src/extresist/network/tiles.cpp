// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The tile queue (capability extresist-network "Start tile and origin",
// "Breakpoints", "Tile processing order"): FindStartTile, ResStartTile,
// ResProcessTiles, ResEachTile with its junctions (ResProcessJunction),
// device terminals (ResNewTermDevice, ResNewSubDevice) and port nodes
// (resMakePortBreakpoints).
// (Magic ref: resis/ResMain.c:700-842, :1698-2066, resis/ResBasic.c:47-409,
// resis/ResJunct.c:51-234)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "extresist/network/builder.hpp"

namespace hefesto::extresist::detail {

namespace {

constexpr std::uint32_t kIgnoreLeft = 1;
constexpr std::uint32_t kIgnoreRight = 2;
constexpr std::uint32_t kIgnoreTop = 4;
constexpr std::uint32_t kIgnoreBottom = 8;

/// One side of a tile in FindStartTile's and ResEachTile's order: the walk,
/// the facing type of a neighbour, and the point on the shared edge.
struct Side {
  bool (Builder::*walk)(const TileRef&, const std::function<bool(const TileRef&)>&);
  tech::TileType (*facing)(const TileTypes&);
  std::uint32_t ignore;
  bool vertical;  // left or right: the shared edge is vertical
  bool high;      // right or top: the edge is at RIGHT or TOP
};

tech::TileType facing_right(const TileTypes& t) {
  return t.split ? t.right : t.left;
}
tech::TileType facing_left(const TileTypes& t) {
  return t.left;
}
tech::TileType facing_bottom(const TileTypes& t) {
  return t.bottom();
}
tech::TileType facing_top(const TileTypes& t) {
  return t.top();
}

// left (neighbours bottom to top), right (top to bottom), top (right to
// left), bottom (left to right).
constexpr Side kSides[] = {
    {&Builder::each_left, &facing_right, kIgnoreLeft, true, false},
    {&Builder::each_right, &facing_left, kIgnoreRight, true, true},
    {&Builder::each_top, &facing_bottom, kIgnoreTop, false, true},
    {&Builder::each_bottom, &facing_top, kIgnoreBottom, false, false},
};

/// The midpoint of the edge `t` shares with `tp` (FindStartTile's and the
/// junctions' location).
db::Point shared_mid(const db::Rect& t, const db::Rect& tp, const Side& s) {
  if (s.vertical) {
    return db::Point{s.high ? t.x1 : t.x0, (std::min(t.y1, tp.y1) + std::max(t.y0, tp.y0)) >> 1};
  }
  return db::Point{(std::min(t.x1, tp.x1) + std::max(t.x0, tp.x0)) >> 1, s.high ? t.y1 : t.y0};
}

/// The device-terminal breakpoint location (the neighbour's centre along the
/// edge) and the device edge facing the tile.
db::Point terminal_point(const db::Rect& t, const db::Rect& tp, const Side& s) {
  if (s.vertical) {
    return db::Point{s.high ? t.x1 : t.x0, (tp.y1 + tp.y0) >> 1};
  }
  return db::Point{(tp.x0 + tp.x1) >> 1, s.high ? t.y1 : t.y0};
}

std::uint32_t device_edge(const Side& s) {
  if (s.vertical) {
    return s.high ? kLeftEdge : kRightEdge;
  }
  return s.high ? kBottomEdge : kTopEdge;
}

}  // namespace

// --- FindStartTile ---

namespace {

class StartFinder {
 public:
  StartFinder(Builder& b, db::Point& source) : b_(b), source_(source) {}

  TileRef find(tech::TileType type) {
    if (b_.tech_.is_contact(type)) {
      const tech::TypeMask& residues = b_.tech_.contact(type)->residues;
      for (tech::TileType r = tech::kFirstUserType; r < b_.tech_.user_type_end(); ++r) {
        if (residues.test(r) && r != type) {
          const TileRef t = find(r);
          if (t) {
            return t;
          }
        }
      }
    }
    const db::Point at = b_.input.driver.location;
    const auto plane = b_.tech_.home_plane(type);
    if (!plane || *plane >= b_.def.plane_count()) {
      return {};
    }
    if (b_.input.driver.drive_only) {
      return drive_point(*plane, type, at);
    }
    return device(*plane, at);
  }

 private:
  TileRef drive_point(tech::PlaneId plane, tech::TileType type, db::Point at) {
    const TileRef tile = b_.goto_point(plane, at);
    source_ = at;
    const TileTypes ty = b_.types(tile);
    if (!ty.split && ty.left == type) {
      return tile;
    }
    // A drive point on the tile's left or bottom edge: GOTOPOINT picks the
    // tile beyond the edge, so look across it.
    const db::Rect r = b_.area(tile);
    TileRef found;
    const auto pick = [&](const TileRef& tp, tech::TileType facing) {
      found = facing == type ? tp : TileRef{};
      return static_cast<bool>(found);
    };
    if (at.x == r.x0) {
      b_.each_left(tile, [&](const TileRef& tp) { return pick(tp, facing_right(b_.types(tp))); });
    } else if (at.y == r.y0) {
      b_.each_bottom(tile, [&](const TileRef& tp) { return pick(tp, b_.types(tp).top()); });
    }
    if (!found) {
      b_.g.message("Couldn't find wire at " + std::to_string(at.x) + " " + std::to_string(at.y));
    }
    return found;
  }

  TileRef device(tech::PlaneId plane, db::Point at) {
    TileRef tile = b_.goto_point(plane, at);
    const TileTypes ty = b_.types(tile);
    tech::TileType t1 = ty.left;
    if (ty.split && !b_.style_.device_types.test(t1)) {
      t1 = ty.right;
    }
    if (!b_.style_.device_types.test(t1)) {
      b_.g.message("Couldn't find device at " + std::to_string(at.x) + " " + std::to_string(at.y));
      return {};
    }
    const auto recs = b_.records(t1);
    for (const extract::DevicePattern* d : recs) {
      const TileRef t = by_record(tile, *d);
      if (t) {
        return t;
      }
    }
    for (const extract::DevicePattern* d : recs) {
      if (d->substrate_types.test(tech::kSpace)) {
        return {};
      }
    }
    b_.g.message("Couldn't find a terminal of the device at " + std::to_string(at.x) + " " + std::to_string(at.y));
    return {};
  }

  /// One device record: the perimeter search per S/D class (with the
  /// depth-first search over the device tiles when the device has more than
  /// one tile), then the other planes.
  TileRef by_record(TileRef& tile, const extract::DevicePattern& d) {
    for (std::int32_t i = 0; i < d.terminal_count; ++i) {
      const tech::TypeMask sd = Builder::sd_types(d, i);
      bool complex = false;
      const TileRef t = perimeter(tile, sd, &complex);
      if (t) {
        return t;
      }
      if (complex) {
        const TileRef deep = device_tiles(tile, sd);
        if (deep) {
          return deep;
        }
      }
    }
    const db::Rect r = b_.area(tile);
    for (std::int32_t i = 0; i < d.terminal_count; ++i) {
      const TileRef t = any_tile(r, Builder::sd_types(d, i));
      if (t) {
        return t;
      }
    }
    return any_tile(r, d.substrate_types);
  }

  /// The four sides of `tile`; `complex` reports a device-tile neighbour.
  /// `push` collects the device tiles not searched yet (the deep search).
  TileRef perimeter(const TileRef& tile, const tech::TypeMask& sd, bool* complex,
                    std::vector<TileRef>* push = nullptr) {
    const db::Rect r = b_.area(tile);
    TileRef found;
    for (const Side& s : kSides) {
      (b_.*s.walk)(tile, [&](const TileRef& tp) {
        const tech::TileType t2 = s.facing(b_.types(tp));
        if (t2 != tech::kSpace && sd.test(t2)) {
          source_ = shared_mid(r, b_.area(tp), s);
          found = tp;
          return true;
        }
        const Id ti = b_.info(tp);
        if (ti != kNil && (b_.g.tiles[ti].status & kTileDev) != 0) {
          if (complex != nullptr) {
            *complex = true;
          }
          if (push != nullptr && (b_.g.tiles[ti].status & kTilePushed) == 0) {
            b_.g.tiles[ti].status |= kTilePushed;
            push->push_back(tp);
          }
        }
        return false;
      });
      if (found) {
        return found;
      }
    }
    return {};
  }

  /// The depth-first search over the device's tiles. Magic leaves `tile`
  /// at the last tile popped; the other-plane search then uses its area.
  TileRef device_tiles(TileRef& tile, const tech::TypeMask& sd) {
    const Id first = b_.info(tile);
    if (first != kNil) {
      b_.g.tiles[first].status |= kTilePushed;
    }
    std::vector<TileRef> stack{tile};
    while (!stack.empty()) {
      tile = stack.back();
      stack.pop_back();
      const TileRef t = perimeter(tile, sd, nullptr, &stack);
      if (t) {
        return t;
      }
    }
    return {};
  }

  /// ResGetTileFunc over every plane: the first tile side of `mask` that is
  /// not space.
  TileRef any_tile(const db::Rect& r, const tech::TypeMask& mask) {
    for (const tech::PlaneId p : b_.user_planes()) {
      TileRef found;
      b_.search(p, r, mask, [&](const TileRef& t, bool side) {
        const TileTypes ty = b_.types(t);
        if (ty.split && ty.left != tech::kSpace && ty.right != tech::kSpace && side) {
          return false;
        }
        const tech::TileType tt = ty.split ? (side ? ty.right : ty.left) : ty.left;
        if (tt != tech::kSpace) {
          found = t;
          return true;
        }
        return false;
      });
      if (found) {
        return found;
      }
    }
    return {};
  }

  Builder& b_;
  db::Point& source_;
};

}  // namespace

// --- ResEachTile and its breakpoints ---

namespace {

class TileStep {
 public:
  TileStep(Builder& b, Id tile) : b_(b), g_(b.g), ti_(tile), ref_(b.tile_of(tile)) {}

  bool run() {
    const TileTypes ty = b_.types(ref_);
    std::uint32_t sides = 0;
    tech::TileType t1 = ty.left;
    if (ty.split) {
      if (ty.left == tech::kSpace) {
        t1 = ty.right;
        sides = kIgnoreLeft | (ty.direction != 0 ? kIgnoreBottom : kIgnoreTop);
      } else {
        sides = kIgnoreRight | (ty.direction != 0 ? kIgnoreTop : kIgnoreBottom);
      }
    }
    gate_node(t1);
    contacts();
    const db::Rect r = b_.area(ref_);
    for (const Side& s : kSides) {
      if ((sides & s.ignore) == 0) {
        side(s, r, t1);
      }
    }
    other_planes();
    g_.tiles[ti_].status |= kTileDone;
    port_breakpoints();
    return b_.calc_tile_resistance(ti_);
  }

 private:
  void gate_node(tech::TileType t1) {
    TileInfo& info = g_.tiles[ti_];
    if (!b_.style_.device_types.test(t1) || (info.status & kTileDev) == 0 || info.device == kNil) {
      return;
    }
    Device& dev = g_.devices[info.device];
    if (dev.terms.empty() || dev.terms[0] != kNil) {
      return;
    }
    const db::Rect r = b_.area(ref_);
    const Id node = g_.new_node(db::Point{(r.x0 + r.x1) >> 1, (r.y0 + r.y1) >> 1}, kNodeJunction);
    g_.devices[info.device].terms[0] = node;
    g_.push(g_.nodes[node].te, info.device);
    g_.add_to_queue(node, g_.pending);
    g_.new_break(ti_, node, g_.nodes[node].loc, kNil);
  }

  void contacts() {
    for (Id c = g_.tiles[ti_].contacts; c != kNil; c = g_.next(c)) {
      const Id cp = g_.item(c);
      if (g_.contacts[cp].cnode[0] == kNil) {
        b_.do_contacts(cp);
      }
    }
    g_.tiles[ti_].contacts = kNil;
  }

  void side(const Side& s, const db::Rect& r, tech::TileType t1) {
    (b_.*s.walk)(ref_, [&](const TileRef& tp) {
      const tech::TileType t2 = s.facing(b_.types(tp));
      if (b_.style_.device_types.test(t2)) {
        terminal(tp, t1, t2, terminal_point(r, b_.area(tp), s), device_edge(s));
      }
      if (t1 < b_.style_.node_conn.size() && b_.style_.node_conn[t1].test(t2)) {
        junction(tp, shared_mid(r, b_.area(tp), s));
      }
      return false;
    });
  }

  /// The first device record of t2 with an S/D class holding t1.
  void terminal(const TileRef& tp, tech::TileType t1, tech::TileType t2, db::Point at, std::uint32_t edge) {
    for (const extract::DevicePattern* d : b_.records(t2)) {
      for (std::int32_t i = 0; i < d->terminal_count; ++i) {
        if (Builder::sd_types(*d, i).test(t1)) {
          new_term_device(b_.info(tp), i, at, edge);
          return;
        }
      }
    }
  }

  /// ResNewTermDevice: the device found through tile `dev_tile`, class n.
  void new_term_device(Id dev_tile, std::int32_t n, db::Point at, std::uint32_t direction) {
    if (dev_tile == kNil || g_.tiles[dev_tile].device == kNil) {
      return;
    }
    const Id device = g_.tiles[dev_tile].device;
    std::vector<Id>& terms = g_.devices[device].terms;
    const auto slot = static_cast<std::size_t>(2 + n);
    if (slot >= terms.size()) {
      return;  // a class past rd_nterms: Magic writes past the array
    }
    Id node = kNil;
    std::size_t created = terms.size();
    if (terms[slot] == kNil) {
      created = slot;
    } else if ((g_.tiles[dev_tile].source_edge & direction) != 0 || terms.size() < 4) {
      node = terms[slot];
    } else if (n == 0 && terms[3] == kNil) {
      created = 3;
    } else if (n == 1 && terms[2] == kNil) {
      created = 2;
    }
    if (created < terms.size()) {
      node = g_.new_node(at, kNodeDevice);
      g_.devices[device].terms[created] = node;
      g_.push(g_.nodes[node].te, device);
      g_.add_to_queue(node, g_.pending);
    }
    if (node != kNil) {
      g_.new_break(ti_, node, at, kNil);
    }
  }

  void new_sub_device(Id dev_tile, db::Point at) {
    if (dev_tile == kNil || g_.tiles[dev_tile].device == kNil) {
      return;
    }
    const Id device = g_.tiles[dev_tile].device;
    if (g_.devices[device].terms.size() < 2) {
      return;
    }
    Id node = g_.devices[device].terms[1];
    if (node == kNil) {
      node = g_.new_node(at, kNodeDevice);
      g_.devices[device].terms[1] = node;
      g_.push(g_.nodes[node].te, device);
      g_.add_to_queue(node, g_.pending);
    }
    g_.new_break(ti_, node, at, kNil);
  }

  /// ResProcessJunction.
  void junction(const TileRef& tp, db::Point at) {
    const Id other = b_.info(tp);
    if (other == kNil || (g_.tiles[other].status & kTileDone) != 0) {
      return;
    }
    const Id node = g_.new_node(at, kNodeJunction);
    g_.junctions.push_back(Junction{{ti_, other}, at, false, node});
    g_.push(g_.nodes[node].je, static_cast<Id>(g_.junctions.size() - 1));
    g_.add_to_queue(node, g_.pending);
    g_.new_break(ti_, node, at, kNil);
    g_.new_break(other, node, at, kNil);
  }

  void other_planes() {
    const auto it = b_.dev_nodes.find(ref_.tile.tile);
    if (it == b_.dev_nodes.end()) {
      return;
    }
    const std::vector<DevTerm> list = std::move(it->second);
    b_.dev_nodes.erase(it);
    for (const DevTerm& d : list) {
      const db::Rect dr = b_.area(d.device_tile);
      const db::Point at{(dr.x1 + dr.x0) / 2, (dr.y1 + dr.y0) / 2};
      if (d.term == -1) {
        new_sub_device(b_.info(d.device_tile), at);
      } else {
        new_term_device(b_.info(d.device_tile), d.term, at, kOtherPlane);
      }
    }
  }

  void port_breakpoints() {
    const std::vector<Port> ports = std::move(g_.tiles[ti_].ports);
    g_.tiles[ti_].ports.clear();
    for (const Port& p : ports) {
      const Id node = g_.new_node(p.loc, kNodeOrigin);
      g_.nodes[node].status = kStatusTrue;
      g_.nodes[node].name = p.name;
      g_.nodes[node].named = true;
      g_.add_to_queue(node, g_.pending);
      g_.new_break(ti_, node, p.loc, kNil);
    }
  }

  Builder& b_;
  Graph& g_;
  Id ti_;
  TileRef ref_;
};

}  // namespace

namespace {

/// ResProcessTiles' pass over one node of the queue: its junctions, then
/// its contacts. True when `n` was merged with another node.
class QueueStep {
 public:
  QueueStep(Builder& b, Id n) : b_(b), g_(b.g), n_(n) {}

  bool run() {
    bool merged = false;
    for (Id c = g_.nodes[n_].je; c != kNil; c = g_.next(c)) {
      merged |= junction(g_.item(c));
    }
    for (Id c = g_.nodes[n_].ce; c != kNil; c = g_.next(c)) {
      merged |= contact(g_.item(c));
    }
    return merged;
  }

 private:
  bool process(Id tile) {
    g_.current = n_;
    return TileStep(b_, tile).run();
  }

  bool junction(Id j) {
    if (g_.junctions[j].handled) {
      return false;
    }
    bool merged = false;
    // A copy: processing a tile adds junctions (the vector may move).
    const std::array<Id, 2> tiles = g_.junctions[j].tile;
    for (const Id tile : tiles) {
      if ((g_.tiles[tile].status & kTileDone) == 0) {
        merged |= process(tile);
      }
    }
    g_.junctions[j].handled = true;
    return merged;
  }

  bool contact(Id cp) {
    if (g_.contacts[cp].handled) {
      return false;
    }
    bool merged = false;
    bool status = true;
    for (std::size_t i = 0; i < static_cast<std::size_t>(g_.contacts[cp].current); ++i) {
      const Id tile = g_.contacts[cp].tile[i];
      if ((g_.tiles[tile].status & kTileDone) != 0) {
        continue;
      }
      if (g_.contacts[cp].cnode[i] == n_) {
        merged |= process(tile);
      } else {
        status = false;
      }
    }
    g_.contacts[cp].handled = status;
    return merged;
  }

  Builder& b_;
  Graph& g_;
  Id n_;
};

/// A node whose tiles are all processed moves to the done list.
void move_to_done(Graph& g, Id n) {
  g.remove_from_queue(n, g.pending);
  Node& node = g.nodes[n];
  node.more = g.done;
  node.less = kNil;
  node.status |= kStatusFinished | kStatusMarked;
  if (g.done != kNil) {
    g.nodes[g.done].less = n;
  }
  if (node.noderes == 0) {
    g.origin = n;
  }
  g.done = n;
  g.clean_node(n, false);
  g.done_with_node(n);
}

}  // namespace

bool Builder::process_tiles() {
  db::Point origin = start;
  StartFinder finder(*this, origin);
  const TileRef first = finder.find(start_type);
  const Id first_info = info(first);
  if (!first || first_info == kNil) {
    return false;
  }
  g.current = kNil;
  // ResStartTile.
  const Id root = g.new_node(origin, kNodeOrigin);
  g.nodes[root].status = kStatusTrue;
  g.nodes[root].noderes = 0;
  g.add_to_queue(root, g.pending);
  g.new_break(first_info, root, origin, kNil);
  g.current = root;
  TileStep(*this, first_info).run();
  while (g.pending != kNil) {
    const Id n = g.pending;
    if (!QueueStep(*this, n).run()) {
      move_to_done(g, n);
    }
  }
  return true;
}

}  // namespace hefesto::extresist::detail
