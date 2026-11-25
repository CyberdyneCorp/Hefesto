// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ResExtractNet's preparation of the copied net (capability
// extresist-network "Net geometry collection" steps 2-5): the device areas
// painted from the source cell (resMakeDevFunc, resExpandDevFunc,
// ResCalcPerimOverlap), the upper contact layers shaved (ResShaveContacts),
// the contact points found (ExtFindRegions with ResFirst and ResEach over
// ResConnectWithSD), the contacts dissolved (ResDissolveContacts) and every
// plane fractured (ResFracture).
//
// The contact walk is ExtFindRegions/ExtFindNeighbors itself rather than
// EXTNODE's RegionFinder: Magic's region pointer here is the head of the
// contact list, which stays NULL until the first contact is found, so a
// region started before that is not expanded (its first tile equals the
// NULL "pending" mark). That decides the order of the contact list, which
// RegionFinder's callbacks cannot express.
// (Magic ref: resis/ResMain.c:120-230, :855-1240, extract/ExtRegion.c:142-222,
// extract/ExtNghbors.c:74-420, resis/ResUtils.c:48-100)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "extresist/network/builder.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/plane.hpp"

namespace hefesto::extresist::detail {

namespace {

tech::TypeMask all_but_space() {
  tech::TypeMask m;
  for (tech::TileType t = tech::kFirstUserType; t < tech::kMaxTileTypes; ++t) {
    m.set(t);
  }
  return m;
}

/// The source-cell side of a tile view: TiGetTopType and friends of the
/// neighbour views Plane::neighbors reports.
tech::TileType facing_type(const db::TileView& v, db::Side side_of_neighbour) {
  if (!v.split) {
    return v.type;
  }
  const bool dir = v.diagonal == db::Diagonal::nw_se;
  switch (side_of_neighbour) {
    case db::Side::top:  // a top neighbour faces with its bottom type
      return dir ? v.left_type : v.right_type;
    case db::Side::bottom:
      return dir ? v.right_type : v.left_type;
    case db::Side::right:
      return v.left_type;
    case db::Side::left:
      return v.right_type;
  }
  return v.type;
}

std::pair<db::Coord, db::Coord> side_range(const db::Rect& r, db::Side side) {
  return side == db::Side::left || side == db::Side::right ? std::pair{r.y0, r.y1} : std::pair{r.x0, r.x1};
}

}  // namespace

// --- devices (resMakeDevFunc, resExpandDevFunc, ResCalcPerimOverlap) ---

namespace {

struct DeviceCopy {
  Builder& b;
  const db::CellDef& src;

  [[nodiscard]] const extract::DevicePattern* record(tech::TileType type, const std::string& model) const {
    const auto recs = b.records(type);
    for (const auto* r : recs) {
      if (r->model == model) {
        return r;
      }
    }
    return recs.empty() ? nullptr : recs.front();
  }

  /// resMakeDevFunc on one tile side: false to keep searching.
  bool make(const db::TileView& v, DevTile& dev) const {
    db::Rect area = v.area;
    tech::TileType ttype = v.type;
    if (v.split && !b.style_.device_types.test(v.left_type) && b.style_.device_types.test(v.right_type)) {
      db::Coord hh = area.y1 - area.y0;
      db::Coord hw = area.x1 - area.x0;
      hh = ((hh & 1) != 0 ? hh + 1 : hh) >> 1;
      hw = ((hw & 1) != 0 ? hw + 1 : hw) >> 1;
      ttype = v.right_type;
      area.x0 += hw;
      if (v.diagonal == db::Diagonal::nw_se) {
        area.y0 += hh;
      } else {
        area.y1 -= hh;
      }
    } else if (v.split) {
      ttype = v.right_side ? v.right_type : v.left_type;
    }
    dev.area = area;
    if (ttype != dev.type) {
      if (b.tech_.home_plane(ttype) != b.tech_.home_plane(dev.type)) {
        return false;
      }
      dev.type = ttype;
    }
    return true;
  }

