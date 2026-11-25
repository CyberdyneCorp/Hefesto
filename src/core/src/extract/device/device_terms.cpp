// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Terminal area, perimeter and sharing, and the terminals off the device
// plane. Capability extract-devices "Terminal area, perimeter and
// sharing", "Terminals off the device plane"; openspec/changes/
// m7-extraction/design.md D10.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtBasic.c:2349-2555
// (extOutputDevices' terminal passes), :3760-3947 (extAddSharedDevice,
// extTermAPFunc) and extract/ExtNghbors.c:440-640 (extEnumTerminal),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTDEV.

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "device_internal.hpp"

namespace hefesto::extract::device_detail {

namespace {

struct AreaPerim {
  std::int64_t area = 0;
  std::int64_t perim = 0;
  std::set<std::size_t> shared;  // the gate nodes of other devices bordering the terminal
};

// The side of a split neighbour across `s` that faces the tile: the right
// side of a left neighbour, the left of a right one, by the diagonal above
// and below.
bool facing_side(db::Side s, const db::TileView& t2) {
  switch (s) {
    case db::Side::left:
      return true;
    case db::Side::right:
      return false;
    case db::Side::bottom:
      return split_direction(t2);
    case db::Side::top:
      break;
  }
  return !split_direction(t2);
}

// extEnumTerminal with extTermAPFunc.
struct TermFlood {
  const Context& ctx;
  tech::PlaneId pnum;
  tech::TypeMask mask;            // eapd_mask: the types that bound the terminal
  const TileSet* self = nullptr;  // the device's own tiles (eapd_gatenode); nullptr: no sharing count
  AreaPerim ap;

  [[nodiscard]] const db::Plane& plane() const { return ctx.cell.plane(pnum); }

  void count_gate(const db::TileView& view) {
    if (self == nullptr || !ctx.style.device_types.test(view.type) || self->contains(key_of(pnum, view))) {
      return;
    }
    if (const NodeRef n = ctx.node(pnum, view)) {
      ap.shared.insert(*n);
    }
  }

  void side(const db::TileView& tile, db::Side side, bool skip) {
    std::vector<db::TileView> out;
    const bool vertical = side == db::Side::left || side == db::Side::right;
    plane().neighbors(tile, side, vertical ? tile.area.y0 : tile.area.x0, vertical ? tile.area.y1 : tile.area.x1,
                      out);
    for (const db::TileView& tp : out) {
      db::TileView view = tp;
      std::int64_t overlap = 0;
      switch (side) {
        case db::Side::top:
          view = side_view(tp, !split_direction(tp));
          overlap = std::min(tile.area.x1, tp.area.x1) - std::max(tile.area.x0, tp.area.x0);
          break;
        case db::Side::bottom:
          view = side_view(tp, split_direction(tp));
          overlap = std::min(tile.area.x1, tp.area.x1) - std::max(tile.area.x0, tp.area.x0);
          break;
        case db::Side::left:
          view = side_view(tp, true);
          overlap = std::min(tile.area.y1, tp.area.y1) - std::max(tile.area.y0, tp.area.y0);
          break;
        case db::Side::right:
          view = side_view(tp, false);
          overlap = std::min(tile.area.y1, tp.area.y1) - std::max(tile.area.y0, tp.area.y0);
          break;
      }
      if (!tp.split) {
        view = tp;
      }
      if (!mask.test(view.type)) {
        continue;
      }
      if (!skip) {
        ap.perim += overlap;
      }
      count_gate(view);
    }
  }

  // extTermAPFunc.
  void visit(const db::TileView& tile) {
    const std::int64_t area = tile.area.width() * tile.area.height();
    if (tile.split) {
      const double w = static_cast<double>(tile.area.width());
      const double h = static_cast<double>(tile.area.height());
      ap.perim += static_cast<std::int64_t>(std::sqrt((w * w) + (h * h)));
      ap.area += area / 2;
    } else {
      ap.area += area;
    }
    const SkippedSides skip = skipped_sides(tile);
    side(tile, db::Side::top, skip.top);
    side(tile, db::Side::bottom, skip.bottom);
    side(tile, db::Side::left, skip.left);
    side(tile, db::Side::right, skip.right);
  }

  // The tiles of the terminal node connected to `tile` on this plane.
  void flood(const db::TileView& start) {
    const NodeRef termreg = ctx.node(pnum, start);
    TileSet seen{key_of(pnum, start)};
    std::vector<db::TileView> pending{start};
    while (!pending.empty()) {
      const db::TileView tile = pending.back();
      pending.pop_back();
      visit(tile);
      const tech::TypeMask& conn = ctx.tech.connects_to(tile.type);
      const auto offer = [&](const db::TileView& view) {
        if (conn.test(view.type) && ctx.node(pnum, view) == termreg && seen.insert(key_of(pnum, view)).second) {
          pending.push_back(view);
        }
      };
      for_each_neighbor(tile, offer);
    }
  }

