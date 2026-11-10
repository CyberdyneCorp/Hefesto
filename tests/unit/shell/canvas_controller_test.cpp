// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/shell/canvas_controller.hpp"

using Catch::Approx;
using hefesto::render::Camera;
using namespace hefesto::shell;

namespace {

CanvasInput pointer(InputKind kind, PointerButton button, double dx = 0.0, double dy = 0.0,
                    std::uint8_t modifiers = 0) {
  CanvasInput in;
  in.kind = kind;
  in.button = button;
  in.modifiers = modifiers;
  in.dx = dx;
  in.dy = dy;
  return in;
}

}  // namespace

TEST_CASE("an Option drag pans; plain buttons are left to the macro tables",
          "[cap:hefesto-input-and-touch][req:Desktop canvas gestures]") {
  Camera cam(0.0, 0.0, 1.0);
  cam.set_viewport(200, 100);
  CanvasController ctl(cam);
  // Middle and right are Magic's paint and box-corner buttons (InputRouter).
  for (const PointerButton b : {PointerButton::left, PointerButton::middle, PointerButton::right}) {
    CHECK_FALSE(ctl.handle(pointer(InputKind::pointer_down, b)));
    CHECK_FALSE(ctl.handle(pointer(InputKind::pointer_move, PointerButton::other, 10.0, 0.0)));
    CHECK(cam.center_x() == 0.0);
    CHECK_FALSE(ctl.handle(pointer(InputKind::pointer_up, b)));
  }
  CanvasInput key;
  key.kind = InputKind::key_down;
  CHECK_FALSE(ctl.handle(key));

  CHECK(ctl.handle(pointer(InputKind::pointer_down, PointerButton::middle, 0.0, 0.0, kModAlt)));
  CHECK(ctl.handle(pointer(InputKind::pointer_move, PointerButton::other, 10.0, -4.0)));
  CHECK(cam.center_x() == Approx(-10.0));
  CHECK(cam.center_y() == Approx(-4.0));
  CHECK(ctl.handle(pointer(InputKind::pointer_up, PointerButton::middle)));
  CHECK_FALSE(ctl.dragging());
}

TEST_CASE("wheel zooms at the cursor; trackpad pans", "[cap:hefesto-architecture][req:App shells per platform]") {
  Camera cam(500.0, 500.0, 1.0);
  cam.set_viewport(100, 100);
  CanvasController ctl(cam);
  ctl.set_pixel_ratio(2.0);  // events in logical pixels, camera in framebuffer pixels
  CanvasInput wheel;
  wheel.kind = InputKind::wheel;
  wheel.x = 10.0;
  wheel.y = 15.0;
  wheel.wheel_y = 1.0;
  const double wx = cam.world_x(20.0);
  const double wy = cam.world_y(30.0);
  CHECK(ctl.handle(wheel));
  CHECK(cam.pixels_per_unit() == Approx(CanvasController::kWheelZoomStep));
  CHECK(cam.world_x(20.0) == Approx(wx));
  CHECK(cam.world_y(30.0) == Approx(wy));

  wheel.precise_wheel = true;
  wheel.wheel_y = 0.0;
  wheel.wheel_x = 0.5;
  const double before = cam.center_x();
  CHECK(ctl.handle(wheel));
  CHECK(cam.center_x() < before);  // content follows the fingers to the right
}

namespace {

CanvasInput pinch(GesturePhase phase, double x, double y, double scale, double dx = 0.0, double dy = 0.0) {
  CanvasInput in;
  in.kind = InputKind::pinch;
  in.phase = phase;
  in.x = x;
  in.y = y;
  in.scale = scale;
  in.dx = dx;
  in.dy = dy;
  return in;
}

}  // namespace