  bool find(db::Point at, DevTile& dev) const {
    const db::Rect unit{at.x, at.y, at.x + 1, at.y + 1};
    const tech::TypeMask mask = all_but_space();
    for (std::size_t p = 0; p < src.plane_count(); ++p) {
      bool found = false;
      src.plane(static_cast<tech::PlaneId>(p)).enumerate(unit, mask, [&](const db::TileView& v) {
        found = make(v, dev);
        return !found;
      });
      if (found) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] bool part_of(tech::TileType t, tech::TileType device) const {
    return t == device || (b.tech_.is_contact(t) && b.tech_.contact(t)->residues.test(device));
  }

  void perim_overlap(const db::Plane& plane, const db::TileView& tile, DevTile& dev) const {
    const db::Rect& r = tile.area;
    dev.perim = static_cast<std::int32_t>((r.y1 - r.y0 - r.x0 + r.x1) << 1);
    const tech::TileType t1 = tile.split ? tile.left_type : tile.type;
    const tech::TypeMask omask = t1 < b.style_.node_conn.size() ? b.style_.node_conn[t1] : tech::TypeMask{};
    std::int32_t overlap = 0;
    std::vector<db::TileView> nbrs;
    for (const db::Side side : {db::Side::left, db::Side::right, db::Side::top, db::Side::bottom}) {
      const auto [lo, hi] = side_range(r, side);
      plane.neighbors(tile, side, lo, hi, nbrs);
      const bool vertical = side == db::Side::left || side == db::Side::right;
      for (const db::TileView& n : nbrs) {
        if (!omask.test(facing_type(n, side))) {
          continue;
        }
        overlap += vertical ? std::min(r.y1, n.area.y1) - std::max(r.y0, n.area.y0)
                            : std::min(r.x1, n.area.x1) - std::max(r.x0, n.area.x0);
      }
    }
    dev.overlap += overlap;
  }

  void paint(const db::TileView& tp, bool right_side, const DevTile& dev) const {
    const auto plane = b.tech_.home_plane(dev.type);
    if (!plane) {
      return;
    }
    const db::PaintRow row = db::paint_row(b.tech_, *plane, dev.type);
    db::Plane& dst = b.def.plane(*plane);
    if (tp.split) {
      (void)dst.paint_triangle(tp.area, tp.diagonal, right_side, row, b.input.magic_compat, nullptr,
                               db::PaintMerge::none);
    } else {
      (void)dst.paint(tp.area, row, db::PaintMerge::none);
    }
  }

  /// resExpandDevFunc from the first tile of the device type in its area.
  void expand(DevTile& dev) const {
    for (std::size_t p = 0; p < src.plane_count(); ++p) {
      const db::Plane& plane = src.plane(static_cast<tech::PlaneId>(p));
      std::optional<db::TileView> first;
      plane.enumerate(dev.area, tech::TypeMask::of(dev.type), [&](const db::TileView& v) {
        first = v;
        return false;
      });
      if (first) {
        flood(plane, *first, dev);
        return;
      }
    }
  }

  void flood(const db::Plane& plane, const db::TileView& start, DevTile& dev) const {
    std::unordered_set<const void*> seen{start.tile};
    std::vector<db::TileView> stack{start};
    std::vector<db::TileView> nbrs;
    while (!stack.empty()) {
      const db::TileView tp = stack.back();
      stack.pop_back();
      std::uint32_t ignore = 0;  // the hypotenuse-adjacent sides of a split tile
      bool right_side = false;
      if (tp.split) {
        const bool dir = tp.diagonal == db::Diagonal::nw_se;
        right_side = !part_of(tp.left_type, dev.type);
        ignore =
            right_side ? (kLeftEdge | (dir ? kBottomEdge : kTopEdge)) : (kRightEdge | (dir ? kTopEdge : kBottomEdge));
      }
      paint(tp, right_side, dev);
      perim_overlap(plane, tp, dev);
      for (const auto [side, edge] : {std::pair{db::Side::top, kTopEdge}, std::pair{db::Side::bottom, kBottomEdge},
                                      std::pair{db::Side::right, kRightEdge}, std::pair{db::Side::left, kLeftEdge}}) {
        if ((ignore & edge) != 0) {
          continue;
        }
        const auto [lo, hi] = side_range(tp.area, side);
        plane.neighbors(tp, side, lo, hi, nbrs);
        for (const db::TileView& n : nbrs) {
          if (!seen.contains(n.tile) && part_of(facing_type(n, side), dev.type)) {
            seen.insert(n.tile);
            stack.push_back(n);
          }
        }
      }
    }
  }
};

}  // namespace

void Builder::copy_devices() {
  if (geo.source == nullptr) {
    return;
  }
  DeviceCopy copy{*this, *geo.source};
  for (std::size_t i = 0; i < in.devices.size(); ++i) {
    const NetDevice& d = in.devices[i];
    DevTile dev;
    dev.type = d.type;
    dev.record = copy.record(d.type, d.model);
    if (!copy.find(d.location, dev)) {
      g.message("No device of type " + type_name(dev.type) + " found at location " + std::to_string(d.location.x) +
                ", " + std::to_string(d.location.y));
      continue;
    }
    if (dev.type != d.type) {
      dev.record = copy.record(dev.type, dev.record != nullptr ? dev.record->model : d.model);
    }
    if (dev.record == nullptr) {
      continue;
    }
    copy.expand(dev);
    dev_tiles.insert(dev_tiles.begin(), dev);
    dev_tile_input.insert(dev_tile_input.begin(), i);
  }
}

// --- contacts: shaving, finding, dissolving ---

void Builder::shave_contacts() {
  const tech::TypeMask mask = all_but_space();
  for (const tech::PlaneId p : user_planes()) {
    std::vector<std::pair<db::Rect, tech::TileType>> found;
    def.plane(p).enumerate(bbox, mask, [&](const db::TileView& v) {
      if (v.split && v.left_type != tech::kSpace && v.right_type != tech::kSpace && v.right_side) {
        return true;
      }
      const tech::TileType t = v.split ? (v.right_side ? v.right_type : v.left_type) : v.type;
      if (tech_.is_contact(t)) {
        found.emplace_back(v.area, t);
      }
      return true;
    });
    for (const auto& [rect, t] : found) {
      const tech::PlaneMask pmask = tech_.type_planes(t);
      bool first = true;
      for (const tech::PlaneId q : user_planes()) {
        if ((pmask & tech::plane_bit(q)) == 0) {
          continue;
        }
        if (!first) {
          (void)def.plane(q).paint(rect, db::erase_row(tech_, q, t), db::PaintMerge::none);
        }
        first = false;
      }
    }
  }
}

namespace {

constexpr std::uintptr_t kPendingMark = 0;  // VISITPENDING: Magic's NULL region

/// ExtFindRegions(ResUse, bbox, DBAllButSpaceAndDRCBits, ResConnectWithSD,
/// ResFirst, ResEach) on the scratch planes.
class ContactWalk {
 public:
  ContactWalk(Builder& b, std::vector<tech::TypeMask> conn) : b_(b), conn_(std::move(conn)) {}

