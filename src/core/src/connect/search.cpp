// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Connectivity searches (layout-connectivity). Owner: SELECT.
//   search_flat          DBSrConnect: a depth-first walk over the tiles of
//                        one cell, neighbours found along each side, then
//                        the contact step to the other planes (Magic's
//                        single-tile step with magic-compat ON, Q-MODEL-17)
//   search_hierarchical  DBTreeCopyConnect: areas queued and searched
//                        through the hierarchy, the net accumulated in
//                        scratch planes that also mark what is done
// Both only read the layout ("Connectivity is computed on demand").
// (Magic ref: database/DBconnect.c:171-668, 700-1227)

#include "hefesto/connect/search.hpp"

#include <algorithm>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "hefesto/db/paint.hpp"
#include "hefesto/diag/codes.hpp"

namespace hefesto::connect {

namespace {

tech::TypeMask connect_mask(const tech::Technology& tech, const SearchOptions& options, tech::TileType type) {
  return options.connect ? options.connect(type) : tech.connects_to(type);
}

[[nodiscard]] bool is_nw_se(const db::TileView& t) noexcept {
  return t.diagonal == db::Diagonal::nw_se;
}

/// The type of a split tile's top / bottom triangle (for `\` the right part
/// is the upper one, for `/` the left part).
[[nodiscard]] tech::TileType top_type(const db::TileView& t) noexcept {
  return is_nw_se(t) ? t.right_type : t.left_type;
}
[[nodiscard]] tech::TileType bottom_type(const db::TileView& t) noexcept {
  return is_nw_se(t) ? t.left_type : t.right_type;
}

// ---- flat search ------------------------------------------------------------

/// A tile reached by the flat search, and the side of a split tile.
struct FlatItem {
  tech::PlaneId plane = 0;
  db::TileView tile;
  bool right_side = false;
};

using TileKey = std::tuple<tech::PlaneId, db::Coord, db::Coord, db::Coord, db::Coord>;

[[nodiscard]] TileKey key_of(tech::PlaneId plane, const db::Rect& r) noexcept {
  return {plane, r.x0, r.y0, r.x1, r.y1};
}

enum class Side : std::uint8_t { left, bottom, right, top };

class FlatSearch {
 public:
  FlatSearch(const db::CellDef& def, const tech::Technology& tech, const SearchStart& start,
             const SearchOptions& options, NetVisitor& visit)
      : def_(def), tech_(tech), start_(start), options_(options), visit_(visit) {}

  void run(const FlatItem& first) {
    stack_.push_back(first);
    while (!stack_.empty() && !stopped_) {
      const FlatItem item = stack_.back();
      stack_.pop_back();
      step(item);
    }
  }

 private:
  [[nodiscard]] bool seen(const FlatItem& item) const { return visited_.count(key_of(item.plane, item.tile.area)) > 0; }

  [[nodiscard]] tech::TileType type_of(const FlatItem& item) const noexcept {
    if (!item.tile.split) {
      return item.tile.type;
    }
    return item.right_side ? item.tile.right_type : item.tile.left_type;
  }

  void step(const FlatItem& item) {
    if (!db::overlap(item.tile.area, start_.bounds) || seen(item)) {
      return;
    }
    // Magic marks the whole tile: the other side of a split tile is not
    // visited again (dbSrConnectFunc's ti_client).
    visited_.insert(key_of(item.plane, item.tile.area));
    const tech::TileType type = type_of(item);
    NetTile out;
    out.plane = item.plane;
    out.tile = item.tile;
    out.tile.type = type;
    out.tile.right_side = item.right_side;
    if (!visit_.on_tile(out)) {
      stopped_ = true;
      return;
    }
    const tech::TypeMask mask = connect_mask(tech_, options_, type);
    for (const Side side : {Side::left, Side::bottom, Side::right, Side::top}) {
      if (!skips_side(item, side)) {
        push_neighbours(item, side, mask);
      }
    }
    cross_planes(item, type, mask);
  }

  /// A triangle has only its two legs and its hypotenuse.
  [[nodiscard]] static bool skips_side(const FlatItem& item, Side side) noexcept {
    if (!item.tile.split) {
      return false;
    }
    const bool dir = is_nw_se(item.tile);
    switch (side) {
      case Side::left:
        return item.right_side;
      case Side::bottom:
        return !item.right_side != dir;
      case Side::right:
        return !item.right_side;
      case Side::top:
        return item.right_side != dir;
    }
    return false;
  }

