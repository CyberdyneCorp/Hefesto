// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mask generation (capability mask-hierarchical-generation;
// openspec/changes/m5-mask-gds-cif/design.md D8):
// - generate_area: CIFGen, the layers of a style over an area of one cell's
//   own paint (no flattening); DRC's mask-layer check (D14) calls it on its
//   yank cell, the writers on each cell ("Own-paint generation area");
// - generate_flat: CIFGen over an area of a cell flattened into a scratch
//   cell ("Flat region generation"); serves `cif see`, `cif paint`,
//   `cif coverage` and the flat CIF writer;
// - generate_hierarchical: the per-cell output of the GDS and CIF writers
//   ("Per-cell output composition": own paint, CIFGenSubcells and
//   CIFGenArrays of CIFhier.c);
// - the `cif see|paint|coverage|statistics` back ends and the `cif *hier`
//   and `cif *array` previews;
// - interaction_areas: the interaction areas of a cell, found by the
//   interaction finder shared with hierarchical DRC (`drc-engine`
//   "Interaction area", Q-MASK-400).
// Generation never mutates the layout; it runs under the shared database
// lock (only the use index's bins change, under its own mutex; D7).
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GEN fills in
// the implementation in src/core/src/mask/gen/ (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/mask/gen/context.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::mask {

/// The generated planes of a style, one per layer in style order (nullptr
/// for a layer not generated). The planes come from a context's pool and go
/// back to it when the LayerPlanes is destroyed or reset, so the pool must
/// outlive it.
class LayerPlanes {
 public:
  LayerPlanes() = default;
  /// `count` entries, all nullptr, releasing into `pool`.
  LayerPlanes(PlanePool& pool, std::size_t count) : pool_(&pool), planes(count, nullptr) {}
  ~LayerPlanes() { reset(); }
  LayerPlanes(LayerPlanes&& other) noexcept;
  LayerPlanes& operator=(LayerPlanes&& other) noexcept;
  LayerPlanes(const LayerPlanes&) = delete;
  LayerPlanes& operator=(const LayerPlanes&) = delete;

 private:
  PlanePool* pool_ = nullptr;

 public:
  std::vector<MaskPlane*> planes;

  [[nodiscard]] bool empty() const noexcept { return planes.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return planes.size(); }
  [[nodiscard]] MaskPlane* operator[](std::size_t layer) const noexcept {
    return layer < planes.size() ? planes[layer] : nullptr;
  }
  /// Gives every plane back to the pool and empties the list.
  void reset() noexcept;
};

/// DRCFindInteractions, the interaction finder shared with hierarchical
/// DRC (`drc-engine` "Interaction area"): the interaction area of the
/// square `area` of `cell` for `radius`, or nullopt.
using InteractionFinder =
    std::function<std::optional<db::Rect>(const db::CellDef& cell, const db::Rect& area, std::int64_t radius)>;

/// What generation reads besides the style and the layout: the technology
/// (the planes searched and the paint tables of the flattening), the
/// session's compat settings and the interaction finder (without one,
/// hierarchical generation finds no subcell interaction).
struct GenerationSource {
  const tech::Technology* technology = nullptr;  // required
  compat::CompatSettings compat;
  InteractionFinder find_interaction;
};

/// The editor's technology and compat settings (the DRC session's pointer
/// to the project's settings, as the dispatcher sets it), and
/// drc::interaction_area over the editor as the finder (the editor must
/// outlive the source).
[[nodiscard]] GenerationSource generation_source(const editor::EditorState& editor);

/// CIFGen's options.
struct GenerateOptions {
  /// The layers to generate (`layers` mask); empty = every layer.
  std::vector<bool> layers;
  /// genAllPlanes: an empty plane for a layer not generated (else nullptr).
  bool all_planes = true;
  /// CIFGen's `hier`: called from hierarchical processing, so the operator
  /// lists stop at the hierarchy-stopping operators.
  bool hierarchical = false;
  /// Report generation errors (CIFErrorDef set); false suppresses them.
  bool report_errors = true;
  /// `cif unfracture` (CIFUnfracture; Q-MASK-413).
  bool unfracture = false;
};

/// The halo radius generation uses, in internal units ("Halo radius"):
/// with magic-compat ON the style's (Magic's cs_radius, kept stale by a
/// non-reducing rescale, Q-MASK-18); with it OFF recomputed with the
/// bloat-max and bloat-min distances and the bridge and bridge-lim widths
/// counted (Q-MASK-404).
[[nodiscard]] std::int64_t generation_radius(const OutputStyle& style, const compat::CompatSettings& compat);

/// CIFGen over `area` (internal units) of `layout`'s own paint: every
/// requested layer in style order over `area` grown by the radius (each
/// layout tile read whole), each result then clipped to `area` times the
/// scale factor. `origin` is the cell output is generated for (origDef:
/// bbox, boundary, net, and the cell generation errors name). Errors go to
/// ctx.sink.
[[nodiscard]] LayerPlanes generate_area(const GenerationSource& source, const db::CellDef& layout,
                                        const db::CellDef* origin, const db::Rect& area, const OutputStyle& style,
                                        GenerationContext& ctx, const GenerateOptions& options = {});

