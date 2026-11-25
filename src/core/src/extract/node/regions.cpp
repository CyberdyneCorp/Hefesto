// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// RegionFinder: ExtFindRegions / ExtFindNeighbors and extFindNodes /
// extNodeAreaFunc over the tile client slot (capability
// extract-nodes-and-labels "Node connectivity", "Node reference point and
// generated name"; openspec/changes/m7-extraction design.md D6, D8). One
// flood engine serves both: same-plane neighbours across each side (only
// the sides a split tile has), the other images of a contact, and the
// 1-unit halo on the other planes a type connects to. The node pass adds
// extNodeAreaFunc's per-class area and perimeter (the resistance classes
// of the style, contacts as their residues); capacitance is EXTCAP's.
// (Magic ref: extract/ExtRegion.c:142-209, extract/ExtNghbors.c:74-438,
// extract/ExtBasic.c:4714-4970, extract/ExtBasic.c:5057-5915)
//
// Owner: EXTNODE (design D3).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract {

namespace {

constexpr std::uintptr_t kPending = 0;  // Magic's VISITPENDING

constexpr std::uintptr_t region_code(std::size_t d) noexcept {
  return 2 * (d + 1);  // std::uintptr_t is std::size_t on every target
}

constexpr bool is_region(std::uintptr_t v) noexcept {
  return v != db::kClientDefault && v != kPending && (v & 1U) == 0;
}

bool two_sided(const db::TileView& v) {
  return v.split && v.left_type != tech::kSpace && v.right_type != tech::kSpace;
}

/// SplitDirection: TT_DIRECTION set, the `\` diagonal.
bool direction(const db::TileView& v) {
  return v.diagonal == db::Diagonal::nw_se;
}

tech::TileType side_type(const db::TileView& v, bool right) {
  if (!v.split) {
    return v.type;
  }
  return right ? v.right_type : v.left_type;
}

const tech::TypeMask& mask_of(const std::vector<tech::TypeMask>& table, tech::TileType type) {
  static const tech::TypeMask kEmpty;
  return type < table.size() ? table[type] : kEmpty;
}

/// A tile side on the pass's stack.
struct Item {
  tech::PlaneId plane = 0;
  db::TileView view;
  bool right = false;
};

/// What the flood does with each tile and each edge segment.
class TileSink {
 public:
  TileSink() = default;
  TileSink(const TileSink&) = delete;
  TileSink& operator=(const TileSink&) = delete;
  virtual ~TileSink() = default;
  virtual void tile(const Item& it, tech::TileType type) = 0;
  /// A boundary segment of length `len` between the tile and type `other`.
  virtual void edge(const Item& it, tech::TileType type, tech::TileType other, const db::Rect& segment) = 0;
};

/// extSetNodeNum: the lowest-leftmost tile on the lowest plane.
void set_node_num(NodeRegion& reg, bool& has_reference, const Item& it, tech::TileType type) {
  const db::Point ll{it.view.area.x0, it.view.area.y0};
  const bool right = it.view.split && it.right;
  if (!has_reference || it.plane < reg.plane) {
    has_reference = true;
    reg.plane = it.plane;
    reg.reference = ll;
    reg.type = type;
    reg.right_side = right;
    return;
  }
  if (it.plane != reg.plane) {
    return;
  }
  if (ll.x < reg.reference.x || (ll.x == reg.reference.x && ll.y < reg.reference.y)) {
    reg.reference = ll;
    reg.type = type;
    reg.right_side = right;
  }
}

/// GEOCLIP of an edge segment, then its length (0 for EDGENULL).
std::int64_t segment_length(db::Rect r, const std::optional<db::Rect>& clip) {
  if (clip) {
    r.x0 = std::max(r.x0, clip->x0);
    r.y0 = std::max(r.y0, clip->y0);
    r.x1 = std::min(r.x1, clip->x1);
    r.y1 = std::min(r.y1, clip->y1);
    if (r.x1 < r.x0 || r.y1 < r.y0) {
      return 0;
    }
  }
  return std::max(r.width(), r.height());
}

}  // namespace

