// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A scripted sequence of pointer and key events (the desktop app's
// --input option), injected through Raven's event injection after --run
// for captures of states that need a pointer or a key press: a Label sweep
// opening its popover, the wiring HUD at the cursor, the key-binding
// sheet capturing a key and warning of a conflict, press-and-hold on Undo,
// Escape after a measure (openspec/changes/m4-scripting tasks 11.2-11.7).
//
// Steps, separated by ';':
//   move X,Y              the pointer moves to window point (X, Y)
//   down X,Y [BUTTON]     moves there and presses BUTTON (left, right, middle)
//   up X,Y [BUTTON]       moves there and releases it
//   key [MOD+]...NAME     presses and releases a key (Escape, Return, Tab,
//                         Space, Backspace, Delete, Up, Down, Left, Right,
//                         F1-F12, a letter or a digit); MOD is shift, ctrl,
//                         alt or super
//   text CHARACTERS       types the rest of the step as text
//   wait MS               the following steps run MS milliseconds later
//
// Toolkit-free and unit-tested; MainWindow plays it frame by frame.
//
// Owner: SHELL (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hefesto::shell {

enum class ScriptKind : unsigned char { move, down, up, key, text, wait };

/// Raven's RMouseButton values.
enum class ScriptButton : unsigned char { left = 0, right = 1, middle = 2 };

struct InputStep {
  ScriptKind kind = ScriptKind::wait;
  float x = 0.0F;  // window point, logical pixels (move, down, up)
  float y = 0.0F;
  ScriptButton button = ScriptButton::left;
  int key = 0;                 // Raven key code (key)
  std::uint8_t modifiers = 0;  // NativeModifier bits (key)
  std::string text;            // text
  int wait_ms = 0;             // wait
  friend bool operator==(const InputStep&, const InputStep&) = default;
};

struct InputScriptParse {
  std::vector<InputStep> steps;
  std::string error;  // empty when the script parsed
};

/// Parses an --input script; `error` names the first bad step.
[[nodiscard]] InputScriptParse parse_input_script(std::string_view text);

/// Raven's key code for a key name (Escape, Return, F5, a, 7, ...).
[[nodiscard]] std::optional<int> input_key_code(std::string_view name);

/// Plays a script by time: each call returns the steps due, up to the next
/// wait that has not elapsed.
class InputPlayer {
 public:
  explicit InputPlayer(std::vector<InputStep> steps) : steps_(std::move(steps)) {}

  /// The steps to inject now; `now_ms` is a monotonic clock in milliseconds.
  [[nodiscard]] std::vector<InputStep> take_due(double now_ms);
  /// Every step ran and the last wait elapsed (seen by a take_due call).
  [[nodiscard]] bool done() const noexcept { return next_ >= steps_.size() && !resume_at_; }

 private:
  std::vector<InputStep> steps_;
  std::size_t next_ = 0;
  std::optional<double> resume_at_;
};

}  // namespace hefesto::shell
