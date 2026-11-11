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
#include "hefesto/ui/tools.hpp"

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

/// cmd::PromptHost through the window's dialogs (design.md D4).
class MainWindow::DialogPromptHost final : public cmd::PromptHost {
 public:
  explicit DialogPromptHost(MainWindow& window) : window_(window) {}
  [[nodiscard]] std::size_t ask(std::string_view question, std::span<const std::string> answers,
                                std::size_t default_answer) override {
    return window_.ask(question, answers, default_answer);
  }

 private:
  MainWindow& window_;
};

MainWindow::MainWindow(ui::Runtime& runtime, const ShellOptions& options, std::mutex& queue_mutex, CoreSession& core)
    : ctx_{runtime, core, options.theme, {}, {}, {}, {}, {}},
      options_(options),
      window_("Hefesto", options.width, options.height),
      command_bar_(ui::CommandBar::create("Type a command (help)")),
      core_sink_(core.dispatcher()) {
  // An undo or redo from any path (typed, `u`/`U` keys, the undo history),
  // checked after the post-command update has closed the command's step:
  // run_line shows the message, or on_frame when no line is running.
  hefesto::ui::state(ctx_.editor()).update_handlers.emplace_back([this](editor::EditorState& ed) {
    if (auto moved = undo_move_message(undo_mark_, ed.undo)) undo_message_ = std::move(moved);
    undo_mark_ = undo_mark(ed.undo);
  });
  ctx_.run = [this](const std::string& line) { run_line(line); };
  pending_pinch_ = options.pinch;
  ctx_.echo = [this](const std::string& command, const std::string& result) { append_console(command, result); };
  ctx_.prompt = [this](const std::string& text) { show_prompt(text); };
  ctx_.menu = [this](const std::string& title, const std::vector<MenuEntry>& entries, float x, float y) {
    open_menu(title, entries, x, y);
  };
  capture_mode_ = !options.capture.empty() || options.smoke_frames > 0 || options.bench_frames > 0;
  window_.set_min_size(1024, 640);
  ctx_.runtime.apply_theme(ctx_.mode);
  window_.set_clear_color(ctx_.tokens().surface0);
  canvas_ = std::make_unique<LayoutCanvas>(ctx_, queue_mutex, [this] { return window_.scale(); });
  canvas_->on_view_changed([this] { refresh_status(); });
  canvas_->router().on_prompt([this](const std::string& text) { route_prompt(text); });
  left_ = std::make_unique<LeftPanel>(ctx_);
  inspector_ = std::make_unique<InspectorPanel>(ctx_);
  pdk_ = std::make_unique<PdkPanel>(ctx_, [this] { toggle_pdk_manager(); });
  color_ = std::make_unique<ColorPanel>(ctx_);
  drc_link_ = std::make_unique<DrcLink>(core.editor());
  drc_panel_ = std::make_unique<DrcPanel>(ctx_);

  build_top_bar();
  build_center();
  canvas_->add_overlay(toast_);
  toast_.place_bottom_center();
  // The DRC stepper over the canvas, top right, below the ruler (board: 16 px
  // from the right edge, 32 px from the top).
  canvas_->add_overlay(drc_panel_->stepper_widget());
  drc_panel_->stepper_widget().place_top_right(static_cast<float>(kCanvasInset),
                                               static_cast<float>(kRulerSize + 12));
  build_label_popover();  // CARRY: the Label tool's popover over the canvas
  build_key_sheet();      // CARRY: the key-binding sheet (`panel keys show`)
  build_wiring_views();   // CARRY: the wiring HUD and the Wire inspector
  build_command_bar();
  build_status_bar();
  build_m4();
  build_m4b();
  window_.set_content(root_);
  window_.install_key_hook([this](int key, std::uint8_t modifiers) { return on_shortcut(key, modifiers); });
  register_view_commands();

  set_message(StatusKind::info, diagnostic_line(StatusKind::info, "ready"));
  core.dispatcher().add_sink(problems_);
  core.set_command_listener(
      [this](const cmd::Invocation& inv, const cmd::CommandResult& result) { on_command(inv, result); });
  if (core.log_warning()) core.dispatcher().report(*core.log_warning());
  // Magic's TxDialog questions (flush, save over a file, color map, quit)
  // and the crash-recovery confirmation become dialogs.
  prompt_host_ = std::make_unique<DialogPromptHost>(*this);
  core.editor().prompt = prompt_host_.get();
  core.editor().cell_session.confirm_recovery = [this](const platform::Path& backup) {
    static const std::vector<std::string> kAnswers = {"no", "yes"};
    return ask("Recover from backup file " + backup.string() + "?", kAnswers, 1) == 1;
  };
  pdk_visible_ = !options.show_pdk_manager;
  toggle_pdk_manager();
  // --console shows the Console whatever the saved layout says.
  if (options.show_console && !console_visible_) toggle_console();
  refresh_all();
}