/// The flood engine of one pass (ExtFindNeighbors / extNodeAreaFunc).
class RegionPass {
 public:
  RegionPass(RegionFinder& finder, const db::CellDef& cell, const std::vector<tech::TypeMask>& connect,
             bool node_pass)
      : finder_(finder), cell_(cell), connect_(connect), node_pass_(node_pass) {}

  [[nodiscard]] std::uintptr_t get(const Item& it) const { return get(it.plane, it.view, it.right); }

  [[nodiscard]] std::uintptr_t get(tech::PlaneId plane, const db::TileView& v, bool right) const {
    const std::uintptr_t slot = cell_.plane(plane).client(v);
    if (!two_sided(v) || slot == db::kClientDefault || slot == kPending || (slot & 1U) == 0) {
      return slot;
    }
    return finder_.split_[slot / 2][right ? 1 : 0];
  }

  /// ExtSetRegion.
  void set(const Item& it, std::uintptr_t value) {
    const db::Plane& plane = cell_.plane(it.plane);
    const std::uintptr_t slot = plane.client(it.view);
    if (slot == value) {
      return;
    }
    if (!two_sided(it.view)) {
      plane.set_client(it.view, value);
      return;
    }
    std::size_t k = 0;
    if (slot == db::kClientDefault || slot == kPending) {
      k = finder_.split_.size();
      finder_.split_.push_back({slot, slot});
      plane.set_client(it.view, 2 * k + 1);
    } else {
      k = slot / 2;
    }
    finder_.split_[k][it.right ? 1 : 0] = value;
  }

  void push(const Item& it) {
    set(it, kPending);
    stack_.push_back(it);
  }

  /// The finder's discovery numbers and substrate region.
  std::size_t next_region() { return finder_.region_count_++; }
  [[nodiscard]] std::size_t peek_region() const { return finder_.region_count_; }
  void mark_substrate(std::size_t d) { finder_.substrate_region_ = d; }

  /// Floods region `d` from the items on the stack.
  void flood(std::size_t d, TileSink& sink) {
    const std::uintptr_t code = region_code(d);
    while (!stack_.empty()) {
      const Item it = stack_.back();
      stack_.pop_back();
      if (get(it) == code) {
        continue;
      }
      set(it, code);
      const tech::TileType type = side_type(it.view, it.right);
      sink.tile(it, type);
      walk_sides(it, type, sink);
      if (technology() != nullptr) {
        visit_contact_images(it, type);
        visit_halo(it, type);
      }
    }
  }

 private:
  [[nodiscard]] const tech::Technology* technology() const { return finder_.technology_; }

  /// Pushes the side `right` of neighbour `nb` when unvisited and
  /// connected; reports the edge.
  void neighbour(const Item& it, tech::TileType type, const db::TileView& nb, bool nb_right, const db::Rect& seg,
                 TileSink& sink) {
    const tech::TileType t = side_type(nb, nb.split && nb_right);
    const Item next{it.plane, nb, nb.split && nb_right};
    if (get(next) == db::kClientDefault && mask_of(connect_, type).test(t)) {
      push(next);
    }
    sink.edge(it, type, t, seg);
  }

  void walk_top(const Item& it, tech::TileType type, TileSink& sink) {
    const db::Rect& a = it.view.area;
    cell_.plane(it.plane).neighbors(it.view, db::Side::top, a.x0, a.x1, nbrs_);
    for (auto n = nbrs_.rbegin(); n != nbrs_.rend(); ++n) {  // RT, then BL: right to left
      const db::Rect seg{std::max(a.x0, n->area.x0), a.y1, std::min(a.x1, n->area.x1), a.y1};
      neighbour(it, type, *n, !direction(*n), seg, sink);  // SplitBottomType
    }
  }

  void walk_left(const Item& it, tech::TileType type, TileSink& sink) {
    const db::Rect& a = it.view.area;
    cell_.plane(it.plane).neighbors(it.view, db::Side::left, a.y0, a.y1, nbrs_);
    for (const db::TileView& n : nbrs_) {
      const db::Rect seg{a.x0, std::max(a.y0, n.area.y0), a.x0, std::min(a.y1, n.area.y1)};
      neighbour(it, type, n, true, seg, sink);  // SplitRightType
    }
  }

