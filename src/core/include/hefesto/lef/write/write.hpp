// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF write entry points (capability lef-write;
// openspec/changes/m6-lef-def-oasis/design.md D4, D7, D14): `lef write
// [cell] [-tech] [-hide [distance]] [-pinonly [distance]] [-toplayer]
// [-nomaster]` calls lef::write, `lef writeall [-notop] [-all] ...` calls
// lef::write_all (lefCmd.c:283-425). Single-phase commands under the exclusive
// database lock.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFW fills
// in the implementation in src/core/src/lef/write/ (design D2).

#pragma once

#include <optional>
#include <string>

#include "hefesto/cmd/command.hpp"
#include "hefesto/db/geometry.hpp"
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
  std::optional<db::Coord> hide;      // -hide [distance]; 0 = whole cell
  std::optional<db::Coord> pin_only;  // -pinonly [distance]
  bool top_layer = false;             // -toplayer
  bool do_master = true;              // -nomaster clears it
  bool top_cell = true;               // writeall: -notop clears it
  bool recurse = false;               // writeall -all
  std::string typed_name;             // the name the user typed
  /// The file system the output is written on; null means the local one.
  platform::FileSystem* files = nullptr;
};

/// Writes `cell` as one LEF macro to `path`. SCAFFOLD: fails with
/// CORE-NOTIMPL-006 ("not implemented yet: lef write") until LEFW lands it.
[[nodiscard]] cmd::CommandResult write(editor::EditorState& editor, db::CellDef& cell, const platform::Path& path,
                                       const LefWriteOptions& options);

/// `lef writeall`: writes every cell under `root` to `path`. SCAFFOLD:
/// fails with CORE-NOTIMPL-006 ("not implemented yet: lef writeall").
[[nodiscard]] cmd::CommandResult write_all(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                                           const LefWriteOptions& options);

}  // namespace hefesto::lef