MainWindow::~MainWindow() {
  save_panels();  // the panel layout as the user leaves it (not in a scripted capture)
  teardown_m4();
  teardown_m4b();
  ctx_.core.editor().prompt = nullptr;
  ctx_.core.editor().cell_session.confirm_recovery = {};
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
  in.drc = drc_view_.status_bar;
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
    if (layout_read_only_) paint_read_only_chip(p, size + 12.0F, h, t);
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
  add_undo_buttons(row);

  ui::Box spacer = ui::Box::horizontal();
  row.add(spacer, 1.0F);
  build_drc_chip(row);
  build_wiring_chip(row);  // CARRY: "Wiring · tracking"
  for (const StatusChip& chip : {lvs_chip_m1()}) {
    auto widget = make_painted(ctx_, [chip](ui::Painter& p, float w, float h, const ThemeTokens& t) {
      paint::chip(p, w, h, chip, t);
    });
    const float chip_width = paint::chip_width(ctx_.runtime, chip);
    widget->set_size(chip_width, static_cast<float>(kChipHeight));
    chips.width += chip_width;
    chips.widgets.push_back(widget.get());
    widget->set_role(ui::Role::text);
    widget->set_accessible_name(chip.label());
    widget->set_tooltip("LVS arrives in M9");
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
  build_tool_rail(middle_);
  ui::Box& center = center_;
  center.add(canvas_->widget(), 1.0F);
  console_.set_font(ctx_.runtime.mono_font());
  console_.set_accessible_name("Console");
  diagnostics_.set_font(ctx_.runtime.mono_font());
  diagnostics_.set_accessible_name("Diagnostics");
  build_m4b_console();  // M4: the Console tab is the project's Tcl console
  console_tabs_.add(console_panel_->widget(), "Console");
  console_tabs_.add(diagnostics_, "Diagnostics");
  feedback_.set_font(ctx_.runtime.mono_font());
  feedback_.set_accessible_name("Feedback");
  console_tabs_.add(feedback_, "Feedback");
  // A Feedback row moves the box to its area (`feedback find N`).
  feedback_.on_select([this](int index) {
    const auto i = static_cast<std::size_t>(index);
    if (i < feedback_rows_.size()) run_line(feedback_rows_[i].find);
  });
  keys_.set_font(ctx_.runtime.mono_font());
  keys_.set_accessible_name("Key bindings");
  console_tabs_.add(keys_, "Keys");
  // A binding row puts its `macro TABLE KEY "TEXT"` line in the command bar to edit.
  keys_.on_select([this](int index) {
    const auto i = static_cast<std::size_t>(index);
    if (i < key_edits_.size() && !key_edits_[i].empty()) show_prompt(key_edits_[i]);
  });
  console_tabs_.set_size(-1.0F, static_cast<float>(kConsoleHeight));
  center.add(console_tabs_);
  arrange_middle();
  build_panels();  // CARRY: the panels in their frames, as the saved layout has them
  root_.add(middle_, 1.0F);
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
    run_typed_line(text);
    command_bar_->set_text("");
  });
  command_bar_->on_history([this](bool older) { return older ? command_model_.previous() : command_model_.next(); });
  command_bar_->on_complete([this](const std::string& text) { return complete(text); });
  // The input takes the free space, vertically centered by its box padding.
  ui::Box input_box = ui::Box::vertical();
  input_box.set_padding(0.0F, 6.0F, 0.0F, 6.0F);
  input_box.add(*command_bar_);
  row.add(input_box, 1.0F);

  // The clock button (and ^R) opens the history popover (M2-Command-Bar board).
  auto history = make_interactive(ctx_, [](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hov,
                                           bool foc) {
    paint::button(p, w, h, "", "history", paint::ButtonStyle::ghost, paint::WidgetState{hov, foc, true, false}, t);
  });
  history->set_size(28.0F, static_cast<float>(kInputHeight));
  history->set_role(ui::Role::button);
  history->set_accessible_name("Command history");
  history->set_tooltip("Command history (^R in the command bar)");
  history->on_click([this](float, float) { show_history(); });
  row.add_centered(*history);
  painted_.push_back(std::move(history));

  static constexpr std::string_view kHint = "⏎ run · ↑ history · ⇥ complete";
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
  // Clears the Diagnostics list and resets the problem count (design.md D10).
  auto clear = make_interactive(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hov,
                                             bool foc) {
    paint::button(p, w, h, "Clear problems", "", paint::ButtonStyle::ghost,
                  paint::WidgetState{hov, foc, problems_.tally().chip().has_value(), false}, t);
  });
  clear->set_size(paint::button_width(ctx_.runtime, "Clear problems", false), static_cast<float>(kInputHeight));
  clear->set_role(ui::Role::button);
  clear->set_accessible_name("Clear problems");
  clear->set_tooltip("Clears the Diagnostics list and the problem count");
  clear->on_click([this](float, float) { clear_problems(); });
  clear_button_ = clear.get();
  row.add_centered(*clear);
  painted_.push_back(std::move(clear));
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
                                      drc_view_.status_bar.kind};
    paint::status_bar(p, w, h, status_text(), kinds, t);
  });
  status_bar_->set_size(-1.0F, static_cast<float>(kStatusBarHeight));
  status_bar_->set_role(ui::Role::status_bar);
  // Clicking (or Enter on) the bar opens the Diagnostics tab with the full text.
  status_bar_->on_click([this](float, float) {
    if (!console_visible_) toggle_console();
    // The rest of a multi-line result is in the Console, problems in Diagnostics.
    console_tabs_.show(result_has_more_ ? 0 : 1);
  });
  update_status_accessibility();
  root_.add(*status_bar_);
}

