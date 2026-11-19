// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF read entry point (capability lef-read;
// openspec/changes/m6-lef-def-oasis/design.md D4, D6, D14): `lef read
// <file> [-import] [-annotate]`, the CLI and the shell all call
// lef::read. It is a single-phase command under the exclusive database
// lock and reports progress through mask::ProgressHost (LefEstimate,
// Q-LEFDEF-88).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFR fills
// in the implementation in src/core/src/lef/read/ (design D2).

#pragma once

#include <string>

#include "hefesto/cmd/command.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::lef {

/// The options of one `lef read`.
struct LefReadOptions {
  bool import = false;     // -import: accepted, no effect (Q-LEFDEF-16, D-5)
  bool annotate = false;   // -annotate: annotate existing cells, create none
  std::string typed_name;  // the name the user typed, for messages
  /// The file system the file is opened on; null means the local one.
  platform::FileSystem* files = nullptr;
};

/// Reads the LEF file named `path` (found by find_lef_file): the LAYER, VIA,
/// VIARULE and NONDEFAULTRULE sections into the session's LEF tables, and
/// each MACRO and SITE section into a new or annotated cell. The read is
/// one undo step (Q-LEFDEF-22). The result carries the read's messages
/// ("Reading LEF data from file ...", the reader's LefError lines,
/// "LEF read: Processed <n> lines." and the totals); it fails only when
/// no file opens.
[[nodiscard]] cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path,
                                      const LefReadOptions& options);

}  // namespace hefesto::lef
