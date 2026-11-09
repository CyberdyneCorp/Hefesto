// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "main_window.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include "hefesto/diag/format.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/render/scene.hpp"

namespace hefesto::shell {

namespace {

constexpr float kBarPadX = 14.0F;  // the mockup's 14 px bar padding
constexpr float kGap = 16.0F;      // the top bar's 16 px gap
constexpr std::string_view kConsoleToggleName = "Console and diagnostics";

std::string quoted(const std::string& path) {
  if (path.find_first_of(" \t\"\\{}[]$;") == std::string::npos) return path;
  std::string out = "\"";
  for (const char c : path) {
    if (c == '"' || c == '\\' || c == '$' || c == '[' || c == ']') out += '\\';
    out += c;
  }
  return out + "\"";
}

/// `$CAD_ROOT/magic/sys` (Magic's system library path for the display
/// styles) and the technology file's directory.
std::vector<platform::Path> style_library_dirs(const tech::Technology& tech) {
  std::vector<platform::Path> dirs;
  if (const char* cad_root = std::getenv("CAD_ROOT"); cad_root != nullptr && *cad_root != '\0') {
    dirs.emplace_back(platform::Path(cad_root) / "magic" / "sys");
  }
  if (!tech.file_name().empty()) dirs.push_back(platform::Path(tech.file_name()).parent_path());
  return dirs;
}

/// Forwards diagnostics raised outside a command (style loading) to the Dispatcher's sinks.
class ReportSink final : public diag::DiagnosticSink {
 public:
  explicit ReportSink(cmd::Dispatcher& dispatcher) : dispatcher_(dispatcher) {}
  void emit(const diag::Diagnostic& d) override { dispatcher_.report(d); }