TEST_CASE("pinch zooms continuously about the fingers", "[cap:hefesto-input-and-touch][req:Desktop canvas gestures]") {
  Camera cam(500.0, 500.0, 1.0);
  cam.set_viewport(400, 300);
  CanvasController ctl(cam);
  ctl.set_pixel_ratio(2.0);  // fingers in logical pixels, camera in framebuffer pixels
  const double wx = cam.world_x(60.0);
  const double wy = cam.world_y(80.0);

  CHECK(ctl.handle(pinch(GesturePhase::begin, 30.0, 40.0, 1.0)));  // taken, nothing moves yet
  CHECK(cam.pixels_per_unit() == Approx(1.0));
  // Many small steps: the zoom follows the fingers, no 2x jumps.
  for (int i = 0; i < 10; ++i) {
    CHECK(ctl.handle(pinch(GesturePhase::update, 30.0, 40.0, std::pow(2.0, 0.1))));
    CHECK(cam.world_x(60.0) == Approx(wx));  // the point under the fingers stays there
    CHECK(cam.world_y(80.0) == Approx(wy));
  }
  CHECK(cam.pixels_per_unit() == Approx(2.0));
  CHECK(ctl.handle(pinch(GesturePhase::end, 30.0, 40.0, 1.0)));
  CHECK(cam.pixels_per_unit() == Approx(2.0));

  // Fingers that pinch while they move: the point under them moves with them.
  const double mx = cam.world_x(60.0);
  const double my = cam.world_y(80.0);
  CHECK(ctl.handle(pinch(GesturePhase::update, 40.0, 35.0, 0.5, 10.0, -5.0)));
  CHECK(cam.pixels_per_unit() == Approx(1.0));
  CHECK(cam.world_x(80.0) == Approx(mx));
  CHECK(cam.world_y(70.0) == Approx(my));
}

TEST_CASE("pinch and wheel zoom stop at the zoom buttons' limits",
          "[cap:hefesto-input-and-touch][req:Desktop canvas gestures]") {
  Camera cam(0.0, 0.0, 1.0);
  cam.set_viewport(200, 200);
  CanvasController ctl(cam);
  const double wx = cam.world_x(50.0);
  CHECK(ctl.handle(pinch(GesturePhase::update, 50.0, 50.0, 1e9)));
  CHECK(cam.pixels_per_unit() == Approx(CanvasController::kMaxPixelsPerUnit));
  CHECK(cam.world_x(50.0) == Approx(wx));  // clamped zoom keeps its anchor
  CHECK(ctl.handle(pinch(GesturePhase::update, 50.0, 50.0, 1e-12)));
  CHECK(cam.pixels_per_unit() == Approx(CanvasController::kMinPixelsPerUnit));
  CanvasInput wheel;
  wheel.kind = InputKind::wheel;
  wheel.wheel_y = -50.0;  // far out
  CHECK(ctl.handle(wheel));
  CHECK(cam.pixels_per_unit() == Approx(CanvasController::kMinPixelsPerUnit));
  // The minimum is the core's smallest mapping scale (1 in 16.16 fixed point).
  CHECK(CanvasController::kMinPixelsPerUnit * 65536.0 == Approx(1.0));
  // Bad factors are ignored.
  CHECK(ctl.handle(pinch(GesturePhase::update, 50.0, 50.0, 0.0)));
  CHECK(cam.pixels_per_unit() == Approx(CanvasController::kMinPixelsPerUnit));
}

TEST_CASE("two fingers on a trackpad pan, a mouse wheel zooms",
          "[cap:hefesto-input-and-touch][req:Desktop canvas gestures]") {
  Camera cam(0.0, 0.0, 1.0);
  cam.set_viewport(200, 200);
  CanvasController ctl(cam);
  CanvasInput scroll;
  scroll.kind = InputKind::wheel;
  scroll.precise_wheel = true;  // REvent::wheel_precise (hasPreciseScrollingDeltas)
  scroll.wheel_y = 0.5;
  CHECK(ctl.handle(scroll));
  CHECK(cam.pixels_per_unit() == Approx(1.0));  // no zoom
  CHECK(cam.center_y() == Approx(0.5 * CanvasController::kTrackpadLinePixels));
  CanvasInput wheel = scroll;
  wheel.precise_wheel = false;
  wheel.wheel_y = 1.0;
  CHECK(ctl.handle(wheel));
  CHECK(cam.pixels_per_unit() == Approx(CanvasController::kWheelZoomStep));
}
