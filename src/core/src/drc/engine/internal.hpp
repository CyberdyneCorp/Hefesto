// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private parts of the DRC engine shared by its translation units: the
// check context of one square (or one `drc why`/flat check), Magic's
// rectangle helpers, the hierarchy search primitives the engine ports from
// database/DBcellsrch.c and database/DBcellbox.c, the yank cell flattening
// and the hierarchical checks.
//
// Everything here only reads the database except the scratch yank cell the
// context owns (Magic's DRCdef, never in the cell library, design D7), so
// it may run on the background worker under the shared database lock.
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/drc/check/basic_check.hpp"
#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/drc/types.hpp"
#include "hefesto/mask/gen/context.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::drc::detail {

// --- Magic's rectangle helpers (utils/geometry.[ch]) ----------------------

/// GeoNullRect: {0, 0, 0, 0}. Kept as Magic's value because the engine
/// expands it in one place (drcFindInteractions' sibling pass).
inline constexpr db::Rect kNullRect{0, 0, 0, 0};

/// GeoClip: may leave an inverted rectangle, which GEO_RECTNULL treats as null.
[[nodiscard]] constexpr db::Rect clip(const db::Rect& r, const db::Rect& area) noexcept {
  return db::Rect{r.x0 < area.x0 ? area.x0 : r.x0, r.y0 < area.y0 ? area.y0 : r.y0, r.x1 > area.x1 ? area.x1 : r.x1,
                  r.y1 > area.y1 ? area.y1 : r.y1};
}

/// C's truncating `(v / s) * s`, lowered by `s` when above `v`: the floor
/// multiple of `s` at or below `v` (drcCheckTile's square origin).
[[nodiscard]] constexpr db::Coord floor_multiple(db::Coord v, db::Coord s) noexcept {
  db::Coord out = (v / s) * s;
  return out > v ? out - s : out;
}

// --- Hierarchy search (database/DBcellsrch.c, DBcellbox.c) ---------------

/// One array element reached by DBCellSrArea: the use, its indices and the
/// transform from the element's child coordinates to the searching cell's.
struct Element {
  const db::CellUse* use = nullptr;
  std::int32_t x = 0;
  std::int32_t y = 0;
  db::Transform to_parent;
};

/// DBArrayOverlap: the element index range of `use` whose images may
/// overlap `parent_rect` (parent coordinates; C truncating division).
struct IndexRange {
  std::int32_t xlo = 0;
  std::int32_t xhi = 0;
  std::int32_t ylo = 0;
  std::int32_t yhi = 0;
};
[[nodiscard]] IndexRange array_overlap(const db::CellUse& use, const db::Rect& parent_rect);

/// DBComputeArrayArea: `area` (child coordinates) moved to element (x, y)
/// of `use`, still in child coordinates.
[[nodiscard]] db::Rect array_area(const db::Rect& area, const db::CellUse& use, std::int32_t x, std::int32_t y);

/// The uses of `def` whose bounding box overlaps `area` (DBSrCellPlaneArea,
/// BPE_OVERLAP), in Magic's subcell enumeration order.
[[nodiscard]] std::vector<const db::CellUse*> uses_overlapping(const db::CellDef& def, const db::Rect& area);

/// Return value of a cell-search callback (dbCellSrFunc): continue, stop
/// the whole search, or skip the remaining elements of the current array.
enum class Visit : std::uint8_t { next, stop, skip_array };

/// DBCellSrArea: for each use of `def` overlapping `area` (def
/// coordinates), each array element in DBArrayOverlap's range (y outer, x
/// inner), calls `visit` with the element and `area` in the element's child
/// coordinates. Returns false when a visit stopped the search.
bool search_elements(const db::CellDef& def, const db::Rect& area,
                     const std::function<Visit(const Element&, const db::Rect& child_area)>& visit);

/// DBArraySr: the elements of `use` whose images may overlap `area`
/// (parent coordinates), with their transforms to the parent.
void array_elements(const db::CellUse& use, const db::Rect& area, const std::function<void(const Element&)>& visit);

// --- The check context -----------------------------------------------------

/// The kernel the engine calls (drc::basic_check unless a test installed
/// another, see kernel_hook.hpp).
using KernelFn = CheckOutcome (*)(const CheckInput&, const db::Rect&, const db::Rect&, ErrorSink&);
[[nodiscard]] KernelFn kernel() noexcept;

/// The synthetic rules of Magic's engine-generated errors (drcArrayCookie,
/// drcOverlapCookie, drcSubcellCookie, drcInSubCookie, drcOffGridCookie).
[[nodiscard]] const Rule& engine_rule(WhyIndex why) noexcept;

/// The masks of drcExactOverlapTile for one tile type: the type with the
/// stacked types containing it, and the types that must not overlap it.
struct OverlapMasks {
  bool known = false;
  tech::TypeMask type;
  tech::TypeMask inverse;
};

