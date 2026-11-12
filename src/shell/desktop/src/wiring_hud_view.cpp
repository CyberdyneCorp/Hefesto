// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The wiring tool's tracking feedback on screen (board M2-Wiring; M4 CARRY
// task 11.7), from the wiring_hud.hpp models:
//   - the HUD next to the cursor while the tool tracks (type · width ·
//     direction · leg length), in canvas tokens;
//   - the "Wiring · tracking" chip in the top bar (a click, R or Esc issue
//     `trackwire cancel`);
//   - the Wire inspector in the Inspector's slot while the wiring tool is
//     the current tool: type, width with − / +, Switch layer, and the
//     technology's wiring stack (a row issues `wire type NAME`).
// Every control issues a `wire` / `trackwire` command through run_line.
//
// Owner: CARRY (openspec/changes/m4-scripting/design.md D2).

#include <algorithm>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/shell/wiring_hud.hpp"
#include "hefesto/ui/tools.hpp"
#include "main_window.hpp"
#include "screen_paint.hpp"

namespace hefesto::shell {

namespace {

constexpr float kHudPad = 8.0F;
constexpr float kHudHeight = 24.0F;

}  // namespace

void MainWindow::build_wiring_chip(ui::Box& row) {
  auto chip = make_interactive(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t, bool, bool) {
    paint::chip(p, w, h, StatusChip{StatusKind::info, wiring_chip_text_}, t);
  });
  wiring_chip_text_ = "Wiring · tracking";
  chip->set_size(paint::chip_width(ctx_.runtime, StatusChip{StatusKind::info, wiring_chip_text_}),
                 static_cast<float>(kChipHeight));
  chip->set_role(ui::Role::button);
  chip->set_tooltip("The wiring tool is tracking: click, R or Esc cancels (trackwire cancel)");
  chip->on_click([this](float, float) {
    if (const auto c = wiring_chip(ctx_.editor())) run_line(c->cancel);
  });
  chip->set_visible(false);
  row.add_centered(*chip);
  // Not counted in the top bar's fit: it shows only while the tool tracks.
  wiring_chip_ = chip.get();
  painted_.push_back(std::move(chip));
}

void MainWindow::build_wiring_views() {
  // The HUD: a painted label over the canvas, moved with the cursor.
  wiring_hud_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) {
    p.fill(0.0F, 0.0F, w, h, 4.0F, Rgba{0.0F, 0.0F, 0.0F, 0.78F});
    p.stroke(0.0F, 0.0F, w, h, 4.0F, 1.0F, t.control_border);
    p.text_centered_y(TextRole::mono_small, kHudPad, h / 2.0F, wiring_hud_line_, Rgba{1.0F, 1.0F, 1.0F, 1.0F});
  });
  wiring_hud_->set_hit_testable(false);
  wiring_hud_->set_accessible_name("Wiring HUD");
  canvas_->add_overlay(*wiring_hud_);
  wiring_hud_->set_visible(false);

  // The Wire inspector, in the Inspector's slot (right_stack_).
  wire_box_.set_padding(16.0F, 12.0F, 16.0F, 12.0F);
  wire_box_.set_spacing(8.0F);
  wire_section_.emplace(ui::Label::create("WIRE"));
  wire_box_.add(*wire_section_);
  wire_type_.emplace(ui::Label::create(""));
  wire_box_.add(*wire_type_);
  ui::Box width = ui::Box::horizontal();
  width.set_spacing(8.0F);
  wire_width_.emplace(ui::Label::create(""));
  width.add(*wire_width_, 1.0F);
  for (std::size_t i = 0; i < 3; ++i) {
    auto b = ui::Button::create(i == 0 ? "Width −" : (i == 1 ? "Width +" : "Switch layer"), false);
    b->on_click([this, i] {
      if (i < wire_view_.actions.size()) run_line(wire_view_.actions[i].command);
    });
    if (i < 2) {
      width.add(*b);
    } else {
      wire_box_.add(width);
      wire_box_.add(*b);
    }
    wire_buttons_.push_back(std::move(b));
  }
  wire_stack_label_.emplace(ui::Label::create("WIRING STACK"));
  wire_box_.add(*wire_stack_label_);
  wire_stack_.set_accessible_name("Wiring stack");
  wire_stack_.on_select([this](int index) {
    const auto i = static_cast<std::size_t>(std::max(0, index));
    if (i < wire_view_.stack.size()) run_line(wire_view_.stack[i].command);
  });
  wire_box_.add(wire_stack_, 1.0F);
  wire_box_.set_size(static_cast<float>(kInspectorWidth), -1.0F);
  right_stack_.add(wire_box_, 1.0F);
  wire_box_.set_visible(false);
}

