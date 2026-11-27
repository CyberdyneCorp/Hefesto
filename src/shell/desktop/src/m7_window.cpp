// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M7 (openspec/changes/m7-extraction task 16.1, design D19): the top bar's
// Extract button. It keeps the M1 slot among the deferred actions, so it
// gives way at narrow widths in the M1 order (top_bar_hidden_groups). A
// click runs `extract all` as a typed line (Console echo, history, session
// log); the command runs through MaskSession::write_host, which
// build_m4b_console points at the M5 job host (running header, Interrupt
// card after 2 s, cancel).
//
// Owner: SHELL.

#include <cstdio>
#include <string>
#include <string_view>

#include "hefesto/shell/extract_button.hpp"
#include "main_window.hpp"

namespace hefesto::shell {

ExtractButtonView MainWindow::extract_button() const {
  const bool running = mask_jobs_ != nullptr && mask_jobs_->busy();
  return extract_button_view(extract_button_input(ctx_.core.editor(), running));
}

MainWindow::TopButton MainWindow::add_extract_button(ui::Box& row, std::string_view label) {
  auto button = make_interactive(ctx_, [this, label](ui::Painter& p, float w, float h, const ThemeTokens& t, bool hov,
                                                     bool foc) {
    const bool enabled = extract_button().enabled;
    paint::button(p, w, h, label, "", paint::ButtonStyle::secondary,
                  paint::WidgetState{hov && enabled, foc, enabled, false}, t);
  });
  const float width = paint::button_width(ctx_.runtime, label, false);
  button->set_size(width, static_cast<float>(kButtonHeight));
  button->set_role(ui::Role::button);
  button->set_accessible_name(label);
  button->on_click([this](float, float) {
    if (const std::optional<std::string> line = extract_click_line(extract_button())) run_typed_line(*line);
  });
  row.add_centered(*button);
  extract_button_ = button.get();
  painted_.push_back(std::move(button));
  refresh_extract_button();
  return TopButton{extract_button_, width};
}

// After an --input script: "hefesto: extract button enabled|disabled:
// <tooltip>" and, when shown, "hefesto: extract button at X Y W H" (window
// points), so a capture script can hover or click it.
void MainWindow::report_extract_button() const {
  std::printf("%s\n", extract_button_status(extract_button()).c_str());
  const ui::Rect r = extract_button_ != nullptr ? extract_button_->rect() : ui::Rect{};
  if (r.width > 0.0F) {  // hidden at narrow widths: collapsed
    std::printf("hefesto: extract button at %.0f %.0f %.0f %.0f\n", static_cast<double>(r.x), static_cast<double>(r.y),
                static_cast<double>(r.width), static_cast<double>(r.height));
  }
  std::fflush(stdout);
}

void MainWindow::refresh_extract_button() {
  if (extract_button_ == nullptr) return;
  const ExtractButtonView view = extract_button();
  extract_button_->set_enabled(view.enabled);
  extract_button_->set_tooltip(view.tooltip);
  extract_button_->invalidate();
}

}  // namespace hefesto::shell