  // The neighbours on the sides extEnumTerminal walks (left, bottom, right,
  // top; a split tile only on its own side of the diagonal), each as the
  // side facing `tile`.
  template <typename F>
  void for_each_neighbor(const db::TileView& tile, const F& offer) const {
    const SkippedSides skip = skipped_sides(tile);
    std::vector<db::TileView> out;
    for (const auto& [side, skipped] : {std::pair{db::Side::left, skip.left}, std::pair{db::Side::bottom, skip.bottom},
                                        std::pair{db::Side::right, skip.right}, std::pair{db::Side::top, skip.top}}) {
      if (skipped) {
        continue;
      }
      const bool vertical = side == db::Side::left || side == db::Side::right;
      plane().neighbors(tile, side, vertical ? tile.area.y0 : tile.area.x0, vertical ? tile.area.y1 : tile.area.x1,
                        out);
      for (const db::TileView& t2 : out) {
        offer(t2.split ? side_view(t2, facing_side(side, t2)) : t2);
      }
    }
  }
};

}  // namespace

void terminal_areas(Context& ctx, const std::vector<PlaneTile>& tiles, TransRec& tr) {
  TileSet self;
  for (const auto& [p, t] : tiles) {
    self.insert(key_of(p, t));
  }
  for (Terminal& term : tr.terms) {
    if (!term.pos.tile) {
      continue;
    }
    TermFlood f{ctx, term.pos.tile_plane, ~ctx.tech.connects_to(term.pos.type), &self, {}};
    f.flood(*term.pos.tile);
    term.area = f.ap.area;
    term.perim = f.ap.perim;
    term.shared = 1 + static_cast<std::int64_t>(f.ap.shared.size());
  }
}

namespace {

// The first node of `tmask` under the region: its representative tile,
// then (when the region is larger) every tile in Magic's extSpecialDevice
// order (the reverse of the walk).
Found off_plane_node(const Context& ctx, const DeviceRegionTiles& reg, const std::vector<PlaneTile>& tiles,
                     const tech::TypeMask& tmask, db::Rect& under) {
  Found f = find_subs(ctx, reg.first, tmask);
  under = reg.first.area;
  if (f.node || reg.first.area.width() * reg.first.area.height() >= reg.region.area) {
    return f;
  }
  for (auto it = tiles.rbegin(); it != tiles.rend(); ++it) {
    f = find_subs(ctx, it->second, tmask);
    if (f.node) {
      under = it->second.area;
      return f;
    }
  }
  return f;
}

// The area and perimeter of the material of `tmask` under `under` on the
// plane of `layer` (extTransFindTermArea), no sharing.
AreaPerim off_plane_area(const Context& ctx, tech::TileType layer, const db::Rect& under,
                         const tech::TypeMask& tmask) {
  const tech::PlaneId pnum = ctx.home(layer);
  TermFlood f{ctx, pnum, ~tmask, nullptr, {}};
  (void)ctx.cell.plane(pnum).enumerate(under, tmask, [&](const db::TileView& t) {
    f.flood(t);
    return false;
  });
  return f.ap;
}

enum class Step : std::uint8_t { found, next_record };

// One terminal not yet found by the perimeter scan.
Step off_plane_terminal(Context& ctx, const DeviceRegionTiles& reg, const std::vector<PlaneTile>& tiles,
                        TransRec& tr, std::size_t termcount, const tech::TypeMask& tmask) {
  db::Rect under;
  Found f = off_plane_node(ctx, reg, tiles, tmask, under);
  if (!f.node) {
    if (!tmask.test(tech::kSpace)) {
      return Step::next_record;
    }
    f.node = ctx.in.substrate_node;  // the device node is the global substrate
  }
  tr.match |= kMatchTerm << termcount;
  Terminal& term = tr.terms[termcount];
  term.node = f.node;
  if (f.layer == tech::kSpace) {
    term.area = 0;  // the substrate: no area or perimeter
    term.perim = 0;
  } else {
    const AreaPerim ap = off_plane_area(ctx, f.layer, under, tmask);
    term.area = ap.area;
    term.perim = ap.perim;
  }
  term.shared = 1;
  return Step::found;
}

// How one pass over a record's terminals ended.
enum class Pass : std::uint8_t { complete, tied, next_record };

// One pass of the terminal loop for record `devptr`; `termcount` is where
// it stopped.
Pass terminal_pass(Context& ctx, const DeviceRegionTiles& reg, const std::vector<PlaneTile>& tiles, TransRec& tr,
                   const DevicePattern& devptr, std::size_t nsd, std::size_t& termcount) {
  const tech::TypeMask device_plane = ctx.tech.plane_types(reg.region.plane);
  for (termcount = 0; termcount < nsd; ++termcount) {
    if ((tr.match & (kMatchTerm << termcount)) != 0) {
      continue;  // found by the perimeter scan
    }
    const tech::TypeMask& tmask = sd_types(devptr, termcount);
    if (tmask.empty()) {
      return Pass::tied;  // tied source and drain (a MOS capacitor)
    }
    const bool off_plane = !tmask.intersects(device_plane) || tmask.test(tech::kSpace);
    if (!off_plane || off_plane_terminal(ctx, reg, tiles, tr, termcount, tmask) == Step::next_record) {
      return Pass::next_record;
    }
  }
  return Pass::complete;
}

}  // namespace

const DevicePattern* off_plane_terminals(Context& ctx, const DeviceRegionTiles& reg,
                                         const std::vector<PlaneTile>& tiles, TransRec& tr, std::size_t& nsd) {
  const DevicePattern* devptr = tr.rec;
  const DevicePattern* deventry = devptr;
  std::size_t termcount = 0;
  while (devptr != nullptr) {
    nsd = std::min<std::size_t>(static_cast<std::size_t>(std::max(devptr->terminal_count, 0)), kMaxTerms);
    if (terminal_pass(ctx, reg, tiles, tr, *devptr, nsd, termcount) != Pass::next_record) {
      break;  // all terminals accounted for, or legally tied
    }
    // Another record with different terminal types, if any.
    devptr = find_match(ctx, tr, devptr, reg.region.type);
    if (devptr == deventry) {
      break;
    }
  }
  tr.nterm = termcount;
  return devptr;
}

}  // namespace hefesto::extract::device_detail
