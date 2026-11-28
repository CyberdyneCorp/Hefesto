// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of extract/cap: the run context (style tables, options,
// node lookup), the tile-side types of Magic's split-tile macros, the
// corner-stitch steps (RT, TR, LB, BL) over db::Plane, and the shield-plane
// recursion shared by overlap, sideoverlap and corner shielding.
//
// Owner: EXTCAP.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/cap/caps.hpp"
#include "hefesto/util/function_ref.hpp"

namespace hefesto::extract::cap {

using Node = std::optional<std::size_t>;

/// Which coupling kinds an edge pass computes.
struct Kinds {
  bool overlap = true;
  bool sidewall = true;
  bool sideoverlap = true;
  bool corners = true;
};

/// A boundary segment of a tile (Magic's Boundary), Manhattan only.
enum class Dir : std::uint8_t { left, right, bottom, top };

struct Boundary {
  db::TileView inside;
  db::TileView outside;
  db::Rect segment;
  Dir dir = Dir::top;
};

/// The corner stitches of a tile.
enum class Stitch : std::uint8_t { rt, tr, lb, bl };

class Context {
 public:
  /// `node_count` bounds the default client-slot lookup.
  Context(const CapInputs& in, NodeRegionList* nodes, CouplingTable* table, std::size_t node_count);
  Context(const CapInputs& in, NodeRegionList* nodes, CouplingTable* table)
      : Context(in, nodes, table, nodes != nullptr ? nodes->size() : 0) {}

  [[nodiscard]] bool valid() const noexcept { return valid_; }
  [[nodiscard]] const ExtStyle& style() const noexcept { return *style_; }
  [[nodiscard]] const CapTables& tables() const noexcept { return style_->caps.tables; }
  [[nodiscard]] const tech::Technology& technology() const noexcept { return *tech_; }
  [[nodiscard]] const compat::CompatSettings& compat() const noexcept { return compat_; }
  [[nodiscard]] const ExtractOptions& options() const noexcept { return options_; }
  [[nodiscard]] ExtractOptions& options() noexcept { return options_; }
  [[nodiscard]] const std::optional<db::Rect>& clip() const noexcept { return clip_; }
  [[nodiscard]] std::int32_t halo() const noexcept { return halo_; }
  [[nodiscard]] std::size_t plane_end() const noexcept { return plane_end_; }
  [[nodiscard]] const db::Plane& plane(tech::PlaneId p) const { return cell_->plane(p); }
  [[nodiscard]] const db::Rect& search_area() const noexcept { return search_; }

  /// The node of a tile side (Magic's ExtGetRegion).
  [[nodiscard]] Node node(tech::PlaneId plane, const db::TileView& tile, bool right_side) const;
  /// The node of a non-split tile, or the left side of a split one.
  [[nodiscard]] Node node(tech::PlaneId plane, const db::TileView& tile) const { return node(plane, tile, false); }

  /// DBPlaneToResidue for a contact, the type itself otherwise.
  [[nodiscard]] tech::TileType residue(tech::TileType type, tech::PlaneId plane) const;
  /// exts_planeOrder[plane] (-1 when the style has none).
  [[nodiscard]] std::int32_t plane_order(tech::PlaneId plane) const;

  // Magic's tables (zero when absent).
  [[nodiscard]] double area_cap(tech::TileType t) const;
  [[nodiscard]] double perim_cap(tech::TileType a, tech::TileType b) const;
  [[nodiscard]] double overlap_cap(tech::TileType a, tech::TileType b) const;
  [[nodiscard]] float mult(tech::TileType a, tech::TileType b) const;

  /// nreg_cap -= value for a node (nothing without one), and the "ignore
  /// residual error at ~zero" clamp when `clamp` is set.
  void subtract_cap(Node node, double value, bool clamp) const;
  /// The coupling table entry of two distinct nodes gains `value`.
  void add_coupling(std::size_t a, std::size_t b, double value) const;
  [[nodiscard]] NodeRegionList* nodes() const noexcept { return nodes_; }

