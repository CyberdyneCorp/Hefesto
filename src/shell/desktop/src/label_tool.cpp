// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Label popover, Escape after a measure and the Paint button state
// (label_tool.hpp). Owner: CARRY.

#include "hefesto/shell/label_tool.hpp"

#include <algorithm>

namespace hefesto::shell {

// --- label popover ----------------------------------------------------------------

PopoverPoint label_popover_point(double box_x, double box_y, double box_w, double canvas_w, double canvas_h) {
  constexpr double kGap = 8.0;
  double x = box_x + box_w + kGap;
  if (x + kLabelPopoverWidth > canvas_w) x = box_x - kGap - kLabelPopoverWidth;  // flip to the left
  x = std::clamp(x, 0.0, std::max(0.0, canvas_w - kLabelPopoverWidth));
  const double y = std::clamp(box_y, 0.0, std::max(0.0, canvas_h - kLabelPopoverHeight));
  return PopoverPoint{x, y};
}

void LabelPopover::open(const db::Rect& box, std::string layer) {
  box_ = box;
  text_.clear();
  position_.clear();
  layer_ = std::move(layer);
}

void LabelPopover::set_position(std::string_view position) {
  if (std::find(kLabelPositions.begin(), kLabelPositions.end(), position) != kLabelPositions.end()) {
    position_ = std::string(position);
  }
}

std::optional<std::string> LabelPopover::commit() {
  if (!box_ || text_.empty()) return std::nullopt;
  std::string line = "label " + command_word(text_);
  if (!position_.empty()) {
    line += " " + position_;
    if (!layer_.empty()) line += " " + command_word(layer_);
  }
  cancel();
  return line;
}

void LabelPopover::cancel() noexcept {
  box_.reset();
  text_.clear();
  position_.clear();
}

// --- Escape after a measure ---------------------------------------------------------------

std::optional<std::string> MeasureEscape::escape(std::uint64_t command_count) noexcept {
  const bool now = pending(command_count);
  after_.reset();
  if (!now) return std::nullopt;
  return std::string("unmeasure");
}

// --- Paint button -------------------------------------------------------------------------

PaintToolState paint_tool_state(std::string_view paint_layer, std::string_view label, std::string_view tooltip) {
  if (!paint_layer.empty()) return PaintToolState{true, std::string(tooltip), std::string(label)};
  return PaintToolState{false, "Choose a layer to paint", std::string(label) + ", unavailable"};
}

EditMode mode_for_paint_layer(EditMode mode, std::string_view paint_layer) noexcept {
  return mode == EditMode::paint && paint_layer.empty() ? EditMode::none : mode;
}

}  // namespace hefesto::shell
