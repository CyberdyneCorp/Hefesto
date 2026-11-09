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
//   pinch (trackpad, touch)  zoom at the fingers, continuous
//   + / = ................... zoom in       - ................ zoom out
//   f / Home ................ fit the layout
//
// A wheel event is told from two fingers on a trackpad by Raven's
// REvent::wheel_precise (macOS: NSEvent hasPreciseScrollingDeltas): precise
// deltas pan, a real wheel zooms. Every continuous zoom (wheel, pinch) is
// clamped to the zoom range the core `zoom` command reaches (kMinPixelsPerUnit
// .. kMaxPixelsPerUnit).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/render/camera.hpp"
#include "hefesto/render/scene.hpp"

namespace hefesto::shell {

enum class InputKind : std::uint8_t { pointer_down, pointer_up, pointer_move, wheel, key_down, pinch, other };
/// Phase of a pinch (Raven's RGesturePhase).
enum class GesturePhase : std::uint8_t { begin, update, end };
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
  /// Pinch: (x, y) is the center of the fingers now, (dx, dy) how far it
  /// moved since the previous event and `scale` how much the distance
  /// between the fingers changed since then (RGestureEvent::scale_delta).
  double scale = 1.0;
  GesturePhase phase = GesturePhase::update;
  PointerButton button = PointerButton::other;
  CanvasKey key = CanvasKey::other;
};

class CanvasController {
 public:
  /// Zoom factor per wheel line.
  static constexpr double kWheelZoomStep = 1.15;
  /// Logical pixels per trackpad wheel line (Raven: 48 units per line).
  static constexpr double kTrackpadLinePixels = 48.0;
  /// Zoom range of continuous gestures, in framebuffer pixels per internal
  /// unit: the core ViewMapping's smallest scale (1 in 16.16 fixed point,
  /// "At minimum scale!") up to the camera's largest, the range the zoom
  /// buttons (`zoom 2` / `zoom 0.5`) reach.
  static constexpr double kMinPixelsPerUnit = 1.0 / 65536.0;
  static constexpr double kMaxPixelsPerUnit = render::Camera::kMaxPixelsPerUnit;

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
  bool handle_pinch(const CanvasInput& input) noexcept;
  /// Camera::zoom_at with the factor limited to the zoom range.
  void zoom_clamped(double factor, double x_px, double y_px) noexcept;

  render::Camera& camera_;
  cmd::Dispatcher* commands_ = nullptr;
  render::Bounds bounds_;
  double ratio_ = 1.0;
  bool dragging_ = false;
};

}  // namespace hefesto::shell