  void walk_bottom(const Item& it, tech::TileType type, TileSink& sink) {
    const db::Rect& a = it.view.area;
    cell_.plane(it.plane).neighbors(it.view, db::Side::bottom, a.x0, a.x1, nbrs_);
    for (const db::TileView& n : nbrs_) {
      const db::Rect seg{std::max(a.x0, n.area.x0), a.y0, std::min(a.x1, n.area.x1), a.y0};
      neighbour(it, type, n, direction(n), seg, sink);  // SplitTopType
    }
  }

  void walk_right(const Item& it, tech::TileType type, TileSink& sink) {
    const db::Rect& a = it.view.area;
    cell_.plane(it.plane).neighbors(it.view, db::Side::right, a.y0, a.y1, nbrs_);
    for (auto n = nbrs_.rbegin(); n != nbrs_.rend(); ++n) {  // TR, then LB: top to bottom
      const db::Rect seg{a.x1, std::max(a.y0, n->area.y0), a.x1, std::min(a.y1, n->area.y1)};
      neighbour(it, type, *n, false, seg, sink);  // SplitLeftType
    }
  }

  /// The four sides, skipping those a split tile's side does not have.
  void walk_sides(const Item& it, tech::TileType type, TileSink& sink) {
    const bool split = it.view.split;
    const bool r = it.right;
    const bool d = direction(it.view);
    if (!split || !(r ^ d)) {
      walk_top(it, type, sink);
    }
    if (!split || !r) {
      walk_left(it, type, sink);
    }
    if (!split || (r ^ d)) {
      walk_bottom(it, type, sink);
    }
    if (!split || r) {
      walk_right(it, type, sink);
    }
  }

