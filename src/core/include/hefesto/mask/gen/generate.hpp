// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mask generation (capability mask-hierarchical-generation;
// openspec/changes/m5-mask-gds-cif/design.md D8):
// - generate_flat: CIFGen/CIFsee over an area of a flattened cell; serves
//   `cif see`, `cif paint`, `cif coverage`, DRC's mask-layer check (D14)
//   and the flat CIF writer;
// - generate_hierarchical: CIFGenSubcells and CIFGenArrays (CIFhier.c) for
//   the GDS and CIF writers;
// - the `cif see|paint|coverage|statistics` back ends;
// - interaction_areas: the interaction-area finder shared with hierarchical
//   DRC (`drc-engine` "Interaction area", Q-MASK-400).
// Generation never mutates the layout; it runs under the shared database
// lock (only the use index's bins change, under its own mutex; D7).
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GEN fills in
// the implementation in src/core/src/mask/gen/ (design D2).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/mask/gen/context.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "hefesto/mask/style/style.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::mask {

/// The generated planes of a style, one per layer in style order (nullptr
/// for a layer not generated). The planes belong to the context's pool.
struct LayerPlanes {
  std::vector<MaskPlane*> planes;

  [[nodiscard]] bool empty() const noexcept { return planes.empty(); }
};

/// CIFGen over `area` (internal units) of `cell` flattened: every layer of
/// `style` in order. Errors go to ctx.sink.
[[nodiscard]] LayerPlanes generate_flat(editor::EditorState& editor, const db::CellDef& cell, const db::Rect& area,
                                        const OutputStyle& style, GenerationContext& ctx);

/// CIFGenSubcells and CIFGenArrays for `cell`: the planes to output for the
/// cell itself once its subcells' and arrays' contributions are accounted
/// for. Errors go to ctx.sink.
[[nodiscard]] LayerPlanes generate_hierarchical(editor::EditorState& editor, const db::CellDef& cell,
                                                const OutputStyle& style, GenerationContext& ctx);

/// `cif see layer`: the tiles of `layer` under `area` of `cell`.
[[nodiscard]] diag::Outcome<std::vector<db::Rect>> see(editor::EditorState& editor, const db::CellDef& cell,
                                                       std::string_view layer, const db::Rect& area,
                                                       GenerationContext& ctx);

/// `cif paint ciflayer magiclayer [cellname]`: paints the generated layer
/// into `target` as `type`.
[[nodiscard]] diag::Status paint(editor::EditorState& editor, const db::CellDef& cell, std::string_view layer,
                                 const db::Rect& area, tech::TileType type, db::CellDef& target,
                                 GenerationContext& ctx);

/// `cif coverage layer`: the generated area and the area searched.
struct Coverage {
  std::int64_t covered = 0;  // area of the layer, in CIF units squared (Q-MASK-325)
  std::int64_t total = 0;    // area searched
};
[[nodiscard]] diag::Outcome<Coverage> coverage(editor::EditorState& editor, const db::CellDef& cell,
                                               std::string_view layer, const db::Rect& area, GenerationContext& ctx);

/// `cif statistics`: Magic's report text for `stats`.
[[nodiscard]] std::string statistics(const GenerationStatistics& stats);

/// The interaction areas of `cell` for halo `halo` (internal units), shared
/// with hierarchical DRC; Q-MASK-400's (0,0) seed under magic-compat ON.
[[nodiscard]] diag::Outcome<std::vector<db::Rect>> interaction_areas(const db::CellDef& cell, std::int64_t halo,
                                                                     const compat::CompatSettings& compat);

}  // namespace hefesto::mask
