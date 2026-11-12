// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The key-binding sheet model (key_bindings_sheet.hpp). Owner: CARRY.

#include "hefesto/shell/key_bindings_sheet.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/shell/screen_models.hpp"
#include "hefesto/ui/bindings.hpp"
#include "hefesto/ui/macro_file.hpp"

namespace hefesto::shell {

namespace {

constexpr std::uint32_t kEscapeKeysym = 0xFF1BU;

bool same_macro(const ui::Macro& a, const ui::Macro& b) {
  return a.text == b.text && a.help == b.help && a.interactive == b.interactive;
}

std::string table_label_of(const ui::MacroTables& macros, std::string_view table) {
  for (const KeyBindingTable& t : key_binding_tables(macros)) {
    if (t.name == table) return t.label;
  }
  return std::string(table);
}

}  // namespace

std::string quote_macro_text(std::string_view text) {
  std::string out = "\"";
  for (const char c : text) {
    if (c == '"' || c == '\\' || c == '$' || c == '[' || c == ']') out += '\\';
    out += c;
  }
  return out + "\"";
}

std::string macro_command(std::string_view table, const ui::Key& key, std::string_view text, bool interactive,
                          std::string_view help) {
  std::string line = std::string(interactive && !text.empty() ? "imacro " : "macro ") + std::string(table) + " " +
                     ui::key_display_name(key) + " " + quote_macro_text(text);
  if (!help.empty() && !text.empty()) line += " " + quote_macro_text(help);
  return line;
}

std::vector<std::string> binding_changes(const ui::MacroTables& current, const ui::MacroTables& target,
                                         std::string_view table) {
  std::vector<std::string> deletions;
  for (const auto& [key, macro] : current.entries(table)) {
    if (macro != nullptr && target.find(table, key) == nullptr) deletions.push_back(macro_command(table, key, ""));
  }
  std::vector<std::string> definitions;
  for (const auto& [key, macro] : target.entries(table)) {
    if (macro == nullptr) continue;
    const ui::Macro* now = current.find(table, key);
    if (now != nullptr && same_macro(*now, *macro)) continue;
    definitions.push_back(macro_command(table, key, macro->text, macro->interactive, macro->help));
  }
  deletions.insert(deletions.end(), definitions.begin(), definitions.end());
  return deletions;
}

std::vector<std::string> reset_table_commands(const ui::MacroTables& current, std::string_view table) {
  editor::EditorState fresh;
  ui::install_defaults(fresh);
  return binding_changes(current, fresh.macros, table);
}

LoadedBindings load_file_commands(const ui::MacroTables& current, std::string_view text,
                                  std::string_view source_name) {
  // Read into a copy of the current tables, then compare: the file's
  // deletions and redefinitions show as commands like its new bindings.
  editor::EditorState scratch;
  ui::install_defaults(scratch);
  scratch.macros = current;
  LoadedBindings out;
  out.diagnostics = ui::read_macro_file(scratch, text, source_name).diagnostics;
  for (const std::string& table : scratch.macros.table_names()) {
    std::vector<std::string> changes = binding_changes(current, scratch.macros, table);
    out.commands.insert(out.commands.end(), changes.begin(), changes.end());
  }
  return out;
}

// --- the sheet ------------------------------------------------------------------------------

void KeyBindingSheet::begin_add() {
  editing_.reset();
  key_.reset();
  text_.clear();
  interactive_ = false;
  warning_.clear();
  replace_ok_ = false;
  mode_ = Mode::capturing;
}

void KeyBindingSheet::begin_edit(const ui::Key& key, const ui::Macro& macro) {
  begin_add();
  editing_ = key;
  key_ = key;
  text_ = macro.text;
  interactive_ = macro.interactive;
}

bool KeyBindingSheet::capture(const ui::KeyEvent& event, bool macos, const ui::MacroTables& macros) {
  if (mode_ != Mode::capturing) return false;
  if (event.keysym == kEscapeKeysym && (event.state & 0xFFU) == 0U) {
    mode_ = Mode::browse;  // Esc cancels the capture; nothing is written
    key_ = editing_;
    return true;
  }
  const auto key = ui::normalize_key_event(event, macos);
  if (!key) return true;  // a modifier alone: keep waiting for the chord
  key_ = *key;
  warning_.clear();
  replace_ok_ = false;
  const ui::Macro* bound = macros.find(table_, *key);
  const bool other = bound != nullptr && (!editing_ || !(*editing_ == *key));
  if (other) {
    warning_ = "▲ " + ui::key_display_name(*key) + " is bound to “" + bound->text + "” in " +
               table_label_of(macros, table_);
    mode_ = Mode::conflict;
  } else {
    mode_ = Mode::browse;
  }
  return true;
}

std::string KeyBindingSheet::key_name() const { return key_ ? ui::key_display_name(*key_) : std::string(); }

std::vector<std::string> KeyBindingSheet::confirm() {
  if (!key_ || text_.empty() || (mode_ == Mode::conflict && !replace_ok_) || mode_ == Mode::capturing) return {};
  std::vector<std::string> lines;
  if (editing_ && !(*editing_ == *key_)) lines.push_back(macro_command(table_, *editing_, ""));
  lines.push_back(macro_command(table_, *key_, text_, interactive_));
  cancel();
  return lines;
}

std::vector<std::string> KeyBindingSheet::replace() {
  if (mode_ != Mode::conflict) return {};
  replace_ok_ = true;
  return confirm();
}

void KeyBindingSheet::cancel() {
  mode_ = Mode::browse;
  editing_.reset();
  key_.reset();
  text_.clear();
  warning_.clear();
  replace_ok_ = false;
}

std::string KeyBindingSheet::delete_command(const ui::Key& key) const { return macro_command(table_, key, ""); }

}  // namespace hefesto::shell