// --- commands ------------------------------------------------------------------------

void MainWindow::register_view_commands() {
  // `panel` is the panel layout's (panels_host.cpp), not the M2 one of ViewActions.
  ViewActions actions;
  actions.theme = [this](ThemeMode mode) { apply_theme(mode); };
  actions.pan = [this](double dx, double dy) { canvas_->pan_pixels(dx, dy); };
  actions.paint_layer = [this](std::optional<std::string> name) { return set_paint_layer(std::move(name)); };
  if (!shell::register_view_commands(ctx_.core.registry(), actions)) {
    std::fprintf(stderr, "hefesto: a view command name is already registered\n");
  }
  register_panel_command();
}

void MainWindow::run_typed_line(const std::string& text) {
  // History expansion (`!!`, `!n`, `^old^new^`) and the session's `history`.
  const TypedLine typed = expand_typed_line(ctx_.editor(), trim(text));
  if (typed.run) {
    // M4: the project's interpreter, shared with the Tcl console.
    if (!run_console_line(*typed.run)) run_line(*typed.run);
    return;
  }
  if (typed.error) {
    set_message(StatusKind::error, diagnostic_line(StatusKind::error, typed.error->text));
    append_console(std::string(trim(text)), diag::format_text(*typed.error));
  } else {
    set_message(StatusKind::info, diagnostic_line(StatusKind::info, typed.print));
    append_console(std::string(trim(text)), typed.print);
  }
  dirty_ = true;
  window_.request_redraw();
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
  // A multi-line result: its first line, and how many more are in the Console.
  const std::string& shown_text =
      !outcome.output.empty() ? outcome.output : (!outcome.printed.empty() && status == "ok" ? outcome.printed : status);
  const ResultLine summary = result_line(shown_text);
  result_has_more_ = summary.more_lines > 0;
  LineStatus shown = command_line_status(outcome.ok(), outcome.ok() ? summary.text() : status, problem);
  // "Undid paint metal1 · 23 more steps" (M2-Undo-Redo board, tile 4), for
  // any command of the line that moved the undo log (on_command).
  if (undo_message_ && outcome.ok() && !problem) {
    shown.text = diagnostic_line(StatusKind::success, *undo_message_);
  }
  undo_message_.reset();
  const bool raised = shown.kind == StatusKind::error || shown.kind == StatusKind::warning;
  set_message(shown.kind, std::move(shown.text));
  if (raised) announce(message_kind_, message_);
  // The line's diagnostics, not a bare "ok" for a line that raised errors.
  append_console(line, console_result(outcome, status));
  dirty_ = true;
  window_.request_redraw();
  if (ctx_.core.quit_requested()) ctx_.runtime.quit();
}

void MainWindow::on_command(const cmd::Invocation& invocation, const cmd::CommandResult& result) {
  note_drc_command(invocation, result);
  note_tool_command(invocation);  // CARRY: a Measure sweep arms Escape
  note_m4b_command(invocation, result);  // M4 managers' tags, the console run's pump
  // Widgets may be running the callback that issued this command: rebuild
  // on the next frame (on_frame), never inside the callback. A `*bypass`
  // command (pointer motion: wiring preview, pick drag) needs only the
  // canvas and the status bar.
  if (invocation.name == "*bypass") {
    canvas_dirty_ = true;
  } else {
    dirty_ = true;
  }
  window_.request_redraw();
}

