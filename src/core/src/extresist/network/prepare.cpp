// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ResExtractNet's preparation of the copied net (capability
// extresist-network "Net geometry collection" steps 2-5): the device areas
// painted from the source cell or its subcells (DBTreeSrTiles with
// resMakeDevFunc and resExpandDevFunc,
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
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "extresist/network/builder.hpp"
#include "hefesto/db/array_overlap.hpp"
#include "hefesto/db/hierarchy.hpp"
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

// DBTreeSrTiles over `cell` and its subcells: each plane's tiles of
// `mask` under `root_area` (root coordinates), then the array elements of
// every use that overlap it (DBArrayOverlap, y outer, as dbCellSrFunc);
// `visit(plane, tile, to_root)` returns false to stop. Returns false when
// stopped.
constexpr int kMaxDepth = 64;  // a crafted hierarchy cannot recurse forever

template <typename Visit>
bool use_tiles(const db::CellUse& use, const db::Transform& to_root, const db::Rect& area,
               const db::Rect& root_area, const tech::TypeMask& mask, Visit&& visit, int depth);

template <typename Visit>
bool tree_tiles(const db::CellDef& cell, const db::Transform& to_root, const db::Rect& root_area,
                const tech::TypeMask& mask, Visit&& visit, int depth = 0) {
  const db::Rect area = db::apply(db::invert(to_root), root_area);
  for (std::size_t p = 0; p < cell.plane_count(); ++p) {
    const db::Plane& plane = cell.plane(static_cast<tech::PlaneId>(p));
    bool go = true;
    plane.enumerate(area, mask, [&](const db::TileView& v) {
      go = visit(plane, v, to_root);
      return go;
    });
    if (!go) {
      return false;
    }
  }
  if (depth >= kMaxDepth) {
    return true;
  }
  for (const auto& use : cell.uses()) {
    if (use->def != nullptr && db::touch(use->bbox, area) &&
        !use_tiles(*use, to_root, area, root_area, mask, visit, depth + 1)) {
      return false;
    }
  }
  return true;
}