bool MainWindow::wiring_tool_current() const {
  const hefesto::ui::Tool* tool = ctx_.core.editor().tools.current();
  return tool != nullptr && tool->name == "wiring";
}

void MainWindow::refresh_wiring() {
  if (!wiring_hud_) return;
  editor::EditorState& ed = ctx_.editor();
  // The chip.
  const auto chip = wiring_chip(ed);
  if (wiring_chip_ != nullptr && wiring_chip_->rect().width >= 0.0F) {
    if (chip) wiring_chip_->set_accessible_name(chip->accessible_name);
    wiring_chip_->set_visible(chip.has_value());
  }
  // The HUD at the cursor.
  std::optional<std::string> text;
  if (ed.box && canvas_->cursor()) text = wiring_hud_text(ed, *ed.box, *canvas_->cursor(), canvas_->unit_scale());
  if (text) {
    wiring_hud_line_ = *text;
    const float w = ctx_.runtime.text_width(TextRole::mono_small, *text) + 2.0F * kHudPad;
    const ui::Rect area = canvas_->widget().rect();
    // popover_point gives the cursor's window point (+8 px, kept on the canvas).
    const db::Point c = *canvas_->cursor();
    const auto [px, py] = canvas_->popover_point(db::Rect{c.x, c.y, c.x, c.y});
    const HudPoint at = wiring_hud_point(static_cast<double>(px - area.x - 8.0F), static_cast<double>(py - area.y),
                                         static_cast<double>(w), static_cast<double>(kHudHeight),
                                         static_cast<double>(area.width), static_cast<double>(area.height));
    wiring_hud_->set_size(w, kHudHeight);
    wiring_hud_->place_top_right(std::max(0.0F, area.width - static_cast<float>(at.x) - w),
                                 std::max(0.0F, static_cast<float>(at.y)));
    wiring_hud_->set_accessible_name(*text);
    wiring_hud_->invalidate();
  }
  wiring_hud_->set_visible(text.has_value());
  refresh_wire_inspector();
}

void MainWindow::refresh_wire_inspector() {
  editor::EditorState& ed = ctx_.editor();
  const bool show = wiring_tool_current() && ed.technology && !pdk_visible_ && !color_visible_ && inspector_visible_;
  if (show && ed.technology.get() != wiring_tech_) {
    wiring_tech_ = ed.technology.get();
    auto section = wiring::parse_wiring_section(*ed.technology);
    wiring_section_ = section ? std::move(section.value()) : wiring::WiringSection{};
  }
  if (show != wire_box_shown_) {
    wire_box_shown_ = show;
    wire_box_.set_visible(show);
    inspector_->widget().set_visible(!show && !pdk_visible_ && !color_visible_ && inspector_visible_);
  }
  if (!show) return;
  WireInspectorView view = wire_inspector(ed, wiring_section_, canvas_->unit_scale());
  if (view.type == wire_view_.type && view.width == wire_view_.width && view.stack.size() == wire_view_.stack.size()) {
    wire_view_ = std::move(view);
    return;
  }
  wire_view_ = std::move(view);
  wire_type_->set_text("Type  " + wire_view_.type);
  wire_width_->set_text("Width  " + wire_view_.width + " " + wire_view_.units);
  wire_stack_.clear();
  for (const WireStackRow& row : wire_view_.stack) wire_stack_.add_item((row.current ? "● " : "   ") + row.name);
}

}  // namespace hefesto::shell
