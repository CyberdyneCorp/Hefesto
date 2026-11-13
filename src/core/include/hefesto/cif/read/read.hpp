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
// the implementation in src/core/src/cif/read/ (design D2).

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

#include "hefesto/cif/command/settings.hpp"
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
struct CifReadOptions {
  CifSettings settings;
  mask::WarningSettings warnings;
  std::string typed_name;
};

/// The symbol numbers of one read and the cells they name.
class SymbolTable {
 public:
  /// The cell of symbol `number`, or nullptr when not yet defined or called.
  [[nodiscard]] db::CellDef* find(std::int64_t number) const noexcept;
  /// Binds symbol `number` to `cell` (DS n, or a forward C n).
  void bind(std::int64_t number, db::CellDef& cell);
  /// `DD n`: magic-compat ON deletes symbol n only; OFF deletes every
  /// symbol >= n (CIF 2.0).
  void delete_from(std::int64_t number, bool magic_compat);

  [[nodiscard]] std::size_t size() const noexcept { return cells_.size(); }

 private:
  std::map<std::int64_t, db::CellDef*> cells_;
};

/// The state of one CIF read.
struct ReadSession {
  SymbolTable symbols;
  std::int64_t errors = 0;    // counted toward the error limit
  std::int64_t warnings = 0;
};

/// Reads `path` into the editor's cell library.
[[nodiscard]] cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path,
                                      const CifReadOptions& options, const mask::Progress* progress = nullptr);

}  // namespace hefesto::cif
