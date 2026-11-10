// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::check_this: Magic's DRCCheckThis, the one call every marking site
// makes after it changes paint or subcells of a cell (drc-engine "Marking
// changed areas"; openspec/changes/m3-drc/design.md D4/D7).
//
// With an area: paints `checkpaint` over the area expanded by the current
// halo into the cell's check plane, appends the cell to the pending list,
// and recursively marks every parent through every non-window use (array
// uses over the bounding box of all element images); internal cells (the
// DRC yank cell, the selection cell, ...) are never marked. Without an
// area: only enqueues the cell. Either way the edit generation is bumped
// (background.hpp) and the worker is woken at the end of the command.
//
// Call it with Magic's area at each site; the marking-site checklist of
// docs/diagnostics/codes/drc.md (ENGINE section) lists them.
//
// Scaffold state: a no-op; ENGINE (tasks.md 6.2) implements it.
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <optional>

#include "hefesto/db/geometry.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::tech {
class Technology;
}

namespace hefesto::drc {

/// DRCCheckThis(cell, TT_CHECKPAINT, area): `area` in `cell` coordinates;
/// std::nullopt only enqueues the cell.
void check_this(editor::EditorState& editor, db::CellDef& cell, std::optional<db::Rect> area);

/// Paints `checkpaint` over `area` of `cell`'s check plane only: no halo,
/// no pending-list entry, no propagation (`select save`: SEL_SAVE in
/// commands/CmdRS.c paints the selection's bounding box before saving it).
void paint_check_area(const tech::Technology& tech, db::CellDef& cell, const db::Rect& area);

}  // namespace hefesto::drc
