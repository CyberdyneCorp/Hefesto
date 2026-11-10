// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal state of one basic check (drc-rule-semantics; openspec/changes/
// m3-drc design.md D6): Magic's drcClientData plus the per-tile edge walk
// of drcTile, split into small functions (Magic's drcTile is one 900-line
// function).
//
//   basic_check.cpp   DRCBasicCheck: plane loop, per-tile dispatch, cancel
//   edges.cpp         drcTile: left/bottom edge walks, the per-pair rule
//                     loop, triggers, exceptions, maxwidth-with-bends
//   standard.cpp      drcTile: check region, corner extensions, areaCheck
//   nonstandard.cpp   DRCextend.c: area, maxwidth (bend_illegal, both),
//                     rectangle size, off-grid, angles
//   diagonal.cpp      drcTile: split-tile (diagonal edge) checks, areaNMCheck
//   euclidean.cpp     drcRadialClear, drcCifPointToSegment, drcDerivedBorder
//   tiles.cpp         stitch navigation (TR/RT/BL/LB) over db::Plane views
//
// Stitch navigation: db::Plane hides its corner stitches, so every stitch
// step of Magic's code is replaced by the tile that contains the unit
// square the stitch points at (BL(t) = tile at (LEFT(t)-1, BOTTOM(t)),
// LB(t) = (LEFT(t), BOTTOM(t)-1), TR(t) = (RIGHT(t), TOP(t)-1),
// RT(t) = (RIGHT(t)-1, TOP(t))); neighbor walks are area enumerations of
// the one-unit strip beside the tile. Tiles are identified by their
// rectangle (unique within a plane).
//
// (Magic ref: drc/DRCbasic.c, drc/DRCextend.c, utils/maxrect.c)
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_set>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/drc/check/basic_check.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/types.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::drc::check_detail {

using db::Coord;
using db::Rect;
using db::TileView;
using tech::TileType;

// --- Tile side types (TiGetLeftType & co., tiles/tile.h) ------------------

[[nodiscard]] inline TileType left_type(const TileView& t) noexcept { return t.split ? t.left_type : t.type; }
[[nodiscard]] inline TileType right_type(const TileView& t) noexcept { return t.split ? t.right_type : t.type; }
/// `\` (nw_se, SplitDirection 1): the top triangle is the right side.
[[nodiscard]] inline TileType top_type(const TileView& t) noexcept {
  if (!t.split) {
    return t.type;
  }
  return t.diagonal == db::Diagonal::nw_se ? t.right_type : t.left_type;
}
[[nodiscard]] inline TileType bottom_type(const TileView& t) noexcept {
  if (!t.split) {
    return t.type;
  }
  return t.diagonal == db::Diagonal::nw_se ? t.left_type : t.right_type;
}
/// TiGetType: the body type, the left type of a split tile.
[[nodiscard]] inline TileType body_type(const TileView& t) noexcept { return left_type(t); }

/// GeoClip: clamps each side of `r` into `area` (the result may be inverted).
inline void geo_clip(Rect& r, const Rect& area) noexcept {
  r.x0 = std::max(r.x0, area.x0);
  r.y0 = std::max(r.y0, area.y0);
  r.x1 = std::min(r.x1, area.x1);
  r.y1 = std::min(r.y1, area.y1);
}

/// The tile containing the unit square at (x, y) (GOTOPOINT).
[[nodiscard]] TileView tile_at(const db::Plane& plane, Coord x, Coord y);
/// The distinct tiles overlapping `area` in enumeration order (a split tile
/// once, as first reported), appended to `out` after clearing it.
void tiles_in(const db::Plane& plane, const Rect& area, std::vector<TileView>& out);
/// Tiles abutting the left side of `t` with y overlapping [y0, y1), bottom
/// to top (BL(t), then RT steps).
void left_neighbors(const db::Plane& plane, const TileView& t, Coord y0, Coord y1, std::vector<TileView>& out);
/// Tiles abutting the bottom of `t` with x overlapping [x0, x1), left to
/// right (LB(t), then TR steps).
void bottom_neighbors(const db::Plane& plane, const TileView& t, Coord x0, Coord x1, std::vector<TileView>& out);

// --- Euclidean arithmetic (drc/DRCbasic.c) --------------------------------

/// Corner marks of a check region (RADIAL_* in drc/DRCbasic.c).
namespace radial {
inline constexpr unsigned kNW = 0x1;
inline constexpr unsigned kNE = 0x2;
inline constexpr unsigned kSW = 0x4;
inline constexpr unsigned kSE = 0x8;
}  // namespace radial

// --- Per-check state ------------------------------------------------------

/// One edge segment of the walk (drcTile): `at` is X (vertical) or Y
/// (horizontal), [lo, hi] the segment along the edge; `tile` is the tile
/// being scanned (right of / above the edge) and `outer` the neighbor
/// (tpleft / tpbot).
struct Edge {
  bool vertical = true;
  Coord at = 0;
  Coord lo = 0;
  Coord hi = 0;
  const TileView* tile = nullptr;
  const TileView* outer = nullptr;
  bool first = false;  // firsttile: the first examined segment of the tile's edge
};

/// The search of one standard rule application (areaCheck's client data).
struct Search {
  const Rule* rule = nullptr;
  Rect region;          // dCD_constraint (E)
  unsigned marks = 0;   // dCD_radial corner bits
  std::int32_t sdist = 0;  // Euclidean corner radius s (Q-DRC-20)
  const TileView* initial = nullptr;  // dCD_initial: the inner-side tile
};

/// The trigger bookkeeping that survives from one rule of a pair to the
/// next (drcTile's `triggered` and `mrd`).
struct RuleState {
  int triggered = 0;
  bool from_maxrects = false;  // mrd != NULL: triggered rectangles come from maxrects_
};

/// Hash of a tile rectangle (tile identity within a plane).
struct RectHash {
  std::size_t operator()(const Rect& r) const noexcept {
    constexpr auto kMix = static_cast<std::size_t>(0x9E3779B97F4A7C15ULL);
    std::size_t h = static_cast<std::uint32_t>(r.x0);
    h = h * kMix ^ static_cast<std::uint32_t>(r.y0);
    h = h * kMix ^ static_cast<std::uint32_t>(r.x1);
    h = h * kMix ^ static_cast<std::uint32_t>(r.y1);
    return h;
  }
};

class Kernel {
 public:
  Kernel(const CheckInput& input, const Rect& check_area, const Rect& clip_area, ErrorSink& sink);

  /// DRCBasicCheck over every paint plane.
  CheckOutcome run();

 private:
  // basic_check.cpp
  bool visit_tile(const TileView& tile);
  void report(const Rect& area, const Rule& rule);
  void count(std::uint64_t CheckStatistics::* counter) const noexcept;
  [[nodiscard]] const db::Plane* plane_of(tech::PlaneId plane) const noexcept;

  // edges.cpp
  void vertical_edges(const TileView& tile);
  void horizontal_edges(const TileView& tile);
  void evaluate_edge(const Edge& edge, std::span<const Rule> rules);
  /// One rule of the pair list; false when the next rule must be skipped.
  bool evaluate_rule(const Edge& edge, const Rule& rule, RuleState& state);
  /// The part of evaluate_rule after the rule passed its exception, kind
  /// and edge-plane filters: area, maxwidth, rectangle size, off-grid or a
  /// standard edge rule.
  bool dispatch_rule(const Edge& edge, const Rule& rule, bool trigger, RuleState& state);
  bool evaluate_vertical_nonstandard(const Edge& edge, const Rule& rule);
  bool maxwidth_bends(const Edge& edge, const Rule& rule, bool trigger, RuleState& state);
  bool maxwidth_trigger(const Edge& edge, const Rule& rule, bool found, RuleState& state) const;
  [[nodiscard]] bool exception_allows(const Edge& edge, const Rule& rule) const;

  // standard.cpp
  bool standard_rule(const Edge& edge, const Rule& rule, bool trigger, RuleState& state);
  [[nodiscard]] std::int32_t corner_distance(const Edge& edge, const Rule& rule) const noexcept;
  void along_range(const Edge& edge, std::int32_t c, RuleState& state, Search& search);
  bool vertical_region(const Edge& edge, const Rule& rule, std::int32_t c, Search& search);
  /// The corner probes of a vertical edge's top (bottom) end on its left
  /// (outer) or right (tile) side, limited by the region's end `limit`.
  [[nodiscard]] TileView top_probe(const Edge& edge, bool left_side, Coord limit) const;
  [[nodiscard]] TileView bottom_probe(const Edge& edge, bool left_side, Coord limit) const;
  /// Extends the region past the top (bottom) end by c when the probe on
  /// the `test_left` side is a corner type and the edge ends there.
  void extend_top(const Edge& edge, const Rule& rule, std::int32_t c, bool test_left, unsigned corner,
                  Search& search) const;
  void extend_bottom(const Edge& edge, const Rule& rule, std::int32_t c, bool test_left, unsigned corner,
                     Search& search) const;
  bool vertical_forward(const Edge& edge, const Rule& rule, std::int32_t c, Search& search);
  bool vertical_reverse(const Edge& edge, const Rule& rule, std::int32_t c, Search& search);
  bool horizontal_region(const Edge& edge, const Rule& rule, std::int32_t c, Search& search);
  bool horizontal_forward(const Edge& edge, const Rule& rule, std::int32_t c, Search& search);
  bool horizontal_reverse(const Edge& edge, const Rule& rule, std::int32_t c, Search& search);
  void mark(const Rule& rule, unsigned corner, Search& search) const noexcept;
  void search_region(const Search& search);
  void candidate(const TileView& found, const Search& search);
  void record_trigger(const Rect& area);
  [[nodiscard]] bool radial_exempt(const TileView& found, const Rect& area, const Search& search) const;
  [[nodiscard]] ContactBorder derived_border(const TileView& found, const Search& search) const;

  // nonstandard.cpp
  void check_area(const TileView& start, const Rule& rule);
  void check_maxwidth(const TileView& start, const Rule& rule, bool both);
  void check_rect_size(const TileView& start, const Rule& rule);
  void check_off_grid(Rect edge, const Rule& rule);
  void check_angles(const TileView& tile, const Rule& rule);
  /// Flood over 4-connected tiles whose facing side is in `ok`, from
  /// `start`; `visit` returns false to stop. Returns false when stopped.
  template <typename Visit>
  bool flood(const TileView& start, const tech::TypeMask& ok, Visit&& visit);
  void push_neighbors(const TileView& tile, const tech::TypeMask& ok);
  bool canonical_maxwidth(const TileView& start, int direction, std::int32_t limit, const Rule& rule);

  // diagonal.cpp
  void split_tile(const TileView& tile);
  void diagonal_rule(const TileView& tile, const Rule& rule);

  const CheckInput& input_;
  const db::CellDef& cell_;
  const RuleSet& rules_;
  Rect check_;  // K (dCD_rect)
  Rect clip_;   // L (dCD_clip)
  ErrorSink& sink_;
  tech::PlaneId plane_ = 0;  // dCD_plane
  const db::Plane* edge_plane_ = nullptr;

  std::vector<Rect> rlist_;  // dCD_rlist (trigger records; never shrunk, as Magic's)
  int entries_ = 0;          // dCD_entries
  std::vector<Rect> maxrects_;  // MaxRectsData::rlist of the last canonical maxwidth search

  // Scratch buffers (reused; one per walk so the walks can nest).
  std::vector<TileView> left_;
  std::vector<TileView> below_;
  std::vector<TileView> flood_stack_;
  std::unordered_set<Rect, RectHash> flood_seen_;
  std::vector<TileView> neighbors_;
};

/// Map directions of drcCanonicalMaxwidth (GEO_NORTH ... GEO_WEST).
namespace direction {
inline constexpr int kNorth = 0;
inline constexpr int kSouth = 1;
inline constexpr int kEast = 2;
inline constexpr int kWest = 3;
}  // namespace direction

}  // namespace hefesto::drc::check_detail