 private:
  cmd::Dispatcher& dispatcher_;
};

}  // namespace

MainWindow::MainWindow(ui::Runtime& runtime, const ShellOptions& options, std::mutex& queue_mutex, CoreSession& core)
    : ctx_{runtime, core, options.theme, {}, {}},
      options_(options),
      window_("Hefesto", options.width, options.height),
      command_bar_(ui::CommandBar::create("Type a command (help)")),
      core_sink_(core.dispatcher()) {
  ctx_.run = [this](const std::string& line) { run_line(line); };
  ctx_.echo = [this](const std::string& command, const std::string& result) { append_console(command, result); };
  window_.set_min_size(1024, 640);
  ctx_.runtime.apply_theme(ctx_.mode);
  window_.set_clear_color(ctx_.tokens().surface0);
  canvas_ = std::make_unique<LayoutCanvas>(ctx_, queue_mutex, [this] { return window_.scale(); });
  canvas_->on_view_changed([this] { refresh_status(); });
  left_ = std::make_unique<LeftPanel>(ctx_);
  inspector_ = std::make_unique<InspectorPanel>(ctx_);
  pdk_ = std::make_unique<PdkPanel>(ctx_, [this] { toggle_pdk_manager(); });

  build_top_bar();
  build_center();
  build_command_bar();
  build_status_bar();
  window_.set_content(root_);
  register_view_commands();

  set_message(StatusKind::info, diagnostic_line(StatusKind::info, "ready"));
  core.dispatcher().add_sink(problems_);
  core.set_command_listener(
      [this](const cmd::Invocation& inv, const cmd::CommandResult& result) { on_command(inv, result); });
  if (core.log_warning()) core.dispatcher().report(*core.log_warning());
  pdk_visible_ = !options.show_pdk_manager;
  toggle_pdk_manager();
  console_visible_ = !options.show_console;
  toggle_console();
  refresh_all();
}

MainWindow::~MainWindow() {
  ctx_.core.set_command_listener({});
  ctx_.core.dispatcher().remove_sink(problems_);
  // First stop Raven from rendering the window: the canvas draw callback
  // uses the renderer and the view mutex on the render thread.
  window_.destroy();
}

/// Columns of a Diagnostics line: the console's width at the minimum window
/// size (the PDK manager open), in the mono font, so that a long path wraps
/// instead of being cut at the panel edge.
std::size_t MainWindow::diagnostic_columns() const {
  constexpr float kMinCenter = 1024.0F - static_cast<float>(kToolRailWidth + kLeftPanelWidth + kInspectorWidth) -
                               36.0F - 3.0F - 24.0F;  // panel edges and list padding
  const float digit = std::max(1.0F, ctx_.runtime.text_width(TextRole::mono, "0"));
  return static_cast<std::size_t>(kMinCenter / digit);
}

void MainWindow::on_problem(StatusKind kind, const std::string& panel_line, const std::string& status) {
  for (const std::string& line : wrap_columns(panel_line, diagnostic_columns(), 4)) diagnostics_.add_item(line);
  diagnostics_.scroll_to_end();
  if (status.empty()) return;
  if (in_command_) {
    // The command line's own result decides the message (run_line).
    // The first error, else the first warning, raised by the line.
    if (command_problem_.empty() || (kind == StatusKind::error && command_problem_kind_ != StatusKind::error)) {
      command_problem_ = status;
      command_problem_kind_ = kind;
    }
  } else {
    set_message(kind, status);
    announce(kind, status);
    reveal_diagnostics_ = reveal_diagnostics_ || kind == StatusKind::error;
  }
  dirty_ = true;
  window_.request_redraw();
}

void MainWindow::set_message(StatusKind kind, std::string text) {
  message_kind_ = kind;
  message_ = std::move(text);
  if (status_bar_) update_status_accessibility();
}

void MainWindow::announce(StatusKind kind, const std::string& status) {
  if (const auto a = problem_announcement(kind, status)) ui::announce(a->text, a->assertive);
}

StatusBarText MainWindow::status_text() {
  const editor::EditorState& ed = ctx_.editor();
  StatusBarInput in;
  in.cursor = canvas_->cursor();
  in.box = visible_box(ed.box, ed.box_root, ed.edit_cell);
  in.edit_cell = ed.edit_cell != nullptr ? ed.edit_cell->name() : std::string();
  in.scale = canvas_->unit_scale();
  in.snap = ed.settings.value_in(app::SettingLayer::session, "snap").value_or("lambda");
  in.message = message_;
  in.problems = problems_.tally().chip();
  return status_bar_text(in);
}

// The painted status text, for screen readers (name) and in full (tooltip).
void MainWindow::update_status_accessibility() {
  const std::string name = status_bar_accessible_name(status_text());
  if (name == status_accessible_name_) return;
  status_accessible_name_ = name;
  status_bar_->set_accessible_name(name);
  status_bar_->set_tooltip(message_ + (problems_.tally().chip() ? " — click for Diagnostics" : ""));
}

ui::Panel& MainWindow::bar(const Rgba ThemeTokens::*token) {
  bars_.emplace_back(ui::Panel::create(ctx_.tokens().*token), token);
  return bars_.back().first;
}

// --- top bar -------------------------------------------------------------------

void MainWindow::build_top_bar() {
  ui::Box row = ui::Box::horizontal();
  row.set_padding(kBarPadX, 0.0F, kBarPadX, 0.0F);
  row.set_spacing(kGap);
  // The app mark and the title block are one painted widget.
  auto mark = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    const auto size = static_cast<float>(kAppMarkSize);
    paint::app_mark(p, 0.0F, (h - size) / 2.0F, size, t);
    p.push_clip(size + 12.0F, 0.0F, w - size - 12.0F, h);
    paint::header_title(p, size + 12.0F, h, header_, t);
    p.pop_clip();
  });
  constexpr float kMarkWidth = 300.0F;
  mark->set_size(kMarkWidth, static_cast<float>(kTopBarHeight));
  mark->set_role(ui::Role::heading);
  row.add(*mark);
  // Groups that give way at narrow widths, in order (top_bar_hidden_groups).
  top_groups_.assign(3, TopBarGroup{});
  TopBarGroup& deferred = top_groups_[0];
  TopBarGroup& chips = top_groups_[1];
  TopBarGroup& view_tabs = top_groups_[2];
  top_full_width_ = 2.0F * kBarPadX + kMarkWidth + kGap;  // padding, mark, and the spacer's gap
  painted_.push_back(std::move(mark));

  std::vector<std::string_view> tabs = {"Layout"};
  for (const DeferredControl& c : deferred_view_tabs()) tabs.push_back(c.label);
  auto segmented = make_painted(ctx_, [tabs](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    paint::segmented(p, w, h, tabs, t);
  });
  const float tabs_width = paint::segmented_width(ctx_.runtime, tabs);
  segmented->set_size(tabs_width, 36.0F);
  view_tabs.width = tabs_width;
  view_tabs.widgets.push_back(segmented.get());
  segmented->set_role(ui::Role::tab_list);
  segmented->set_accessible_name("Editor view: Layout (Schematic arrives in M8, 3D in M10, Waveforms in M8)");
  segmented->set_tooltip("Layout is the M1 view; Schematic and Waveforms arrive in M8, 3D in M10");
  row.add_centered(*segmented);
  painted_.push_back(std::move(segmented));

  ui::Box spacer = ui::Box::horizontal();
  row.add(spacer, 1.0F);
  for (const StatusChip& chip : {drc_chip_m1(), lvs_chip_m1()}) {
    auto widget = make_painted(ctx_, [chip](ui::Painter& p, float w, float h, const ThemeTokens& t) {
      paint::chip(p, w, h, chip, t);
    });
    const float chip_width = paint::chip_width(ctx_.runtime, chip);
    widget->set_size(chip_width, static_cast<float>(kChipHeight));
    chips.width += chip_width;
    chips.widgets.push_back(widget.get());
    widget->set_role(ui::Role::text);
    widget->set_accessible_name(chip.label());
    widget->set_tooltip(chip.text == "DRC not run" ? "DRC arrives in M3" : "LVS arrives in M9");
    row.add_centered(*widget);
    painted_.push_back(std::move(widget));
  }
  add_top_button(row, "Open", "open", paint::ButtonStyle::secondary, "Open a .mag cell file (load)",
                 [this] { open_file(); });
  add_top_button(row, "PDKs", "pdk", paint::ButtonStyle::secondary, "PDK manager: install, enable and pin PDKs",
                 [this] { toggle_pdk_manager(); });
  for (const DeferredControl& c : deferred_actions()) {
    const bool agent = c.label == "Ask agent";
    const TopButton b = add_top_button(row, c.label, agent ? "agent" : "",
                                       agent ? paint::ButtonStyle::primary : paint::ButtonStyle::secondary, c.tooltip,
                                       nullptr);
    deferred.width += b.width;
    deferred.widgets.push_back(b.widget);
  }
  // Every item but the mark adds its width and one gap; hidden items are
  // counted without their gaps (conservative if Raven keeps them).
  top_full_width_ += top_items_width_ + deferred.width + chips.width + view_tabs.width +
                     kGap * static_cast<float>(top_item_count_ + deferred.widgets.size() + chips.widgets.size() +
                                               view_tabs.widgets.size());
  ui::Panel& panel = bar(&ThemeTokens::bar);
  panel.set_child(row);
  panel.set_size(-1.0F, static_cast<float>(kTopBarHeight));
  root_.add(panel);
  auto line = make_separator(ctx_, false);
  root_.add(*line);
  painted_.push_back(std::move(line));
}

