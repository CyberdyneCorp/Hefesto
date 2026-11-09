// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Label and port operations on a cell (capability layout-labels-ports):
// creation with automatic justification, layer choice, adjustment after
// paint changes, erasing, stacked-type mapping and port renumbering.
// (Magic ref: database/DBlabel.c, commands/CmdLQ.c PORT_RENUMBER,
// database/DBcellname.c strcmpbynum)
//
// Owner: MODEL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <optional>
#include <string_view>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::db {

/// "Automatic justification": the position that keeps the text of a label
/// at `rect` inside `cell_bbox` (DBPutFontLabel with align < 0).
[[nodiscard]] Position auto_justification(const Rect& cell_bbox, const Rect& rect) noexcept;

/// DBPutFontLabel: appends `label` to `def` (the `.mag` order), choosing
/// the justification with auto_justification() when `auto_justify`, and
/// marks the cell modified. Empty text is refused (DB-LABEL-001).
[[nodiscard]] diag::Outcome<Label*> put_label(CellDef& def, Label label, bool auto_justify = false);

/// "Choosing a label's layer" (DBPickLabelLayer with doCalma = FALSE).
[[nodiscard]] tech::TileType choose_label_layer(const CellDef& def, const tech::Technology& tech,
                                                const Label& label);

/// "Label adjustment after paint changes" (DBAdjustLabels) for the labels
/// touching `area`. Marks the cell modified and returns true if a label
/// changed. Messages (verbose mode) go to `messages` when given.
bool adjust_labels(CellDef& def, const tech::Technology& tech, const Rect& area,
                   std::vector<std::string>* messages = nullptr);

/// "Erasing labels together with paint" (DBEraseGlobLabel). `types` may
/// contain kLabelsPseudoType to erase regardless of paint. Returns the
/// number of labels deleted (the cell is marked modified if > 0).
inline constexpr tech::TileType kLabelsPseudoType = tech::kMaxTileTypes - 2;  // L_LABEL
std::size_t erase_labels(CellDef& def, const tech::Technology& tech, const Rect& area, const tech::TypeMask& types,
                         std::optional<std::string_view> glob = std::nullopt);

/// Tcl-style glob match (`*`, `?`, `[...]`, `\`), as Magic's Match().
[[nodiscard]] bool glob_match(std::string_view pattern, std::string_view text) noexcept;

/// "Labels on contacts and stacked contacts": a generated stacked type maps
/// to its highest-numbered component contact; any other type is unchanged.
[[nodiscard]] tech::TileType label_layer_for(const tech::Technology& tech, tech::TileType type) noexcept;

/// The total natural order of cell-file-format "Natural ordering of names"
/// (Q-MODEL-9, Q-CELL-12): negative, zero or positive.
[[nodiscard]] int natural_compare(std::string_view a, std::string_view b) noexcept;

/// "Port renumbering". Ties in the natural order keep creation order.
/// `magic_compat` false shares indices case-insensitively (Q-MODEL-10).
void renumber_ports(CellDef& def, bool magic_compat = true);

}  // namespace hefesto::db
