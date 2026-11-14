// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CIF read (capability cif-format-io; openspec/changes/m5-mask-gds-cif
// design.md D4 "Commands and progress", D13): a table-driven lexer and
// command dispatcher (DS, DF, DD, C, L, B, R, P, W, E and the user
// extensions 9, 91, 94, 95) driving the shared read engine through
// mask::ReadSink. The symbol table belongs to one read (ReadSession), never
// to the library (Q-MASK-318, fix in both modes): symbol n, in `DS n` and in
// a forward `C n` alike, always maps to a cell created by this read, a name
// collision with an existing cell gives `n_k`, and no cell already in memory
// is cleared or redefined.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); CIF fills in
// the implementation in src/core/src/cif/read/ (design D2). The parser is
// cif/read/parser.hpp and the symbol table cif/read/symbols.hpp.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "hefesto/cif/command/settings.hpp"
#include "hefesto/cif/read/symbols.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/input/warnings.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::cif {

/// The options of one read: the session's `cif` settings and read warnings
/// at the time of the call, and the name the user typed.
/// The default CifReadOptions::max_file_bytes: 2 GiB (hardening; Magic
/// streams the file).
inline constexpr std::uint64_t kMaxCifFileBytes = std::uint64_t{2} << 30U;

struct CifReadOptions {
  CifSettings settings;
  mask::WarningSettings warnings;
  std::string typed_name;
  /// The file system the file and the redirect file are on; null means the
  /// local one (added by CIF in round 4).
  platform::FileSystem* files = nullptr;
  /// The largest file read() accepts (the text is held in memory while it
  /// is parsed); a larger one fails with CIF-READ-010 before it is read.
  std::uint64_t max_file_bytes = kMaxCifFileBytes;
};

/// The state of one CIF read.
struct ReadSession {
  SymbolTable symbols;
  std::int64_t errors = 0;    // counted toward the error limit
  std::int64_t warnings = 0;
  bool saw_end = false;       // the E command was read
};

/// Reads `path` into the editor's cell library (CIFReadFile): the
/// definitions become cells of this read, top-level geometry goes into the
/// edit cell. Holds the exclusive database lock and records no undo. Fails
/// when the file cannot be read or no input style is set.
[[nodiscard]] cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path,
                                      const CifReadOptions& options, const mask::Progress* progress = nullptr);
/// The same over CIF text already in memory.
[[nodiscard]] cmd::CommandResult read_text(editor::EditorState& editor, std::string_view text,
                                           const CifReadOptions& options, const mask::Progress* progress = nullptr);

}  // namespace hefesto::cif