MainWindow::TopButton MainWindow::add_top_button(ui::Box& row, std::string_view label, std::string_view icon,
                                                 paint::ButtonStyle style, std::string_view tooltip,
                                                 std::function<void()> click) {
  const bool enabled = static_cast<bool>(click);
  auto button = make_interactive(ctx_, [label, icon, style, enabled](ui::Painter& p, float w, float h,
                                                                     const ThemeTokens& t, bool hov, bool foc) {
    paint::button(p, w, h, label, icon, style, paint::WidgetState{hov, foc, enabled, false}, t);
  });
  const float width = paint::button_width(ctx_.runtime, label, !icon.empty());
  button->set_size(width, static_cast<float>(kButtonHeight));
  button->set_role(ui::Role::button);
  button->set_accessible_name(label);
  button->set_tooltip(tooltip);
  if (enabled) {
    button->on_click([click = std::move(click)](float, float) { click(); });
  } else {
    button->set_enabled(false);
  }
  row.add_centered(*button);
  const TopButton placed{button.get(), width};
  if (enabled) {
    top_items_width_ += width;  // always shown
    ++top_item_count_;
  }
  painted_.push_back(std::move(button));
  return placed;
}

void MainWindow::fit_top_bar() {
  const int width = window_.logical_width();
  if (width <= 0 || width == top_fit_width_) return;
  top_fit_width_ = width;
  std::vector<float> widths;
  for (const TopBarGroup& g : top_groups_) widths.push_back(g.width);
  const std::size_t hidden = top_bar_hidden_groups(static_cast<float>(width), top_full_width_, widths);
  for (std::size_t i = 0; i < top_groups_.size(); ++i) {
    for (ui::Widget* w : top_groups_[i].widgets) w->set_visible(i >= hidden);
  }
}

