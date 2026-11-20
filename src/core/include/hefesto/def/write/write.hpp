// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF write entry point (capability def-write;
// openspec/changes/m6-lef-def-oasis/design.md D4, D9, D14): `def write
// [filename] [-units value] [-allspecial] [-anal]`, the CLI and the shell
// all call def::write (the command's parser is def/write/command.hpp).
// Single-phase command under the exclusive database lock. The nets come
// from the `.ext` netlist (extflat::read_flat, Open Question 1 (a)). The
// file is built in memory (the nets sections too, Q-LEFDEF-90) and written
// in one piece through FileSystem::write_atomic; a write that cannot open
// its file changes no session state.
//
// Owner: DEFW (design D2; the header was created by the M6 scaffold).

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "hefesto/cmd/command.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::def {

/// The options of one `def write`.
struct DefWriteOptions {
  std::optional<std::int32_t> units;  // -units value (DEF database units); nullopt = 1000
  bool anal = false;                  // -anal: every route layer in each NONDEFAULTRULES rule
  bool all_special = false;           // -allspecial: every net in SPECIALNETS
  std::string typed_name;             // the name the user typed
  /// The file system the output is written on; null means the local one.
  platform::FileSystem* files = nullptr;
};

/// DefWriteCell: writes `cell` as a DEF file named after `path` (the typed
/// name or the cell name; def-write "Output file naming"). Fails with
/// DEF-WRITE-003 for the unnamed cell, DEF-WRITE-002 when no file can be
/// written, DEF-WRITE-010 with no technology and DEF-WRITE-011 for units
/// <= 0 (Q-LEFDEF-104 fix).
[[nodiscard]] cmd::CommandResult write(editor::EditorState& editor, db::CellDef& cell, const platform::Path& path,
                                       const DefWriteOptions& options);

}  // namespace hefesto::def
