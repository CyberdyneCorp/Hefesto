// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Tcl console panel (see console_panel.hpp).

#include "console_panel.hpp"

#include <utility>

#include "hefesto/shell/input_router.hpp"
#include "screen_paint.hpp"
#include "side_panels.hpp"

namespace hefesto::shell {

namespace {

constexpr float kPadX = 12.0F;
constexpr float kHeaderHeight = 28.0F;
constexpr float kHintHeight = 18.0F;
constexpr float kEditorHeight = 40.0F;
constexpr float kCardWidth = 380.0F;
constexpr float kCardHeight = 64.0F;
constexpr float kCardButtonWidth = 108.0F;

bool shift_only(std::uint8_t modifiers) {
  return modifiers == kModShift;
}

}  // namespace

ConsolePanel::ConsolePanel(ScreenContext& ctx, TclConsole& console, SavePath save_path)
    : ctx_(ctx), console_(console), save_path_(std::move(save_path)) {
  build_header();
  transcript_ = ui::Table::create();
  transcript_->add_column("Tcl console", 600.0F, 1.0F, false);
  transcript_->set_header_visible(false);
  transcript_->set_font(ctx_.runtime.mono_font());
  transcript_->set_accessible_name("Tcl console transcript");
  root_.add(*transcript_, 1.0F);
  build_input();
  card_ = make_interactive(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hov, bool foc) {
    const auto& run = console_.current_run();
    if (!run) {
      if (const auto card = script_card_ ? script_card_() : std::nullopt) paint_script_card(p, *card, t, hov, foc);
      return;
    }
    p.fill(0.0F, 0.0F, w, h, 8.0F, t.surface2);
    p.stroke(0.0F, 0.0F, w, h, 8.0F, 1.0F, t.border);
    p.text(TextRole::body_strong, kPadX, 10.0F, run->header_text(ConsoleClock::now()), t.text_primary);
    p.text(TextRole::mono_small, kPadX, 32.0F, "% " + run->command().substr(0, run->command().find('\n')),
           t.text_secondary);
    const std::string label = run->interrupt_requested() ? "Stopping…" : "Interrupt ⌃C";
    paint::button_at(p, w - kCardButtonWidth - kPadX, (h - 28.0F) / 2.0F, kCardButtonWidth, 28.0F, label,
                     paint::ButtonStyle::primary, paint::WidgetState{hov, foc, !run->interrupt_requested(), false}, t);
  });
  card_->set_size(kCardWidth, kCardHeight);
  card_->set_role(ui::Role::button);
  card_->set_accessible_name("Interrupt the running Tcl command");
  card_->on_click([this](float, float) { interrupt_shown(); });
  card_->set_visible(false);
  refresh();
}

ConsolePanel::~ConsolePanel() = default;

void ConsolePanel::set_script_card(ScriptCard card, std::function<void()> interrupt) {
  script_card_ = std::move(card);
  script_interrupt_ = std::move(interrupt);
}

bool ConsolePanel::card_wanted() const {
  const auto& run = console_.current_run();
  if (run) return run->card_visible(ConsoleClock::now());
  return script_card_ && script_card_().has_value();
}

void ConsolePanel::interrupt_shown() {
  if (console_.running()) {
    (void)console_.interrupt();
    return;
  }
  if (script_interrupt_ && script_card_ && script_card_()) {
    script_interrupt_requested_ = true;
    script_interrupt_();
  }
}