  /// The strip just outside `side` of `r`, one unit thick.
  [[nodiscard]] static db::Rect strip(const db::Rect& r, Side side) noexcept {
    switch (side) {
      case Side::left:
        return {r.x0 - 1, r.y0, r.x0, r.y1};
      case Side::bottom:
        return {r.x0, r.y0 - 1, r.x1, r.y0};
      case Side::right:
        return {r.x1, r.y0, r.x1 + 1, r.y1};
      case Side::top:
        return {r.x0, r.y1, r.x1, r.y1 + 1};
    }
    return r;
  }

  /// The type a neighbour shows towards us and the side of it we reach.
  [[nodiscard]] static std::pair<tech::TileType, bool> facing(const db::TileView& t, Side side) noexcept {
    if (!t.split) {
      return {t.type, false};
    }
    switch (side) {
      case Side::left:  // our left neighbour faces us with its right side
        return {t.right_type, true};
      case Side::bottom:
        return {top_type(t), is_nw_se(t)};
      case Side::right:
        return {t.left_type, false};
      case Side::top:
        return {bottom_type(t), !is_nw_se(t)};
    }
    return {t.type, false};
  }

  void push_neighbours(const FlatItem& item, Side side, const tech::TypeMask& mask) {
    std::vector<db::TileView> found;
    def_.plane(item.plane).enumerate(strip(item.tile.area, side), tech::TypeMask::all(), [&](const db::TileView& t) {
      if (found.empty() || found.back().area != t.area) {
        found.push_back(t);
      }
      return true;
    });
    // Magic's stitch walks: left side upwards, bottom rightwards, right
    // side downwards, top leftwards (pushed in that order).
    const auto by_y = [](const db::TileView& a, const db::TileView& b) { return a.area.y0 < b.area.y0; };
    const auto by_x = [](const db::TileView& a, const db::TileView& b) { return a.area.x0 < b.area.x0; };
    const bool vertical = side == Side::left || side == Side::right;
    std::sort(found.begin(), found.end(), vertical ? std::function(by_y) : std::function(by_x));
    if (side == Side::right || side == Side::top) {
      std::reverse(found.begin(), found.end());
    }
    for (const db::TileView& t : found) {
      const auto [type, right] = facing(t, side);
      const FlatItem next{item.plane, t, right};
      if (mask.test(type) && !seen(next)) {
        stack_.push_back(next);
      }
    }
  }

  /// The contact step: other planes of a contact.
  void cross_planes(const FlatItem& item, tech::TileType type, const tech::TypeMask& mask) {
    const tech::PlaneMask planes = tech_.contact_planes(type) & ~tech::plane_bit(item.plane);
    if (planes == 0) {
      return;
    }
    for (std::size_t p = tech::kFirstUserPlane; p < def_.plane_count(); ++p) {
      const auto plane = static_cast<tech::PlaneId>(p);
      if ((planes & tech::plane_bit(plane)) != 0) {
        search_plane(item, plane, mask);
      }
    }
  }

  void search_plane(const FlatItem& item, tech::PlaneId plane, const tech::TypeMask& mask) {
    // magic-compat: the first tile within one unit of the contact, which may
    // touch it only diagonally (Q-MODEL-17). Native: the contact's own
    // image; its neighbours follow from the walk.
    const bool single = options_.magic_compat;
    const db::Rect area = single ? db::expand(item.tile.area, 1) : item.tile.area;
    const auto take = [&](const db::TileView& t) {
      stack_.push_back(FlatItem{plane, t, t.split && t.right_side});
      return !single;
    };
    const db::Plane& target = def_.plane(plane);
    if (item.tile.split) {
      (void)target.enumerate_triangle(area, item.tile.diagonal, item.right_side, mask, take);
    } else {
      (void)target.enumerate(area, mask, take);
    }
  }

  const db::CellDef& def_;
  const tech::Technology& tech_;
  const SearchStart& start_;
  const SearchOptions& options_;
  NetVisitor& visit_;
  std::vector<FlatItem> stack_;
  std::set<TileKey> visited_;
  bool stopped_ = false;
};

// ---- hierarchical search ------------------------------------------------------

/// An area waiting to be searched (Magic's conSrArea).
struct Pending {
  db::Rect area;
  tech::TileType mask_type = tech::kSpace;  // whose connect mask (dedupe key)
  tech::TypeMask mask;
  std::optional<db::TriangleSide> triangle;
};

using LabelKey = std::tuple<db::Coord, db::Coord, db::Coord, db::Coord, tech::TileType, std::string>;

class TreeSearch {
 public:
  TreeSearch(const db::CellDef& root, const tech::Technology& tech, const SearchStart& start,
             const SearchOptions& options, NetVisitor& visit)
      : root_(root), tech_(tech), start_(start), options_(options), visit_(visit) {
    net_.reserve(root.plane_count());
    for (std::size_t p = 0; p < root.plane_count(); ++p) {
      net_.emplace_back();
    }
  }

