// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internals of mask generation (capability mask-hierarchical-generation;
// openspec/changes/m5-mask-gds-cif/design.md D8): the hierarchy search
// (DBCellSrArea, DBArrayOverlap), the flattening into scratch cells
// (DBTreeSrTiles with cifHierCopyFunc, CIFCopyMaskHints) and the generation
// error report of the hierarchical passes.
//
// Owner: GEN.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <unordered_map>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::mask::gen_detail {

// Diagnostic codes of GEN (src/core/src/mask/gen/codes.inc).
inline constexpr std::string_view kUnknownLayer = "MASK-GEN-001";
inline constexpr std::string_view kNoStyle = "MASK-GEN-002";
inline constexpr std::string_view kDisagree = "MASK-GEN-003";
inline constexpr std::string_view kTooDeep = "MASK-GEN-004";

/// The deepest hierarchy the generators descend (a recursion bound for
/// malformed input; real designs are a few dozen levels deep).
inline constexpr int kMaxDepth = 1024;

/// The empty accumulator of the interaction rectangle algebra (GeoNullRect).
inline constexpr db::Rect kNullRect{0, 0, 0, 0};

/// GeoClip: each side of `r` clipped to `area`; the result may be
/// degenerate.
[[nodiscard]] constexpr db::Rect clip(const db::Rect& r, const db::Rect& area) noexcept {
  return db::Rect{r.x0 < area.x0 ? area.x0 : r.x0, r.y0 < area.y0 ? area.y0 : r.y0, r.x1 > area.x1 ? area.x1 : r.x1,
                  r.y1 > area.y1 ? area.y1 : r.y1};
}

/// `v` clamped into the coordinate range (64-bit intermediate math).
[[nodiscard]] constexpr db::Coord clamp_coord(std::int64_t v) noexcept {
  constexpr std::int64_t lo = db::kMinusInfinity;
  constexpr std::int64_t hi = db::kInfinity;
  return static_cast<db::Coord>(v < lo ? lo : (v > hi ? hi : v));
}

/// GEO_EXPAND without overflow: each side moved by `n`, clamped.
[[nodiscard]] constexpr db::Rect grow(const db::Rect& r, std::int64_t n) noexcept {
  return db::Rect{clamp_coord(std::int64_t{r.x0} - n), clamp_coord(std::int64_t{r.y0} - n),
                  clamp_coord(std::int64_t{r.x1} + n), clamp_coord(std::int64_t{r.y1} + n)};
}

/// `r` with every side multiplied by `s` (the SCALE macro), clamped.
[[nodiscard]] constexpr db::Rect scaled(const db::Rect& r, std::int64_t s) noexcept {
  return db::Rect{clamp_coord(std::int64_t{r.x0} * s), clamp_coord(std::int64_t{r.y0} * s),
                  clamp_coord(std::int64_t{r.x1} * s), clamp_coord(std::int64_t{r.y1} * s)};
}

/// The style's scale factor, never below 1.
[[nodiscard]] inline std::int32_t scale_of(const OutputStyle& style) noexcept {
  return style.scale.scale_factor > 0 ? style.scale.scale_factor : 1;
}

/// DBAllButSpaceAndDRCBits.
[[nodiscard]] const tech::TypeMask& all_but_space_and_drc() noexcept;

// --- Hierarchy search --------------------------------------------------------

/// One array element of a use, with its transform to the parent.
struct ElementRef {
  const db::CellUse* use = nullptr;
  std::int32_t x = 0;
  std::int32_t y = 0;
  db::Transform to_parent;
};

/// DBArrayOverlap: the element index range of `use` whose images may
/// overlap `parent_area` (C truncating division, 64-bit).
struct IndexRange {
  std::int32_t xlo = 0;
  std::int32_t xhi = 0;
  std::int32_t ylo = 0;
  std::int32_t yhi = 0;
};
[[nodiscard]] IndexRange array_overlap(const db::CellUse& use, const db::Rect& parent_area);

