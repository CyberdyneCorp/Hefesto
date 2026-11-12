// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PCell parameter dialog window (board M4-PCell-Dialog; model
// hefesto/shell/pcell_dialog_model.hpp): a non-modal utility window kept
// above the layout window, showing editor::EditorState::script.gencell.
// dialog. It is rebuilt from the model between frames (sync), never inside
// a widget callback, so a button whose script reopens the dialog on the new
// instance does not delete itself while it runs. Every action is a Tcl
// script given to `run` (the project's interpreter; logged commands).
// Raven-free: built from the raven_ui wrappers.
//
// Owner: SHELL (openspec/changes/m4-scripting/design.md D2).

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/pcell/framework/dialog.hpp"
#include "hefesto/shell/pcell_dialog_model.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"

namespace hefesto::shell {

class PCellDialogWindow {
 public:
  using Run = std::function<cmd::CommandResult(const std::string& script)>;

  PCellDialogWindow(ScreenContext& ctx, Run run);
  ~PCellDialogWindow();
  PCellDialogWindow(const PCellDialogWindow&) = delete;
  PCellDialogWindow& operator=(const PCellDialogWindow&) = delete;

  /// Between frames: opens, rebuilds or closes the window for the session's
  /// dialog model (closes it when the model is gone).
  void sync();
  [[nodiscard]] bool open() const noexcept { return window_ != nullptr; }
  /// The open window (--capture-window); nullptr when closed.
  [[nodiscard]] const ui::Window* window() const noexcept { return window_.get(); }

 private:
  [[nodiscard]] pcell::DialogModel* model() noexcept;
  void build(const pcell::DialogModel& model);
  void add_header(ui::Box& column, const PCellDialogView& view);
  void add_row(ui::Box& body, const PCellRow& row);
  void add_buttons(ui::Box& column, const PCellDialogView& view);
  void edit(const std::string& parameter, const std::string& value);
  /// Runs the value scripts of entries typed into but not committed.
  void commit_pending();
  void press(PCellButtonRole role, bool closes);

  ScreenContext& ctx_;
  Run run_;
  std::unique_ptr<ui::Window> window_;
  std::optional<pcell::DialogModel> shown_;  // the model the widgets show
  PCellEditState edits_;
  std::map<std::string, std::string, std::less<>> pending_;  // parameter -> typed text not committed
  bool close_requested_ = false;
  // The widgets of the current build (owned here, children of window_).
  std::vector<std::unique_ptr<ui::TextInput>> inputs_;
  std::vector<std::unique_ptr<ui::CheckBox>> checks_;
  std::vector<std::unique_ptr<ui::ComboBox>> combos_;
  std::vector<std::unique_ptr<ui::Button>> buttons_;
};

}  // namespace hefesto::shell
