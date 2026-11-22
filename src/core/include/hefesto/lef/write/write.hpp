// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF write entry points (capability lef-write;
// openspec/changes/m6-lef-def-oasis/design.md D4, D7, D14): `lef write
// [filename] [-tech] [-hide [distance]] [-pinonly [distance]] [-toplayer]
// [-nomaster]` calls lef::write, `lef writeall [-notop] [-all] ...` calls
// lef::write_all (lefCmd.c:283-425). Single-phase commands under the
// exclusive database lock. The written cells are not modified, nothing is
// recorded for undo, and the selection is cleared after a macro is
// written (lef-write "Write side effects").
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFW fills
// in the implementation in src/core/src/lef/write/ (design D2).

#pragma once

#include <optional>
#include <string>

#include "hefesto/cmd/command.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/write/subtree.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::lef {

/// The options of one `lef write` or `lef writeall`.
struct LefWriteOptions {
  bool tech = false;                  // -tech: write the technology section too
  std::optional<db::Coord> hide;      // -hide [distance]: the setback; nullopt or < 0 disables it
  std::optional<db::Coord> pin_only;  // -pinonly [distance]; nullopt or < 0 disables it
  bool top_layer = false;             // -toplayer
  bool do_master = true;              // -nomaster clears it
  bool top_cell = true;               // writeall: -notop clears it
  bool recurse = false;               // writeall -all
  std::string typed_name;             // the name the user typed
  /// The file system the output is written on; null means the local one.
  platform::FileSystem* files = nullptr;
  /// writeall: how the subtree is read before writing (DBCellReadArea;
  /// gds::command_subtree_loader in a command). Empty members: an
  /// unavailable cell cannot be read and fails the write.
  gds::SubtreeLoader loader;
  /// `lef write`: the sink `loader` prints to. Its messages are taken after
  /// the subcells are read and printed where Magic prints them, after the
  /// macro's "Writing LEF output for cell" line (LEFDEF-128). nullptr: they
  /// go where the loader sends them, before the write's own messages.
  diag::CollectingSink* read_messages = nullptr;
  /// writeall: an interrupt skips the remaining macros; `END LIBRARY` is
  /// still written. nullptr: never interrupted.
  const mask::Progress* progress = nullptr;
};

/// LefWriteCell: writes `cell` as one LEF macro, with the library header
/// when `cell` is the edit root. `path` is the base name of the output
/// (the typed file name, or the cell name), from which the file name is
/// derived (lef-write "Output file naming").
[[nodiscard]] cmd::CommandResult write(editor::EditorState& editor, db::CellDef& cell, const platform::Path& path,
                                       const LefWriteOptions& options);

/// LefWriteAll (`lef writeall`): loads the subtree of `root`, then writes
/// the library header and a macro for every collected cell in reverse
/// collection order. An empty `path` derives the base name from the root's
/// file path, else its name.
[[nodiscard]] cmd::CommandResult write_all(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                                           const LefWriteOptions& options);

}  // namespace hefesto::lef