  diag::Status run() {
    // The first tiles: anything of the start types overlapping the start
    // area (a 1-unit start area accepts corner overlaps).
    search(start_.area, start_.types, std::nullopt);
    while (!queue_.empty() && status_.ok() && !stopped_) {
      const Pending next = queue_.back();
      queue_.pop_back();
      last_top_ = queue_.size();
      search(next.area, next.mask, next.triangle);
      if (options_.labels != LabelPolicy::none && !stopped_) {
        copy_labels(next.area, next.mask);
      }
    }
    return status_;
  }

 private:
  void search(const db::Rect& area, const tech::TypeMask& mask, const std::optional<db::TriangleSide>& triangle) {
    current_area_ = area;
    (void)search_tree_tiles(
        root_, area, mask, options_.descend, options_.magic_compat, [&](const TreeTile& t) { return found(t); },
        triangle);
  }

  [[nodiscard]] static db::Rect clip(const db::Rect& a, const db::Rect& b) noexcept {
    return db::Rect{std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1)};
  }

  bool found(const TreeTile& t) {
    const db::Rect local_area = db::apply(db::invert(t.to_root), current_area_);
    if (corner_rule(t.tile.area, local_area)) {
      return true;
    }
    const db::Rect piece = clip(db::apply(t.to_root, t.tile.area), start_.bounds);
    if (piece.is_null()) {
      return true;
    }
    std::optional<db::TriangleSide> triangle;
    tech::TileType type = t.tile.type;
    if (t.tile.split) {
      triangle = db::transform_triangle(t.to_root, t.tile.diagonal, t.tile.right_side);
      type = t.tile.right_side ? t.tile.right_type : t.tile.left_type;
    }
    const bool fresh = unprocessed(t.plane, piece, type, triangle);
    if (!paint_net(t.plane, piece, type, triangle) || !report(t, piece, type, triangle)) {
      return false;
    }
    if (fresh) {
      enqueue(grow(piece, triangle), type, connect_mask(tech_, options_, type), triangle);
    }
    return true;
  }

  /// dbcConnectFunc's corner rule, in the tile's own frame: a tile within
  /// one unit of the search area's corner touches the previous tile only
  /// diagonally (unless the area is one unit wide or tall).
  [[nodiscard]] static bool corner_rule(const db::Rect& tile, const db::Rect& s) noexcept {
    const bool x_edge = tile.x0 >= s.x1 - 1 || tile.x1 <= s.x0 + 1;
    const bool y_edge = tile.y0 >= s.y1 - 1 || tile.y1 <= s.y0 + 1;
    const bool thin = s.x1 - 1 == s.x0 || s.y1 - 1 == s.y0;
    return x_edge && y_edge && !thin;
  }

  /// True when the net does not yet hold connected material over all of
  /// `piece` on `plane` (then the area must be searched).
  [[nodiscard]] bool unprocessed(tech::PlaneId plane, const db::Rect& piece, tech::TileType type,
                                 const std::optional<db::TriangleSide>& triangle) const {
    const tech::TypeMask other =
        tech_.is_contact(type) ? tech_.not_connected(type) : ~connect_mask(tech_, options_, type);
    const auto any = [](const db::TileView&) { return false; };
    const db::Plane& p = net_[plane];
    return triangle ? !p.enumerate_triangle(piece, triangle->diagonal, triangle->right_side, other, any)
                    : !p.enumerate(piece, other, any);
  }

  bool paint_net(tech::PlaneId plane, const db::Rect& piece, tech::TileType type,
                 const std::optional<db::TriangleSide>& triangle) {
    const db::PaintRow row = db::paint_row(tech_, plane, type);
    diag::Status st = triangle ? net_[plane].paint_triangle(piece, triangle->diagonal, triangle->right_side, row,
                                                             options_.magic_compat)
                               : net_[plane].paint(piece, row);
    if (!st.ok()) {
      status_ = st;
      return false;
    }
    return true;
  }

  bool report(const TreeTile& t, const db::Rect& piece, tech::TileType type,
              const std::optional<db::TriangleSide>& triangle) {
    NetTile out;
    out.plane = t.plane;
    out.tile.area = piece;
    out.tile.type = type;
    if (triangle) {
      out.tile.split = true;
      out.tile.diagonal = triangle->diagonal;
      out.tile.right_side = triangle->right_side;
      (triangle->right_side ? out.tile.right_type : out.tile.left_type) = type;
    }
    out.use_path = *t.path;
    if (!visit_.on_tile(out)) {
      stopped_ = true;
      return false;
    }
    return true;
  }

