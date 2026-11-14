// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The per-tile part of the mask-layer check (drcCifTile, areaCifCheck,
// drcCheckCifArea, drcCheckCifMaxwidth; capability drc-cif-rules "Mask-
// layer check procedure", "Mask area rule", "Mask maxwidth rule"), private
// to src/core/src/drc/check/mask_check*.cpp.
//
// Stitch steps are replaced by point locations and neighbor walks over
// db::Plane as in the basic check (kernel.hpp): each corner sample is the
// tile containing the unit square the spec names. Arithmetic on distances
// is 64-bit and clamped to the plane, so hostile rule values cannot wrap.
//
// Owner: DRCCIF (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/drc/check/basic_check.hpp"
#include "hefesto/drc/check/mask_check.hpp"
#include "hefesto/drc/types.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "kernel.hpp"

namespace hefesto::drc::mask_detail {

using db::Coord;
using db::Rect;
using db::TileView;

/// `v` clamped into the plane's coordinate range.
[[nodiscard]] constexpr Coord clamp_coord(std::int64_t v) noexcept {
  constexpr std::int64_t lo = db::kMinusInfinity;
  constexpr std::int64_t hi = db::kInfinity;
  return static_cast<Coord>(v < lo ? lo : (v > hi ? hi : v));
}

/// K times the style's scale (drcCifCheck's cifrect), clamped.
[[nodiscard]] constexpr Rect scaled(const Rect& r, std::int64_t s) noexcept {
  return Rect{clamp_coord(r.x0 * s), clamp_coord(r.y0 * s), clamp_coord(r.x1 * s), clamp_coord(r.y1 * s)};
}

/// Euclidean corner marks of drcCifTile (RADIAL_NW, ...).
namespace corner_mark {
inline constexpr unsigned kNW = 0x1;
inline constexpr unsigned kNE = 0x2;
inline constexpr unsigned kSW = 0x4;
inline constexpr unsigned kSE = 0x8;
}  // namespace corner_mark

class MaskChecker {
 public:
  /// `cif_rect` is K in mask units, `clip` is L in internal units.
  MaskChecker(const CheckInput& input, const mask::LayerPlanes& planes, std::int32_t scale, const Rect& cif_rect,
              const Rect& clip, ErrorSink& sink);

  /// Every rule of `rules`, layer by layer in style order, the solid-tile
  /// list before the space-tile list.
  CheckOutcome run(const MaskRules& rules);
  /// Region errors that collapsed under truncating division (DRC-MASK-*).
  [[nodiscard]] std::size_t collapsed() const noexcept { return collapsed_; }

 private:
  /// One rule over the tiles of layer `layer` of the list's kind; false
  /// when cancelled.
  bool run_rule(std::size_t layer, MaskList which, const MaskRule& rule);
  /// drcCifTile.
  bool visit(const TileView& tile);
  void vertical_edges(const TileView& tile);
  void horizontal_edges(const TileView& tile);
  /// The rule's check region from one edge segment (corner extensions and
  /// the far side); false when the one-tile shortcut proves it clean.
  bool vertical_forward(const TileView& tile, Rect& region);
  bool vertical_reverse(const TileView& tile, const TileView& outer, Rect& region);
  bool horizontal_forward(const TileView& tile, Rect& region);
  bool horizontal_reverse(const TileView& tile, const TileView& outer, Rect& region);
  /// Extends `side` by the corner distance when `type` is a corner type.
  void extend(tech::TileType type, Coord& side, std::int64_t delta, unsigned mark);
  /// The radial bookkeeping and the search of a region (DBSrPaintArea with
  /// areaCifCheck).
  void search(const Rect& region);
  /// areaCifCheck.
  void candidate(const TileView& found, const Rect& region);
  [[nodiscard]] bool radial_exempt(const TileView& found, const Rect& area, const Rect& region) const;
  /// drcCheckCifArea and drcCheckCifMaxwidth.
  void check_area(const TileView& start);
  void check_maxwidth(const TileView& start);
  /// Floods the 4-connected tiles whose body type is in the rule's set,
  /// from `start`; `visit` returns false to stop. False when stopped.
  template <typename Visit>
  bool flood(const TileView& start, Visit&& visit);
  /// Reports a whole tile of an area or maxwidth error.
  void report_region(const Rect& tile_area);
  /// The tile at (x, y) of the current layer, located from `near`.
  [[nodiscard]] TileView probe(std::int64_t x, std::int64_t y, const TileView& near) const;
  void count(std::uint64_t CheckStatistics::* counter) const noexcept {
    if (CheckStatistics* stats = sink_.statistics()) {
      ++(stats->*counter);
    }
  }
  [[nodiscard]] const Rule& rule() const noexcept { return current_->rule; }

  const CheckInput& input_;
  const mask::LayerPlanes& planes_;
  std::int32_t scale_;
  Rect rect_;  // K in mask units (dCD_rect)
  Rect clip_;  // L (dCD_clip)
  ErrorSink& sink_;

  const MaskRule* current_ = nullptr;  // drcCifCur
  std::size_t layer_ = 0;              // dCD_plane
  const db::Plane* plane_ = nullptr;
  unsigned marks_ = 0;    // dCD_radial corner bits (per tile, kept across its edges)
  std::int32_t sdist_ = 0;  // dCD_radial & 0xfff
  bool cancelled_ = false;
  std::size_t collapsed_ = 0;

  std::vector<TileView> outer_;
  std::vector<TileView> flood_stack_;
  std::vector<TileView> neighbors_;
  check_detail::RectSet flood_seen_;
};

}  // namespace hefesto::drc::mask_detail
