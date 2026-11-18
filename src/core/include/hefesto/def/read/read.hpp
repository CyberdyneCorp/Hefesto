// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF read entry point (capability def-read;
// openspec/changes/m6-lef-def-oasis/design.md D4, D8, D14): `def read
// <file> [-labels] [-annotate] [-noblockage]`, the CLI and the shell all
// call def::read. Single-phase command under the exclusive database lock,
// with progress through mask::ProgressHost.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); DEFR fills
// in the implementation in src/core/src/def/read/ (design D2).

#pragma once

#include <string>

#include "hefesto/cmd/command.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::def {

/// The options of one `def read`.
struct DefReadOptions {
  bool labels = false;       // -labels: label nets on their geometry
  bool annotate = false;     // -annotate: annotate existing cells only
  bool no_blockage = false;  // -noblockage: skip BLOCKAGES
  std::string typed_name;    // the name the user typed, for messages
  /// The file system the file is opened on; null means the local one.
  platform::FileSystem* files = nullptr;
};

/// Reads the DEF file `path`. SCAFFOLD: fails with CORE-NOTIMPL-006 ("not
/// implemented yet: def read") until DEFR lands it.
[[nodiscard]] cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path,
                                      const DefReadOptions& options);

}  // namespace hefesto::def