/// Return value of a search callback (dbCellSrFunc): go on, stop the whole
/// search, or skip the remaining elements of the current array.
enum class Visit : std::uint8_t { next, stop, skip_array };

using ElementVisitor = std::function<Visit(const ElementRef& element, const db::Rect& child_area)>;

/// DBCellSrArea: every element of every use of `def` overlapping `area`
/// (def coordinates; the use index's BPE_OVERLAP search), elements y outer
/// and x inner, with `area` in the element's child coordinates. Returns
/// false when a visit stopped the search.
bool search_elements(const db::CellDef& def, const db::Rect& area, const ElementVisitor& visit);

/// DBArraySr: the elements of `use` overlapping `parent_area`.
void array_elements(const db::CellUse& use, const db::Rect& parent_area,
                    const std::function<void(const ElementRef&)>& visit);

// --- Flattening --------------------------------------------------------------

/// A scratch cell (CIFTotalDef, CIFComponentDef): never in a library.
[[nodiscard]] std::unique_ptr<db::CellDef> make_scratch(std::string_view name, const tech::Technology& technology);
/// DBCellClearDef: every plane and property of `scratch` emptied.
void clear_scratch(db::CellDef& scratch);

/// The flattening of mask generation ("Flattening for interaction
/// processing"): DBTreeSrTiles with cifHierCopyFunc (whole tiles of the
/// requested types at every depth, transformed and painted on every plane
/// the type paints, with the technology's paint tables; vendor paint is
/// copied too, Q-MASK-401) and the mask-hint copies (CIFCopyMaskHints of
/// the root, then of every element of the subtree, whole and unclipped).
class Flattener {
 public:
  /// `magic_compat` false rounds split-tile diagonals half up (Q-MODEL-3).
  Flattener(const tech::Technology& technology, bool magic_compat)
      : technology_(technology), magic_compat_(magic_compat) {}

  /// DBTreeSrTiles over `area` (`root` coordinates) of `root` placed by
  /// `to_target` in `target`. False when the hierarchy was deeper than
  /// kMaxDepth (the deeper levels are not copied).
  bool copy_paint(const db::CellDef& root, const db::Rect& area, const db::Transform& to_target,
                  const tech::TypeMask& types, db::CellDef& target);
  /// CIFCopyMaskHints of `root`, then DBTreeSrCells(CIFCopyMaskHints) over
  /// `area`.
  void copy_hints(const db::CellDef& root, const db::Rect& area, const db::Transform& to_target,
                  db::CellDef& target);
  /// Both, paint first.
  bool copy(const db::CellDef& root, const db::Rect& area, const db::Transform& to_target,
            const tech::TypeMask& types, db::CellDef& target) {
    const bool complete = copy_paint(root, area, to_target, types, target);
    copy_hints(root, area, to_target, target);
    return complete;
  }

 private:
  bool copy_cell(const db::CellDef& def, const db::Rect& area, const db::Transform& t, const tech::TypeMask& types,
                 db::CellDef& target, int depth);
  void copy_tile(const db::TileView& tile, const db::Transform& t, db::CellDef& target);
  [[nodiscard]] const db::PaintRow& row(tech::PlaneId plane, tech::TileType type);

  const tech::Technology& technology_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  bool magic_compat_ = true;
  std::unordered_map<std::uint32_t, db::PaintRow> rows_;
};

// --- Errors --------------------------------------------------------------------

/// CIFError on `cell` for layer `layer`: "CIF error in cell <cell>, layer
/// <layer>: <message>" at `mask_area` divided by the scale factor, to
/// ctx.sink; nothing with `options no-errors`, without a cell or a sink.
void report_error(const OutputStyle& style, const db::CellDef* cell, std::size_t layer, const db::Rect& mask_area,
                  std::string_view code, std::string_view message, GenerationContext& ctx);

}  // namespace hefesto::mask::gen_detail