/// Everything one check reads, plus the scratch yank cell and the error
/// type the current reports get (DRCErrorType). Built on the command thread
/// or on the worker under the shared database lock.
struct Context {
  Context(const editor::EditorState& editor, ErrorSink& sink, const CancelToken& cancel);
  ~Context();  // detaches own_stats from the sink
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  std::shared_ptr<const tech::Technology> technology;
  std::shared_ptr<const RuleSet> rules;
  std::int32_t halo = 0;  // DRCTechHalo (the current halo)
  std::int32_t step = 1;  // DRCStepSize
  CheckInput input;       // kernel input template (cell set per call)
  ErrorSink& sink;
  tech::TileType error_type = tech::kErrorS;  // DRCErrorType
  CheckStatistics own_stats;
  CheckStatistics* stats = &own_stats;  // the sink's counters when it has some
  db::CellDef yank;   // DRCdef: the flattening buffer, never in a library
  bool cancelled = false;
  /// Set while yank_paint() runs for an interaction check
  /// (DBCellCheckCopyAllPaint): an off-grid triangle fill reports why 5.
  bool flag_off_grid = false;
  /// exact_overlap_tile's type masks per tile type (array_overlap.cpp),
  /// computed on first use; empty until then.
  std::vector<OverlapMasks> overlap_masks;
  /// The mask-layer check's scratch planes (M5 design D14), shared by the
  /// basic checks of this context; created only when the style has
  /// mask-layer rules.
  std::unique_ptr<mask::GenerationContext> generation;

  [[nodiscard]] const tech::Technology& tech() const noexcept { return *technology; }
  /// Polls the cancel token; latches `cancelled`.
  bool cancel_requested();
  /// DRCBasicCheck(cell, K, L) through the kernel; false when cancelled.
  bool basic(const db::CellDef& cell, const db::Rect& check_area, const db::Rect& clip_area);
  /// Reports one engine-generated error (dCD_function with `rule`).
  void report(const db::Rect& area, const Rule& rule) { sink.report(area, rule); }
  /// DBCellClearDef on the yank cell (planes and properties).
  void clear_yank();
};

// --- Flattening (database/DBcellcopy.c with the DRC paint table) -----------

/// DBCellCopyAllPaint into the yank cell: the paint of `def` and of every
/// descendant (all array elements) overlapping `area` (`def` coordinates),
/// transformed by `to_yank` and clipped to its image, painted with the
/// style's DRC paint table (illegal overlaps become error_s tiles).
void yank_paint(Context& ctx, const db::CellDef& def, const db::Rect& area, const db::Transform& to_yank);

/// DBFlatCopyMaskHints: the mask hints of every descendant use of `def`
/// (not of `def` itself, as DBTreeSrCells visits children only) within
/// `area`, transformed into the yank cell.
void yank_hints(Context& ctx, const db::CellDef& def, const db::Rect& area, const db::Transform& to_yank);

// --- Hierarchical checks (drc/DRCsubcell.c, drc/DRCarray.c) ----------------

/// DRCFindInteractions: -1 no subcells near `area`, 0 no interaction,
/// 1 interaction (then `interaction` is set, clipped to `area`).
int find_interactions(Context& ctx, const db::CellDef& def, const db::Rect& area, std::int32_t radius,
                      db::Rect& interaction);

/// drcSubCopyFunc over DBCellSrArea: copies the error tiles of each child
/// element overlapping `strip` up as why-4 errors.
void copy_child_errors(Context& ctx, const db::CellDef& def, const db::Rect& strip);

/// drcArrayFunc over DBCellSrArea for the uses overlapping `search` with
/// the shared clip rectangle `clip` (dCD_clip), which Magic narrows in
/// place while it checks each array region (replicated: the caller's next
/// strips see the narrowed rectangle).
void array_checks(Context& ctx, const db::CellDef& def, const db::Rect& search, db::Rect& clip);

/// DBTreeSrUniqueTiles + drcExactOverlapTile over `area` of `def` against
/// the yank cell, reporting with `rule` (why 3) clipped to `area`.
void exact_overlap_check(Context& ctx, const db::CellDef& def, const db::Rect& area, const Rule& rule);

/// drcExactOverlapTile for one tile `tile` of `owner` (on `plane`), its
/// transform `to_yank`, the clip rectangle and the reporting rule.
void exact_overlap_tile(Context& ctx, const db::CellDef& owner, tech::PlaneId plane, const db::TileView& tile,
                        const db::Transform& to_yank, const db::Rect& clip_area, const Rule& rule);

/// DRCInteractionCheck(def, area, erasebox): see engine.hpp interaction_check.
void interaction_check(Context& ctx, const db::CellDef& def, const db::Rect& area, const db::Rect& erase_box);

}  // namespace hefesto::drc::detail