  /// One image of a contact on `plane`: tp is the tile at the contact's
  /// lower-left corner there.
  void contact_image(const Item& it, tech::TileType type, tech::PlaneId plane) {
    const db::TileView tp = cell_.plane(plane).tile_at({it.view.area.x0, it.view.area.y0});
    if (node_pass_ && cell_.plane(plane).client(tp) != db::kClientDefault) {
      return;
    }
    const tech::TypeMask& mask = mask_of(connect_, type);
    const auto try_side = [&](bool right) {
      const Item next{plane, tp, tp.split && right};
      if ((node_pass_ || get(next) == db::kClientDefault) && mask.test(side_type(tp, right))) {
        push(next);
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

  void visit_contact_images(const Item& it, tech::TileType type) {
    const tech::Technology& t = *technology();
    if (!t.is_contact(type)) {
      return;
    }
    const tech::PlaneMask planes = t.contact_planes(type) & ~tech::plane_bit(it.plane);
    for (std::size_t p = tech::kFirstUserPlane; p < cell_.plane_count(); ++p) {
      if ((planes & tech::plane_bit(static_cast<tech::PlaneId>(p))) != 0) {
        contact_image(it, type, static_cast<tech::PlaneId>(p));
      }
    }
  }

  /// extNbrPushFunc over the 1-unit halo of the tile on `plane`.
  void halo_on(const Item& it, const tech::TypeMask& mask, tech::PlaneId plane) {
    const db::Rect& area = it.view.area;
    const db::Rect bigger = db::expand(area, 1);
    const auto visit = [&](const db::TileView& v) {
      const Item next{plane, v, v.split && v.right_side};
      if (get(next) != db::kClientDefault) {
        return true;
      }
      if (!db::overlap(v.area, area)) {
        const db::Rect c{std::max(v.area.x0, area.x0), std::max(v.area.y0, area.y0), std::min(v.area.x1, area.x1),
                         std::min(v.area.y1, area.y1)};
        if (c.x0 >= c.x1 && c.y0 >= c.y1) {
          return true;
        }
      }
      push(next);
      return true;
    };
    const db::Plane& target = cell_.plane(plane);
    if (it.view.split) {
      (void)target.enumerate_triangle(bigger, it.view.diagonal, it.right, mask, visit);
    } else {
      (void)target.enumerate(bigger, mask, visit);
    }
  }

  void visit_halo(const Item& it, tech::TileType type) {
    const tech::PlaneMask planes = technology()->cross_planes(type);
    if (planes == 0) {
      return;
    }
    const tech::TypeMask& mask = mask_of(connect_, type);
    for (std::size_t p = tech::kFirstUserPlane; p < cell_.plane_count(); ++p) {
      const auto plane = static_cast<tech::PlaneId>(p);
      if (plane != it.plane && (planes & tech::plane_bit(plane)) != 0) {
        halo_on(it, mask, plane);
      }
    }
  }

  RegionFinder& finder_;
  const db::CellDef& cell_;
  const std::vector<tech::TypeMask>& connect_;
  bool node_pass_ = false;
  std::vector<Item> stack_;
  std::vector<db::TileView> nbrs_;
};

namespace {

/// The generic pass: the reference of every tile, the caller's callbacks.
class GenericSink final : public TileSink {
 public:
  GenericSink(NodeRegion& reg, RegionCallbacks& callbacks) : reg_(reg), callbacks_(callbacks) {}
  void tile(const Item& it, tech::TileType type) override {
    set_node_num(reg_, has_reference_, it, type);
    if (callbacks_.each) {
      callbacks_.each(it.view.area, type, it.plane);
    }
  }
  void edge(const Item& /*it*/, tech::TileType /*type*/, tech::TileType /*other*/,
            const db::Rect& /*segment*/) override {}

 private:
  NodeRegion& reg_;
  RegionCallbacks& callbacks_;
  bool has_reference_ = false;
};

/// extNodeAreaFunc's per-class area and perimeter.
class NodeSink final : public TileSink {
 public:
  NodeSink(NodeRegion& reg, const ExtStyle& style, const tech::Technology* technology,
           const std::optional<db::Rect>& clip)
      : reg_(reg), style_(style), technology_(technology), clip_(clip), area_(style.resist_classes.size()) {}

  void tile(const Item& it, tech::TileType type) override {
    set_node_num(reg_, has_reference_, it, type);
    const tech::TileType residue = residue_of(type, it.plane);
    const std::int32_t cls = style_.resist_class[residue];
    if (cls >= 0 && static_cast<std::size_t>(cls) < area_.size()) {
      area_[static_cast<std::size_t>(cls)].area += tile_area(it.view);
    }
    if (it.view.split) {
      diagonal_edge(it, residue, cls);
    }
  }

  void edge(const Item& it, tech::TileType type, tech::TileType other, const db::Rect& segment) override {
    const tech::TileType residue = residue_of(type, it.plane);
    const std::int32_t cls = style_.resist_class[residue];
    if (cls < 0 || static_cast<std::size_t>(cls) >= area_.size()) {
      return;
    }
    if (mask_of(style_.resist_changed, residue).test(residue_of(other, it.plane))) {
      area_[static_cast<std::size_t>(cls)].perimeter += segment_length(segment, clip_);
    }
  }

  [[nodiscard]] std::vector<ClassArea> take() { return std::move(area_); }

 private:
  [[nodiscard]] tech::TileType residue_of(tech::TileType type, tech::PlaneId plane) const {
    if (technology_ != nullptr && technology_->is_contact(type)) {
      return technology_->residue_on(type, plane);
    }
    return type;
  }

  [[nodiscard]] std::int64_t tile_area(const db::TileView& v) const {
    db::Rect r = v.area;
    std::int64_t area = r.width() * r.height();
    if (clip_) {
      if (!db::overlap(r, *clip_)) {
        return 0;
      }
      r = db::Rect{std::max(r.x0, clip_->x0), std::max(r.y0, clip_->y0), std::min(r.x1, clip_->x1),
                   std::min(r.y1, clip_->y1)};
      area = r.width() * r.height();
    }
    return v.split ? area / 2 : area;  // split tiles are half the area
  }

  /// The diagonal of a split tile: `(int)sqrt(w*w + h*h)` against the
  /// other side's type (computed in 64 bits: Magic's int product overflows
  /// for sides past 46340, fixed in both modes).
  void diagonal_edge(const Item& it, tech::TileType residue, std::int32_t cls) {
    const double w = static_cast<double>(it.view.area.width());
    const double h = static_cast<double>(it.view.area.height());
    const auto len = static_cast<std::int64_t>(std::sqrt(w * w + h * h));
    const tech::TileType other = side_type(it.view, !it.right);
    if (cls >= 0 && static_cast<std::size_t>(cls) < area_.size() &&
        mask_of(style_.resist_changed, residue).test(residue_of(other, it.plane))) {
      area_[static_cast<std::size_t>(cls)].perimeter += len;
    }
  }

  NodeRegion& reg_;
  const ExtStyle& style_;
  const tech::Technology* technology_;
  const std::optional<db::Rect>& clip_;
  std::vector<ClassArea> area_;
  bool has_reference_ = false;
};

/// The seed test of DBSrPaintClient (the tile's slot) or, with
/// magic-compat OFF in the node pass, of the reported side.
bool unvisited_seed(const RegionPass& pass, const db::CellDef& cell, const Item& it, bool by_side) {
  if (by_side) {
    return pass.get(it) == db::kClientDefault;
  }
  return cell.plane(it.plane).client(it.view) == db::kClientDefault;
}

}  // namespace

namespace {

/// Starts a pass: a pass run while the slots of the previous one are kept
/// continues its numbering and split table; otherwise both start afresh
/// (the slots are already clear: every pass resets them before it ends).
void begin_pass(bool clients_live, std::size_t& first, std::size_t count,
                std::vector<std::array<std::uintptr_t, 2>>& split) {
  if (clients_live) {
    first = count;
    return;
  }
  split.clear();
  first = 0;
}

/// Reverses a discovery-ordered list into Magic's list order.
NodeRegionList to_list(std::vector<NodeRegion>&& found) {
  std::reverse(found.begin(), found.end());
  return std::move(found);
}

}  // namespace

diag::Outcome<NodeRegionList> RegionFinder::find(const db::CellDef& cell, const db::Rect& area,
                                                 const tech::TypeMask& mask, const ConnectTable& connect,
                                                 RegionCallbacks& callbacks) {
  if (!clients_live_) {
    substrate_region_.reset();
  }
  begin_pass(clients_live_, first_region_, region_count_, split_);
  region_count_ = first_region_;
  RegionPass pass(*this, cell, connect.connects, false);
  std::vector<NodeRegion> found;
  for (std::size_t p = tech::kFirstUserPlane; p < cell.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    (void)cell.plane(plane).enumerate(area, mask, [&](const db::TileView& v) {
      const Item it{plane, v, v.split && v.right_side};
      if (!unvisited_seed(pass, cell, it, false)) {
        return true;
      }
      if (callbacks.first) {
        callbacks.first(v.area, side_type(v, it.right), plane);
      }
      found.emplace_back();
      GenericSink sink(found.back(), callbacks);
      pass.push(it);
      pass.flood(region_count_++, sink);
      return true;
    });
  }
  clients_live_ = keep_clients;
  if (!keep_clients) {
    reset(cell);
  }
  return to_list(std::move(found));
}

namespace {

/// One extFindNodes run: the substrate region, then the second pass.
class NodePassRun {
 public:
  NodePassRun(RegionPass& pass, const db::CellDef& cell, const ExtStyle& style, const tech::Technology* technology,
              const NodePassOptions& options)
      : pass_(pass), cell_(cell), style_(style), technology_(technology), options_(options) {}

  /// extNodeAreaFunc from `seeds` (one region).
  void flood_region(const std::vector<Item>& seeds) {
    found_.emplace_back();
    NodeSink sink(found_.back(), style_, technology_, options_.clip);
    for (const Item& seed : seeds) {
      pass_.push(seed);
    }
    pass_.flood(pass_.next_region(), sink);
    found_.back().classes = sink.take();
    if (!options_.resistance) {
      std::fill(found_.back().classes.begin(), found_.back().classes.end(), ClassArea{});
    }
  }

  /// The first pass: one region from all substrate seeds, or the
  /// tile-less substrate node.
  void substrate() {
    if (!options_.substrate_seeds.empty()) {
      std::vector<Item> seeds;
      for (const RegionSeed& s : options_.substrate_seeds) {
        const db::TileView v = cell_.plane(s.plane).tile_at(s.point);
        seeds.push_back(Item{s.plane, v, v.split && s.right_side});
      }
      pass_.mark_substrate(pass_.peek_region());
      flood_region(seeds);
      return;
    }
    if (options_.substrate_without_tiles) {
      NodeRegion subs;
      subs.plane = *options_.substrate_without_tiles;
      subs.type = tech::kSpace;
      subs.reference = {db::kMinusInfinity + 3, db::kMinusInfinity + 3};
      subs.classes.resize(style_.resist_classes.size());
      found_.push_back(std::move(subs));
      pass_.mark_substrate(pass_.next_region());
    }
  }

  /// The second pass: a region per unvisited active tile, plane by plane.
  void scan() {
    const bool by_side = !options_.magic_compat;
    for (std::size_t p = tech::kFirstUserPlane; p < cell_.plane_count(); ++p) {
      const auto plane = static_cast<tech::PlaneId>(p);
      (void)cell_.plane(plane).enumerate(db::whole_plane(), style_.active, [&](const db::TileView& v) {
        const Item it{plane, v, v.split && v.right_side};
        if (unvisited_seed(pass_, cell_, it, by_side)) {
          flood_region({it});
        }
        return true;
      });
    }
  }

  [[nodiscard]] std::vector<NodeRegion>&& take() { return std::move(found_); }

 private:
  RegionPass& pass_;
  const db::CellDef& cell_;
  const ExtStyle& style_;
  const tech::Technology* technology_;
  const NodePassOptions& options_;
  std::vector<NodeRegion> found_;
};

}  // namespace

diag::Outcome<NodeRegionList> RegionFinder::find_nodes(const db::CellDef& cell, const ExtStyle& style,
                                                       const NodePassOptions& options) {
  begin_pass(clients_live_, first_region_, region_count_, split_);
  region_count_ = first_region_;
  substrate_region_.reset();
  RegionPass pass(*this, cell, style.node_conn, true);
  NodePassRun run(pass, cell, style, technology_, options);
  run.substrate();
  if (!options.substrate_only) {
    run.scan();
  }
  clients_live_ = keep_clients;
  if (!keep_clients) {
    reset(cell);
  }
  return to_list(run.take());
}

std::optional<std::size_t> RegionFinder::region_of(const db::Plane& plane, const db::TileView& tile) const {
  const std::uintptr_t slot = plane.client(tile);
  std::uintptr_t state = slot;
  if (two_sided(tile) && slot != db::kClientDefault && slot != kPending && (slot & 1U) != 0) {
    state = split_[slot / 2][tile.split && tile.right_side ? 1 : 0];
  }
  if (!is_region(state)) {
    return std::nullopt;
  }
  const std::size_t region = state / 2 - 1;  // std::uintptr_t is std::size_t on every target
  return region;
}

std::optional<std::size_t> RegionFinder::node_of(const db::Plane& plane, const db::TileView& tile,
                                                  std::size_t list_size) const {
  const std::optional<std::size_t> d = region_of(plane, tile);
  if (!d || *d < first_region_ || *d - first_region_ >= list_size) {
    return std::nullopt;
  }
  return node_index(list_size, *d - first_region_);
}

void reset(const db::CellDef& cell) {
  for (std::size_t p = tech::kFirstUserPlane; p < cell.plane_count(); ++p) {
    cell.plane(static_cast<tech::PlaneId>(p)).reset_clients(db::whole_plane());
  }
}

bool clients_clear(const db::CellDef& cell) {
  bool clear = true;
  for (std::size_t p = 0; p < cell.plane_count() && clear; ++p) {
    const db::Plane& plane = cell.plane(static_cast<tech::PlaneId>(p));
    (void)plane.enumerate(db::whole_plane(), tech::TypeMask::all(), [&](const db::TileView& v) {
      clear = plane.client(v) == db::kClientDefault;
      return clear;
    });
  }
  return clear;
}

}  // namespace hefesto::extract
