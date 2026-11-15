// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internals shared by the subcell (hier.cpp) and array (arrays.cpp)
// corrections of hierarchical generation (design D8).
//
// Owner: GEN.

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "hefesto/mask/gen/classify.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "mask/gen/internal.hpp"

namespace hefesto::mask::gen_detail {

/// The state of one cell's hierarchical generation.
struct HierPass {
  HierPass(const GenerationSource& src, const OutputStyle& st, GenerationContext& c, const db::CellDef& def)
      : source(src),
        style(st),
        ctx(c),
        cell(def),
        radius(generation_radius(st, src.compat)),
        flattener(*src.technology, src.compat.magic_compat),
        unfracture(src.unfracture) {}

  // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
  const GenerationSource& source;
  const OutputStyle& style;
  GenerationContext& ctx;
  const db::CellDef& cell;
  // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
  std::int64_t radius;
  Flattener flattener;
  /// `cif unfracture` for the yank generations (the source's switch).
  bool unfracture = false;
  /// The style's layer classes (set by the array pass).
  const LayerClasses* classes = nullptr;
  /// CIFErrorDef of the residual step (the parent).
  const db::CellDef* error_cell = nullptr;
};

/// Q-MASK-402: false for a layer that stopped hierarchically, with
/// magic-compat OFF (it then has no residual).
[[nodiscard]] bool keeps_residual(const HierPass& pass, std::size_t layer);

/// cifCheckAndErase: every component tile of a mask layer not covered by
/// the total is reported (not for templayers), then every component tile is
/// erased from the total.
void check_and_erase(HierPass& pass, LayerPlanes& total, const LayerPlanes& comp);

/// cifHierPaintFunc over every layer: the residual tiles (grown by
/// cifGrowSliver when the style has min-width) OR'ed into `out`.
void paint_residual(HierPass& pass, const LayerPlanes& total, LayerPlanes& out);

/// Paints `t`'s shape over `area` (a triangle for a split tile).
void paint_piece(MaskPlane& plane, const db::TileView& t, const db::Rect& area);

/// cifGenHierArea over the interaction rectangle `area`.
void gen_hier_area(HierPass& pass, const db::Rect& area, const tech::TypeMask& yank, const std::vector<bool>& gen,
                   LayerPlanes& out);

/// cifGrowSliver ("Sliver growth of hierarchical pieces"): the area tile
/// `t` of `plane` (the residual plane) is painted over, grown to
/// `min_width` (mask units). magic-compat OFF grows each short axis to at
/// least the width, independently (Q-MASK-409).
[[nodiscard]] db::Rect grow_sliver(const db::Plane& plane, const db::TileView& t, std::int32_t min_width,
                                   bool magic_compat);

/// The interaction squares of CIFGenSubcells over `area` clipped to the
/// cell's bounding box, y outer and x inner, each grown by the radius;
/// squares that no use of the cell can reach are skipped.
class SquareGrid {
 public:
  SquareGrid(const db::CellDef& cell, const db::Rect& area, std::int64_t radius, std::int64_t step);

  /// The number of squares Magic counts (totcuts).
  [[nodiscard]] std::uint64_t count() const noexcept;
  /// Visits the squares; false from `visit` stops.
  void for_each(const std::function<bool(const db::Rect& square)>& visit) const;

 private:
  bool walk_row(const std::vector<db::Rect>& row, std::int64_t y, std::int64_t y1,
                const std::function<bool(const db::Rect&)>& visit) const;

  db::Rect total_;
  std::int64_t radius_;
  std::int64_t step_;
  std::vector<db::Rect> reach_;
};

}  // namespace hefesto::mask::gen_detail