  void run() {
    const tech::TypeMask mask = all_but_space();
    for (const tech::PlaneId p : b_.user_planes()) {
      b_.search(p, b_.bbox, mask, [&](const TileRef& t, bool side) {
        if (!visited(t, side)) {
          first(t, side);
          neighbours(t, side);
        }
        return false;
      });
    }
  }

  std::vector<Contact> found;  // in discovery order (ResContactList is the reverse)

 private:
  struct Key {
    const void* tile;
    bool side;
    friend bool operator==(const Key&, const Key&) = default;
  };
  struct KeyHash {
    std::size_t operator()(const Key& k) const noexcept {
      return std::hash<const void*>{}(k.tile) ^ (k.side ? 0x9e3779b9U : 0U);
    }
  };
  struct Item {
    TileRef tile;
    bool side;
  };

  [[nodiscard]] Key key(const TileRef& t, bool side) { return Key{t.tile.tile, b_.types(t).split && side}; }
  [[nodiscard]] bool visited(const TileRef& t, bool side) { return region_.contains(key(t, side)); }
  [[nodiscard]] std::uintptr_t region(const TileRef& t, bool side) {
    const auto it = region_.find(key(t, side));
    return it == region_.end() ? db::kClientDefault : it->second;
  }
  [[nodiscard]] std::uintptr_t current() const { return head_ == kNil ? kPendingMark : head_ + 1U; }

