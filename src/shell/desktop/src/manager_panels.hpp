// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M4 manager panels (docs/design/mockups/M4-Managers.dc.html, approved
// 2026-11-11; task 10.4): the Cells and Libraries tabs of the left dock and
// the technology manager of the right dock. Each is a virtual Raven Table
// (rows asked for on draw; keyboard, sorting and screen readers from Raven)
// or painted fields over the views of manager_views.hpp, and every button
// runs the command line Magic's manager issues through ScreenContext::run
// (logged, replayable). The panels refresh after the commands Magic's
// managers tag (`load`, `getcell`, `select`; `addpath`, `path`, `save`,
// `writeall`; `tech`, `cif`, `extract`, `drc`, `scalegrid`), and when the
// `cellmanager`, `libmanager` or `techmanager` commands run.
// Raven-free: built from the raven_ui wrappers.
//
// Owner: SHELL (SHELL-B).

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/shell/manager_views.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"

namespace hefesto::shell {

/// Runs `body` with a command context over the shell's session (for the
/// gui:: model functions, which run engine queries nested in it). The
/// queries' diagnostics are dropped: a manager refresh reports nothing.
void with_command_context(CoreSession& core, const std::function<void(cmd::CommandContext&)>& body);
/// One engine query (`port last`, `setlabel text`, ...) nested in such a
/// context: its value as text, empty on failure. Not logged: a read only.
[[nodiscard]] std::string engine_query(CoreSession& core, const std::vector<std::string>& words);

class ManagerPanels {
 public:
  explicit ManagerPanels(ScreenContext& ctx);
  ~ManagerPanels();
  ManagerPanels(const ManagerPanels&) = delete;
  ManagerPanels& operator=(const ManagerPanels&) = delete;

  [[nodiscard]] ui::Widget& cells_widget() noexcept { return cells_root_; }
  [[nodiscard]] ui::Widget& libraries_widget() noexcept { return libraries_root_; }
  [[nodiscard]] ui::Widget& tech_widget() noexcept { return tech_root_; }

  /// A command ran (any origin): marks what Magic's manager tags refresh;
  /// `select` highlights the selected instance (mgrselect).
  void note_command(const cmd::Invocation& invocation, const cmd::CommandResult& result);
  /// Rebuilds what is marked (call between frames, outside callbacks).
  void refresh();
  /// Forces every panel to rebuild on the next refresh.
  void invalidate_all();
  void repaint();
  /// The technology manager is shown in the right dock (`techmanager`).
  [[nodiscard]] bool tech_requested() const noexcept { return tech_requested_; }
  void set_tech_requested(bool shown) noexcept { tech_requested_ = shown; }

 private:
  struct Dirty {
    bool cells = true;
    bool libraries = true;
    bool tech = true;
  };
  void build_cells();
  void build_libraries();
  void build_tech();
  ui::Box button_row(const std::vector<std::pair<std::string, std::function<void()>>>& buttons);
  std::unique_ptr<ui::Painted> make_button(std::string label, std::string tooltip, std::function<bool()> enabled,
                                           std::function<void(float, float)> click);
  std::unique_ptr<ui::Painted> title_row(std::string_view text);
  void run_lines(const std::vector<std::string>& lines);
  void refresh_cells();
  void refresh_libraries();
  void refresh_tech();
  void highlight_selection(const cmd::CommandResult& result);
  /// Hands the Cells view's visible rows and selection to the table (scrolled to it).
  void show_cells_rows();
  void open_tech_menu(const ui::Widget& anchor);
  void open_style_menu(TechStyle style, const ui::Widget& anchor);
  void apply_units();

  ScreenContext& ctx_;
  Dirty dirty_;
  bool tech_requested_ = false;
  bool library_filter_ = true;

  CellsView cells_;
  LibrariesView libraries_;
  TechView tech_;

  ui::Box cells_root_ = ui::Box::vertical();
  ui::Box libraries_root_ = ui::Box::vertical();
  ui::Box tech_root_ = ui::Box::vertical();
  std::unique_ptr<ui::Table> cells_table_;
  std::unique_ptr<ui::Table> libraries_table_;
  std::unique_ptr<ui::Painted> cells_target_;
  std::unique_ptr<ui::Painted> libraries_filter_;
  std::unique_ptr<ui::Painted> tech_body_;
  std::unique_ptr<ui::TextInput> units_numerator_;
  std::unique_ptr<ui::TextInput> units_denominator_;
  std::vector<std::unique_ptr<ui::Painted>> painted_;
};

}  // namespace hefesto::shell
