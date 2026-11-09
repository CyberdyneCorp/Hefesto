// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/canvas_controller.hpp"

#include <cmath>
#include <cstdio>
#include <optional>
#include <utility>

namespace hefesto::shell {

namespace {

// Pixel counts as short command arguments ("100", "12.5").
std::string format_pixels(double pixels) {
  char buffer[32];
  std::snprintf(buffer, sizeof buffer, "%g", pixels);
  return buffer;
}

}  // namespace

bool CanvasController::handle(const CanvasInput& input) {
  switch (input.kind) {
    case InputKind::pointer_down:
      dragging_ = input.button == PointerButton::middle || input.button == PointerButton::right;
      return dragging_;
    case InputKind::pointer_up: {
      const bool was = dragging_;
      dragging_ = false;
      return was;
    }
    case InputKind::pointer_move:
      if (!dragging_) {
        return false;
      }
      camera_.pan_pixels(input.dx * ratio_, input.dy * ratio_);
      return true;
    case InputKind::wheel:
      return handle_wheel(input);
    case InputKind::key_down:
      return handle_key(input.key);
    case InputKind::other:
      break;
  }
  return false;
}

bool CanvasController::handle_wheel(const CanvasInput& input) noexcept {
  if (input.precise_wheel) {
    // Content follows the fingers.
    camera_.pan_pixels(input.wheel_x * kTrackpadLinePixels * ratio_, input.wheel_y * kTrackpadLinePixels * ratio_);
    return true;
  }
  if (input.wheel_y == 0.0) {
    return false;
  }
  camera_.zoom_at(std::pow(kWheelZoomStep, input.wheel_y), input.x * ratio_, input.y * ratio_);
  return true;
}

bool CanvasController::run_command(std::string name, std::vector<std::string> args) {
  if (commands_ == nullptr) {
    return false;
  }
  const cmd::Invocation invocation{std::move(name), std::move(args), cmd::Origin::key, std::nullopt};
  return commands_->execute(invocation).ok();
}

bool CanvasController::handle_key(CanvasKey key) {
  const std::string step_x = format_pixels(camera_.viewport_width() / 8.0);
  const std::string step_y = format_pixels(camera_.viewport_height() / 8.0);
  switch (key) {
    case CanvasKey::left: return run_command("pan", {step_x, "0"});
    case CanvasKey::right: return run_command("pan", {"-" + step_x, "0"});
    case CanvasKey::up: return run_command("pan", {"0", step_y});
    case CanvasKey::down: return run_command("pan", {"0", "-" + step_y});
    // Magic's `zoom`: a factor below 1 zooms in, above 1 zooms out.
    case CanvasKey::zoom_in: return run_command("zoom", {"0.5"});
    case CanvasKey::zoom_out: return run_command("zoom", {"2"});
    case CanvasKey::fit: return run_command("view", {});
    case CanvasKey::other: break;
  }
  return false;
}

}  // namespace hefesto::shell
