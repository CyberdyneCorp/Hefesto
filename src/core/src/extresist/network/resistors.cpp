// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The resistors of a processed tile (capability extresist-network "Tile
// resistors", "Near-device cutting", "Contact resistors"):
// ResCalcTileResistance, ResCalcEastWest, ResCalcNorthSouth,
// ResCalcNearDevice (Q-NETLIST-43: with magic-compat OFF a lone device
// breakpoint keeps the tile's area) and ResDoContacts. Breakpoint sorts are
// stable, as Magic's bubble and merge sorts are. Values are single
// precision, areas are integer products added to single-precision node
// areas, as in Magic.
// (Magic ref: resis/ResMakeRes.c:55-1232, cif/CIFtech.c:2306-2382)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "extresist/network/builder.hpp"
#include "hefesto/mask/style/style.hpp"

namespace hefesto::extresist::detail {

namespace {

/// The axis a cut runs along: x for east-west, y for north-south.
struct Axis {
  bool x;

  [[nodiscard]] db::Coord of(db::Point p) const { return x ? p.x : p.y; }
  [[nodiscard]] db::Coord lo(const db::Rect& r) const { return x ? r.x0 : r.y0; }
  [[nodiscard]] db::Coord hi(const db::Rect& r) const { return x ? r.x1 : r.y1; }
  /// The tile's extent across the cut (height for east-west).
  [[nodiscard]] std::int64_t across(const db::Rect& r) const { return x ? r.y1 - r.y0 : r.x1 - r.x0; }
  [[nodiscard]] db::Coord centerline(const db::Rect& r) const { return x ? (r.y1 + r.y0) >> 1 : (r.x0 + r.x1) >> 1; }
};

void sort_breaks(std::vector<Breakpoint>& breaks, Axis axis) {
  std::stable_sort(breaks.begin(), breaks.end(),
                   [&](const Breakpoint& a, const Breakpoint& b) { return axis.of(a.loc) < axis.of(b.loc); });
}

bool is_device(const Graph& g, const Breakpoint& b) {
  return g.nodes[b.node].why == kNodeDevice;
}

/// The cut of ResCalcEastWest (axis x) and ResCalcNorthSouth (axis y).
class Cut {
 public:
  Cut(Builder& b, Id tile, Axis axis) : b_(b), g_(b.g), ti_(tile), axis_(axis), r_(b.area(b.tile_of(tile))) {}

  bool run() {
    auto& breaks = g_.tiles[ti_].breaks;
    const std::int64_t across = axis_.across(r_);
    if (breaks.size() == 1) {
      add_area(breaks.front().node, across * (axis_.hi(r_) - axis_.lo(r_)));
      breaks.clear();
      return false;
    }
    const TileTypes ty = b_.types(b_.tile_of(ti_));
    type_ = ty.split ? (ty.left == tech::kSpace ? ty.right : ty.left) : ty.left;
    status_ =
        ty.split ? (kStatusDiagonal | (ty.direction != 0 ? kStatusNS : kStatusEW)) : (axis_.x ? kStatusEW : kStatusNS);
    sort_breaks(breaks, axis_);
    std::vector<Breakpoint> list = std::move(breaks);
    breaks.clear();
    add_area(list.front().node, across * (axis_.of(list.front().loc) - axis_.lo(r_)));
    std::size_t cur = 0;
    while (cur + 1 < list.size()) {
      cur = step(list, cur);
    }
    add_area(list[cur].node, across * (axis_.hi(r_) - axis_.of(list[cur].loc)));
    return merged_;
  }

 private:
  void add_area(Id node, std::int64_t area) { g_.nodes[node].area += static_cast<float>(area); }

  /// One pair (list[cur], list[cur + 1]); returns the next `cur`.
  std::size_t step(std::vector<Breakpoint>& list, std::size_t cur) {
    const std::size_t p1 = cur;
    const std::size_t p2 = cur + 1;
    if (axis_.of(list[p1].loc) != axis_.of(list[p2].loc)) {
      resistor(list[p1], list[p2]);
      return p2;
    }
    const Id n1 = list[p1].node;
    const Id n2 = list[p2].node;
    if (n1 == n2) {
      list.erase(list.begin() + static_cast<std::ptrdiff_t>(p2));
      return p1;
    }
    std::size_t next = p2;
    Id gone = n1;
    if (n2 == g_.current) {
      g_.merge_nodes(n2, n1);
      merged_ = true;
    } else if (n1 == g_.current) {
      gone = n2;
      list.erase(list.begin() + static_cast<std::ptrdiff_t>(p2));
      g_.merge_nodes(n1, n2);
      merged_ = true;
      next = p1;
    } else {
      g_.merge_nodes(n2, n1);
    }
    for (std::size_t i = next + 1; i < list.size(); ++i) {
      if (list[i].node == gone) {
        list[i].node = list[next].node;
      }
    }
    return next;
  }

