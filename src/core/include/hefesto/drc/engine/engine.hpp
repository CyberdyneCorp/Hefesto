// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The incremental and hierarchical DRC engine (drc-engine;
// openspec/changes/m3-drc/design.md D7/D8): step squares, interaction
// checks, flattening, copied child errors, array and exact-overlap checks,
// the forced recheck, the flat check, catch-up and the error-plane paint
// table. COMMANDS and the shell call these; the background worker
// (background.hpp) runs process_square() on its own and the host commits.
//
// Thread rules (design D8): process_square(), interaction_check(),
// interaction_area() and flat_check() only read the database (each builds
// its scratch flattening cell privately) and may run on the worker while
// editor::EditorState::database_lock is held shared; commit(), catchup(),
// force_recheck() and every other function that changes a plane run on the
// command thread (inside a command or between commands).
//
// Not yet ported (open items): the off-grid error of a non-Manhattan
// overlap (why 5) found while flattening, which needs db::Plane to report
// DBNMPaintPlane0's 1x1 fill; split tiles are flattened with
// db::Plane::paint_triangle, which merges triangles where Magic's
// DBPaintPlaneMark does not.
//
// (Magic ref: drc/DRCcontin.c DRCContinuous, drcCheckTile, drcPutBackFunc;
// drc/DRCsubcell.c DRCInteractionCheck, DRCFindInteractions, DRCFlatCheck;
// drc/DRCarray.c DRCArrayCheck; drc/DRCmain.c DRCCheck, DRCCatchUp,
// drcPaintError)
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/check/basic_check.hpp"
#include "hefesto/drc/engine/background.hpp"
#include "hefesto/drc/types.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::drc {

/// One error tile of a square's result: `error_p` (plain checks),
/// `error_s` (interaction areas, illegal overlaps) or `error_ps` (both
/// painted into the same area of the temporary plane).
struct ErrorTile {
  db::Rect area;
  tech::TileType type = tech::kErrorP;

  friend bool operator==(const ErrorTile&, const ErrorTile&) = default;
};

/// The checked, not yet committed result of one step square
/// (drc-engine "Step square processing"), or the note that the first
/// pending cell has no check tiles left (`cell_done`).
struct SquareResult {
  db::CellDef* cell = nullptr;
  bool cell_done = false;         // no check tile: commit removes the cell from the list
  tech::TileType check_type = tech::kCheckPaint;  // type of the check tile found
  db::Rect square;                // S
  db::Rect erase_box;             // B: checkpaint and error_p are erased here
  db::Rect check_box;             // B+: error_s is erased here
  std::vector<ErrorTile> errors;  // the temporary error plane's tiles
  std::uint64_t generation = 0;   // edit generation when the square started
  std::uint64_t epoch = 0;        // commit epoch when the square started (BackgroundChecker)
  CheckStatistics statistics;
};

/// The halo in effect: the style halo, or the smaller `*halo` override
/// (drc-engine "Halo override"; never above the style halo).
[[nodiscard]] std::int32_t current_halo(const editor::EditorState& editor);

/// The step size in effect: the `*stepsize` value or the style's.
[[nodiscard]] std::int32_t current_step_size(const editor::EditorState& editor);

/// Checks the next square of the first pending cell (reads only).
/// std::nullopt when the list is empty or `cancel` stopped the square.
[[nodiscard]] std::optional<SquareResult> process_square(const editor::EditorState& editor,
                                                         const CancelToken& cancel = {});

/// Applies a finished square: erases the check type and error_p in B and
/// error_s in B+, paints the errors, marks the cell modified unless it is
/// non-editable or vendor GDS (its timestamp untouched), publishes the
/// square through DrcSession::events. A `cell_done` result recomputes the
/// cell's bounding box and removes it from the pending list. Never
/// recorded for undo.
void commit(editor::EditorState& editor, const SquareResult& result);

/// `drc catchup` (DRCCatchUp): runs process_square()/commit()
/// synchronously until the pending list is empty, regardless of the
/// background state, publishing busy/idle when there was work. After each
/// checked square it runs drc::after_basic_check (drcCifCheck's style
/// bookkeeping), whose message goes to `messages` when given.
[[nodiscard]] diag::Status catchup(editor::EditorState& editor, diag::DiagnosticSink* messages = nullptr);

/// Reads an unavailable cell (DBCellRead); false when it cannot be read.
using CellReadFn = std::function<bool(db::CellDef&)>;

/// `drc check` (DRCCheck): reads the hierarchy under `area` (root
/// coordinates) with `read` (unavailable cells are skipped without one),
/// then marks `area` of `root` and of every cell under it, children before
/// parents, without upward propagation (drc-engine "Forced recheck of an
/// area"). When a cell cannot be read it fails with DRC-ENG-001 ("Failure
/// to read in entire subtree of cell.", then "Failed on cell <name>.") and
/// marks nothing.
[[nodiscard]] diag::Status force_recheck(editor::EditorState& editor, db::CellDef& root, const db::Rect& area,
                                         const CellReadFn& read = {});

/// `drc *flatcheck` (DRCFlatCheck): the number of errors of the flat check
/// of `area` under `root` (drc-engine "Flat check"); no error plane changes.
/// The caller prints `<n> total errors found.`.
[[nodiscard]] diag::Outcome<std::size_t> flat_check(const editor::EditorState& editor, const db::CellDef& root,
                                                     const db::Rect& area);

/// DRCFindInteractions: the interaction area of `area` in `cell` for
/// `radius`; std::nullopt when there is none (drc-engine "Interaction area").
[[nodiscard]] diag::Outcome<std::optional<db::Rect>> interaction_area(const editor::EditorState& editor,
                                                                      const db::CellDef& cell, const db::Rect& area,
                                                                      std::int32_t radius);

/// DRCInteractionCheck: every error the step-square processing of `cell`
/// over `area` with erase box `erase_box` finds (plain, copied child,
/// array, interaction and exact-overlap errors), reported to `sink` in
/// Magic's order (`drc why`, `drc find`, `drc count` re-checks). Errors are
/// clipped as for the square; the sink's statistics pointer is honoured.
CheckOutcome interaction_check(const editor::EditorState& editor, const db::CellDef& cell, const db::Rect& area,
                               const db::Rect& erase_box, ErrorSink& sink, const CancelToken& cancel = {});

/// The error-plane paint table: painting `paint` (error_p, error_s or
/// error_ps) over each type of the error plane (P and S compose into PS;
/// drc-engine "DRC check and error planes").
[[nodiscard]] const db::PaintRow& error_paint_row(tech::TileType paint) noexcept;

/// The error-plane erase table: erasing `erase` from each type (erasing P
/// from PS leaves S and vice versa).
[[nodiscard]] const db::PaintRow& error_erase_row(tech::TileType erase) noexcept;

/// The background state around one command (txCommands.c
/// TxDispatch): `begin_command` sets "not set" unless off and returns the
/// state to restore; `end_command` restores it unless the command changed
/// the state from "not set" (Magic's `drc on`/`drc off` inside a command).
[[nodiscard]] BackgroundMode begin_command(editor::EditorState& editor);
void end_command(editor::EditorState& editor, BackgroundMode saved);

}  // namespace hefesto::drc
