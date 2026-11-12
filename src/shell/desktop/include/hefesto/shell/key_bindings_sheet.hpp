// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The modal key-binding sheet (board M2-Key-Bindings; carried from M2 to
// M4, openspec/changes/m4-scripting/design.md D17): add or edit a binding
// by capturing the next key chord (Esc cancels the capture), a conflict
// warning with Replace when the key is already bound in the same table
// (nothing is written until confirmed), Reset table (the table's startup
// bindings of ui::install_defaults) and Load file… (a key-binding file read
// with ui::read_macro_file's rules, so only `macro`/`imacro` lines apply).
//
// The sheet changes nothing itself: every action yields the `macro` /
// `imacro` command lines to issue (`macro TABLE KEY "TEXT"`, deleting with
// `macro TABLE KEY ""`), so the change is logged and replayable like a
// typed command, and the rows (screen_models.hpp key_binding_rows) round-
// trip with them.
//
// Toolkit-independent; unit-tested in tests/unit/shell/key_bindings_sheet_test.cpp.
// Owner: CARRY (openspec/changes/m4-scripting/design.md D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/ui/input.hpp"
#include "hefesto/ui/macros.hpp"

namespace hefesto::shell {

/// `TEXT` in double quotes with `"`, `\`, `$`, `[` and `]` escaped, as the
/// sheet's rows spell a macro text.
[[nodiscard]] std::string quote_macro_text(std::string_view text);

/// The command line that binds `key` in `table` (`imacro` when
/// `interactive`); an empty `text` deletes the binding. A non-empty `help`
/// is passed as the third word.
[[nodiscard]] std::string macro_command(std::string_view table, const ui::Key& key, std::string_view text,
                                        bool interactive = false, std::string_view help = {});

/// The commands that turn `table` of `current` into `table` of `target`:
/// a binding missing from `target` is deleted, a new or different one is
/// defined; in `target`'s order, deletions first.
[[nodiscard]] std::vector<std::string> binding_changes(const ui::MacroTables& current,
                                                       const ui::MacroTables& target, std::string_view table);

/// Reset table: the commands that restore `table` to its startup bindings
/// (ui::install_defaults into a fresh session).
[[nodiscard]] std::vector<std::string> reset_table_commands(const ui::MacroTables& current, std::string_view table);

/// Load file…: what a key-binding file would change, as commands over every
/// table it touches (tables in Magic's order), plus the file's diagnostics
/// (a line that is not `macro`/`imacro` is reported and skipped).
struct LoadedBindings {
  std::vector<std::string> commands;
  std::vector<diag::Diagnostic> diagnostics;
};
[[nodiscard]] LoadedBindings load_file_commands(const ui::MacroTables& current, std::string_view text,
                                                std::string_view source_name);

/// The sheet's add/edit state for one table.
class KeyBindingSheet {
 public:
  enum class Mode : std::uint8_t { browse, capturing, conflict };

  explicit KeyBindingSheet(std::string table) : table_(std::move(table)) {}

  [[nodiscard]] const std::string& table() const noexcept { return table_; }
  [[nodiscard]] Mode mode() const noexcept { return mode_; }

  /// "Add binding": an empty command, then capture.
  void begin_add();
  /// Edits the binding of `key` (its row): the command and flag are
  /// prefilled, then capture.
  void begin_edit(const ui::Key& key, const ui::Macro& macro);
  /// The command field (the macro text).
  void set_text(std::string text) { text_ = std::move(text); }
  [[nodiscard]] const std::string& text() const noexcept { return text_; }
  void set_interactive(bool interactive) noexcept { interactive_ = interactive; }

  /// A key press while capturing (`macos`: Command and Option are Meta, as
  /// in the canvas). Modifier-only keys keep capturing; Escape without
  /// modifiers cancels the capture (back to browse). Another chord is the
  /// captured key; when the table binds it to something else (and it is not
  /// the key being edited) the sheet enters `conflict`. Returns true when
  /// the event was used.
  bool capture(const ui::KeyEvent& event, bool macos, const ui::MacroTables& macros);

  /// The captured key and its display name ("Control_z").
  [[nodiscard]] const std::optional<ui::Key>& key() const noexcept { return key_; }
  [[nodiscard]] std::string key_name() const;

  /// The inline warning in `conflict` ("▲ Control_z is bound to “undo” in
  /// Layout (box tool)"); empty otherwise.
  [[nodiscard]] const std::string& warning() const noexcept { return warning_; }

  /// ✓: the commands that write the binding (an edited key that changed
  /// deletes the old one first). Empty while there is no key or command, or
  /// while a conflict waits for Replace. Back to browse when it returns
  /// commands.
  [[nodiscard]] std::vector<std::string> confirm();
  /// Replace (in `conflict`): accepts the conflict, then confirm().
  [[nodiscard]] std::vector<std::string> replace();
  /// Leaves add/edit without writing (Esc outside capture, Cancel).
  void cancel();

  /// The command that deletes `key`'s binding (a row's delete button).
  [[nodiscard]] std::string delete_command(const ui::Key& key) const;

 private:
  std::string table_;
  Mode mode_ = Mode::browse;
  std::optional<ui::Key> editing_;  // the key of the edited row
  std::optional<ui::Key> key_;
  std::string text_;
  bool interactive_ = false;
  std::string warning_;
  bool replace_ok_ = false;
};

}  // namespace hefesto::shell
