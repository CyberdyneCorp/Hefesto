// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Schematic tab's Raven view (openspec/changes/m8-schematic design D15,
// task 13.2; board docs/design/mockups/Schematic.dc.html; layout recorded in
// docs/design/m8-ui-conformance.md):
//
//   Symbols panel | breadcrumb / canvas / netlist bar / messages | Properties
//
// - the canvas draws the session's active document from the canvas view
//   model (schematic_canvas.hpp) with its symbols resolved off the UI thread
//   (schematic_symbols.hpp: placeholders until the worker finishes, then a
//   repaint through ui::post_task) and the nets and instances of
//   `schematic hilight list` highlighted in their colour;
// - a Raven viewport under the painted canvas takes the input
//   (schematic_input.hpp): wheel, pinch and drag pan and zoom, a click
//   selects, and with the key focus (a click focuses it) keys run the
//   `schematic` bindings, which live in the editor's macro table
//   `schematic` (the core's `macro schematic ...` edits them); Copy and
//   Paste link the schematic clipboard with the system clipboard
//   (schematic_clipboard.hpp);
// - the symbol library browser, the properties panel, the netlist action
//   with the M4/M5 Interrupt card and the messages panel show the view
//   models of schematic_library.hpp and schematic_panels.hpp; the
//   properties panel is the field view (schematic-editing "Property
//   editing"): a click on a row puts its key and value in the field editor
//   under the rows, Return or Set runs the one `schematic setprop` line of
//   the change (an empty value removes the attribute, a new key appends it,
//   "Apply to all selected" runs `setprop selection`).
//
// Every action is a command line run through the window (ctx.run: echoed,
// logged, replayable). The view observes the dispatcher (cmd::
// CommandObserver) for the results of `schematic` commands (messages, the
// netlist run) and reads `hilight list`, `hierarchy path` and `getprop`
// with origin `internal` (not replayed) after a command changed the session.
//
// Owner: SHELL.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/shell/schematic_canvas.hpp"
#include "hefesto/shell/schematic_clipboard.hpp"
#include "hefesto/shell/schematic_input.hpp"
#include "hefesto/shell/schematic_library.hpp"
#include "hefesto/shell/schematic_panels.hpp"
#include "hefesto/shell/schematic_symbols.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"

namespace hefesto::shell {

class SchematicView final : public cmd::CommandObserver {
 public:
  explicit SchematicView(ScreenContext& ctx);
  SchematicView(const SchematicView&) = delete;
  SchematicView& operator=(const SchematicView&) = delete;
  ~SchematicView() override;

  [[nodiscard]] ui::Widget& widget() noexcept { return root_; }
  /// Rebuilds the scene and the panels from the session's active document
  /// (after a command, when the tab is shown).
  void refresh();
  /// The key focus to the canvas, so the `schematic` bindings get the keys
  /// (a click on the canvas focuses it too). For the window's F6 cycle and
  /// the tab switch.
  void focus() { input_viewport_->focus(); }
  /// The netlist action (run_netlist): the netlist bar's button, the `n`
  /// binding and the top bar's "Netlist" button (main_window.cpp).
  void start_netlist();
  /// The top bar's "Netlist" button (netlist_button): enabled while a
  /// schematic is open and no run is going on, with the matching tooltip.
  [[nodiscard]] NetlistButtonView netlist_button_view() const;
  /// Called when a netlist run starts or ends (the top bar's button repaints).
  void set_on_netlist_change(std::function<void()> changed) { netlist_changed_ = std::move(changed); }
  /// What the view shows, for scripted checks after an --input script
  /// (main_window.cpp prints it): the breadcrumb, the instances drawn and
  /// the symbols still pending and the canvas size in pixels,
  /// "ihp_style_tb.sch, 5 instances, 0 pending, canvas 980 x 520".
  [[nodiscard]] std::string status_line() const;
  /// The rest of the view's state for the CI captures of task 13.2: the
  /// instance shown in the properties panel and its rows, the highlighted
  /// nets and instances, the zoom and whether the user moved the view, the
  /// netlist run and whether its Interrupt card was drawn, and where the
  /// canvas and the property rows are (window units, for scripted clicks):
  /// "properties R1 2 rows name=R1 value=1k, highlights 1 nets 1 instances,
  /// zoom 0.82 user view, netlist done, card seen, canvas at 252,76, rows at 1032,108".
  [[nodiscard]] std::string state_line() const;

  /// cmd::CommandObserver: the results of `schematic` commands.
  void on_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) override;

 private:
  struct CrumbSpan {
    float x0 = 0.0F;
    float x1 = 0.0F;
  };

  void build_library();
  void build_centre();
  void build_properties();
  void build_netlist_bar();
  void build_messages();
  void build_field_editor();

