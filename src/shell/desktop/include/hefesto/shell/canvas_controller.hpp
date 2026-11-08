// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Turns pointer, wheel and key input on the canvas viewport into camera
// moves. Toolkit-independent: the Raven wrapper translates its events into
// CanvasInput, so the mapping is unit-tested without a window.
//
// Key bindings are commands (capability hefesto-architecture, "Single command
// API"): each key issues `pan`, `zoom` or `view` through the core Dispatcher
// with origin `key`, so it is logged and replayable like a typed command.
// Continuous gestures (drag, wheel, trackpad) move the camera directly: one
// command per input event would flood the command log. They are view state,
// not document edits; design.md D6 records this M0 deviation.
//
// M0 bindings (provisional; hefesto-app-ux owns the final ones):
//   middle or right drag ... pan            wheel ............ zoom at cursor
//   trackpad two-finger .... pan            arrows ........... pan 1/8 view
//   + / = ................... zoom in       - ................ zoom out
//   f / Home ................ fit the layout

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/render/camera.hpp"
#include "hefesto/render/scene.hpp"

namespace hefesto::shell {

enum class InputKind : std::uint8_t { pointer_down, pointer_up, pointer_move, wheel, key_down, other };
enum class PointerButton : std::uint8_t { left, right, middle, other };
enum class CanvasKey : std::uint8_t { left, right, up, down, zoom_in, zoom_out, fit, other };

/// One input event in viewport-local logical pixels.
struct CanvasInput {
  InputKind kind = InputKind::other;
  double x = 0.0;
  double y = 0.0;
  double dx = 0.0;
  double dy = 0.0;
  double wheel_x = 0.0;
  double wheel_y = 0.0;  // positive = away from the user (up)
  bool precise_wheel = false;  // trackpad: pan instead of zoom
  PointerButton button = PointerButton::other;
  CanvasKey key = CanvasKey::other;
};

class CanvasController {
 public:
  /// Zoom factor per wheel line.
  static constexpr double kWheelZoomStep = 1.15;
  /// Logical pixels per trackpad wheel line (Raven: 48 units per line).
  static constexpr double kTrackpadLinePixels = 48.0;

  explicit CanvasController(render::Camera& camera) noexcept : camera_(camera) {}

  /// Sets the layout extent used by "fit".
  void set_bounds(const render::Bounds& bounds) noexcept { bounds_ = bounds; }
  /// Logical-to-framebuffer pixel ratio (Retina: 2).
  void set_pixel_ratio(double ratio) noexcept { ratio_ = ratio > 0.0 ? ratio : 1.0; }
  /// Dispatcher that runs the key bindings (with the view commands
  /// registered). Without one, keys are not handled.
  void set_commands(cmd::Dispatcher* commands) noexcept { commands_ = commands; }

  /// Applies `input`; returns true when the camera changed (or the event was consumed).
  bool handle(const CanvasInput& input);

  [[nodiscard]] bool dragging() const noexcept { return dragging_; }

 private:
  bool handle_key(CanvasKey key);
  bool run_command(std::string name, std::vector<std::string> args);
  bool handle_wheel(const CanvasInput& input) noexcept;

  render::Camera& camera_;
  cmd::Dispatcher* commands_ = nullptr;
  render::Bounds bounds_;
  double ratio_ = 1.0;
  bool dragging_ = false;
};

}  // namespace hefesto::shell
