// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The main window "Hefesto": the Desktop-Layout screen of the forge design
// system (docs/design/mockups/Desktop-Layout.dc.html; capability
// hefesto-design-system, hefesto-app-ux "Main window layout"):
//
//   top bar 48 px     app mark, cell name + PDK / version / magic-compat,
//                     Layout / Schematic / 3D / Waveforms, DRC / LVS chips,
//                     Open, PDKs, Extract, Simulate, Ask agent
//   tool rail 52 px | left panel 240 px (Layers, Hierarchy) | canvas with
//   rulers, zoom control and minimap (+ Console / Diagnostics, collapsible)
//   | Inspector 284 px (or the PDK manager)
//   command bar 40 px  "%" prompt, input with history, hints
//   status bar 26 px   cursor, box, edit cell, snap/units, message, DRC
//
// Every UI action runs a core command through the Dispatcher (origin `gui`),
// so it is logged and replayable; the window refreshes after every command,
// whatever its origin. Controls of later milestones are present but
// disabled, with a tooltip naming the milestone. Raven-free: built from the
// raven_ui wrappers.

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/shell/command_bar_model.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/options.hpp"
#include "hefesto/shell/view_commands.hpp"
#include "layout_canvas.hpp"
#include "pdk_panel.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"
#include "side_panels.hpp"

namespace hefesto::tech {
class Technology;
}

namespace hefesto::shell {

class MainWindow {
 public:
  /// `queue_mutex` is the lock shared with Raven around the Vulkan queue.
  /// `core` must outlive the window.
  MainWindow(ui::Runtime& runtime, const ShellOptions& options, std::mutex& queue_mutex, CoreSession& core);
  ~MainWindow();
  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;

  /// Shows rectangles that are not a cell (the benchmark's synthetic layout).
  void show_layout(const std::vector<render::LayeredRect>& rects);
  /// Runs one command line as typed in the command bar (Console echo, status).
  void run_line(const std::string& line);
  /// Runs `line` like run_line once the canvas has its size (the first
  /// frames), so view commands (`zoom`, `view`) act on the real viewport
  /// instead of being replaced by the initial fit (--run).
  void run_when_ready(std::string line) { pending_run_ = std::move(line); }
  /// Applies refreshes requested since the last frame (call once per frame,
  /// on the UI thread, outside any widget callback).
  void on_frame();

  [[nodiscard]] bool open() const { return window_.valid(); }
  [[nodiscard]] ui::FrameStats frame_stats() const { return window_.frame_stats(); }
  [[nodiscard]] std::size_t canvas_instances() const { return canvas_->instance_count(); }
  bool capture(const std::string& path) const { return window_.capture_ppm(path); }
  [[nodiscard]] std::string window_system() const { return window_.window_system(); }

  /// Benchmark hooks: realtime redraw, and a programmatic pan in pixels.
  void set_realtime(bool realtime) { canvas_->set_realtime(realtime); }
  void pan_pixels(double dx, double dy) { canvas_->pan_pixels(dx, dy); }

 private:
  void build_top_bar();
  struct TopButton {
    ui::Widget* widget = nullptr;  // owned by painted_
    float width = 0.0F;
  };
  TopButton add_top_button(ui::Box& row, std::string_view label, std::string_view icon, paint::ButtonStyle style,
                           std::string_view tooltip, std::function<void()> click);
  /// Hides top-bar groups that do not fit the window's width (on resize).
  void fit_top_bar();
  void build_center();
  void build_command_bar();
  void build_status_bar();
  void register_view_commands();
  void on_command(const cmd::Invocation& invocation, const cmd::CommandResult& result);
  void on_problem(StatusKind kind, const std::string& panel_line, const std::string& status);
  void set_message(StatusKind kind, std::string text);
  /// Tells a screen reader of an error (assertive) or a warning (polite).
  static void announce(StatusKind kind, const std::string& status);
  [[nodiscard]] StatusBarText status_text();
  void update_status_accessibility();
  [[nodiscard]] std::size_t diagnostic_columns() const;
  void open_file();
  void toggle_pdk_manager();
  void toggle_console();
  void apply_theme(ThemeMode mode);
  void sync_styles();
  void refresh_all();
  void refresh_header();
  void refresh_status();
  void append_console(const std::string& command, const std::string& result);
  [[nodiscard]] ui::Panel& bar(const Rgba ThemeTokens::*token);

  ScreenContext ctx_;
  ShellOptions options_;
  ui::Window window_;
  ui::Box root_ = ui::Box::vertical();
  std::vector<std::pair<ui::Panel, const Rgba ThemeTokens::*>> bars_;  // backgrounds re-tinted on theme change
  std::vector<std::unique_ptr<ui::Painted>> painted_;                   // every fixed painted widget
  std::unique_ptr<ui::Painted> status_bar_;
  std::string status_accessible_name_;  // last name given to status_bar_
  ui::Painted* console_toggle_ = nullptr;  // owned by painted_
  struct TopBarGroup {
    float width = 0.0F;
    std::vector<ui::Widget*> widgets;  // owned by painted_
  };
  std::vector<TopBarGroup> top_groups_;  // deferred actions, chips, view tabs (hidden in this order)
  float top_full_width_ = 0.0F;          // the top bar with every group shown
  float top_items_width_ = 0.0F;         // the always-shown buttons
  std::size_t top_item_count_ = 0;
  int top_fit_width_ = -1;               // the window width fit_top_bar last applied

  std::unique_ptr<LayoutCanvas> canvas_;
  std::unique_ptr<LeftPanel> left_;
  std::unique_ptr<InspectorPanel> inspector_;
  std::unique_ptr<PdkPanel> pdk_;
  bool pdk_visible_ = false;
  ui::Tabs console_tabs_ = ui::Tabs::create();
  ui::ListPanel console_ = ui::ListPanel::create();  // command echo and full results
  ui::ListPanel diagnostics_ = ui::ListPanel::create();
  bool console_visible_ = false;
  // Diagnostics panel lines, problem counts and status messages.
  ProblemSink problems_{[this](StatusKind kind, const std::string& line, const std::string& status) {
    on_problem(kind, line, status);
  }};
  std::unique_ptr<ui::CommandBar> command_bar_;

  CoreCommandSink core_sink_;
  CommandBarModel command_model_{core_sink_};
  HeaderText header_;
  std::string message_;  // status-bar message, always "<shape> <text>"
  StatusKind message_kind_ = StatusKind::info;
  std::string pending_run_;          // run_when_ready
  bool in_command_ = false;          // run_line is executing a command line
  std::string command_problem_;      // first warning raised by that command line
  StatusKind command_problem_kind_ = StatusKind::warning;
  bool reveal_diagnostics_ = false;  // an error outside the command bar: open the Diagnostics tab
  const tech::Technology* styled_tech_ = nullptr;  // technology whose display styles are loaded
  bool dirty_ = true;
};

}  // namespace hefesto::shell
