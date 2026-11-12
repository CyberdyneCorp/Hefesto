// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A drag from a painted widget (board docs/design/mockups/M2-Panels.dc.html
// tile 2: dragging a panel header to a dock; openspec/changes/m4-scripting
// task 11.2). A press that moves kDragThreshold or more, or leaves the
// widget, before its release starts a drag, once; a press released before
// that is a click.
//
// Toolkit-free and unit-tested; raven_ui's Painted::on_drag drives it from
// the widget's mouse events and starts Raven's in-application drag
// (Rdrag_begin) when it says so.
//
// Owner: SHELL (design D2).

#pragma once

#include <cmath>

namespace hefesto::shell {

/// How far (logical units, either axis) the pointer moves before a press
/// becomes a drag.
inline constexpr float kDragThreshold = 4.0F;

class DragGesture {
 public:
  /// The button went down at (x, y).
  void press(float x, float y) noexcept {
    pressed_ = true;
    dragging_ = false;
    x_ = x;
    y_ = y;
  }
  /// The pointer moved to (x, y): true exactly once, when the drag starts.
  bool move(float x, float y) noexcept {
    if (!pressed_ || dragging_) return false;
    dragging_ = std::fabs(x - x_) >= kDragThreshold || std::fabs(y - y_) >= kDragThreshold;
    return dragging_;
  }
  /// The pointer left the widget with the button down (a fast move: the
  /// widget gets no more moves): true when the drag starts now.
  bool leave() noexcept {
    if (!pressed_ || dragging_) return false;
    dragging_ = true;
    return true;
  }
  /// The button came up: true when this is a click (no drag started).
  bool release() noexcept {
    const bool click = pressed_ && !dragging_;
    pressed_ = false;
    dragging_ = false;
    return click;
  }
  [[nodiscard]] bool pressed() const noexcept { return pressed_; }
  [[nodiscard]] bool dragging() const noexcept { return dragging_; }
  /// Where the press was.
  [[nodiscard]] float press_x() const noexcept { return x_; }
  [[nodiscard]] float press_y() const noexcept { return y_; }

 private:
  bool pressed_ = false;
  bool dragging_ = false;
  float x_ = 0.0F;
  float y_ = 0.0F;
};

}  // namespace hefesto::shell