// --- tool rail, center, panels ----------------------------------------------------

void MainWindow::build_center() {
  ui::Box middle = ui::Box::horizontal();

  ui::Box tools = ui::Box::vertical();
  const float inset = static_cast<float>(kToolRailWidth - kToolButtonSize) / 2.0F;
  tools.set_padding(inset, 10.0F, inset, 10.0F);
  tools.set_spacing(6.0F);
  for (const ToolButton& tool : tool_rail()) {
    const bool active = tool.icon == "box";
    auto button = make_interactive(ctx_, [tool, active](ui::Painter& p, float w, float h, const ThemeTokens& t,
                                                        bool hov, bool foc) {
      paint::tool(p, w, h, tool, paint::WidgetState{hov, foc, tool.enabled, active}, t);
    });
    button->set_size(static_cast<float>(kToolButtonSize), static_cast<float>(kToolButtonSize));
    button->set_role(active ? ui::Role::toggle_button : ui::Role::button);
    button->set_accessible_name(active ? toggle_accessible_name(tool.label, true) : std::string(tool.label));
    button->set_tooltip(tool.tooltip);
    if (tool.icon == "fit") {
      button->on_click([this](float, float) { run_line("view"); });
    } else if (!tool.enabled) {
      button->set_enabled(false);
    }
    tools.add(*button);
    painted_.push_back(std::move(button));
  }
  ui::Panel& rail = bar(&ThemeTokens::bar);
  rail.set_child(tools);
  rail.set_size(static_cast<float>(kToolRailWidth), -1.0F);
  rail.set_accessible_name("Tools");
  middle.add(rail);
  auto rail_edge = make_separator(ctx_, true);
  middle.add(*rail_edge);
  painted_.push_back(std::move(rail_edge));
  middle.add(left_->widget());

  ui::Box center = ui::Box::vertical();
  center.add(canvas_->widget(), 1.0F);
  console_.set_font(ctx_.runtime.mono_font());
  console_.set_accessible_name("Console");
  diagnostics_.set_font(ctx_.runtime.mono_font());
  diagnostics_.set_accessible_name("Diagnostics");
  console_tabs_.add(console_, "Console");
  console_tabs_.add(diagnostics_, "Diagnostics");
  console_tabs_.set_size(-1.0F, static_cast<float>(kConsoleHeight));
  center.add(console_tabs_);
  middle.add(center, 1.0F);
  middle.add(inspector_->widget());
  middle.add(pdk_->widget());
  root_.add(middle, 1.0F);
}

void MainWindow::build_command_bar() {
  auto top_line = make_separator(ctx_, false);
  root_.add(*top_line);
  painted_.push_back(std::move(top_line));
  ui::Box row = ui::Box::horizontal();
  row.set_padding(kBarPadX, 0.0F, kBarPadX, 0.0F);
  row.set_spacing(10.0F);
  auto prompt = make_painted(ctx_, [](ui::Painter& p, float, float h, const ThemeTokens& t) {
    p.text_centered_y(TextRole::command_prompt, 0.0F, h / 2.0F, "%", t.accent);
  });
  prompt->set_size(10.0F, static_cast<float>(kCommandBarHeight));
  prompt->set_hit_testable(false);
  row.add(*prompt);
  painted_.push_back(std::move(prompt));

  command_bar_->set_accessible_name("Command");
  command_bar_->set_font(ctx_.runtime.mono_font());
  command_bar_->set_font_size(text_spec(TextRole::mono).size_px);
  command_bar_->set_size(-1.0F, static_cast<float>(kControlHeightMin));
  command_bar_->on_commit([this](const std::string& text) {
    run_line(text);
    command_bar_->set_text("");
  });
  command_bar_->on_history([this](bool older) { return older ? command_model_.previous() : command_model_.next(); });
  // The input takes the free space, vertically centered by its box padding.
  ui::Box input_box = ui::Box::vertical();
  input_box.set_padding(0.0F, 6.0F, 0.0F, 6.0F);
  input_box.add(*command_bar_);
  row.add(input_box, 1.0F);

  static constexpr std::string_view kHint = "⏎ run · ↑ history";
  auto hint = make_painted(ctx_, [](ui::Painter& p, float, float h, const ThemeTokens& t) {
    p.text_centered_y(TextRole::mono_small, 0.0F, h / 2.0F, kHint, t.text_tertiary);
  });
  hint->set_size(ctx_.runtime.text_width(TextRole::mono_small, kHint) + 2.0F, static_cast<float>(kCommandBarHeight));
  hint->set_hit_testable(false);
  row.add(*hint);
  painted_.push_back(std::move(hint));
  auto console_toggle = make_interactive(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hov,
                                                      bool foc) {
    paint::button(p, w, h, console_visible_ ? "Hide console" : "Console", "", paint::ButtonStyle::ghost,
                  paint::WidgetState{hov, foc, true, console_visible_}, t);
  });
  console_toggle->set_size(paint::button_width(ctx_.runtime, "Hide console", false), static_cast<float>(kInputHeight));
  console_toggle->set_role(ui::Role::toggle_button);
  console_toggle->set_accessible_name(toggle_accessible_name(kConsoleToggleName, console_visible_));
  console_toggle->on_click([this](float, float) { toggle_console(); });
  console_toggle_ = console_toggle.get();
  row.add_centered(*console_toggle);
  painted_.push_back(std::move(console_toggle));

  ui::Panel& panel = bar(&ThemeTokens::bar);
  panel.set_child(row);
  panel.set_size(-1.0F, static_cast<float>(kCommandBarHeight));
  root_.add(panel);
}