  void push(const TileRef& t, bool side) {
    region_[key(t, side)] = kPendingMark;
    stack_.push_back(Item{t, side});
  }

  tech::TileType side_type(const TileRef& t, bool side) {
    const TileTypes ty = b_.types(t);
    return ty.split && side ? ty.right : ty.left;
  }

  void first(const TileRef& t, bool side) {  // ResFirst
    const tech::TileType type = side_type(t, side);
    if (!b_.tech_.is_contact(type)) {
      return;
    }
    const db::Rect r = b_.area(t);
    Contact c;
    c.center = db::Point{(r.x0 + r.x1) >> 1, (r.y0 + r.y1) >> 1};
    c.type = type;
    c.width = r.x1 - r.x0;
    c.height = r.y1 - r.y0;
    c.rect = r;
    found.push_back(c);
    found_tiles_.push_back(t.tile.tile);
    head_ = static_cast<Id>(found.size() - 1);
  }

  void each(const TileRef& t, bool side) {  // ResEach
    if (head_ != kNil && found_tiles_[head_] != t.tile.tile) {
      first(t, side);
    }
  }

  void neighbours(const TileRef& start, bool start_side) {  // ExtFindNeighbors
    push(start, start_side);
    while (!stack_.empty()) {
      const Item it = stack_.back();
      stack_.pop_back();
      if (region(it.tile, it.side) == current()) {
        continue;
      }
      region_[key(it.tile, it.side)] = current();
      visit(it.tile, it.side);
    }
  }

  void visit(const TileRef& t, bool side) {
    const tech::TileType type = side_type(t, side);
    const tech::TypeMask& mask = conn_[type];
    const TileTypes ty = b_.types(t);
    const bool split = ty.split;
    const int sd = side ? 1 : 0;
    if (!(split && (sd ^ ty.direction) != 0)) {
      b_.each_top(t, [&](const TileRef& tp) {
        const TileTypes n = b_.types(tp);
        offer(tp, n.split ? n.direction == 0 : false, n.split ? n.bottom() : n.left, mask);
        return false;
      });
    }
    if (!(split && side)) {
      b_.each_left(t, [&](const TileRef& tp) {
        const TileTypes n = b_.types(tp);
        offer(tp, n.split, n.split ? n.right : n.left, mask);
        return false;
      });
    }
    if (!(split && (sd ^ ty.direction) == 0)) {
      b_.each_bottom(t, [&](const TileRef& tp) {
        const TileTypes n = b_.types(tp);
        offer(tp, n.split ? n.direction != 0 : false, n.split ? n.top() : n.left, mask);
        return false;
      });
    }
    if (!(split && !side)) {
      b_.each_right(t, [&](const TileRef& tp) {
        offer(tp, false, b_.types(tp).left, mask);
        return false;
      });
    }
    each(t, side);
    other_planes(t, side, type, mask);
  }

  void offer(const TileRef& tp, bool side, tech::TileType t, const tech::TypeMask& mask) {
    if (region(tp, side) == db::kClientDefault && mask.test(t)) {
      push(tp, side);
    }
  }