void ConsolePanel::paint_script_card(ui::Painter& p, const InterruptCardView& card, const ThemeTokens& t, bool hov,
                                     bool foc) const {
  // Board M4-Script-Consent tile 7: "Running <script> · N s", the file and
  // line being executed, Interrupt (Ctrl-C).
  const float w = kCardWidth;
  const float h = kCardHeight;
  p.fill(0.0F, 0.0F, w, h, 8.0F, t.surface2);
  p.stroke(0.0F, 0.0F, w, h, 8.0F, 1.0F, t.border);
  p.text(TextRole::body_strong, kPadX, 10.0F, card.title, t.text_primary);
  p.text(TextRole::mono_small, kPadX, 32.0F, card.detail, t.text_secondary);
  const std::string label = script_interrupt_requested_ ? "Stopping…" : card.action + " ⌃C";
  paint::button_at(p, w - kCardButtonWidth - kPadX, (h - 28.0F) / 2.0F, kCardButtonWidth, 28.0F, label,
                   paint::ButtonStyle::primary, paint::WidgetState{hov, foc, !script_interrupt_requested_, false}, t);
}

void ConsolePanel::build_header() {
  header_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    paint::fill_rect(p, w, h, t.surface1);
    const bool all = console_.filter() == ConsoleFilter::all;
    p.text_centered_y(TextRole::small, kPadX, h / 2.0F, all ? "Showing all lines" : "Showing \u25B2 stderr and \u25A0 errors",
                      t.text_secondary);
    if (const auto& run = console_.current_run(); run && run->card_visible(ConsoleClock::now())) {
      p.text_centered_y(TextRole::small, kPadX + 220.0F, h / 2.0F, run->header_text(ConsoleClock::now()),
                        status_text_color(t, StatusKind::warning));
    }
  });
  header_->set_size(-1.0F, kHeaderHeight);
  header_->set_role(ui::Role::toolbar);
  header_->set_accessible_name("Console header");
  ui::Box row = ui::Box::horizontal();
  row.set_spacing(6.0F);
  row.add(*header_, 1.0F);
  const auto add = [&](std::string label, std::string tooltip, std::function<bool()> enabled,
                       std::function<void()> click) {
    auto button = make_interactive(ctx_, [label, enabled](ui::Painter& p, float w, float h, const ThemeTokens& t,
                                                          bool hov, bool foc) {
      paint::button(p, w, h, label, "", paint::ButtonStyle::ghost, paint::WidgetState{hov, foc, enabled(), false}, t);
    });
    button->set_size(paint::button_width(ctx_.runtime, label, false), kHeaderHeight - 4.0F);
    button->set_role(ui::Role::button);
    button->set_accessible_name(label);
    button->set_tooltip(tooltip);
    button->on_click([enabled, click](float, float) {
      if (enabled()) click();
    });
    row.add_centered(*button);
    buttons_.push_back(std::move(button));
  };
  add("All / stderr", "Show every line, or only stderr and errors", [] { return true; }, [this] {
    console_.set_filter(console_.filter() == ConsoleFilter::all ? ConsoleFilter::stderr_only : ConsoleFilter::all);
    shown_revision_ = ~std::uint64_t{0};
    refresh();
  });
  add("Interrupt", "Stop the running command (Ctrl-C); its edits are kept",
      [this] { return console_.running(); }, [this] { console_.interrupt(); });
  add("Clear", "Clear the transcript", [] { return true; }, [this] {
    console_.transcript().clear();
    shown_revision_ = ~std::uint64_t{0};
    refresh();
  });
  add("Save…", "Save the transcript to a file", [] { return true; }, [this] { save_transcript(); });
  root_.add(row);
}

void ConsolePanel::build_input() {
  prompt_ = make_painted(ctx_, [this](ui::Painter& p, float, float, const ThemeTokens& t) {
    p.text(TextRole::mono_small, kPadX, 6.0F, console_.prompt(), t.accent);
  });
  prompt_->set_size(64.0F, kEditorHeight);
  input_ = ui::TextEditor::create("Tcl command · ⏎ run · ⇧⏎ new line");
  input_->set_font(ctx_.runtime.mono_font(), 12.0F);
  input_->set_size(-1.0F, kEditorHeight);
  input_->set_accessible_name("Tcl console input");
  input_->on_change([this](std::string text) {
    console_.set_input(std::move(text));
    hint_->invalidate();
  });
  hint_ = make_painted(ctx_, [this](ui::Painter& p, float, float h, const ThemeTokens& t) {
    p.text_centered_y(TextRole::small, kPadX, h / 2.0F, console_.input_status().hint(), t.text_tertiary);
  });
  hint_->set_size(-1.0F, kHintHeight);
  ui::Box row = ui::Box::horizontal();
  row.add(*prompt_);
  row.add(*input_, 1.0F);
  root_.add(row);
  root_.add(*hint_);
}

