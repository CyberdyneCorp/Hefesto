// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Schematic canvas input (schematic_input.hpp). Owner: SHELL.

#include "hefesto/shell/schematic_input.hpp"

#include <cmath>
#include <optional>
#include <string>

#include "hefesto/shell/input_router.hpp"
#include "hefesto/shell/manager_views.hpp"
#include "hefesto/shell/schematic_panels.hpp"
#include "hefesto/ui/input.hpp"

namespace hefesto::shell {

namespace {

SchematicInputResult consumed(bool repaint = false) {
  SchematicInputResult r;
  r.consumed = true;
  r.repaint = repaint;
  return r;
}

// Copy (C) and Paste (V) with the platform's clipboard modifier alone.
bool clipboard_chord(const CanvasInput& input, bool macos, int letter) {
  return input.native_key == letter && input.modifiers == (macos ? kModSuper : kModCtrl);
}

bool pans(const CanvasInput& input) {
  return input.button == PointerButton::middle || (input.modifiers & kModAlt) != 0;
}

constexpr double kPivotSnap = 10.0;  // xschem's default snap, as Place and Paste

// "Rotate and flip": in the editor the pivot is the snapped pointer
// position, so the default `R` and `F` lines name it (a script's
// `schematic rotate` without a pivot uses the origin with xschem-compat ON).
// Before the pointer came over the canvas, the view's centre (as Paste).
std::string with_pointer_pivot(const std::string& line, schem::Point at) {
  if (line != "schematic rotate" && line != "schematic flip") return line;
  const auto snapped = [](double v) {
    return std::to_string(std::llround(v / kPivotSnap) * static_cast<long long>(kPivotSnap));
  };
  return line + " " + snapped(at.x) + " " + snapped(at.y);
}

}  // namespace

void SchematicCanvasInput::reset_view() noexcept {
  user_view_ = false;
  dragging_ = false;
  selected_.clear();
}

SchematicInputResult SchematicCanvasInput::handle(const CanvasInput& input, SchematicViewport& viewport,
                                                  const SchematicScene& scene) {
  if (input.kind == InputKind::pointer_move || input.kind == InputKind::pointer_down) {
    pointer_ = CanvasPoint{input.x, input.y};
  }
  switch (input.kind) {
    case InputKind::pointer_down: return pointer_down(input, viewport, scene);
    case InputKind::pointer_up: {
      const bool was = dragging_;
      dragging_ = false;
      return was ? consumed() : SchematicInputResult{};
    }
    case InputKind::pointer_move:
      if (!dragging_) return {};
      viewport.pan_by(input.dx, input.dy);
      user_view_ = true;
      return consumed(true);
    case InputKind::wheel: return wheel(input, viewport);
    case InputKind::pinch:
      if (input.phase == GesturePhase::begin || !std::isfinite(input.scale) || !(input.scale > 0.0)) {
        return consumed();
      }
      // Zoom about where the fingers' centre was, then follow its move.
      viewport.zoom_at(input.scale, CanvasPoint{input.x - input.dx, input.y - input.dy});
      viewport.pan_by(input.dx, input.dy);
      user_view_ = true;
      return consumed(true);
    case InputKind::key_down: return key(input, viewport);
    case InputKind::other: break;
  }
  return {};
}

SchematicInputResult SchematicCanvasInput::pointer_down(const CanvasInput& input, const SchematicViewport& viewport,
                                                        const SchematicScene& scene) {
  if (pans(input)) {
    dragging_ = true;
    return consumed();
  }
  if (input.button != PointerButton::left) return {};
  const std::size_t hit = scene.instance_at(viewport.to_schematic(CanvasPoint{input.x, input.y}));
  if (hit >= scene.instances.size() || scene.instances[hit].name.empty()) return consumed();
  selected_ = scene.instances[hit].name;
  SchematicInputResult r = consumed();
  r.line = command_line({"schematic", "select", "instance", selected_});
  return r;
}

SchematicInputResult SchematicCanvasInput::wheel(const CanvasInput& input, SchematicViewport& viewport) {
  if (input.precise_wheel) {  // a trackpad: the drawing follows the fingers
    viewport.pan_by(input.wheel_x * CanvasController::kTrackpadLinePixels,
                    input.wheel_y * CanvasController::kTrackpadLinePixels);
    user_view_ = true;
    return consumed(true);
  }
  if (input.wheel_y == 0.0 || !std::isfinite(input.wheel_y)) return {};
  viewport.zoom_at(std::pow(CanvasController::kWheelZoomStep, input.wheel_y), CanvasPoint{input.x, input.y});
  user_view_ = true;
  return consumed(true);
}

// A binding of the `schematic` table comes before the Copy and Paste chords,
// so a user can rebind them; Ctrl-C while it interrupts (always on macOS,
// during a netlist run elsewhere) is not rebindable.
SchematicInputResult SchematicCanvasInput::key(const CanvasInput& input, const SchematicViewport& viewport) {
  const auto event = key_event_for(input.native_key, input.modifiers);
  const bool interrupt = event && ui::is_interrupt_key(*event);
  if (event && !(interrupt && (macos_ || running_))) {
    if (const auto action = keys_.lookup(*event, macos_)) {
      SchematicInputResult r = bound(*action);
      r.line = with_pointer_pivot(r.line, pointer_ ? viewport.to_schematic(*pointer_) : viewport.centre());
      return r;
    }
  }
  if (SchematicInputResult clip = clipboard_key(input, viewport); clip.consumed) return clip;
  if (!interrupt) return {};
  SchematicInputResult r = consumed();
  r.interrupt = true;
  return r;
}

SchematicInputResult SchematicCanvasInput::clipboard_key(const CanvasInput& input,
                                                         const SchematicViewport& viewport) const {
  if (clipboard_chord(input, macos_, 'C') && (macos_ || !running_)) {
    SchematicInputResult r = consumed();
    r.copy = true;
    return r;
  }
  if (!clipboard_chord(input, macos_, 'V')) return {};
  SchematicInputResult r = consumed();
  r.paste_at = pointer_ ? viewport.to_schematic(*pointer_) : viewport.centre();
  return r;
}

SchematicInputResult SchematicCanvasInput::bound(const BoundAction& action) const {
  SchematicInputResult r = consumed();
  if (!action.line.empty()) {
    r.line = action.line;
  } else if (action.action == "wire" || action.action == "move" || action.action == "copy") {
    r.prompt = "schematic " + action.action + " ";  // coordinates typed in the command bar
  } else if (action.action == "descend") {
    if (!selected_.empty()) r.line = HierarchyBreadcrumb::descend_command(selected_);
  } else {
    r.action = action.action;  // properties, place
  }
  return r;
}

}  // namespace hefesto::shell
