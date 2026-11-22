// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF read entry point (capability def-read;
// openspec/changes/m6-lef-def-oasis/design.md D4, D8, D14): `def read
// <file> [-labels] [-annotate] [-noblockage]`, the CLI and the shell all
// call def::read. Single-phase command under the exclusive database lock,
// with progress through mask::ProgressHost (the caller passes the
// host's mask::Progress in the options).
//
// A read is one undoable step (Q-LEFDEF-22): its changes to an existing
// design cell are recorded as edits, and a design cell it creates is
// recorded as one event that deletes the cell on undo and makes it again on
// redo. The LEF/DEF via and rule tables it fills are session state, as
// Magic's, and are not undone.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); DEFR fills
// in the implementation in src/core/src/def/read/ (design D2).

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::db {
class CellDef;
}

namespace hefesto::gds {
class ByteSource;
}

namespace hefesto::mask {
struct Progress;
}

namespace hefesto::def {

/// The options of one `def read`.
struct DefReadOptions {
  bool labels = false;       // -labels: label nets on their geometry
  bool annotate = false;     // -annotate: annotate existing cells only (implies labels)
  bool no_blockage = false;  // -noblockage: skip BLOCKAGES
  std::string typed_name;    // the name the user typed: messages and the fallback design name
  /// The file system the file is opened on; null means the local one.
  platform::FileSystem* files = nullptr;
  /// Reads the `.mag` file of a component master into its new, empty
  /// definition (DBCellRead(def, TRUE, TRUE, NULL)); false when there is
  /// none. Empty: only loaded cells can be masters.
  std::function<bool(db::CellDef&)> read_master;
  /// The sink `read_master` prints to. After each master read the reader
  /// moves its messages into the read's own, so they come out in Magic's
  /// order among the section counts (LEFDEF-232). nullptr: they go where
  /// read_master sends them.
  diag::CollectingSink* master_messages = nullptr;
  /// Progress reports and cancellation (Q-LEFDEF-88); null: none. A
  /// cancelled read stops as if the file ended there.
  const mask::Progress* progress = nullptr;
};

/// "DEF file lookup": a name whose extension is not `.def` as given, then
/// with `.def` in place of its extension; a `.def` name as given; a name
/// without extension with `.def` appended. Each candidate is searched along
/// the cell search path, then the cell library path. nullopt when none
/// opens.
[[nodiscard]] std::optional<cell::FoundFile> find_def_file(cell::LookupContext& ctx, std::string_view name);

/// Reads the DEF file `path` (the file def read resolved). The value of a
/// successful result is the design cell's name (empty when the file chose
/// none); the diagnostics are the read's messages.
[[nodiscard]] cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path,
                                      const DefReadOptions& options);

/// Reads DEF text from `source` (tests, fuzzing); `file_name` is the name
/// the "Reading DEF data" message prints.
[[nodiscard]] cmd::CommandResult read(editor::EditorState& editor, gds::ByteSource& source,
                                      std::string_view file_name, const DefReadOptions& options,
                                      std::uint64_t size_hint = 0);

}  // namespace hefesto::def