  void show_document(schem::Session& session);
  void show_nothing();
  void rebuild_scene();
  void reload_library(const schem::Session& session);
  void read_session_state();
  [[nodiscard]] std::optional<std::string> read(const std::string& line);
  void symbols_ready();
  void note_symbol_changes(const cmd::Invocation& invocation, const cmd::CommandResult& result);
  void invalidate_all();

  bool canvas_input(const CanvasInput& input);
  void apply(const SchematicInputResult& result);
  void run(const std::string& line);
  void interrupt();
  void copy_selection();
  void paste(schem::Point at);
  void tick_netlist();
  void place(std::size_t row);
  void edit_property(std::size_t row);
  void commit_field();
  void show_field_editor();
  void follow_selection(const cmd::Invocation& invocation, const cmd::CommandResult& result);
  [[nodiscard]] std::optional<std::size_t> instance_index(const std::string& name) const;
  void click_crumb(float x);
  void click_netlist_bar(float x);

  void paint_canvas(ui::Painter& p, float w, float h, const ThemeTokens& t);
  void paint_items(ui::Painter& p, const ThemeTokens& t) const;
  void paint_crumbs(ui::Painter& p, float w, float h, const ThemeTokens& t);
  void paint_netlist_bar(ui::Painter& p, float w, float h, const ThemeTokens& t);
  void paint_messages(ui::Painter& p, float w, float h, const ThemeTokens& t) const;
  void paint_library_rows(ui::Painter& p, float w, const ThemeTokens& t) const;
  void paint_property_rows(ui::Painter& p, float w, float h, const ThemeTokens& t) const;
  [[nodiscard]] Rgba instance_color(const ThemeTokens& t, std::size_t index) const;
  [[nodiscard]] Rgba wire_color(const ThemeTokens& t, std::size_t index) const;

  ScreenContext& ctx_;
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);  // guards posted tasks
  ui::Box root_ = ui::Box::horizontal();

  // Left: the symbol library browser.
  ui::Panel library_panel_;
  ui::Box library_column_ = ui::Box::vertical();
  std::unique_ptr<ui::Painted> library_title_;
  std::unique_ptr<ui::TextInput> search_;
  ui::ScrollBox library_scroll_ = ui::ScrollBox::create();
  std::unique_ptr<ui::Painted> library_rows_;

  // Centre: breadcrumb, canvas (input viewport under the painted scene),
  // netlist bar, messages.
  ui::Box centre_ = ui::Box::vertical();
  std::unique_ptr<ui::Painted> crumbs_;
  ui::Overlay stage_ = ui::Overlay::create();
  std::unique_ptr<ui::Viewport> input_viewport_;
  std::unique_ptr<ui::Painted> canvas_;
  std::unique_ptr<ui::Painted> netlist_bar_;
  std::unique_ptr<ui::Painted> messages_view_;

  // Right: the properties panel.
  ui::Panel properties_panel_;
  ui::Box properties_column_ = ui::Box::vertical();
  std::unique_ptr<ui::Painted> properties_title_;
  ui::ScrollBox properties_scroll_ = ui::ScrollBox::create();
  std::unique_ptr<ui::Painted> property_rows_;
  // The field editor under the rows ("Field view").
  ui::Box field_box_ = ui::Box::vertical();
  std::unique_ptr<ui::TextInput> field_key_;
  std::unique_ptr<ui::TextInput> field_value_;
  std::unique_ptr<ui::CheckBox> field_all_;
  std::unique_ptr<ui::Button> field_set_;
  bool field_all_selected_ = false;
  std::vector<std::unique_ptr<ui::Painted>> separators_;

  // View models.
  SchematicScene scene_;
  SchematicViewport viewport_;
  SchematicCanvasInput input_;
  SchematicSymbolResolver resolver_;
  HierarchyBreadcrumb breadcrumb_;
  NetHighlight highlight_;
  PropertiesPanel properties_;
  MessagesPanel messages_;
  NetlistAction netlist_;
  SymbolLibraryBrowser library_;
  std::unique_ptr<ClipboardHost> clipboard_host_;  // Raven's system clipboard
  SchematicClipboardLink clipboard_;
  platform::Path install_root_;
  std::vector<platform::Path> library_dirs_;  // the folders the browser lists
  bool library_loaded_ = false;

  std::vector<int> wire_colours_;      // per wire: the highlight colour, -1 when not highlighted
  std::vector<int> instance_colours_;  // per instance
  std::vector<CrumbSpan> crumb_spans_;
  float netlist_button_x1_ = 0.0F;
  float interrupt_x0_ = -1.0F;
  schem::DocumentId shown_id_ = 0;
  bool has_document_ = false;
  bool session_stale_ = true;  // a command ran: read the highlights, path and properties again
  bool refreshing_ = false;
  bool card_seen_ = false;  // the netlist bar drew the Interrupt card (state_line)
  std::function<void()> netlist_changed_;
};

}  // namespace hefesto::shell