// tree_tiles over the array elements of `use` whose images overlap `area`
// (the parent's coordinates). The indices are 64-bit so that a range
// ending at INT32_MAX ends the loop.
template <typename Visit>
bool use_tiles(const db::CellUse& use, const db::Transform& to_root, const db::Rect& area,
               const db::Rect& root_area, const tech::TypeMask& mask, Visit&& visit, int depth) {
  const db::ElementRange r = db::array_overlap(use, area);
  for (std::int64_t y = r.ylo; y <= r.yhi; ++y) {
    for (std::int64_t x = r.xlo; x <= r.xhi; ++x) {
      const db::Transform element =
          db::element_transform(use, static_cast<std::int32_t>(x), static_cast<std::int32_t>(y));
      const db::Transform child = db::compose(to_root, element);
      if (!tree_tiles(*use.def, child, root_area, mask, visit, depth)) {
        return false;
      }
    }
  }
  return true;
}

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

  /// resMakeDevFunc on one tile side: false to keep searching. The area is
  /// in root coordinates (GeoTransRect before the split-tile adjustment).
  bool make(const db::TileView& v, const db::Transform& to_root, DevTile& dev) const {
    db::Rect area = db::apply(to_root, v.area);
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

  /// The device under `at`, in the source cell or any of its subcells
  /// (DBTreeSrTiles with resMakeDevFunc).
  bool find(db::Point at, DevTile& dev) const {
    const db::Rect unit{at.x, at.y, at.x + 1, at.y + 1};
    return !tree_tiles(src, db::kIdentity, unit, all_but_space(),
                       [&](const db::Plane&, const db::TileView& v, const db::Transform& to_root) {
                         return !make(v, to_root, dev);
                       });
  }

  [[nodiscard]] bool part_of(tech::TileType t, tech::TileType device) const {
    return t == device || (b.tech_.is_contact(t) && b.tech_.contact(t)->residues.test(device));
  }

  void perim_overlap(const db::Plane& plane, const db::TileView& tile, DevTile& dev) const {
    const db::Rect& r = tile.area;
    dev.perim = (static_cast<std::int64_t>(r.y1) - r.y0 - r.x0 + r.x1) * 2;
    const tech::TileType t1 = tile.split ? tile.left_type : tile.type;
    const tech::TypeMask omask = t1 < b.style_.node_conn.size() ? b.style_.node_conn[t1] : tech::TypeMask{};
    std::int64_t overlap = 0;
    std::vector<db::TileView> nbrs;
    for (const db::Side side : {db::Side::left, db::Side::right, db::Side::top, db::Side::bottom}) {
      const auto [lo, hi] = side_range(r, side);
      plane.neighbors(tile, side, lo, hi, nbrs);
      const bool vertical = side == db::Side::left || side == db::Side::right;
      for (const db::TileView& n : nbrs) {
        if (!omask.test(facing_type(n, side))) {
          continue;
        }
        overlap += vertical ? std::int64_t{std::min(r.y1, n.area.y1)} - std::max(r.y0, n.area.y0)
                            : std::int64_t{std::min(r.x1, n.area.x1)} - std::max(r.x0, n.area.x0);
      }
    }
    dev.overlap += overlap;
  }

  /// resExpandDevFunc's paint of one device tile. With magic-compat ON the
  /// tile is painted at its own cell's coordinates, as Magic's TiToRect
  /// without the search context's transform does (a device in a moved or
  /// rotated subcell lands at the wrong place); with it OFF it is
  /// transformed to the root (proposed Q-NETLIST-76).
  void paint(const db::TileView& tp, bool right_side, const db::Transform& to_root, const DevTile& dev) const {
    const auto plane = b.tech_.home_plane(dev.type);
    if (!plane) {
      return;
    }
    const db::PaintRow row = db::paint_row(b.tech_, *plane, dev.type);
    db::Plane& dst = b.def.plane(*plane);
    const bool moved = !b.input.magic_compat;
    const db::Rect area = moved ? db::apply(to_root, tp.area) : tp.area;
    if (tp.split) {
      const db::TriangleSide side =
          moved ? db::transform_triangle(to_root, tp.diagonal, right_side) : db::TriangleSide{tp.diagonal, right_side};
      (void)dst.paint_triangle(area, side.diagonal, side.right_side, row, b.input.magic_compat, nullptr,
                               db::PaintMerge::none);
    } else {
      (void)dst.paint(area, row, db::PaintMerge::none);
    }
  }

  /// resExpandDevFunc from the first tile of the device type in its area,
  /// in the source cell or a subcell.
  void expand(DevTile& dev) const {
    (void)tree_tiles(src, db::kIdentity, dev.area, tech::TypeMask::of(dev.type),
                     [&](const db::Plane& plane, const db::TileView& v, const db::Transform& to_root) {
                       flood(plane, v, to_root, dev);
                       return false;
                     });
  }

  /// The device side of a split tile and its hypotenuse-adjacent sides
  /// (the edges the flood ignores); {0, false} for a Manhattan tile.
  [[nodiscard]] std::pair<std::uint32_t, bool> split_sides(const db::TileView& tp, tech::TileType device) const {
    if (!tp.split) {
      return {0U, false};
    }
    const bool dir = tp.diagonal == db::Diagonal::nw_se;
    const bool right_side = !part_of(tp.left_type, device);
    const std::uint32_t ignore =
        right_side ? (kLeftEdge | (dir ? kBottomEdge : kTopEdge)) : (kRightEdge | (dir ? kTopEdge : kBottomEdge));
    return {ignore, right_side};
  }

  struct Flood {
    std::unordered_set<const void*> seen;
    std::vector<db::TileView> stack;
    std::vector<db::TileView> nbrs;
  };

  /// Pushes the unseen device tiles across `side` of `tp`.
  void spread(const db::Plane& plane, const db::TileView& tp, db::Side side, tech::TileType device, Flood& f) const {
    const auto [lo, hi] = side_range(tp.area, side);
    plane.neighbors(tp, side, lo, hi, f.nbrs);
    for (const db::TileView& n : f.nbrs) {
      if (!f.seen.contains(n.tile) && part_of(facing_type(n, side), device)) {
        f.seen.insert(n.tile);
        f.stack.push_back(n);
      }
    }
  }

  void flood(const db::Plane& plane, const db::TileView& start, const db::Transform& to_root, DevTile& dev) const {
    Flood f{{start.tile}, {start}, {}};
    while (!f.stack.empty()) {
      const db::TileView tp = f.stack.back();
      f.stack.pop_back();
      const auto [ignore, right_side] = split_sides(tp, dev.type);
      paint(tp, right_side, to_root, dev);
      perim_overlap(plane, tp, dev);
      for (const auto& [side, edge] : {std::pair{db::Side::top, kTopEdge}, std::pair{db::Side::bottom, kBottomEdge},
                                      std::pair{db::Side::right, kRightEdge}, std::pair{db::Side::left, kLeftEdge}}) {
        if ((ignore & edge) == 0) {
          spread(plane, tp, side, dev.type, f);
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

std::vector<std::pair<db::Rect, tech::TileType>> Builder::contact_tiles(tech::PlaneId p) const {
  std::vector<std::pair<db::Rect, tech::TileType>> found;
  def.plane(p).enumerate(bbox, all_but_space(), [&](const db::TileView& v) {
    if (v.split && v.left_type != tech::kSpace && v.right_type != tech::kSpace && v.right_side) {
      return true;
    }
    const tech::TileType t = v.split ? (v.right_side ? v.right_type : v.left_type) : v.type;
    if (tech_.is_contact(t)) {
      found.emplace_back(v.area, t);
    }
    return true;
  });
  return found;
}

void Builder::shave_contacts() {
  for (const tech::PlaneId p : user_planes()) {
    for (const auto& [rect, t] : contact_tiles(p)) {
      // Erased from every plane of the contact but the lowest.
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
    // A split tile skips the sides its hypotenuse faces.
    const bool diag_top = ty.split && ((side ? 1 : 0) ^ ty.direction) != 0;
    if (!ty.split || !diag_top) {
      b_.each_top(t, [&](const TileRef& tp) { return offer_top(tp, mask); });
    }
    if (!ty.split || !side) {
      b_.each_left(t, [&](const TileRef& tp) { return offer_left(tp, mask); });
    }
    if (!ty.split || diag_top) {
      b_.each_bottom(t, [&](const TileRef& tp) { return offer_bottom(tp, mask); });
    }
    if (!ty.split || side) {
      b_.each_right(t, [&](const TileRef& tp) {
        offer(tp, false, b_.types(tp).left, mask);
        return false;
      });
    }
    each(t, side);
    other_planes(t, side, type, mask);
  }

  // The neighbour sides facing the visited tile, as ExtFindNeighbors
  // reads them; each returns false to keep walking.
  bool offer_top(const TileRef& tp, const tech::TypeMask& mask) {
    const TileTypes n = b_.types(tp);
    offer(tp, n.split && n.direction == 0, n.split ? n.bottom() : n.left, mask);
    return false;
  }
  bool offer_left(const TileRef& tp, const tech::TypeMask& mask) {
    const TileTypes n = b_.types(tp);
    offer(tp, n.split, n.split ? n.right : n.left, mask);
    return false;
  }
  bool offer_bottom(const TileRef& tp, const tech::TypeMask& mask) {
    const TileTypes n = b_.types(tp);
    offer(tp, n.split && n.direction != 0, n.split ? n.top() : n.left, mask);
    return false;
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
    for (const tech::PlaneId q : b_.user_planes()) {
      if (q != t.plane && (cross & tech::plane_bit(q)) != 0) {
        cross_plane(q, r, mask);
      }
    }
  }

  /// The tiles of `mask` on plane `q` touching `r` (not only at a corner).
  void cross_plane(tech::PlaneId q, const db::Rect& r, const tech::TypeMask& mask) {
    const db::Rect bigger{r.x0 - 1, r.y0 - 1, r.x1 + 1, r.y1 + 1};
    b_.search(q, bigger, mask, [&](const TileRef& tp, bool s) {
      if (region(tp, s) == db::kClientDefault && touches(b_.area(tp), r)) {
        push(tp, s);
      }
      return false;
    });
  }

  [[nodiscard]] static bool touches(const db::Rect& a, const db::Rect& r) {
    if (db::overlap(a, r)) {
      return true;
    }
    const db::Rect c{std::max(a.x0, r.x0), std::max(a.y0, r.y0), std::min(a.x1, r.x1), std::min(a.y1, r.y1)};
    return !(c.x0 >= c.x1 && c.y0 >= c.y1);
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

namespace {

/// ResInitializeConn for one device record: its S/D and substrate types
/// connect to the device type.
void connect_record(std::vector<tech::TypeMask>& conn, tech::TileType dev, const extract::DevicePattern& d) {
  for (tech::TileType t = tech::kFirstUserType; t < tech::kMaxTileTypes; ++t) {
    for (const tech::TypeMask& sd : d.terminals) {
      if (sd.empty()) {
        break;
      }
      if (sd.test(t)) {
        conn[t].set(dev);
      }
    }
    if (d.substrate_types.test(t)) {
      conn[t].set(dev);
    }
  }
}

}  // namespace

void Builder::find_contacts() {
  // ResInitializeConn: S/D and substrate types connect to their devices.
  std::vector<tech::TypeMask> conn(tech::kMaxTileTypes);
  for (tech::TileType dev = tech::kFirstUserType; dev < tech::kMaxTileTypes; ++dev) {
    for (const extract::DevicePattern* d : records(dev)) {
      if (!d->model.empty() && d->model != "None") {
        connect_record(conn, dev, *d);
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
