// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Schematic canvas input (schematic_input.hpp). Owner: SHELL.

#include "hefesto/shell/schematic_input.hpp"

#include <cmath>

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

bool pans(const CanvasInput& input) {
  return input.button == PointerButton::middle || (input.modifiers & kModAlt) != 0;
}

}  // namespace

void SchematicCanvasInput::reset_view() noexcept {
  user_view_ = false;
  dragging_ = false;
  selected_.clear();
}

SchematicInputResult SchematicCanvasInput::handle(const CanvasInput& input, SchematicViewport& viewport,
                                                  const SchematicScene& scene) {
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
    case InputKind::key_down: return key(input);
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

SchematicInputResult SchematicCanvasInput::key(const CanvasInput& input) {
  const auto event = key_event_for(input.native_key, input.modifiers);
  if (!event) return {};
  if (ui::is_interrupt_key(*event)) {
    SchematicInputResult r = consumed();
    r.interrupt = true;
    return r;
  }
  const auto action = keys_.lookup(*event, macos_);
  return action ? bound(*action) : SchematicInputResult{};
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
