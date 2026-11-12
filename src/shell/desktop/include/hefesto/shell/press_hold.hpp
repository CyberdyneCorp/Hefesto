// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Press-and-hold on a painted button (board docs/design/mockups/
// M2-Undo-Redo.dc.html: holding Undo opens the undo history;
// hefesto-app-ux "Undo history"; openspec/changes/m4-scripting task 11.5).
// A press that ends before kHoldDelay is a click; a press still held when
// the delay passes opens the hold action and its release does nothing.
//
// Toolkit-free and unit-tested; raven_ui's Painted::on_hold drives it from
// the widget's mouse events and a one-shot Raven timer.
//
// Owner: SHELL (design D2).

#pragma once

#include <chrono>

namespace hefesto::shell {

/// How long a press is held before it opens the hold action (the press
/// length of a macOS pop-up button).
inline constexpr std::chrono::milliseconds kHoldDelay{500};

class PressHold {
 public:
  /// The button went down: the hold delay starts.
  void press() noexcept {
    pressed_ = true;
    held_ = false;
  }
  /// The delay passed; `still_down`: the button is still held. True when the
  /// hold action runs now.
  bool elapse(bool still_down) noexcept {
    if (!pressed_) return false;
    pressed_ = false;
    held_ = still_down;
    return held_;
  }
  /// The button came up: true when this is a click (released before the delay).
  bool release() noexcept {
    const bool click = pressed_;
    pressed_ = false;
    held_ = false;
    return click;
  }
  /// The press is waiting for the delay.
  [[nodiscard]] bool pending() const noexcept { return pressed_; }
  /// The hold action ran for the current press.
  [[nodiscard]] bool held() const noexcept { return held_; }

 private:
  bool pressed_ = false;
  bool held_ = false;
};

}  // namespace hefesto::shell
