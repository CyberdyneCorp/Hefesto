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
// M2 (DESIGN boards docs/design/mockups/M2-*.dc.html): the live tool rail
// (Magic's tools plus the Paint, Label and Measure modes), canvas buttons
// and keys through the macro tables (InputRouter), undo/redo in the top bar,
// Tab completion in the command bar, multi-line results summarized in the
// status bar, the selection inspector, paint layer and lock toggles in the
// palette, a Feedback tab, cmd::PromptHost questions as dialogs, and the
// `panel` command that collapses the Layers, Inspector and Console panels.
// M2 round 3: popovers through Raven menus (command history and
// completion, undo history, layer row and orientation menus), clickable
// Feedback rows, the "undo off" chip and "Undid …" status, a Keys tab (the
// key-binding sheet), the color window screen in the right dock, docking
// the side panels left or right (`panel … dock`), F6 focus cycling, and the
// text-field key fixes (TextKeyFilter).
//
// M3 (DESIGN boards docs/design/mockups/M3-DRC-Status and DRC-Browser): the
// live DRC chip with its menu and the status-bar DRC segment (drc::DrcEvents
// and the root cell's error-tile count), the DRC results browser and its
// error-area inspector (`panel drc`), the `drc why` popover at the box, and
// the job-completion notice and Dock progress of the background checker.
//
// Every UI action runs a core command through the Dispatcher (origin `gui`),
// so it is logged and replayable; the window refreshes after every command,
// whatever its origin. Controls of later milestones are present but
// disabled, with a tooltip naming the milestone. Raven-free: built from the
// raven_ui wrappers.

