// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The key-binding sheet on screen (board M2-Key-Bindings; M4 CARRY task
// 11.3), from the KeyBindingSheet model (key_bindings_sheet.hpp). `panel
// keys show` opens it as a sheet over the canvas (the Keys tab behind it
// keeps the full list); while it is open it takes the keyboard: a capture
// takes the next chord, Escape cancels the capture, then closes the sheet.
//   - TABLE chooses the macro table (Magic's order, the current tool's first);
//   - the list shows the table's bindings (key display names, the command,
//     "imacro" for interactive ones); Edit and Delete act on the chosen row;
//   - Add binding / Edit capture the next key; a key bound in the same table
//     shows the inline ▲ warning with Replace, and nothing is written until
//     ✓ Save (or Replace);
//   - Reset table and Load file… issue the model's commands.
// Every change is a `macro` / `imacro` command run through run_line, so it is
// logged and replayable. Raven has no modal sheet for custom content; the
// sheet is a card over the canvas that holds the keyboard while open.
//
// Owner: CARRY (openspec/changes/m4-scripting/design.md D2).

#include <algorithm>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/shell/key_bindings_sheet.hpp"
#include "hefesto/ui/tools.hpp"
#include "main_window.hpp"

namespace hefesto::shell {

namespace {

constexpr float kSheetWidth = 640.0F;
constexpr float kSheetHeight = 460.0F;

#if defined(__APPLE__)
constexpr bool kMacos = true;
#else
constexpr bool kMacos = false;
#endif

}  // namespace

void MainWindow::build_key_sheet() {
  key_card_.emplace(ui::Panel::create(ctx_.tokens().control_border));
  key_card_->set_padding(1.0F, 1.0F, 1.0F, 1.0F);
  ui::Panel& inner = key_inner_.emplace(ui::Panel::create(ctx_.tokens().surface1));
  inner.set_padding(16.0F, 12.0F, 16.0F, 12.0F);
  key_box_.set_spacing(8.0F);
  key_title_.emplace(ui::Label::create("Key bindings"));
  key_box_.add(*key_title_);
  ui::Box top = ui::Box::horizontal();
  top.set_spacing(8.0F);
  top.add(key_table_host_, 1.0F);  // the TABLE chooser (open_key_sheet)
  const auto button = [this, &top](std::string_view text, bool primary, std::function<void()> click) {
    auto b = ui::Button::create(text, primary);
    b->on_click(std::move(click));
    top.add(*b);
    key_buttons_.push_back(std::move(b));
  };
  button("Reset table", false, [this] { issue_key_commands(reset_table_commands(ctx_.editor().macros, key_sheet_->table())); });
  button("Load file…", false, [this] { load_key_file(); });
  button("Close", false, [this] { run_line("panel keys hide"); });
  key_box_.add(top);
  key_list_ = ui::Table::create();
  key_list_->set_accessible_name("Bindings");
  key_list_->add_column("Key", 160.0F, 0.0F, false);
  key_list_->add_column("Command", 300.0F, 1.0F, false);
  key_list_->add_column("", 70.0F, 0.0F, false);
  key_list_->on_activate([this](int row) { edit_key_row(row); });
  key_box_.add(*key_list_, 1.0F);
  ui::Box rows = ui::Box::horizontal();
  rows.set_spacing(8.0F);
  const auto row_button = [this, &rows](std::string_view text, std::function<void()> click) {
    auto b = ui::Button::create(text, false);
    b->on_click(std::move(click));
    rows.add(*b);
    key_buttons_.push_back(std::move(b));
  };
  row_button("Add binding", [this] {
    key_sheet_->begin_add();
    refresh_key_sheet();
  });
  row_button("Edit", [this] { edit_key_row(key_list_->selected()); });
  row_button("Delete", [this] {
    const auto i = static_cast<std::size_t>(std::max(0, key_list_->selected()));
    if (key_list_->selected() >= 0 && i < key_rows_.size()) {
      issue_key_commands({key_sheet_->delete_command(key_rows_[i].first)});
    }
  });
  key_box_.add(rows);
  // The add/edit row: the captured key, the command, ✓ and Cancel; the conflict warning.
  ui::Box edit = ui::Box::horizontal();
  edit.set_spacing(8.0F);
  key_capture_.emplace(ui::Label::create(""));
  key_capture_->set_size(180.0F, -1.0F);
  edit.add(*key_capture_);
  key_command_ = ui::TextInput::create("Command");
  key_command_->set_accessible_name("Command");
  key_command_->on_change([this](std::string text) { key_sheet_->set_text(std::move(text)); });
  edit.add(*key_command_, 1.0F);
  key_save_ = ui::Button::create("✓ Save", true);
  key_save_->on_click([this] { issue_key_commands(key_sheet_->confirm()); });
  edit.add(*key_save_);
  key_cancel_ = ui::Button::create("Cancel", false);
  key_cancel_->on_click([this] {
    key_sheet_->cancel();
    refresh_key_sheet();
  });
  edit.add(*key_cancel_);
  key_box_.add(edit);
  ui::Box warning = ui::Box::horizontal();
  warning.set_spacing(8.0F);
  key_warning_.emplace(ui::Label::create(""));
  warning.add(*key_warning_, 1.0F);
  key_replace_ = ui::Button::create("Replace", true);
  key_replace_->on_click([this] { issue_key_commands(key_sheet_->replace()); });
  warning.add(*key_replace_);
  key_box_.add(warning);
  inner.set_child(key_box_);
  key_card_->set_child(inner);
  key_card_->set_size(kSheetWidth, kSheetHeight);
  key_card_->set_accessible_name("Key bindings sheet");
  canvas_->add_overlay(*key_card_);
  key_card_->set_visible(false);
  key_sheet_.emplace("layout");
}

void MainWindow::open_key_sheet() {
  // The current tool's table first: it answers the canvas's keys and buttons.
  std::vector<KeyBindingTable> tables = key_binding_tables(ctx_.editor().macros);
  const hefesto::ui::Tool* tool = ctx_.editor().tools.current();
  const std::string current = tool != nullptr ? tool->name : std::string("box");
  std::stable_partition(tables.begin(), tables.end(), [&current](const KeyBindingTable& t) { return t.name == current; });
  key_tables_.clear();
  std::vector<std::string> labels;
  for (const KeyBindingTable& t : tables) {
    key_tables_.push_back(t.name);
    labels.push_back(t.label + " · " + t.name);
  }
  if (key_tables_.empty()) return;
  if (labels != key_table_labels_) {
    // Raven combo items are fixed: a new chooser for a new table list (the old one stays hidden).
    if (key_table_choice_) {
      key_table_choice_->set_visible(false);
      key_old_choices_.push_back(std::move(key_table_choice_));
    }
    key_table_choice_ = ui::ComboBox::create(labels);
    key_table_choice_->set_accessible_name("Table");
    key_table_choice_->on_select([this](int index) {
      const auto i = static_cast<std::size_t>(std::max(0, index));
      if (i < key_tables_.size()) select_key_table(key_tables_[i]);
    });
    key_table_host_.add(*key_table_choice_, 1.0F);
    key_table_labels_ = std::move(labels);
  }
  key_table_choice_->set_selected(0);
  select_key_table(key_tables_.front());
  const ui::Rect area = canvas_->widget().rect();
  key_card_->place_top_right(std::max(0.0F, (area.width - kSheetWidth) / 2.0F), 32.0F);
  key_card_->set_visible(true);
  key_list_->focus();
}

void MainWindow::close_key_sheet() {
  if (key_sheet_) key_sheet_->cancel();
  if (key_card_) key_card_->set_visible(false);
}

void MainWindow::select_key_table(const std::string& table) {
  key_sheet_.emplace(table);
  refresh_key_sheet();
}

void MainWindow::refresh_key_sheet() {
  if (!key_card_ || !key_sheet_) return;
  const ::hefesto::ui::MacroTables& macros = ctx_.editor().macros;
  key_rows_.clear();
  for (const auto& [key, macro] : macros.entries(key_sheet_->table())) {
    if (macro != nullptr) key_rows_.emplace_back(key, *macro);
  }
  key_list_->set_rows(static_cast<int>(key_rows_.size()), [this](int row, int column) -> std::string {
    const auto& [key, macro] = key_rows_[static_cast<std::size_t>(row)];
    if (column == 0) return ::hefesto::ui::key_display_name(key);
    if (column == 1) return macro.text;
    return macro.interactive ? "imacro" : "";
  });
  using Mode = KeyBindingSheet::Mode;
  const Mode mode = key_sheet_->mode();
  std::string capture;
  if (mode == Mode::capturing) {
    capture = "Press a key… (Esc cancels)";
  } else if (key_sheet_->key()) {
    capture = "Key: " + key_sheet_->key_name();
  } else {
    capture = "Choose a row, or Add binding";
  }
  key_capture_->set_text(capture);
  key_command_->set_text(key_sheet_->text());
  key_command_->set_enabled(mode != Mode::browse);
  key_save_->set_enabled(mode == Mode::capturing ? false : key_sheet_->key().has_value());
  key_cancel_->set_enabled(mode != Mode::browse || key_sheet_->key().has_value());
  key_warning_->set_text(key_sheet_->warning());
  key_replace_->set_visible(mode == Mode::conflict);
}

void MainWindow::edit_key_row(int row) {
  const auto i = static_cast<std::size_t>(std::max(0, row));
  if (row < 0 || i >= key_rows_.size()) return;
  key_sheet_->begin_edit(key_rows_[i].first, key_rows_[i].second);
  refresh_key_sheet();
}

void MainWindow::issue_key_commands(const std::vector<std::string>& lines) {
  for (const std::string& line : lines) run_line(line);
  refresh_key_sheet();
  refresh_keys();
}

void MainWindow::load_key_file() {
  const auto path = window_.open_file_dialog("Load key bindings", "Key-binding files", "*");
  if (!path) return;
  auto text = ctx_.core.files().read(platform::Path(*path));
  if (!text) {
    set_message(StatusKind::error, diagnostic_line(StatusKind::error, text.error().text));
    return;
  }
  const LoadedBindings loaded = load_file_commands(ctx_.editor().macros, text.value(), *path);
  for (const diag::Diagnostic& d : loaded.diagnostics) ctx_.core.dispatcher().report(d);
  issue_key_commands(loaded.commands);
}

bool MainWindow::key_sheet_shortcut(int key, std::uint8_t modifiers) {
  if (!key_card_ || !key_sheet_ || !keys_sheet_open()) return false;
  if (key_sheet_->mode() == KeyBindingSheet::Mode::capturing) {
    if (const auto event = key_event_for(key, modifiers)) {
      (void)key_sheet_->capture(*event, kMacos, ctx_.editor().macros);
      refresh_key_sheet();
      if (key_sheet_->mode() != KeyBindingSheet::Mode::capturing) key_command_->focus();
    }
    return true;  // the sheet holds the keyboard while it captures
  }
  if (key == raven_key::kEscape && modifiers == 0) {
    if (key_sheet_->mode() != KeyBindingSheet::Mode::browse) {
      key_sheet_->cancel();
      refresh_key_sheet();
    } else {
      run_line("panel keys hide");
    }
    return true;
  }
  return false;
}

}  // namespace hefesto::shell