  /// Grows the piece by one unit; a triangle only on its two leg sides.
  [[nodiscard]] static db::Rect grow(db::Rect r, const std::optional<db::TriangleSide>& triangle) noexcept {
    if (!triangle) {
      return db::expand(r, 1);
    }
    (triangle->right_side ? r.x1 : r.x0) += triangle->right_side ? 1 : -1;
    const bool top = triangle->right_side == (triangle->diagonal == db::Diagonal::nw_se);
    (top ? r.y1 : r.y0) += top ? 1 : -1;
    return r;
  }

  /// Queues an area unless one of the five areas queued last before the
  /// current one was taken (Magic's csa2_lasttop window) has the same mask
  /// and surrounds it.
  void enqueue(const db::Rect& area, tech::TileType mask_type, const tech::TypeMask& mask,
               const std::optional<db::TriangleSide>& triangle) {
    const std::size_t first = last_top_ > 5 ? last_top_ - 5 : 0;
    for (std::size_t i = std::min(last_top_, queue_.size()); i > first; --i) {
      const Pending& q = queue_[i - 1];
      if (q.mask_type == mask_type && db::surround(q.area, area)) {
        return;
      }
    }
    queue_.push_back(Pending{area, mask_type, mask, triangle});
  }

  void copy_labels(const db::Rect& area, const tech::TypeMask& mask) {
    (void)search_tree_labels(root_, area, mask, options_.descend, options_.magic_compat,
                             [&](const TreeLabel& l) { return label(l); });
  }

  bool label(const TreeLabel& l) {
    const db::Label& lab = *l.label;
    db::Label copy = transform_label(lab, l.to_root, options_.magic_compat);
    const bool top = l.path->empty();
    if (!top && options_.labels == LabelPolicy::full) {
      copy.text = *l.path + "/" + lab.text;
    }
    const auto key = [&](const std::string& text) {
      return LabelKey{copy.rect.x0, copy.rect.y0, copy.rect.x1, copy.rect.y1, lab.type, text};
    };
    if (labels_.count(key(lab.text)) > 0 || labels_.count(key(copy.text)) > 0) {
      return true;
    }
    labels_.insert(key(copy.text));
    if (!visit_.on_label(NetLabel{&lab, copy.text, copy.rect, copy})) {
      stopped_ = true;
      return false;
    }
    if (lab.is_port()) {
      queue_equivalent_port(*l.def, lab, l.to_root);
    }
    return true;
  }

  /// Material under another port label of the same index in the same cell
  /// is connected (one such label per call, as Magic does).
  void queue_equivalent_port(const db::CellDef& def, const db::Label& lab, const db::Transform& to_root) {
    for (const db::Label& other : def.labels()) {
      if (&other == &lab || !other.is_port() || other.port_index != lab.port_index || other.type == tech::kSpace) {
        continue;
      }
      const db::Rect area = db::apply(to_root, other.rect);
      const tech::TypeMask mask = connect_mask(tech_, options_, other.type);
      const auto home = tech_.home_plane(other.type);
      if (home && !area.is_null() && !net_[*home].enumerate(area, mask, [](const db::TileView&) { return false; })) {
        continue;  // already in the net
      }
      enqueue(db::expand(area, 1), other.type, mask, std::nullopt);
      return;
    }
  }

  const db::CellDef& root_;
  const tech::Technology& tech_;
  const SearchStart& start_;
  const SearchOptions& options_;
  NetVisitor& visit_;
  std::vector<db::Plane> net_;  // the net so far, per plane (Magic's destination cell)
  std::vector<Pending> queue_;
  std::size_t last_top_ = 0;
  db::Rect current_area_;
  std::set<LabelKey> labels_;
  diag::Status status_ = diag::Done{};
  bool stopped_ = false;
};

}  // namespace

diag::Status search_flat(const db::CellDef& def, const tech::Technology& tech, const SearchStart& start,
                         const SearchOptions& options, NetVisitor& visit) {
  std::optional<FlatItem> first;
  for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count() && !first; ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    (void)def.plane(plane).enumerate(start.area, start.types, [&](const db::TileView& t) {
      first = FlatItem{plane, t, t.split && t.right_side};
      return false;
    });
  }
  if (first) {
    FlatSearch(def, tech, start, options, visit).run(*first);
  }
  return diag::Done{};
}

diag::Status search_hierarchical(const db::CellDef& root, const tech::Technology& tech, const SearchStart& start,
                                 const SearchOptions& options, NetVisitor& visit) {
  return TreeSearch(root, tech, start, options, visit).run();
}

}  // namespace hefesto::connect