#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "hefesto/cmd/prompt.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/shell/editing_models.hpp"
#include "hefesto/shell/command_bar_model.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/options.hpp"
#include "hefesto/shell/view_commands.hpp"
#include "color_panel.hpp"
#include "drc_panel.hpp"
#include "hefesto/shell/drc_link.hpp"
#include "hefesto/shell/drc_models.hpp"
#include "hefesto/shell/screen_models.hpp"
#include "layout_canvas.hpp"
#include "hefesto/shell/menu_bar_model.hpp"
#include "hefesto/shell/pcell_dialog_model.hpp"
#include "hefesto/shell/script_consent_model.hpp"
#include "pcell_dialog.hpp"
#include "console_panel.hpp"
#include "hefesto/shell/console_session.hpp"
#include "hefesto/shell/file_picker_host.hpp"
#include "hefesto/shell/tcl_console.hpp"
#include "manager_panels.hpp"
#include "utility_windows.hpp"
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
  /// A line typed in the command bar: history-expanded, then run_line.
  void run_typed_line(const std::string& text);
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

  /// After the background DRC pump ran BackgroundHost::service() (UI
  /// thread, between commands): redraws the committed squares.
  void on_drc_service(std::size_t committed);

  /// M4 Tcl console (m4_managers_window.cpp, SHELL-B): attaches the console
  /// and the manager procedures to the project's interpreter (startup's
  /// app::StartupEnvironment::configure_interpreter).
  void attach_interpreter(script::Interpreter& interp);

  /// M4 (m4_window.cpp): the desktop's script::ConsentHost asks through
  /// this window (board M4-Script-Consent). `host` must outlive the window.
  void set_consent_host(DesktopConsentHost* host);
  /// The consent prompt as a modal dialog; Escape answers Don't run. In a
  /// scripted capture the dialog is shown without waiting and the scripts
  /// are declined (nothing runs without an answer).
  script::ConsentDecision ask_consent(const ConsentPromptView& view);

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
  void build_tool_rail(ui::Box& middle);
  void rail_click(std::string_view icon);
  void refresh_rail();
  void add_undo_buttons(ui::Box& row);
  /// cmd::PromptHost: a dialog (modal, or shown without waiting in a
  /// scripted capture, which then takes the default answer).
  std::size_t ask(std::string_view question, std::span<const std::string> answers, std::size_t default_answer);
  [[nodiscard]] std::optional<std::string> complete(const std::string& text);
  void show_prompt(const std::string& text);
  bool set_panel(PanelId panel, std::optional<bool> show);
  bool dock_panel(PanelId panel, DockSide side);
  void reset_layout();
  /// Places the side panels around the canvas in the middle row.
  void arrange_middle();
  /// Opens the shared popover menu at window point (x, y).
  void open_menu(const std::string& title, std::vector<MenuEntry> entries, float x, float y);
  /// The window point above the command bar where its popovers open.
  [[nodiscard]] std::pair<float, float> above_command_bar(std::size_t rows) const;
  void show_history();
  void show_undo_history(float x, float y);
  /// F6 / Shift-F6 and ^R in the command bar (the window's key hook).
  bool on_shortcut(int key, std::uint8_t modifiers);
  void cycle_focus(bool backward);
  void refresh_keys();
  void refresh_color_panel();
  /// The right dock shows the PDK manager, else the color window, else the Inspector.
  void update_right_dock();
  [[nodiscard]] bool panel_shown(PanelId panel) const;
  std::optional<std::string> set_paint_layer(std::optional<std::string> name);
  void clear_problems();
  void refresh_feedback();
  void build_command_bar();
  void build_status_bar();
  // M3 DRC (drc_window.cpp)
  void build_drc_chip(ui::Box& row);  // into top_groups_[1] (the chips)
  void open_drc_menu(float x, float y);
  [[nodiscard]] DrcStatusInput drc_status_input(const db::CellDef* root) const;
  [[nodiscard]] std::size_t count_root_errors(const db::CellDef* root) const;
  /// The chip, status bar, Dock progress and DRC panel from the session.
  void refresh_drc();
  void refresh_drc_panel(const db::CellDef* root);
  /// Rebuilds the browser's list: DRCWhyAll (`drc listall why`) over the root.
  void rebuild_drc_list(const db::CellDef& root);
  void note_drc_command(const cmd::Invocation& invocation, const cmd::CommandResult& result);
  void show_why_popover();
  void post_drc_job_notice(double seconds);
  void register_view_commands();
  // M4 screens (m4_window.cpp; SHELL-A, openspec/changes/m4-scripting tasks 10.1-10.3)
  void build_m4();      // menu bar, script events, PCell dialog, declined banner
  void teardown_m4();   // first thing of the destructor
  void refresh_m4();    // after commands and between frames
  void refresh_menu_bar();
  void choose_menu_item(std::size_t menu, std::size_t entry);
  void drain_script_events();
  [[nodiscard]] bool scripts_declined() const;
  [[nodiscard]] PlaceDeviceView place_device_state(std::string_view filter = {}) const;
  void open_place_device();
  void refresh_declined();
  void review_scripts();
  /// Runs an M4 screen's Tcl script (gui_script.hpp) with the Console echo.
  cmd::CommandResult run_script_action(const std::string& script, cmd::Origin origin = cmd::Origin::gui);
  // M4 managers and Tcl console (m4_managers_window.cpp; SHELL-B, tasks 10.4-10.5)
  void build_m4b_console();  // in build_center: the Console tab's page
  void build_m4b();          // managers, utility windows, Interrupt card, FilePicker host
  void teardown_m4b();
  void refresh_m4b();        // between frames
  bool m4b_shortcut(int key, std::uint8_t modifiers);
  /// A typed command-bar line runs in the project's interpreter through the
  /// console (one interpreter, one transcript); false without an interpreter.
  bool run_console_line(const std::string& line);
  void note_m4b_command(const cmd::Invocation& invocation, const cmd::CommandResult& result);
  void open_reorder();
  /// The left dock's tabs: Layers, Cells, Libraries.
  [[nodiscard]] ui::Widget& left_dock();
  void on_command(const cmd::Invocation& invocation, const cmd::CommandResult& result);
  /// --pinch: injects the trackpad pinch and prints the zoom before and after.
  void poll_background();
  void run_pending_pinch();
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
  struct RailButton {
    ToolButton tool;
    ui::Painted* widget = nullptr;  // owned by painted_
  };
  std::vector<RailButton> rail_;
  ui::Painted* undo_button_ = nullptr;  // owned by painted_
  ui::Painted* redo_button_ = nullptr;  // owned by painted_
  UndoAffordance undo_state_;
  ui::Painted* clear_button_ = nullptr;  // owned by painted_
  ui::Painted* undo_off_chip_ = nullptr;  // owned by painted_: "▲ undo off" while recording is off
  ui::PopupMenu menu_;                    // the shared popover (one open at a time)
  std::vector<MenuEntry> menu_entries_;   // what menu_ shows
  FocusRegion focus_region_ = FocusRegion::canvas;
  ui::Box middle_ = ui::Box::horizontal();  // rail | docks and canvas
  ui::Box center_ = ui::Box::vertical();    // canvas over the console tabs
  DockSide layers_side_ = DockSide::left;
  DockSide inspector_side_ = DockSide::right;
  class DialogPromptHost;
  std::unique_ptr<cmd::PromptHost> prompt_host_;
  bool capture_mode_ = false;  // --capture: dialogs do not wait
  bool layers_visible_ = true;
  bool inspector_visible_ = true;
  bool result_has_more_ = false;  // the last result had more lines than the status bar shows
  bool canvas_dirty_ = false;     // a `*bypass` command (motion): refresh the canvas and status only
  const db::CellDef* shown_root_ = nullptr;  // problem count resets with a new document (compared only)
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
  std::unique_ptr<ColorPanel> color_;
  bool color_visible_ = false;  // a color window is open
  ui::Tabs console_tabs_ = ui::Tabs::create();
  ui::ListPanel console_ = ui::ListPanel::create();  // command echo and full results
  ui::ListPanel diagnostics_ = ui::ListPanel::create();
  ui::ListPanel feedback_ = ui::ListPanel::create();
  ui::ListPanel keys_ = ui::ListPanel::create();  // the key-binding sheet (Keys tab)
  std::vector<std::string> key_edits_;            // per Keys row: its `macro …` line (empty: a heading)
  std::size_t keys_signature_ = 0;                // bindings shown (rebuilt when it changes)
  bool keys_shown_ = false;                       // `panel keys show`
  std::vector<FeedbackRow> feedback_rows_;
  bool console_visible_ = false;
  // Diagnostics panel lines, problem counts and status messages.
  ProblemSink problems_{[this](StatusKind kind, const std::string& line, const std::string& status) {
    on_problem(kind, line, status);
  }};
  std::unique_ptr<ui::CommandBar> command_bar_;

  // M3 DRC.
  std::unique_ptr<DrcLink> drc_link_;
  std::unique_ptr<DrcPanel> drc_panel_;
  ui::Painted* drc_chip_ = nullptr;  // owned by painted_
  ui::Toast toast_ = ui::Toast::create();
  DrcStatusView drc_view_;
  bool drc_visible_ = false;
  bool drc_progress_ = false;           // the Dock shows the indeterminate progress
  std::size_t drc_errors_ = 0;          // the root's error tiles (read while idle)
  std::uint64_t drc_serial_ = ~std::uint64_t{0};  // DrcLink::serial() last applied
  std::optional<DrcFindReport> last_find_;  // the last `drc find` of the shown root
  const db::CellDef* find_root_ = nullptr;  // the root last_find_ belongs to (compared only)
  std::optional<db::Rect> find_box_;        // where last_find_ left the box (the canvas tag shows while it stays)
  std::optional<DrcWhyPopover> pending_why_;  // a `drc why` to show at the box
  bool why_open_ = false;
  bool rebuild_requested_ = false;      // a `drc catchup` ran: rebuild the list even with DRC off
  bool drc_rows_changed_ = true;
  const db::CellDef* list_root_ = nullptr;  // compared, never dereferenced
  DrcLink::Stamp list_stamp_;

  // M4 screens (SHELL-A).
  std::unique_ptr<ui::MenuBar> menu_bar_;
  MenuBarView menu_view_;
  std::size_t menu_signature_ = 0;
  bool menu_built_ = false;
  bool menu_native_ = false;  // the system shows the bar (macOS)
  ScriptEventCollector script_events_;
  std::unique_ptr<PCellDialogWindow> pcell_dialog_;
  ui::PopupMenu place_menu_;
  std::vector<PlaceDeviceEntry> place_entries_;
  std::string recent_device_;  // the last Place device script (listed last)
  DesktopConsentHost* consent_host_ = nullptr;
  ui::Box declined_bar_ = ui::Box::horizontal();  // banner over the canvas while scripts are declined
  std::optional<ui::Label> declined_text_;
  std::unique_ptr<ui::Button> review_button_;
  bool declined_shown_ = false;

  // M4 managers and Tcl console (SHELL-B).
  std::unique_ptr<TclConsoleSession> console_session_;
  std::unique_ptr<TclConsole> tcl_console_;
  std::unique_ptr<ConsolePanel> console_panel_;
  std::unique_ptr<ManagerPanels> managers_;
  std::unique_ptr<TextHelperWindow> text_helper_;
  std::unique_ptr<ReorderWindow> reorder_;
  std::unique_ptr<DesktopFilePicker> file_picker_;
  ui::Tabs left_tabs_ = ui::Tabs::create();  // Layers | Cells | Libraries
  bool tech_shown_ = false;          // the technology manager in the right dock (`techmanager create`)
  bool select_tag_pending_ = false;  // a `select` just ran: its tag refreshes, never opens, the text helper

  CoreCommandSink core_sink_;
  CommandBarModel command_model_{core_sink_};
  HeaderText header_;
  std::string message_;  // status-bar message, always "<shape> <text>"
  StatusKind message_kind_ = StatusKind::info;
  std::string pending_run_;          // run_when_ready
  std::optional<PinchOption> pending_pinch_;  // --pinch, after pending_run_
  UndoMark undo_mark_;                        // the undo log after the last command
  std::optional<std::string> undo_message_;  // "Undid … · N more steps", not shown yet
  bool in_command_ = false;          // run_line is executing a command line
  std::string command_problem_;      // first warning raised by that command line
  StatusKind command_problem_kind_ = StatusKind::warning;
  bool reveal_diagnostics_ = false;  // an error outside the command bar: open the Diagnostics tab
  const tech::Technology* styled_tech_ = nullptr;  // technology whose display styles are loaded
  bool dirty_ = true;
};

}  // namespace hefesto::shell
