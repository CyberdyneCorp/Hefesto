// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M4 utility windows of the managers board (docs/design/mockups/
// M4-Managers.dc.html tiles 4 and 6, approved 2026-11-11; task 10.4):
//
//   - TextHelperWindow (Edit › Text…, `magic::update_texthelper`): a
//     non-modal window with Magic's texthelper fields, the 3×3 justification
//     grid plus center / default, and the preview of what Apply or Okay
//     issues (text_helper_view.hpp).
//   - ReorderWindow (Layers › Reorder layers…): the toolbar order with
//     search, Move up / Move down, Group with the row above, Delete, the
//     presets of `<tech>_toolbars.tcl` (read as data), Load, Save and
//     Confirm (toolbar_views.hpp).
//
// Every action that changes the layout is a logged command run through
// ScreenContext::run. Raven-free: built from the raven_ui wrappers.
//
// Owner: SHELL (SHELL-B).

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/gui/toolbar_presets.hpp"
#include "hefesto/shell/text_helper_view.hpp"
#include "hefesto/shell/toolbar_views.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"

namespace hefesto::shell {

class TextHelperWindow {
 public:
  explicit TextHelperWindow(ScreenContext& ctx);
  ~TextHelperWindow();
  TextHelperWindow(const TextHelperWindow&) = delete;
  TextHelperWindow& operator=(const TextHelperWindow&) = delete;

  /// Opens (or raises) the window and reads the selection (update_texthelper).
  void show();
  /// The selection changed (the `select` tag): re-read while open.
  void update();
  [[nodiscard]] bool open() const noexcept { return window_ != nullptr; }
  /// The open window (--capture-window); nullptr when closed.
  [[nodiscard]] const ui::Window* window() const noexcept { return window_.get(); }
  void close();

 private:
  void build();
  void read_selection();
  void fields_to_model();
  void model_to_fields();
  void apply();

  ScreenContext& ctx_;
  TextHelperView view_;
  std::unique_ptr<ui::Window> window_;
  ui::Box root_ = ui::Box::vertical();
  std::optional<ui::Label> title_;
  std::optional<ui::Label> preview_;
  std::unique_ptr<ui::TextInput> text_;
  std::unique_ptr<ui::ComboBox> font_;
  std::vector<std::string> fonts_;
  std::unique_ptr<ui::TextInput> size_;
  std::unique_ptr<ui::TextInput> rotation_;
  std::unique_ptr<ui::TextInput> offset_;
  std::unique_ptr<ui::TextInput> layer_;
  std::unique_ptr<ui::CheckBox> default_layer_;
  std::unique_ptr<ui::CheckBox> sticky_;
  std::unique_ptr<ui::CheckBox> port_;
  std::unique_ptr<ui::TextInput> port_index_;
  std::vector<std::unique_ptr<ui::Button>> buttons_;
  bool port_exists_ = false;
  std::string units_ = "lambda";
};

class ReorderWindow {
 public:
  /// `confirm` applies an order to the palette in memory.
  ReorderWindow(ScreenContext& ctx, std::function<void(std::vector<std::string>)> confirm);
  ~ReorderWindow();
  ReorderWindow(const ReorderWindow&) = delete;
  ReorderWindow& operator=(const ReorderWindow&) = delete;

  /// Opens the dialog on `toolbar` (gui::build_toolbar's order).
  void show(const gui::ToolbarModel& toolbar);
  [[nodiscard]] bool open() const noexcept { return window_ != nullptr; }
  /// The open window (--capture-window); nullptr when closed.
  [[nodiscard]] const ui::Window* window() const noexcept { return window_.get(); }
  void close();

 private:
  void build();
  void reset_rows();
  void load_preset();
  void save_preset();
  void move_selected(int delta);
  [[nodiscard]] std::optional<std::size_t> selected_layer() const;

  ScreenContext& ctx_;
  std::function<void(std::vector<std::string>)> confirm_;
  std::optional<ReorderModel> model_;
  std::string presets_path_;  // `<tech>_toolbars.tcl` in the working directory
  std::string search_;
  std::vector<std::size_t> rows_;  // filtered: layer index per table row
  std::size_t preset_ = 0;
  std::unique_ptr<ui::Window> window_;
  ui::Box root_ = ui::Box::vertical();
  std::unique_ptr<ui::TextInput> search_input_;
  std::unique_ptr<ui::ComboBox> presets_;
  std::unique_ptr<ui::Table> table_;
  std::optional<ui::Label> note_;
  std::vector<std::unique_ptr<ui::Button>> buttons_;
};

}  // namespace hefesto::shell