void MainWindow::on_frame() {
  fit_top_bar();
  fit_panels();
  if (undo_message_ && !in_command_) {  // an undo outside run_line (the `u` and `U` keys)
    set_message(StatusKind::success, diagnostic_line(StatusKind::success, *undo_message_));
    undo_message_.reset();
  }
  poll_background();
  if (!pending_run_.empty() && canvas_->viewport_ready()) {
    const std::string line = std::move(pending_run_);
    pending_run_.clear();
    run_line(line);
  } else if (!pending_run_.empty()) {
    window_.request_redraw();
  }
  if (dirty_) {
    dirty_ = false;
    if (reveal_diagnostics_) {
      reveal_diagnostics_ = false;
      if (!console_visible_) toggle_console();
      console_tabs_.show(1);  // the Diagnostics tab
    }
    refresh_all();
  } else if (canvas_dirty_) {
    canvas_->update_scene();
    canvas_->refresh_highlights();
    canvas_->update_cursor();
    refresh_status();
  }
  canvas_dirty_ = false;
  // After the refresh: the first scene build frames the cell, which would
  // replace a pinch made before it.
  if (pending_pinch_ && pending_run_.empty() && canvas_->viewport_ready()) run_pending_pinch();
  refresh_m4();
  refresh_m4b();
  refresh_tools();
  refresh_wiring();
}

// Work that finished off the event loop: a PDK install, the minimap, the
// background DRC worker's committed squares and finished jobs.
void MainWindow::poll_background() {
  if (pdk_->poll()) window_.request_redraw();  // a background install: keep frames coming
  if (canvas_->idle()) window_.request_redraw();  // the minimap of a large scene is due
  if (drc_link_->serial() != drc_serial_ && !dirty_) refresh_drc();
  if (const auto job = drc_link_->take_finished_job()) post_drc_job_notice(*job);
  if (pending_why_ && !in_command_) show_why_popover();
}

void MainWindow::run_pending_pinch() {
  const PinchOption pinch = *pending_pinch_;
  pending_pinch_.reset();
  const std::string before = canvas_->zoom_text();
  window_.inject_trackpad_pinch(pinch.x, pinch.y, pinch.factor, 8);
  std::printf("hefesto: pinch at %g,%g by %g: zoom %s -> %s\n", static_cast<double>(pinch.x),
              static_cast<double>(pinch.y), pinch.factor, before.c_str(), canvas_->zoom_text().c_str());
  std::fflush(stdout);
  dirty_ = true;
  window_.request_redraw();
}

void MainWindow::append_console(const std::string& command, const std::string& result) {
  for (const std::string& line : console_lines(command, result)) console_.add_item(line);
  console_.scroll_to_end();
  if (!tcl_console_) return;
  // M4: GUI commands echo in the Tcl console's transcript.
  tcl_console_->transcript().add_input(command);
  if (!result.empty()) tcl_console_->transcript().add(ConsoleLineKind::result, result);
}

void MainWindow::open_file() {
  const auto path = window_.open_file_dialog("Open a cell", "Magic cells (*.mag)", "*.mag");
  if (path) run_line("load " + quoted(*path));
}

void MainWindow::toggle_pdk_manager() {
  pdk_visible_ = !pdk_visible_;
  update_right_dock();
  if (pdk_visible_) pdk_->refresh();
}

void MainWindow::update_right_dock() {
  // The Inspector's slot shows the PDK manager, else the color window, else
  // the DRC error area while the DRC panel is shown, else the Inspector.
  pdk_->widget().set_visible(pdk_visible_);
  color_->widget().set_visible(!pdk_visible_ && color_visible_);
  const bool drc = !pdk_visible_ && !color_visible_ && drc_visible_ && inspector_visible_;
  drc_panel_->inspector_widget().set_visible(drc);
  inspector_->widget().set_visible(!pdk_visible_ && !color_visible_ && !drc && !wire_box_shown_ && inspector_visible_);
  drc_panel_->stepper_widget().set_visible(drc_visible_);
  // The frames, docks and strips (panels_host.cpp): the DRC browser, the
  // technology manager, the Layers tabs and the Console as the layout has them.
  arrange_middle();
}

void MainWindow::refresh_color_panel() {
  const bool open = color_->refresh();
  if (open == color_visible_) return;
  color_visible_ = open;
  update_right_dock();
}

