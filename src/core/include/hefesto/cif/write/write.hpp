// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CIF write (capability cif-format-io; openspec/changes/m5-mask-gds-cif
// design.md D4 "Commands and progress", D7, D13): split in two phases as the
// GDS writer (gds/write/write.hpp):
// - prepare_write, on the command thread under the exclusive database lock
//   (style check, subtree loading through UseIndex::search, mismatch
//   fix-up);
// - emit_write, under the shared lock: symbol numbering and `C` call order
//   from the use index at Magic's two sampling points (Q-MASK-323), the
//   preamble, DS/9/L/B/P lines, labels, calls and arrays, and the flat
//   `__CIF2__` form for `cif flat`.
// write() calls both in sequence. A cancelled write leaves no output file.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); CIF fills in
// the implementation in src/core/src/cif/write/ (design D2).

#pragma once

#include <memory>
#include <string>

#include "hefesto/cif/command/settings.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::cif {

/// The options of one write: the session's `cif` settings at the time of
/// the call, whether it is `cif flat`, and the name the user typed (for the
/// `.cif` suffix rule, Q-MASK-320).
struct CifWriteOptions {
  CifSettings settings;
  bool flat = false;
  std::string typed_name;
};

/// The result of the prepare phase: everything emit_write needs.
struct PreparedWrite {
  editor::EditorState* editor = nullptr;
  db::CellDef* root = nullptr;
  CifWriteOptions options;
  std::shared_ptr<const mask::OutputStyle> style;  // the current output style, kept for the emit phase
};

/// The prepare phase (exclusive lock).
[[nodiscard]] diag::Outcome<PreparedWrite> prepare_write(editor::EditorState& editor, db::CellDef& root,
                                                         const CifWriteOptions& options);

/// The emit phase (shared lock).
[[nodiscard]] cmd::CommandResult emit_write(const PreparedWrite& prepared, const platform::Path& path,
                                            const mask::Progress* progress = nullptr);

/// prepare_write then emit_write.
[[nodiscard]] cmd::CommandResult write(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                                       const CifWriteOptions& options, const mask::Progress* progress = nullptr);

}  // namespace hefesto::cif
