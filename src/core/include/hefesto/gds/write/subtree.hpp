// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Subtree loading of the writers' prepare phase (capabilities gds-write
// "Write preconditions and subtree loading", "Structure ordering";
// cif-format-io, its CIF counterpart; openspec/changes/m5-mask-gds-cif/
// design.md D4, D7). Both writers start as Magic's CalmaWrite.c:316-327 and
// CIFwrite.c:137-145 do:
//
//   DBCellReadArea(root, root's bounding box, halt_on_error);
//   DBFixMismatch();
//
// load_subtree is that sequence: dbReadAreaFunc reads every unavailable
// cell it reaches, then makes the BPE_OVERLAP search of the cell's use
// index (UseIndex::search, which can rebuild the bins) over the area in its
// coordinates and recurses into each use found, array element by element
// (DBArrayOverlap's range, y outer, x inner), skipping the remaining
// elements of an array once the area surrounds the child's bounding box
// (database/DBexpand.c:296-340, DBcellsrch.c:1131-1222). A repeated visit
// that cannot change anything (same cell and area, nothing read or rebuilt
// since a visit that itself read and rebuilt nothing) is skipped, so the
// walk is not exponential in the depth (subtree.cpp). With
// halt_on_error a cell that cannot be read stops the walk and is returned
// (Magic's "Failure to read entire subtree"); without it the walk goes on
// (`gds undefined allow`). Then DBFixMismatch re-places the uses of each
// child whose bounding box changed (DBReComputeBbox) before anything is
// enumerated. It runs under the exclusive database lock; the GDS writer
// calls it from gds::prepare_write, the CIF writer from cif::prepare_write.
//
// Owner: GDSW (design D2); shared with CIF (task 11.3).

#pragma once

#include <atomic>
#include <cstddef>
#include <functional>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"

namespace hefesto::cmd {
class CommandContext;
}
namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::gds {

/// How the walk reads and fixes up cells.
struct SubtreeLoader {
  /// DBCellRead(def, ignoreTech TRUE, dereference TRUE): reads an
  /// unavailable cell; false when it cannot be read. Empty: an unavailable
  /// cell cannot be read.
  std::function<bool(db::CellDef&)> read;
  /// DBFixMismatch: processes the pending timestamp mismatches. Empty:
  /// nothing to process.
  std::function<void()> fix_mismatches;
  /// Set by the host to stop the walk (mask::Progress::cancel); nullptr:
  /// never cancelled. Polled at every cell visit.
  const std::atomic<bool>* cancel = nullptr;
};

/// The walk is recursive, as Magic's; a hierarchy deeper than this fails
/// the walk instead of exhausting the stack (hardening; Magic has no
/// limit). The same bound as the mask generators' (MASK-GEN-004), so a
/// write never reaches a depth its generation refuses. The writers'
/// recursion at this depth needs more stack than a secondary thread's
/// default: a host that runs a write off the command thread uses
/// platform::run_on_stack with platform::kDeepRecursionStackBytes.
inline constexpr std::size_t kMaxSubtreeDepth = 1024;

/// What load_subtree found.
struct SubtreeResult {
  /// The cell that could not be read (`halt_on_error`), or the cell at
  /// which the hierarchy exceeded kMaxSubtreeDepth; nullptr on success.
  db::CellDef* failed = nullptr;
  bool too_deep = false;
  /// The host cancelled the walk (SubtreeLoader::cancel).
  bool cancelled = false;

  [[nodiscard]] bool ok() const noexcept { return failed == nullptr && !cancelled; }
};

/// DBCellReadArea(root, root.bbox(), halt_on_error) then DBFixMismatch.
/// The searches bin the indexes alike in both modes; with magic-compat OFF
/// in `compat` each search visits its matches in the canonical order
/// (UseIndex::search(area, compat, visit)). On failure nothing is fixed up.
SubtreeResult load_subtree(db::CellDef& root, const SubtreeLoader& loader, bool halt_on_error,
                           const compat::CompatSettings& compat);

/// The loader of a command: DBCellRead through the cell area's reader
/// (technology ignored, the dereference flag followed, as subcell expansion
/// reads) and its deferred mismatch processing, which prints Magic's
/// "Processing timestamp mismatches." lines to the command's sink and
/// queues the DRC rechecks. Bound to `ctx` and `editor`, which must outlive
/// the loader.
[[nodiscard]] SubtreeLoader command_subtree_loader(cmd::CommandContext& ctx, editor::EditorState& editor);

}  // namespace hefesto::gds