void MainWindow::refresh_keys() {
  // Rebuilt only when the bindings change (macro, imacro, a key file).
  const ::hefesto::ui::MacroTables& macros = ctx_.editor().macros;
  std::vector<std::string> lines;
  std::vector<std::string> edits;
  std::size_t signature = 0;
  // The current tool's table first: it answers the canvas's keys and buttons.
  std::vector<KeyBindingTable> tables = key_binding_tables(macros);
  const hefesto::ui::Tool* tool = ctx_.editor().tools.current();
  const std::string current = tool != nullptr ? tool->name : std::string("box");
  std::stable_partition(tables.begin(), tables.end(),
                        [&current](const KeyBindingTable& t) { return t.name == current; });
  for (const KeyBindingTable& table : tables) {
    const auto rows = key_binding_rows(macros, table.name);
    lines.push_back("── " + table.label + " · " + table.name + " · " + std::to_string(rows.size()) + " bindings");
    edits.emplace_back();
    for (const KeyBindingRow& row : rows) {
      std::string key = row.key;
      if (key.size() < 14) key.resize(14, ' ');
      lines.push_back("  " + key + row.command);
      edits.push_back(row.edit);
    }
  }
  for (const std::string& l : lines) signature = signature * 1000003U + std::hash<std::string>{}(l);
  if (signature == keys_signature_ && !key_edits_.empty()) return;
  keys_signature_ = signature;
  key_edits_ = std::move(edits);
  keys_.clear();
  for (const std::string& l : lines) keys_.add_item(l);
}

void MainWindow::toggle_console() {
  console_visible_ = !console_visible_;
  arrange_middle();  // the Console frame (panels_host.cpp)
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
  color_->repaint();
  drc_panel_->repaint();
  canvas_->refresh_overlay();
  arrange_middle();  // panel cards take the new tokens
  for (auto& f : frames_) f->header->invalidate();
  for (auto& s : strips_) s->invalidate();
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
  const view::LayoutWindow* win = ctx_.editor().windows.current();
  const db::CellDef* root = win != nullptr ? win->root() : nullptr;
  if (root != shown_root_) {
    // A new document: the problem count starts again (design.md D10).
    if (shown_root_ != nullptr && root != nullptr) clear_problems();
    shown_root_ = root;
  }
  canvas_->router().set_paint_layer(ctx_.paint_layer);
  canvas_->update_scene();
  canvas_->refresh_highlights();
  canvas_->update_cursor();
  left_->refresh(win);
  inspector_->refresh(canvas_->unit_scale(), root);
  refresh_header();
  refresh_rail();
  refresh_feedback();
  refresh_color_panel();
  if (keys_shown_) refresh_keys();
  refresh_drc();
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
  // M4: a read-only layout (board M4-Script-Consent tile 6) gets the chip
  // after the cell name and "read-only" in the subtitle.
  layout_read_only_ = win != nullptr && win->root() != nullptr && win->root()->flags().no_edit;
  float chip_w = 0.0F;
  if (layout_read_only_) {
    header_.subtitle = read_only_subtitle(header_.subtitle);
    chip_w = kTitleChipGap + paint::chip_width(ctx_.runtime, read_only_chip());
  }
  // The title block is as wide as its text; the tabs follow 24 px further
  // (the mockup's margin-left: 24px on top of the 16 px gap).
  const float text_w = std::max(ctx_.runtime.text_width(TextRole::cell_title, header_.title) + chip_w,
                                ctx_.runtime.text_width(TextRole::cell_subtitle, header_.subtitle));
  painted_.front()->set_size(static_cast<float>(kAppMarkSize) + 12.0F + text_w + 24.0F,
                             static_cast<float>(kTopBarHeight));
  window_.set_title(in.cell.empty() ? std::string("Hefesto") : in.cell + " — Hefesto");
  for (auto& w : painted_) w->invalidate();
}

void MainWindow::paint_read_only_chip(ui::Painter& p, float x, float h, const ThemeTokens& t) const {
  // Board M4-Script-Consent tile 6: a 20 px warning chip on the title line,
  // 8 px after the cell name.
  const StatusChip chip = read_only_chip();
  constexpr float kChipH = 20.0F;
  const float title_h = p.line_height(TextRole::cell_title);
  const float top = (h - title_h - p.line_height(TextRole::cell_subtitle)) / 2.0F;
  const float cx = x + ctx_.runtime.text_width(TextRole::cell_title, header_.title) + kTitleChipGap;
  const float cy = top + (title_h - kChipH) / 2.0F;
  Rgba tint = status_color(t, chip.kind);
  tint.a = kChipTintAlpha;
  p.fill(cx, cy, paint::chip_width(ctx_.runtime, chip), kChipH, kChipH / 2.0F, tint);
  p.text_centered_y(TextRole::chip, cx + 10.0F, cy + kChipH / 2.0F, chip.label(), status_text_color(t, chip.kind));
}

void MainWindow::refresh_status() {
  // Called on every pointer move: the inspector refreshes after commands (refresh_all).
  update_status_accessibility();
  status_bar_->invalidate();
}

// --- M2: tool rail, undo/redo, prompts, completion, panels -------------------------