void MainWindow::build_status_bar() {
  status_bar_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    const auto problems = problems_.tally().chip();
    const paint::StatusBarKinds kinds{message_kind_, problems ? problems->kind : StatusKind::error,
                                      drc_chip_m1().kind};
    paint::status_bar(p, w, h, status_text(), kinds, t);
  });
  status_bar_->set_size(-1.0F, static_cast<float>(kStatusBarHeight));
  status_bar_->set_role(ui::Role::status_bar);
  // Clicking (or Enter on) the bar opens the Diagnostics tab with the full text.
  status_bar_->on_click([this](float, float) {
    if (!console_visible_) toggle_console();
    console_tabs_.show(1);
  });
  update_status_accessibility();
  root_.add(*status_bar_);
}

// --- commands ------------------------------------------------------------------------

void MainWindow::register_view_commands() {
  const ViewActions actions{[this](ThemeMode mode) { apply_theme(mode); },
                            [this](double dx, double dy) { canvas_->pan_pixels(dx, dy); }};
  if (!shell::register_view_commands(ctx_.core.registry(), actions)) {
    std::fprintf(stderr, "hefesto: a view command name is already registered\n");
  }
}

void MainWindow::run_line(const std::string& line) {
  in_command_ = true;
  command_problem_.clear();
  const CommandOutcome outcome = command_model_.submit(line);
  in_command_ = false;
  const std::string& status = command_model_.status_text();
  // Every message carries its shape: ■ failure, ▲ a warning the command
  // raised, ● success ("Status is never color-only").
  std::optional<LineStatus> problem;
  if (!command_problem_.empty()) problem.emplace(LineStatus{command_problem_kind_, command_problem_});
  LineStatus shown = command_line_status(outcome.ok(), status, problem);
  const bool raised = shown.kind == StatusKind::error || shown.kind == StatusKind::warning;
  set_message(shown.kind, std::move(shown.text));
  if (raised) announce(message_kind_, message_);
  // The line's diagnostics, not a bare "ok" for a line that raised errors.
  append_console(line, console_result(outcome, status));
  dirty_ = true;
  window_.request_redraw();
  if (ctx_.core.quit_requested()) ctx_.runtime.quit();
}

void MainWindow::on_command(const cmd::Invocation& /*invocation*/, const cmd::CommandResult& /*result*/) {
  // Widgets may be running the callback that issued this command: rebuild
  // on the next frame (on_frame), never inside the callback.
  dirty_ = true;
  window_.request_redraw();
}

void MainWindow::on_frame() {
  fit_top_bar();
  if (pdk_->poll()) window_.request_redraw();  // a background install: keep frames coming
  if (!pending_run_.empty() && canvas_->viewport_ready()) {
    const std::string line = std::move(pending_run_);
    pending_run_.clear();
    run_line(line);
  } else if (!pending_run_.empty()) {
    window_.request_redraw();
  }
  if (!dirty_) return;
  dirty_ = false;
  if (reveal_diagnostics_) {
    reveal_diagnostics_ = false;
    if (!console_visible_) toggle_console();
    console_tabs_.show(1);  // the Diagnostics tab
  }
  refresh_all();
}