/// CIFGen with `replace` false: the layers generated as generate_area does,
/// OR'ed into `into` (an entry that is nullptr takes the new plane).
void generate_area_into(const GenerationSource& source, const db::CellDef& layout, const db::CellDef* origin,
                        const db::Rect& area, const OutputStyle& style, GenerationContext& ctx,
                        const GenerateOptions& options, LayerPlanes& into);

/// "Flat region generation": every paint type but space and the DRC types
/// of `cell`'s whole subtree within `area` grown by the radius flattened
/// into a scratch cell (whole tiles, paint composition, mask hints), then
/// CIFGen over `area`. Errors go to ctx.sink and name `cell`.
[[nodiscard]] LayerPlanes generate_flat(const GenerationSource& source, const db::CellDef& cell,
                                        const db::Rect& area, const OutputStyle& style, GenerationContext& ctx,
                                        const GenerateOptions& options = {});
/// The same with the editor's technology and compat settings, every layer.
[[nodiscard]] LayerPlanes generate_flat(editor::EditorState& editor, const db::CellDef& cell, const db::Rect& area,
                                        const OutputStyle& style, GenerationContext& ctx);

/// The options of the hierarchical generation of one cell.
struct HierarchyOptions {
  /// Subcell interaction output (`cif *hier write`, CIFHierWriteDisable
  /// inverted; a `CIFhier` property is the writer's to apply).
  bool subcells = true;
  /// Array interaction output (`cif *array write`).
  bool arrays = true;
  /// Enlarge the generation area by a `FIXED_BBOX` property (GDS only).
  bool fixed_bbox = false;
  /// `cif unfracture`.
  bool unfracture = false;
};

/// "Hierarchy and array output switches": the options for generating
/// `cell` when `inherited` is in effect (the session's `cif *hier` and
/// `cif *array` switches, or what a `CIFhier` ancestor forced). A cell with
/// a `CIFhier` property (any value) forces both switches on, for itself and
/// for the descendants the writer first writes while processing its
/// subcells (the writer passes the result down as their `inherited`). The
/// GDS writer (`gds` true) always honours the property; the CIF writer only
/// with magic-compat OFF (Q-MASK-410).
[[nodiscard]] HierarchyOptions hierarchy_options(const db::CellDef& cell, const HierarchyOptions& inherited, bool gds,
                                                 const compat::CompatSettings& compat);

/// "Per-cell output composition" for `cell`: the own-paint layers over the
/// cell's bounding box grown by the radius (and its FIXED_BBOX with
/// options.fixed_bbox), then the subcell (CIFGenSubcells) and array
/// (CIFGenArrays) corrections OR'ed in. Every layer, templayers included,
/// is returned; the writers skip the templayers. Errors go to ctx.sink.
/// A cancelled progress stops between interaction squares; the result is
/// then incomplete and the caller discards it.
[[nodiscard]] LayerPlanes generate_hierarchical(const GenerationSource& source, const db::CellDef& cell,
                                                const OutputStyle& style, GenerationContext& ctx,
                                                const HierarchyOptions& options);
/// The same with the editor's technology, compat settings and `cif *hier`,
/// `*array` and `unfracture` switches, without FIXED_BBOX (CIF).
[[nodiscard]] LayerPlanes generate_hierarchical(editor::EditorState& editor, const db::CellDef& cell,
                                                const OutputStyle& style, GenerationContext& ctx);

/// The generation area of a cell (own paint, Hier and Array): its bounding
/// box grown by the radius, enlarged to a four-value FIXED_BBOX property
/// with `fixed_bbox` ("Own-paint generation area").
[[nodiscard]] db::Rect generation_area(const db::CellDef& cell, std::int64_t radius, bool fixed_bbox);

/// CIFGenSubcells: the subcell-interaction correction of `cell` over
/// `area` OR'ed into `into` (Hier_L). Errors go to ctx.sink.
void generate_subcells(const GenerationSource& source, const db::CellDef& cell, const db::Rect& area,
                       const OutputStyle& style, GenerationContext& ctx, LayerPlanes& into);
/// CIFGenArrays: the array-interaction correction of `cell` over `area`
/// OR'ed into `into` (Array_L).
void generate_arrays(const GenerationSource& source, const db::CellDef& cell, const db::Rect& area,
                     const OutputStyle& style, GenerationContext& ctx, LayerPlanes& into);

/// "Flat CIF output": `cell`'s whole hierarchy within its bounding box
/// grown by the radius flattened into `scratch` (every type but space and
/// the DRC types; with magic-compat OFF also the labels `flatten` copies
/// and the mask hints, Q-MASK-411), and its bounding box recomputed. The
/// caller names the scratch cell (`__CIF2__` ON, the root's name OFF,
/// Q-MASK-321) and writes it with own-paint generation only.
void flatten_for_output(const GenerationSource& source, const db::CellDef& cell, const OutputStyle& style,
                        db::CellDef& scratch);

