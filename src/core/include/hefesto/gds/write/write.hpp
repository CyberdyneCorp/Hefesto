// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDS write (capability gds-write; openspec/changes/m5-mask-gds-cif/design.md
// D4 "Commands and progress", D7, D12). Magic's write mutates the database
// before it generates anything, so a write has two phases:
// - prepare_write, on the command thread under the exclusive database lock:
//   checks the style (Q-MASK-219) before any file is opened, loads the
//   subtree with Magic's recursive area search through UseIndex::search
//   (which can rebuild bins), resolves timestamp mismatches with the
//   DBReComputeBbox re-placement, and returns the checked options and root;
// - emit_write, under the shared lock: generates (mask::generate_hierarchical)
//   and writes. It mutates nothing in the layout (Q-MASK-208); the only
//   state it changes is the use index's bins, under the index mutex.
// write() calls both in sequence; the shell runs emit_write on a worker
// (D16). A cancelled write leaves no output file: the writer writes to a
// temporary file in the target directory and renames it on success.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GDSW fills
// in the implementation in src/core/src/gds/write/ (design D2).

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/gds/command/settings.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::gds {

/// The options of one write: the session's `gds` settings at the time of
/// the call, and the name the user typed (for the extension rule, Q-MASK-210).
struct GdsWriteOptions {
  GdsSettings settings;
  std::string typed_name;
};

/// The result of the prepare phase: everything emit_write needs.
struct PreparedWrite {
  editor::EditorState* editor = nullptr;
  db::CellDef* root = nullptr;
  GdsWriteOptions options;
  std::shared_ptr<const mask::OutputStyle> style;  // the current output style, kept for the emit phase
};

/// The prepare phase (exclusive lock; see the file comment).
[[nodiscard]] diag::Outcome<PreparedWrite> prepare_write(editor::EditorState& editor, db::CellDef& root,
                                                         const GdsWriteOptions& options);

/// The emit phase (shared lock; see the file comment).
[[nodiscard]] cmd::CommandResult emit_write(const PreparedWrite& prepared, const platform::Path& path,
                                            const mask::Progress* progress = nullptr);

/// prepare_write then emit_write.
[[nodiscard]] cmd::CommandResult write(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                                       const GdsWriteOptions& options, const mask::Progress* progress = nullptr);

/// The merged polygons of a mask plane (`gds merge yes`; Magic's merge and
/// collinear passes, slits included, Q-MASK-217), in output order.
[[nodiscard]] std::vector<mask::Polygon> merge_polygons(const mask::MaskPlane& plane);

}  // namespace hefesto::gds