  void other_planes(const TileRef& t, bool side, tech::TileType type, const tech::TypeMask& mask) {
    const db::Rect r = b_.area(t);
    if (b_.tech_.is_contact(type)) {
      const tech::PlaneMask pmask = b_.tech_.contact_planes(type) & ~tech::plane_bit(t.plane);
      for (const tech::PlaneId q : b_.user_planes()) {
        if ((pmask & tech::plane_bit(q)) != 0) {
          contact_plane(q, db::Point{r.x0, r.y0}, b_.types(t).split, side, mask);
        }
      }
    }
    const tech::PlaneMask cross = b_.tech_.cross_planes(type);
    if (cross == 0) {
      return;
    }
    const db::Rect bigger{r.x0 - 1, r.y0 - 1, r.x1 + 1, r.y1 + 1};
    for (const tech::PlaneId q : b_.user_planes()) {
      if (q == t.plane || (cross & tech::plane_bit(q)) == 0) {
        continue;
      }
      b_.search(q, bigger, mask, [&](const TileRef& tp, bool s) {
        if (region(tp, s) != db::kClientDefault) {
          return false;
        }
        db::Rect c = b_.area(tp);
        if (!db::overlap(c, r)) {
          c = db::Rect{std::max(c.x0, r.x0), std::max(c.y0, r.y0), std::min(c.x1, r.x1), std::min(c.y1, r.y1)};
          if (c.x0 >= c.x1 && c.y0 >= c.y1) {
            return false;
          }
        }
        push(tp, s);
        return false;
      });
    }
  }

  void contact_plane(tech::PlaneId q, db::Point ll, bool split, bool side, const tech::TypeMask& mask) {
    const TileRef tp = b_.goto_point(q, ll);
    if (!tp) {
      return;
    }
    const TileTypes n = b_.types(tp);
    if (split && n.split) {
      offer(tp, side, side ? n.right : n.left, mask);
    } else if (n.split) {
      offer(tp, true, n.right, mask);
      offer(tp, false, n.left, mask);
    } else {
      offer(tp, false, n.left, mask);
    }
  }

  Builder& b_;
  std::vector<tech::TypeMask> conn_;
  std::unordered_map<Key, std::uintptr_t, KeyHash> region_;
  std::vector<Item> stack_;
  std::vector<const void*> found_tiles_;
  Id head_ = kNil;
};

}  // namespace

void Builder::find_contacts() {
  // ResInitializeConn: S/D and substrate types connect to their devices.
  std::vector<tech::TypeMask> conn(tech::kMaxTileTypes);
  for (tech::TileType dev = tech::kFirstUserType; dev < tech::kMaxTileTypes; ++dev) {
    for (const extract::DevicePattern* d : records(dev)) {
      if (d->model.empty() || d->model == "None") {
        continue;
      }
      for (tech::TileType t = tech::kFirstUserType; t < tech::kMaxTileTypes; ++t) {
        for (const tech::TypeMask& sd : d->terminals) {
          if (sd.empty()) {
            break;
          }
          if (sd.test(t)) {
            conn[t].set(dev);
          }
        }
        if (d->substrate_types.test(t)) {
          conn[t].set(dev);
        }
      }
    }
    conn[dev] |= tech_.connects_to(dev);
  }
  ContactWalk walk(*this, std::move(conn));
  walk.run();
  g.contacts.assign(walk.found.rbegin(), walk.found.rend());
}

void Builder::dissolve_contacts() {
  for (const Contact& c : g.contacts) {
    const tech::PlaneMask pmask = tech_.type_planes(c.type);
    for (const tech::PlaneId p : user_planes()) {
      if ((pmask & tech::plane_bit(p)) == 0) {
        continue;
      }
      (void)def.plane(p).paint(c.rect, db::erase_row(tech_, p, c.type), db::PaintMerge::none);
      (void)def.plane(p).paint(c.rect, db::paint_row(tech_, p, tech_.residue_on(c.type, p)), db::PaintMerge::none);
    }
  }
}

bool Builder::fracture_planes() {
  if (bbox.is_null()) {
    return true;
  }
  for (const tech::PlaneId p : user_planes()) {
    if (!fracture(ed(p), bbox, input.magic_compat).ok()) {
      return false;
    }
  }
  return true;
}

}  // namespace hefesto::extresist::detail
