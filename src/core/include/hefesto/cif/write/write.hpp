// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CIF write (capability cif-format-io; openspec/changes/m5-mask-gds-cif
// design.md D4 "Commands and progress", D7, D13): split in two phases as the
// GDS writer (gds/write/write.hpp):
// - prepare_write, on the command thread under the exclusive database lock
//   (style check, subtree loading through UseIndex::search with the GDS
//   writer's shared helper gds::load_subtree, mismatch fix-up);
// - emit_write, under the shared lock: symbol numbering and `C` call order
//   from the use index at Magic's two sampling points (Q-MASK-323), the
//   preamble, DS/9/L/B/P lines, labels, calls and arrays, and the flat
//   `__CIF2__` form for `cif flat`.
// write() calls both in sequence. A cancelled write leaves no output file.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); CIF fills in
// the implementation in src/core/src/cif/write/ (design D2). Round 4 (CIF)
// added CifWriteOptions::{loader, files, user, machine, time_zone, now},
// PreparedWrite::compat and output_file_name.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cif/command/settings.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/gds/write/subtree.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::cif {

/// The options of one write: the session's `cif` settings at the time of
/// the call, whether it is `cif flat`, and the name the user typed (the
/// messages name the file as typed).
struct CifWriteOptions {
  CifSettings settings;
  bool flat = false;
  std::string typed_name;
  /// How the prepare phase reads unavailable cells and fixes timestamp
  /// mismatches (gds::command_subtree_loader in a command); empty members:
  /// unavailable cells cannot be read and no mismatch is processed.
  gds::SubtreeLoader loader;
  /// The file system the output is written on; null means the local one.
  platform::FileSystem* files = nullptr;
  /// The preamble's host values ("CIF output header", Q-MASK-324): the
  /// command fills them from the environment (USER, HOSTNAME); "?" when
  /// unset.
  std::string user = "?";
  std::string machine = "?";
  /// The zone of the `@@date` line (a POSIX TZ string); empty: the host zone.
  std::string time_zone;
  /// The time of the `@@date` line (seconds since the epoch); nullopt: the
  /// system clock.
  std::optional<std::int64_t> now;
};

/// The result of the prepare phase: everything emit_write needs.
struct PreparedWrite {
  editor::EditorState* editor = nullptr;
  db::CellDef* root = nullptr;
  CifWriteOptions options;
  std::shared_ptr<const mask::OutputStyle> style;  // the current output style, taken after subtree loading
  compat::CompatSettings compat;                   // the session's compat settings
};

/// The prepare phase (exclusive lock): checks that an output style is set;
/// for `cif write` loads the subtree (DBCellReadArea over the root's
/// bounding box, halting on the first cell that cannot be read) and fixes
/// the timestamp mismatches; for `cif flat` reads the cells the flattening
/// reaches (DBTreeSrTiles reads them, without halting). Then it takes the
/// current style and refuses one whose reducer is 0 (Magic refuses it for
/// `cif write`, and divides by zero for `cif flat`).
[[nodiscard]] diag::Outcome<PreparedWrite> prepare_write(editor::EditorState& editor, db::CellDef& root,
                                                         const CifWriteOptions& options);

/// The emit phase (shared lock): the preamble, the symbols in Magic's
/// numbering order (Q-MASK-323) or the single flat symbol (Q-MASK-321,
/// 411), then `C <root>;` and `End`. It mutates nothing in the layout. The
/// output goes to a temporary file renamed onto `path`; a cancelled write
/// leaves no file.
[[nodiscard]] cmd::CommandResult emit_write(const PreparedWrite& prepared, const platform::Path& path,
                                            const mask::Progress* progress = nullptr);

/// prepare_write then emit_write. When the prepare phase fails reading the
/// subtree or refuses a reducer of 0, the output file is left empty and the
/// failure is followed by Magic's "I/O error in writing file <typed>."
/// (CmdCif opens the file before CIFWrite fails); without an output style
/// nothing is written.
[[nodiscard]] cmd::CommandResult write(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                                       const CifWriteOptions& options, const mask::Progress* progress = nullptr);

/// The output file of `cif write NAME` / `cif flat NAME` (Q-MASK-320): with
/// magic-compat ON `.cif` is always appended, as Magic's PaOpen does; with
/// it OFF only when NAME does not already end in `.cif`.
[[nodiscard]] std::string output_file_name(std::string_view name, const compat::CompatSettings& compat);

}  // namespace hefesto::cif