  void resistor(const Breakpoint& p1, const Breakpoint& p2) {
    const Id r = g_.new_resistor(p1.node, p2.node);
    Resistor& res = g_.res[r];
    const std::int64_t across = axis_.across(r_);
    const std::int64_t length = axis_.of(p2.loc) - axis_.of(p1.loc);
    res.cl = axis_.centerline(r_);
    res.width = static_cast<std::int32_t>(across);
    res.tt = type_;
    res.status = status_;
    res.value =
        static_cast<float>(b_.style_.sheet_resistance[type_]) * static_cast<float>(length) / static_cast<float>(across);
    const auto half = static_cast<float>((length * across) / 2);
    g_.nodes[p1.node].area += half;
    g_.nodes[p2.node].area += half;
    res.area = 0.0F;
  }

  Builder& b_;
  Graph& g_;
  Id ti_;
  Axis axis_;
  db::Rect r_;
  tech::TileType type_ = 0;
  std::uint32_t status_ = 0;
  bool merged_ = false;
};

/// The distance from the device breakpoint to its neighbours along one
/// axis, in the list's current (sorted) order.
std::int64_t device_gap(const Graph& g, const std::vector<Breakpoint>& list, Axis axis) {
  std::size_t p1 = 0;
  std::size_t p2 = list.size();
  for (; p1 < list.size(); ++p1) {
    if (is_device(g, list[p1])) {
      break;
    }
    if (p1 + 1 < list.size() && list[p1].loc != list[p1 + 1].loc) {
      p2 = p1;
    }
  }
  if (p1 == list.size()) {
    return kInfinity;
  }
  const db::Coord at = axis.of(list[p1].loc);
  const auto distance = [&](const Breakpoint& b) -> std::int64_t {
    if (b.crect == kNil) {
      return std::abs(static_cast<std::int64_t>(at) - axis.of(b.loc));
    }
    const db::Rect& c = g.contacts[b.crect].rect;
    if (axis.lo(c) > at) {
      return axis.lo(c) - at;
    }
    return axis.hi(c) < at ? at - axis.hi(c) : 0;
  };
  std::int64_t delta = kInfinity;
  std::size_t p3 = p1 + 1;
  while (p3 < list.size() && list[p3].loc == list[p1].loc) {
    ++p3;
  }
  if (p3 < list.size()) {
    delta = distance(list[p3]);
  }
  if (p2 < list.size()) {
    delta = std::min(delta, distance(list[p2]));
  }
  return delta;
}

/// Deletes every other breakpoint of a device breakpoint's node that is
/// not on the cut's two edges (the duplicate S/D pointers).
void drop_duplicates(const Graph& g, std::vector<Breakpoint>& list, Axis edge_axis, const db::Rect& r) {
  std::vector<bool> gone(list.size(), false);
  const auto on_edge = [&](const Breakpoint& b) {
    return edge_axis.of(b.loc) == edge_axis.lo(r) || edge_axis.of(b.loc) == edge_axis.hi(r);
  };
  for (std::size_t i = 0; i < list.size(); ++i) {
    if (gone[i] || !is_device(g, list[i]) || !on_edge(list[i])) {
      continue;
    }
    for (std::size_t j = 0; j < list.size(); ++j) {
      if (j != i && !gone[j] && list[j].node == list[i].node && !on_edge(list[j])) {
        gone[j] = true;
      }
    }
  }
  std::vector<Breakpoint> kept;
  for (std::size_t i = 0; i < list.size(); ++i) {
    if (!gone[i]) {
      kept.push_back(list[i]);
    }
  }
  list = std::move(kept);
}

}  // namespace

bool Builder::calc_tile_resistance(Id tile) {
  auto& breaks = g.tiles[tile].breaks;
  if (breaks.empty()) {
    return false;
  }
  db::Coord max_x = -kInfinity;
  db::Coord min_x = kInfinity;
  db::Coord max_y = -kInfinity;
  db::Coord min_y = kInfinity;
  bool device = false;
  for (const Breakpoint& b : breaks) {
    max_x = std::max(max_x, b.loc.x);
    min_x = std::min(min_x, b.loc.x);
    max_y = std::max(max_y, b.loc.y);
    min_y = std::min(min_y, b.loc.y);
    device = device || is_device(g, b);
  }
  if (device) {
    return near_device(tile);
  }
  return Cut(*this, tile, Axis{max_y - min_y <= max_x - min_x}).run();
}

bool Builder::near_device(Id tile) {
  auto& breaks = g.tiles[tile].breaks;
  const db::Rect r = area(tile_of(tile));
  if (breaks.size() == 1) {
    // Q-NETLIST-43: Magic drops the lone breakpoint and its tile's area.
    if (!input.magic_compat) {
      g.nodes[breaks.front().node].area += static_cast<float>(r.width() * r.height());
    }
    breaks.clear();
    return false;
  }
  int count = 0;
  std::uint32_t edges = 0;
  for (const Breakpoint& b : breaks) {
    if (!is_device(g, b)) {
      continue;
    }
    ++count;
    if (b.loc.x == r.x0) {
      edges |= kLeftEdge;
    } else if (b.loc.x == r.x1) {
      edges |= kRightEdge;
    } else if (b.loc.y == r.y1) {
      edges |= kTopEdge;
    } else if (b.loc.y == r.y0) {
      edges |= kBottomEdge;
    }
  }
  const auto one_edge = [&](std::uint32_t e) {
    return (edges & e) == edges;
  };
  if (count == 1 || one_edge(kLeftEdge) || one_edge(kRightEdge) || one_edge(kTopEdge) || one_edge(kBottomEdge)) {
    sort_breaks(breaks, Axis{true});
    const std::int64_t dx = device_gap(g, breaks, Axis{true});
    sort_breaks(breaks, Axis{false});
    const std::int64_t dy = device_gap(g, breaks, Axis{false});
    return Cut(*this, tile, Axis{!(dy > dx)}).run();
  }
  const bool top_bottom = (edges & kTopEdge) != 0 || (edges & kBottomEdge) != 0;
  const bool left_right = (edges & kLeftEdge) != 0 || (edges & kRightEdge) != 0;
  const bool north_south = ((edges & kTopEdge) != 0 && (edges & kBottomEdge) != 0 && !left_right) ||
                           (top_bottom && left_right && r.width() > r.height());
  if (north_south) {
    sort_breaks(breaks, Axis{false});
    drop_duplicates(g, breaks, Axis{false}, r);
    return Cut(*this, tile, Axis{false}).run();
  }
  drop_duplicates(g, breaks, Axis{true}, r);
  return Cut(*this, tile, Axis{true}).run();
}

std::pair<std::int32_t, std::int32_t> Builder::via_counts(Id contact, std::int64_t minside, std::int64_t pitch,
                                                          std::int64_t cscale) {
  const Contact& c = g.contacts[contact];
  if (c.width * cscale < minside || c.height * cscale < minside) {
    if (input.small_contact_warned == nullptr || !*input.small_contact_warned) {
      g.message("Warning: " + type_name(c.type) + " at " + std::to_string(c.center.x) + " " +
                std::to_string(c.center.y) + " smaller than extract section allows");
      if (input.small_contact_warned != nullptr) {
        *input.small_contact_warned = true;
      }
    }
    return {1, 1};
  }
  const auto step = static_cast<float>(pitch);
  return {static_cast<std::int32_t>(static_cast<float>(c.width * cscale - minside) / step) + 1,
          static_cast<std::int32_t>(static_cast<float>(c.height * cscale - minside) / step) + 1};
}

void Builder::do_contacts(Id contact) {
  Contact& c = g.contacts[contact];
  std::optional<mask::OutputStyle::ContactCut> cut;
  if (input.cif != nullptr) {
    cut = input.cif->contact_cut(c.type);
  }
  // CIFGetContactSize leaves the cut size unset when no cut is found (Magic
  // then reads an uninitialised value); no cut is a contact without
  // resistance here.
  const std::int64_t minside = cut ? cut->size() : 0;
  const std::int64_t viawidth = cut ? cut->edge : 0;
  const std::int64_t spacing = cut ? cut->spacing : 0;
  const std::int64_t cscale = input.cif != nullptr ? input.cif->scale.scale_factor : 1;
  const std::int32_t via = style_.via_resistance[c.type];
  if (via == 0 || viawidth == 0) {
    const Id node = g.new_node(c.center, kNodeContact);
    g.add_to_queue(node, g.pending);
    g.push(g.nodes[node].ce, contact);
    for (std::int32_t i = 0; i < c.current; ++i) {
      g.contacts[contact].cnode[static_cast<std::size_t>(i)] = node;
      g.new_break(g.contacts[contact].tile[static_cast<std::size_t>(i)], node, c.center, contact);
    }
    return;
  }
  const auto [nx, ny] = via_counts(contact, minside, viawidth + spacing, cscale);
  for (std::int32_t i = 0; i < c.current; ++i) {
    const auto at = static_cast<std::size_t>(i);
    const Id node = g.new_node(c.center, kNodeContact);
    g.add_to_queue(node, g.pending);
    g.push(g.nodes[node].ce, contact);
    g.contacts[contact].cnode[at] = node;
    g.new_break(g.contacts[contact].tile[at], node, c.center, contact);
    if (i == 0) {
      continue;
    }
    const Id r = g.new_resistor(g.contacts[contact].cnode[at - 1], node);
    Resistor& res = g.res[r];
    res.cl = ny;
    res.width = nx;
    res.value = static_cast<float>(via) / static_cast<float>(nx * ny);
    res.tt = c.type;
    res.area = 0.0F;
    res.status = 0;
  }
}

}  // namespace hefesto::extresist::detail