bool ConsolePanel::input_focused() const {
  return input_->has_focus();
}

void ConsolePanel::focus() {
  input_->focus();
}

bool ConsolePanel::on_key(int key, std::uint8_t modifiers) {
  if (key == 'C' && modifiers == kModCtrl && console_.running()) return console_.interrupt();
  if (key == 'C' && modifiers == kModCtrl && card_shown_) {
    interrupt_shown();
    return true;
  }
  if (!input_focused()) return false;
  console_.set_input(input_->text());
  if (key == raven_key::kEnter || key == raven_key::kKpEnter) {
    if (shift_only(modifiers)) return false;  // the editor inserts the line
    if (modifiers != 0) return false;
    console_.press_return(false);
    sync_input_from_model();
    refresh();
    return true;
  }
  const bool first_line = input_->cursor_line() == 0;
  const bool last_line = input_->cursor_line() >= input_->line_count() - 1;
  if (key == raven_key::kUp && modifiers == 0 && first_line) {
    console_.history_older();
    sync_input_from_model();
    return true;
  }
  if (key == raven_key::kDown && modifiers == 0 && last_line) {
    console_.history_newer();
    sync_input_from_model();
    return true;
  }
  return false;
}

void ConsolePanel::sync_input_from_model() {
  input_->set_text(console_.input());
  hint_->invalidate();
  prompt_->invalidate();
}

void ConsolePanel::reset_rows() {
  rows_ = console_.transcript().visible(console_.filter());
  transcript_->set_rows(static_cast<int>(rows_.size()), [this](int row, int) {
    const auto& lines = console_.transcript().lines();
    const auto r = static_cast<std::size_t>(row);
    return r < rows_.size() && rows_[r] < lines.size() ? console_line_text(lines[rows_[r]]) : std::string{};
  });
  if (!rows_.empty()) transcript_->scroll_to_row(static_cast<int>(rows_.size()) - 1);  // newest at the bottom
}

void ConsolePanel::refresh() {
  const ConsoleTranscript& t = console_.transcript();
  if (console_.revision() != shown_revision_ || t.lines().size() != shown_lines_ || t.dropped() != shown_dropped_) {
    shown_revision_ = console_.revision();
    shown_lines_ = t.lines().size();
    shown_dropped_ = t.dropped();
    reset_rows();
    header_->invalidate();
    prompt_->invalidate();
    hint_->invalidate();
    for (const auto& b : buttons_) b->invalidate();
  }
  const bool card = card_wanted();
  if (!card) script_interrupt_requested_ = false;
  if (card != card_shown_) {
    card_shown_ = card;
    card_->set_visible(card);
  }
  if (card) {
    card_->invalidate();
    header_->invalidate();
  }
}

void ConsolePanel::repaint() {
  header_->invalidate();
  prompt_->invalidate();
  hint_->invalidate();
  card_->invalidate();
  for (const auto& b : buttons_) b->invalidate();
}

void ConsolePanel::save_transcript() {
  if (!save_path_) return;
  const auto path = save_path_();
  if (!path) return;
  const auto written = ctx_.core.files().write_atomic(platform::Path{*path}, console_.transcript().text());
  console_.transcript().add(written.ok() ? ConsoleLineKind::info : ConsoleLineKind::error,
                            (written.ok() ? "transcript saved to " : "cannot write ") + *path);
  shown_revision_ = ~std::uint64_t{0};
  refresh();
}

}  // namespace hefesto::shell
