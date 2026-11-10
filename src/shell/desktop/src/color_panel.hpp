// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The color window screen (DESIGN board docs/design/mockups/M2-Color-Window
// .dc.html; capability ui-color-window): when `specialopen color` has opened
// a color window, it takes the right dock in place of the Inspector and
// shows the chrome the board adds around Magic's color editor (caption
// `COLOR = 0<octal>`, "map modified", index stepper, Load… / Save…, the
// value strip), the edited color and the six bars with their − / + pumps.
// Every control issues the core command Magic's window would: pumps and
// bars `setpoint X Y ID; pushbutton left` at their point of the color
// window, the stepper `color N`, Load… / Save… `load` / `save` in the
// command bar. The view comes from color_window_view() (screen_models.hpp).
// Raven-free: built from the raven_ui wrappers.

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/shell/screen_models.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"

namespace hefesto::shell {

class ColorPanel {
 public:
  explicit ColorPanel(ScreenContext& ctx);
  [[nodiscard]] ui::Widget& widget() noexcept { return root_; }
  /// Re-reads the active color window; false when none is open (hide the panel).
  bool refresh();
  void repaint();

 private:
  /// `line` sent to the color window: a command point in it first, so the
  /// Dispatcher gives the command to the `color` client.
  [[nodiscard]] std::string in_window(const std::string& line) const;
  void add_button(ui::Box& row, std::string label, std::function<void()> click, bool primary = false);
  void build_bar(std::size_t index);

  ScreenContext& ctx_;
  std::optional<ColorWindowView> view_;
  ui::Box root_ = ui::Box::horizontal();
  ui::Panel background_;
  ui::Box column_ = ui::Box::vertical();
  ui::ScrollBox scroll_ = ui::ScrollBox::create();
  std::vector<std::unique_ptr<ui::Painted>> widgets_;
};

}  // namespace hefesto::shell