/// The layers named by `names` (CIFNameToMask; a comma-separated list, each
/// name selecting every layer of that name) and their transitive
/// dependencies (the earlier layers they read, and the mask layers of a
/// bloat-all). Fails with Magic's message listing the valid names.
struct LayerSelection {
  std::vector<bool> layers;
  std::vector<bool> depend;
};
[[nodiscard]] diag::Outcome<LayerSelection> select_layers(const OutputStyle& style, std::string_view names);

/// `cif see layer`: the tiles of `layer` generated flat under `area` of
/// `cell`, in mask units (split tiles by their bounding rectangle).
[[nodiscard]] diag::Outcome<std::vector<db::Rect>> see(editor::EditorState& editor, const db::CellDef& cell,
                                                       std::string_view layer, const db::Rect& area,
                                                       GenerationContext& ctx);
/// The same with the split-tile geometry (the feedback keeps it).
[[nodiscard]] diag::Outcome<std::vector<db::TileView>> see_tiles(const GenerationSource& source,
                                                                 const OutputStyle& style, const db::CellDef& cell,
                                                                 std::string_view layer, const db::Rect& area,
                                                                 GenerationContext& ctx);

/// `cif *hier layer` and `cif *array layer` (CIFSeeHierLayer): only the
/// subcell (`subcells`) and/or array (`arrays`) contribution of `layer` for
/// `cell` over `area`, in mask units.
[[nodiscard]] diag::Outcome<std::vector<db::TileView>> see_hierarchical(const GenerationSource& source,
                                                                        const OutputStyle& style,
                                                                        const db::CellDef& cell,
                                                                        std::string_view layer, const db::Rect& area,
                                                                        bool arrays, bool subcells,
                                                                        GenerationContext& ctx);

/// `cif paint ciflayer magiclayer [cellname]`: paints the generated layer
/// into `target` as `type` (each tile divided by the scale factor,
/// truncating; degenerate results skipped), then recomputes `target`'s
/// bounding box.
[[nodiscard]] diag::Status paint(editor::EditorState& editor, const db::CellDef& cell, std::string_view layer,
                                 const db::Rect& area, tech::TileType type, db::CellDef& target,
                                 GenerationContext& ctx);

/// `cif coverage layer`.
struct Coverage {
  /// Area of the layer, in CIF units squared: a triangle counts as its
  /// rectangle with magic-compat ON, as half of it with it OFF (Q-MASK-325).
  std::int64_t covered = 0;
  /// The area searched (the box or the cell's bounding box) in CIF units
  /// squared.
  std::int64_t total = 0;
  /// The bounding box of the layer's tiles (mask units); (0,0)-(0,0) when
  /// there are none.
  db::Rect bounds{0, 0, 0, 0};

  /// Magic's `fcover`: (float)covered / (float)total, 0 when total is 0.
  [[nodiscard]] float fraction() const noexcept;
  /// The layer bounding area (`atotal`).
  [[nodiscard]] std::int64_t bounds_area() const noexcept { return bounds.width() * bounds.height(); }
};
[[nodiscard]] diag::Outcome<Coverage> coverage(editor::EditorState& editor, const db::CellDef& cell,
                                               std::string_view layer, const db::Rect& area, GenerationContext& ctx);
[[nodiscard]] diag::Outcome<Coverage> coverage(const GenerationSource& source, const OutputStyle& style,
                                               const db::CellDef& cell, std::string_view layer, const db::Rect& area,
                                               GenerationContext& ctx);
/// Magic's coverage report: `Cell|Cursor Box Area = …`, `Layer Bounding
/// Area = …`, `Layer Total Area = …` and `Coverage in cell|box = …%`
/// lines (`box` selects the cursor-box wording).
[[nodiscard]] std::string coverage_report(const Coverage& coverage, bool box);

/// The recent and cumulative statistics counters of a session (Magic's
/// CIFTileOps, cifTotalTileOps, ...).
struct StatisticsCounters {
  GenerationStatistics recent;
  GenerationStatistics total;

  /// Adds a generation's counters to the recent values.
  void add(const GenerationStatistics& stats) noexcept;
};

/// `cif statistics` (CIFPrintStats): the four `recent/total` lines of
/// `counters` after adding the recent values to the totals; then the
/// recent values are reset.
[[nodiscard]] std::string statistics(StatisticsCounters& counters);
/// The four lines for `stats` as both the recent and the total values.
[[nodiscard]] std::string statistics(const GenerationStatistics& stats);

/// The interaction areas of `cell` for halo `halo` (internal units) over
/// `area`: the interaction search grid of hierarchical generation
/// ("Interaction search grid", with `step` 0 meaning max(20·halo, 50)),
/// each square's interaction area as the finder shared with hierarchical
/// DRC computes it (Q-MASK-400's (0,0) seed under magic-compat ON), in
/// visiting order.
[[nodiscard]] diag::Outcome<std::vector<db::Rect>> interaction_areas(const editor::EditorState& editor,
                                                                     const db::CellDef& cell, const db::Rect& area,
                                                                     std::int64_t halo, std::int32_t step);

}  // namespace hefesto::mask
