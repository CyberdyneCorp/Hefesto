// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF write entry point (capability def-write;
// openspec/changes/m6-lef-def-oasis/design.md D4, D9, D14): `def write
// [cell] [-units value] [-analog] [-allspecial]`, the CLI and the shell all
// call def::write. Single-phase command under the exclusive database lock.
// The nets come from the `.ext` netlist (extflat::read_flat, Open Question
// 1 (a)).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); DEFW fills
// in the implementation in src/core/src/def/write/ (design D2).

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
  std::optional<std::int32_t> units;  // -units value (DEF database units)
  bool analog = false;                // -analog: device ports as pins
  bool all_special = false;           // -allspecial: every net in SPECIALNETS
  std::string typed_name;             // the name the user typed
  /// The file system the output is written on; null means the local one.
  platform::FileSystem* files = nullptr;
};

/// Writes `cell` as a DEF file to `path`. SCAFFOLD: fails with
/// CORE-NOTIMPL-006 ("not implemented yet: def write") until DEFW lands it.
[[nodiscard]] cmd::CommandResult write(editor::EditorState& editor, db::CellDef& cell, const platform::Path& path,
                                       const DefWriteOptions& options);

}  // namespace hefesto::def
