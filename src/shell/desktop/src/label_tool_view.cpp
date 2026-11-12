// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The rail's box-tool modes on screen (board M2-Tool-States; capability
// hefesto-input-and-touch "Desktop Paint, Label and Measure tools"; M4
// CARRY task 11.4), from the label_tool.hpp models:
//   - the Label sweep opens a popover card on the canvas next to the box
//     (Raven 0.2.2 has no anchored popover, so it is a card in the canvas
//     overlay): text, position, layer; Enter or Add label issues `label …`,
//     Escape or Cancel closes it and issues nothing;
//   - Escape right after a Measure sweep issues `unmeasure`;
//   - the Paint button is disabled, with "Choose a layer to paint", while the
//     palette has no paint layer, and the Paint mode leaves when it is cleared.
//
// Owner: CARRY (openspec/changes/m4-scripting/design.md D2).

#include <algorithm>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/shell/label_tool.hpp"
#include "main_window.hpp"

namespace hefesto::shell {

namespace {

std::vector<std::string> position_items() {
  std::vector<std::string> items;
  for (const std::string_view p : kLabelPositions) items.emplace_back(p.empty() ? "auto (from the box)" : p);
  return items;
}

}  // namespace

void MainWindow::build_label_popover() {
  label_card_.emplace(ui::Panel::create(ctx_.tokens().control_border));
  label_card_->set_padding(1.0F, 1.0F, 1.0F, 1.0F);
  ui::Panel& inner = label_inner_.emplace(ui::Panel::create(ctx_.tokens().surface1));
  inner.set_padding(12.0F, 10.0F, 12.0F, 10.0F);
  label_box_.set_spacing(8.0F);
  label_title_.emplace(ui::Label::create("Label"));
  label_box_.add(*label_title_);
  label_text_ = ui::TextInput::create("Label text");
  label_text_->set_accessible_name("Label text");
  label_text_->on_change([this](std::string text) { label_popover_.set_text(std::move(text)); });
  label_text_->on_commit([this](std::string text) {
    label_popover_.set_text(std::move(text));
    commit_label();
  });
  label_box_.add(*label_text_);
  ui::Box row = ui::Box::horizontal();
  row.set_spacing(8.0F);
  label_position_ = ui::ComboBox::create(position_items());
  label_position_->set_accessible_name("Position");
  label_position_->on_select([this](int index) {
    const auto i = static_cast<std::size_t>(std::max(0, index));
    label_popover_.set_position(i < kLabelPositions.size() ? kLabelPositions[i] : std::string_view());
    label_layer_->set_enabled(label_popover_.layer_enabled());
  });
  row.add(*label_position_, 1.0F);
  label_layer_ = ui::TextInput::create("Layer");
  label_layer_->set_accessible_name("Layer (with a position)");
  label_layer_->on_change([this](std::string layer) { label_popover_.set_layer(std::move(layer)); });
  row.add(*label_layer_, 1.0F);
  label_box_.add(row);
  ui::Box buttons = ui::Box::horizontal();
  buttons.set_spacing(8.0F);
  ui::Box spacer = ui::Box::horizontal();
  buttons.add(spacer, 1.0F);
  label_cancel_ = ui::Button::create("Cancel", false);
  label_cancel_->on_click([this] { close_label_popover(); });
  buttons.add(*label_cancel_);
  label_ok_ = ui::Button::create("Add label", true);
  label_ok_->on_click([this] { commit_label(); });
  buttons.add(*label_ok_);
  label_box_.add(buttons);
  inner.set_child(label_box_);
  label_card_->set_child(inner);
  label_card_->set_size(static_cast<float>(kLabelPopoverWidth), static_cast<float>(kLabelPopoverHeight) + 40.0F);
  label_card_->set_accessible_name("Label popover");
  canvas_->add_overlay(*label_card_);
  label_card_->set_visible(false);
}

void MainWindow::route_prompt(const std::string& text) {
  // The Label sweep asks for the label text: the popover, not the command bar.
  const editor::EditorState& ed = ctx_.editor();
  if (text == "label " && ed.box && label_card_) {
    open_label_popover(*ed.box);
    return;
  }
  show_prompt(text);
}

void MainWindow::open_label_popover(const db::Rect& box) {
  label_popover_.open(box, ctx_.paint_layer);
  label_text_->set_text("");
  label_layer_->set_text(label_popover_.layer());
  label_position_->set_selected(0);
  label_layer_->set_enabled(false);
  // Right of the box (flipped and kept on the canvas: popover_point).
  const auto [x, y] = canvas_->popover_point(box);
  const ui::Rect area = canvas_->widget().rect();
  const float w = static_cast<float>(kLabelPopoverWidth);
  label_card_->place_top_right(std::max(0.0F, area.x + area.width - x - w), std::max(0.0F, y - area.y));
  label_card_->set_visible(true);
  label_text_->focus();
}

void MainWindow::commit_label() {
  const std::optional<std::string> line = label_popover_.commit();
  if (!line) {
    label_text_->focus();  // nothing to add yet
    return;
  }
  label_card_->set_visible(false);
  run_line(*line);
  canvas_->focus();
}

void MainWindow::close_label_popover() {
  label_popover_.cancel();
  label_card_->set_visible(false);
  canvas_->focus();
}

bool MainWindow::tool_shortcut(int key, std::uint8_t modifiers) {
  if (key_sheet_shortcut(key, modifiers)) return true;  // the open key-binding sheet holds the keyboard
  if (key != raven_key::kEscape || modifiers != 0) return false;
  if (label_popover_.is_open()) {
    close_label_popover();
    return true;
  }
  // Escape with no sweep in progress, right after a measure: `unmeasure`.
  if (command_bar_->has_focus() || canvas_->router().sweep()) return false;
  if (const auto line = measure_escape_.escape(ctx_.editor().command_count)) {
    run_line(*line);
    return true;
  }
  return false;
}

void MainWindow::note_tool_command(const cmd::Invocation& invocation) {
  // Armed once the sweep's line has finished (refresh_tools, between frames).
  if (invocation.name == "measure" && canvas_->router().mode() == EditMode::measure) measure_armed_ = true;
}

void MainWindow::refresh_tools() {
  if (measure_armed_ && !in_command_) {
    measure_armed_ = false;
    measure_escape_.measured(ctx_.editor().command_count);
  }
  // The Paint mode cannot stay without a paint layer.
  InputRouter& router = canvas_->router();
  const EditMode mode = mode_for_paint_layer(router.mode(), ctx_.paint_layer);
  if (mode != router.mode()) router.set_mode(mode);
}

PaintToolState MainWindow::paint_state(const ToolButton& tool) const {
  return paint_tool_state(ctx_.paint_layer, tool.label, tool.tooltip);
}

}  // namespace hefesto::shell