void MainWindow::build_tool_rail(ui::Box& middle) {
  ui::Box tools = ui::Box::vertical();
  const float inset = static_cast<float>(kToolRailWidth - kToolButtonSize) / 2.0F;
  tools.set_padding(inset, 10.0F, inset, 10.0F);
  tools.set_spacing(6.0F);
  for (const ToolButton& tool : tool_rail()) {
    const std::size_t index = rail_.size();
    auto button = make_interactive(ctx_, [this, index](ui::Painter& p, float w, float h, const ThemeTokens& t,
                                                       bool hov, bool foc) {
      const RailButton& b = rail_[index];
      const auto active = active_rail_icon(ctx_.editor().tools.current() != nullptr
                                               ? std::string_view(ctx_.editor().tools.current()->name)
                                               : std::string_view("box"),
                                           canvas_->router().mode());
      const bool enabled =
          !rail_tool_locked(b.tool.icon) && (b.tool.icon == "paint" ? paint_state(b.tool).enabled : b.tool.enabled);
      paint::tool(p, w, h, b.tool, paint::WidgetState{hov, foc, enabled, active && *active == b.tool.icon}, t);
    });
    button->set_size(static_cast<float>(kToolButtonSize), static_cast<float>(kToolButtonSize));
    button->set_role(tool.icon == "fit" ? ui::Role::button : ui::Role::toggle_button);
    button->set_accessible_name(tool.label);
    button->set_tooltip(tool.tooltip);
    if (tool.enabled) {
      button->on_click([this, icon = tool.icon](float, float) { rail_click(icon); });
    } else {
      button->set_enabled(false);
    }
    tools.add(*button);
    rail_.push_back(RailButton{tool, button.get()});
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
}

bool MainWindow::rail_tool_locked(std::string_view icon) const {
  return layout_read_only_ && rail_tool_edits(icon);
}

void MainWindow::rail_click(std::string_view icon) {
  if (rail_tool_locked(icon)) {  // board M4-Script-Consent tile 6: editing tools are off
    set_message(StatusKind::warning, diagnostic_line(StatusKind::warning, "The layout is read-only"));
    status_bar_->invalidate();
    return;
  }
  if (icon == "place") {  // M4: a popover of the PDK's device menus, not a tool mode
    open_place_device();
    return;
  }
  const hefesto::ui::Tool* tool = ctx_.editor().tools.current();
  const auto action = rail_action(icon, tool != nullptr ? std::string_view(tool->name) : std::string_view("box"));
  if (!action) return;
  if (!action->command.empty()) run_line(action->command);
  if (icon != "fit") canvas_->router().set_mode(action->mode);
  canvas_->update_cursor();
  refresh_rail();
}

void MainWindow::refresh_rail() {
  const hefesto::ui::Tool* tool = ctx_.editor().tools.current();
  const auto active =
      active_rail_icon(tool != nullptr ? std::string_view(tool->name) : std::string_view("box"), canvas_->router().mode());
  for (RailButton& b : rail_) {
    if (b.tool.icon == "fit") continue;
    if (rail_tool_locked(b.tool.icon)) {
      b.widget->set_enabled(false);
      b.widget->set_tooltip(read_only_tool_tooltip(b.tool.label));
      b.widget->set_accessible_name(read_only_tool_tooltip(b.tool.label));
      b.widget->invalidate();
      continue;
    }
    if (b.tool.icon == "paint") {
      // Disabled with "Choose a layer to paint" until the palette has a paint layer.
      const PaintToolState paint = paint_state(b.tool);
      b.widget->set_enabled(paint.enabled);
      b.widget->set_tooltip(paint.tooltip);
      b.widget->set_accessible_name(paint.enabled ? toggle_accessible_name(b.tool.label, active && *active == "paint")
                                                  : paint.accessible_name);
      b.widget->invalidate();
      continue;
    }
    b.widget->set_enabled(b.tool.enabled);
    b.widget->set_accessible_name(toggle_accessible_name(b.tool.label, active && *active == b.tool.icon));
    b.widget->set_tooltip(rail_tooltip(b.tool, b.tool.enabled));
    b.widget->invalidate();
  }
}

void MainWindow::add_undo_buttons(ui::Box& row) {
  for (const bool redo : {false, true}) {
    const std::string_view label = redo ? "Redo" : "Undo";
    auto button = make_interactive(ctx_, [this, redo, label](ui::Painter& p, float w, float h, const ThemeTokens& t,
                                                            bool hov, bool foc) {
      const bool enabled = redo ? undo_state_.can_redo : undo_state_.can_undo;
      paint::button(p, w, h, label, redo ? "redo" : "undo", paint::ButtonStyle::ghost,
                    paint::WidgetState{hov, foc, enabled, false}, t);
    });
    const float width = paint::button_width(ctx_.runtime, label, true);
    button->set_size(width, static_cast<float>(kButtonHeight));
    // Always shown: the top bar's fit (fit_top_bar) must count them.
    top_items_width_ += width;
    ++top_item_count_;
    button->set_role(ui::Role::button);
    button->set_accessible_name(label);
    button->on_click([this, redo](float, float) {
      const bool enabled = redo ? undo_state_.can_redo : undo_state_.can_undo;
      if (enabled) run_line(redo ? "redo" : "undo");
    });
    // Right-click (or the Menu key): the undo history (M2-Undo-Redo board).
    button->on_context([this](float x, float y) { show_undo_history(x, y); });
    row.add_centered(*button);
    (redo ? redo_button_ : undo_button_) = button.get();
    painted_.push_back(std::move(button));
  }
  // "▲ undo off" after `undo disable`; a click issues `undo enable`.
  const StatusChip off{StatusKind::warning, "undo off"};
  auto chip = make_interactive(ctx_, [off](ui::Painter& p, float w, float h, const ThemeTokens& t, bool, bool) {
    paint::chip(p, w, h, off, t);
  });
  chip->set_size(paint::chip_width(ctx_.runtime, off), static_cast<float>(kChipHeight));
  chip->set_role(ui::Role::button);
  chip->set_accessible_name("Undo is off: enable undo");
  chip->set_tooltip("Undo recording is off (undo disable): click to issue undo enable");
  chip->on_click([this](float, float) { run_line("undo enable"); });
  chip->set_visible(false);
  row.add_centered(*chip);
  undo_off_chip_ = chip.get();
  // Counted as always shown, so that the top bar keeps room for it.
  top_items_width_ += paint::chip_width(ctx_.runtime, off);
  ++top_item_count_;
  painted_.push_back(std::move(chip));
}

std::size_t MainWindow::ask(std::string_view question, std::span<const std::string> answers,
                            std::size_t default_answer) {
  const std::vector<std::string> list(answers.begin(), answers.end());
  const PromptDialog dialog = prompt_dialog(question, list, default_answer);
  append_console("? " + dialog.title, "");
  // A scripted capture shows the dialog and goes on with the default answer.
  const int result = window_.run_dialog(dialog, !capture_mode_);
  return dialog_answer(result, list.size(), default_answer);
}

std::optional<std::string> MainWindow::complete(const std::string& text) {
  const Completion c = complete_command_line(text, ctx_.core.registry(), ctx_.editor());
  if (!c.candidates.empty()) {
    std::string listed;
    for (std::size_t i = 0; i < c.candidates.size() && i < 12; ++i) listed += (i == 0 ? "" : " · ") + c.candidates[i];
    if (c.candidates.size() > 12) listed += " …";
    set_message(StatusKind::info, diagnostic_line(StatusKind::info, std::to_string(c.candidates.size()) +
                                                                        " matches: " + listed));
    append_console("⇥ " + text, [&c] {
      std::string all;
      for (const std::string& name : c.candidates) all += (all.empty() ? "" : "  ") + name;
      return all;
    }());
    status_bar_->invalidate();
    // Several candidates: the completion popover above the command bar.
    if (c.candidates.size() > 1) {
      const auto entries = completion_entries(c.line, c);
      const auto [x, y] = above_command_bar(entries.size());
      open_menu("Completions", entries, x, y);
    }
  }
  if (c.line == text) return std::nullopt;
  return c.line;
}

void MainWindow::open_menu(const std::string& title, std::vector<MenuEntry> entries, float x, float y) {
  menu_entries_ = std::move(entries);
  std::vector<ui::MenuItem> items;
  for (const MenuEntry& e : menu_entries_) items.push_back({e.label, e.checked, e.enabled, e.separator_before});
  menu_.set_items(title, items, [this](std::size_t index) {
    if (index >= menu_entries_.size()) return;
    const MenuEntry e = menu_entries_[index];  // a command may replace the entries
    if (e.command.empty()) return;
    if (e.prompt) {
      show_prompt(e.command);
    } else {
      run_line(e.command);
    }
  });
  menu_.open(window_, x, y);
}

std::pair<float, float> MainWindow::above_command_bar(std::size_t rows) const {
  // Raven menu rows are the theme's row height plus the menu padding.
  constexpr float kMenuRow = 28.0F;
  constexpr float kMenuPad = 12.0F;
  const ui::Rect bar = command_bar_->rect();
  const float height = kMenuRow * static_cast<float>(rows) + kMenuPad;
  return {bar.x, std::max(0.0F, bar.y - height - 4.0F)};
}

void MainWindow::show_history() {
  const auto entries = history_entries(command_model_.history());
  if (entries.empty()) {
    set_message(StatusKind::info, diagnostic_line(StatusKind::info, "no commands in the history yet"));
    status_bar_->invalidate();
    return;
  }
  const auto [x, y] = above_command_bar(entries.size());
  open_menu("Command history", entries, x, y);
}

void MainWindow::show_undo_history(float x, float y) {
  UndoHistory h = undo_history(ctx_.editor().undo);
  if (h.entries.empty()) {
    set_message(StatusKind::info, diagnostic_line(StatusKind::info, "nothing to undo or redo"));
    status_bar_->invalidate();
    return;
  }
  open_menu("Undo history · " + h.title, std::move(h.entries), x, y);
}

bool MainWindow::on_shortcut(int key, std::uint8_t modifiers) {
  if (tool_shortcut(key, modifiers)) return true;  // CARRY: Escape closes the label popover, unmeasure
  if (m4b_shortcut(key, modifiers)) return true;  // the Tcl console's Return, history and Ctrl-C
  constexpr int kF6 = raven_key::kF1 + 5;
  if (key == kF6 && (modifiers & ~kModShift) == 0) {
    cycle_focus((modifiers & kModShift) != 0);
    return true;
  }
  if (key == 'R' && modifiers == kModCtrl && command_bar_->has_focus()) {
    show_history();
    return true;
  }
  return false;
}

void MainWindow::cycle_focus(bool backward) {
  const FocusRegions shown{layers_visible_ || drc_visible_, inspector_visible_ && !pdk_visible_ && !color_visible_,
                           console_visible_};
  for (int tries = 0; tries < 6; ++tries) {
    focus_region_ = next_focus_region(focus_region_, shown, backward);
    switch (focus_region_) {
      case FocusRegion::tools: rail_.front().widget->focus(); return;
      case FocusRegion::layers:
        if (drc_visible_) {
          drc_panel_->focus();
        } else {
          left_->focus();
        }
        return;
      case FocusRegion::canvas: canvas_->focus(); return;
      case FocusRegion::inspector:
        if (inspector_->focus()) return;
        break;  // no action to focus: on to the next region
      case FocusRegion::console: console_panel_->focus(); return;  // the tab strip itself takes no focus
      case FocusRegion::command: command_bar_->focus(); return;
    }
  }
}

void MainWindow::show_prompt(const std::string& text) {
  command_bar_->set_text(text);
  command_bar_->focus();
}

std::optional<std::string> MainWindow::set_paint_layer(std::optional<std::string> name) {
  if (!name) return ctx_.paint_layer;
  const editor::EditorState& ed = ctx_.editor();
  if (!name->empty()) {
    const auto found = ed.technology->find_type(*name);
    if (!found.ok()) return std::nullopt;
    *name = std::string(ed.technology->type_name(*found.id));  // the palette's long name
  }
  ctx_.paint_layer = std::move(*name);
  canvas_->router().set_paint_layer(ctx_.paint_layer);
  return ctx_.paint_layer;
}

void MainWindow::clear_problems() {
  problems_.clear_counts();
  diagnostics_.clear();
  update_status_accessibility();
  status_bar_->invalidate();
  if (clear_button_ != nullptr) clear_button_->invalidate();
}

void MainWindow::refresh_feedback() {
  std::vector<FeedbackRow> rows = feedback_rows(ctx_.editor(), canvas_->unit_scale());
  undo_state_ = undo_affordance(ctx_.editor());
  if (undo_off_chip_ != nullptr) undo_off_chip_->set_visible(!undo_state_.recording);
  for (ui::Painted* b : {undo_button_, redo_button_}) {
    if (b == nullptr) continue;
    const std::string step = next_undo_step(ctx_.editor().undo, b == redo_button_);
    b->set_tooltip((b == undo_button_ ? undo_state_.undo_tooltip : undo_state_.redo_tooltip) +
                   (step.empty() ? std::string() : " · next: " + step) + " · right-click for history");
    b->set_accessible_name(b == undo_button_ ? (undo_state_.can_undo ? "Undo" : "Undo, unavailable")
                                             : (undo_state_.can_redo ? "Redo" : "Redo, unavailable"));
    b->invalidate();
  }
  if (rows.size() == feedback_rows_.size() &&
      std::equal(rows.begin(), rows.end(), feedback_rows_.begin(),
                 [](const FeedbackRow& a, const FeedbackRow& b) { return a.text == b.text; })) {
    return;
  }
  feedback_rows_ = std::move(rows);
  feedback_.clear_selection();
  feedback_.clear();
  if (feedback_rows_.empty()) feedback_.add_item("No feedback areas (feedback add, or a DRC/extract run)");
  for (const FeedbackRow& row : feedback_rows_) feedback_.add_item(row.text);
}

void MainWindow::show_layout(const std::vector<render::LayeredRect>& rects) {
  canvas_->show_rects(rects, render::spike_layer_styles());
  set_message(StatusKind::info, diagnostic_line(StatusKind::info, std::to_string(rects.size()) + " rectangles"));
  status_bar_->invalidate();
}

}  // namespace hefesto::shell