 private:
  const db::CellDef* cell_ = nullptr;
  const ExtStyle* style_ = nullptr;
  const tech::Technology* tech_ = nullptr;
  compat::CompatSettings compat_;
  ExtractOptions options_;
  std::optional<db::Rect> clip_;
  db::Rect search_;
  std::int32_t halo_ = 0;
  std::size_t plane_end_ = 0;
  NodeLookup node_of_;
  NodeRegionList* nodes_ = nullptr;
  CouplingTable* table_ = nullptr;
  bool valid_ = false;
};

// --- Tile sides (tiles/tile.h split macros) ---

[[nodiscard]] tech::TileType side_type(const db::TileView& t, bool right_side) noexcept;
[[nodiscard]] tech::TileType left_type(const db::TileView& t) noexcept;    // TiGetLeftType
[[nodiscard]] tech::TileType right_type(const db::TileView& t) noexcept;   // TiGetRightType
[[nodiscard]] tech::TileType top_type(const db::TileView& t) noexcept;     // TiGetTopType
[[nodiscard]] tech::TileType bottom_type(const db::TileView& t) noexcept;  // TiGetBottomType
/// TiGetType: the left type of a split tile.
[[nodiscard]] tech::TileType stored_type(const db::TileView& t) noexcept;
/// TiGetTypeExact(t) == TT_SPACE.
[[nodiscard]] bool exact_space(const db::TileView& t) noexcept;
/// SplitDirection: the diagonal runs from upper left to lower right.
[[nodiscard]] bool split_direction(const db::TileView& t) noexcept;

/// One corner-stitch step from `t` (a view `plane` reported); past the
/// usable plane, a space view beyond infinity that ends every walk.
[[nodiscard]] db::TileView step(const db::Plane& plane, const db::TileView& t, Stitch s);

/// GEOCLIP: `r` clipped to `c` (may come out inverted).
[[nodiscard]] db::Rect geoclip(db::Rect r, const db::Rect& c) noexcept;

/// The first user plane in `planes` at or after kFirstUserPlane, below `end`.
[[nodiscard]] std::optional<tech::PlaneId> first_plane(tech::PlaneMask planes, std::size_t end) noexcept;

/// The shield recursion of extSubtractOverlap{,2}, extSubtractSideOverlap{,2}
/// and extSubtractCornerOverlap{,2}: the first plane of `planes` (in plane
/// number order) is searched; with no plane after it, every tile of a
/// `shield` type calls `on(tile, piece)`; otherwise each tile does when its
/// type is a shield type, and the search goes on to the next plane over the
/// tile's piece when it is not. `piece` is the tile clipped to the area,
/// never empty in the recursive step.
using ShieldVisit = util::FunctionRef<void(const db::TileView&, const db::Rect&)>;
void shield_pieces(const Context& ctx, tech::PlaneMask planes, const tech::TypeMask& shield, const db::Rect& area,
                   ShieldVisit on);

/// The tile continuing beyond endpoint `cx` on the inside of a horizontal
/// boundary (tcont) and the tile diagonally beyond it on the outside
/// (tdiag) (ExtBasic.c extNodeCornerCap, ExtCouple.c extAddCornerCouple).
struct CornerPair {
  db::TileView cont;
  db::TileView diag;
};
[[nodiscard]] CornerPair corner_pair(const db::Plane& plane, const db::TileView& inside, const db::TileView& outside,
                                     bool topside, bool right_end, db::Coord cx, db::Coord cy);

/// EXTRACT-CAP-002: an entry point called without a cell, a style or a
/// technology.
[[nodiscard]] diag::Status missing_inputs(std::string_view what);

/// The tiles of each node (indexed by node) in extNodeAreaFunc's visit
/// order (flood_order.cpp); every tile with a node appears exactly once.
[[nodiscard]] std::vector<std::vector<NodeTile>> flood_order(const Context& ctx, std::size_t node_count);

/// The edge pass (extBasicCouple and its walks) of one plane.
void couple_plane(const Context& ctx, tech::PlaneId plane, const Kinds& kinds);
/// The overlap pass (extBasicOverlap) of one plane.
void overlap_plane(const Context& ctx, tech::PlaneId plane);

}  // namespace hefesto::extract::cap