void MainWindow::append_console(const std::string& command, const std::string& result) {
  for (const std::string& line : console_lines(command, result)) console_.add_item(line);
  console_.scroll_to_end();
}

void MainWindow::open_file() {
  const auto path = window_.open_file_dialog("Open a cell", "Magic cells (*.mag)", "*.mag");
  if (path) run_line("load " + quoted(*path));
}

void MainWindow::toggle_pdk_manager() {
  pdk_visible_ = !pdk_visible_;
  pdk_->widget().set_visible(pdk_visible_);
  inspector_->widget().set_visible(!pdk_visible_);
  if (pdk_visible_) pdk_->refresh();
}

void MainWindow::toggle_console() {
  console_visible_ = !console_visible_;
  console_tabs_.set_visible(console_visible_);
  if (console_toggle_ != nullptr) {
    console_toggle_->set_accessible_name(toggle_accessible_name(kConsoleToggleName, console_visible_));
  }
  for (auto& w : painted_) w->invalidate();
}

void MainWindow::apply_theme(ThemeMode mode) {
  ctx_.mode = mode;
  options_.theme = mode;
  ctx_.runtime.apply_theme(mode);
  window_.set_clear_color(ctx_.tokens().surface0);
  for (auto& [panel, token] : bars_) panel.set_fill(ctx_.tokens().*token);
  for (auto& w : painted_) w->invalidate();
  status_bar_->invalidate();
  left_->repaint();
  inspector_->repaint();
  pdk_->repaint();
  canvas_->refresh_overlay();
}

// --- refresh ---------------------------------------------------------------------------

void MainWindow::sync_styles() {
  editor::EditorState& ed = ctx_.editor();
  if (!ed.technology || ed.technology.get() == styled_tech_) return;
  styled_tech_ = ed.technology.get();
  display::StyleLoadOptions options;
  options.library_dirs = style_library_dirs(*ed.technology);
  ReportSink sink(ctx_.core.dispatcher());
  auto styles = display::load_style_set(ctx_.core.files(), *ed.technology, options, sink);
  if (styles) {
    ed.styles = std::move(styles.value());
  } else {
    ed.styles.reset();
    ctx_.core.dispatcher().report(styles.error());
  }
}

void MainWindow::refresh_all() {
  sync_styles();
  canvas_->update_scene();
  left_->refresh(ctx_.editor().windows.current());
  inspector_->refresh(canvas_->unit_scale());
  refresh_header();
  refresh_status();
}

void MainWindow::refresh_header() {
  const editor::EditorState& ed = ctx_.editor();
  const view::LayoutWindow* win = ed.windows.current();
  HeaderInput in;
  if (win != nullptr && win->root() != nullptr) {
    in.cell = win->root()->name();
  } else if (ed.edit_cell != nullptr) {
    in.cell = ed.edit_cell->name();
  }
  in.technology = ed.technology ? ed.technology->name() : std::string("minimum");
  if (ed.technology && in.technology != "minimum") {
    in.pdk = in.technology;
    in.pdk_version = short_tech_version(ed.technology->version().version_lines);
  }
  in.magic_compat = ctx_.core.project().compat.magic_compat;
  header_ = header_text(in);
  // The title block is as wide as its text; the tabs follow 24 px further
  // (the mockup's margin-left: 24px on top of the 16 px gap).
  const float text_w = std::max(ctx_.runtime.text_width(TextRole::cell_title, header_.title),
                                ctx_.runtime.text_width(TextRole::cell_subtitle, header_.subtitle));
  painted_.front()->set_size(static_cast<float>(kAppMarkSize) + 12.0F + text_w + 24.0F,
                             static_cast<float>(kTopBarHeight));
  window_.set_title(in.cell.empty() ? std::string("Hefesto") : in.cell + " — Hefesto");
  for (auto& w : painted_) w->invalidate();
}

void MainWindow::refresh_status() {
  update_status_accessibility();
  status_bar_->invalidate();
  inspector_->refresh(canvas_->unit_scale());
}

void MainWindow::show_layout(const std::vector<render::LayeredRect>& rects) {
  canvas_->show_rects(rects, render::spike_layer_styles());
  set_message(StatusKind::info, diagnostic_line(StatusKind::info, std::to_string(rects.size()) + " rectangles"));
  status_bar_->invalidate();
}

}  // namespace hefesto::shell
