// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pointer and key input of the schematic canvas (schematic_input.hpp; M8
// task 13.2): wheel, pinch and drag change only the view, a click selects
// the instance under the pointer, keys run the xschem-style bindings.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/shell/input_router.hpp"
#include "hefesto/shell/schematic_input.hpp"

using namespace hefesto::shell;
namespace schem = hefesto::schem;
using Catch::Approx;
using schem::Point;

namespace {

// One instance R1 whose box is (-10,-30)..(10,30), centred in a 200 x 100 view.
struct Fixture {
  SchematicScene scene;
  SchematicViewport viewport;
  SchematicCanvasInput input{false};

  Fixture() {
    SceneInstance r1;
    r1.name = "R1";
    r1.box.add(Point{-10, -30});
    r1.box.add(Point{10, 30});
    scene.instances.push_back(r1);
    scene.box = r1.box;
    viewport.set_size(200, 100);
  }
  SchematicInputResult send(CanvasInput in) { return input.handle(in, viewport, scene); }
};

CanvasInput pointer(InputKind kind, double x, double y, PointerButton button = PointerButton::left,
                    std::uint8_t modifiers = 0) {
  CanvasInput in;
  in.kind = kind;
  in.x = x;
  in.y = y;
  in.button = button;
  in.modifiers = modifiers;
  return in;
}

CanvasInput key(int native, std::uint8_t modifiers = 0) {
  CanvasInput in;
  in.kind = InputKind::key_down;
  in.native_key = native;
  in.modifiers = modifiers;
  return in;
}

}  // namespace

TEST_CASE("the wheel and a pinch zoom the schematic about the pointer",
          "[cap:schematic-editing][req:Desktop and touch interaction]") {
  Fixture f;
  CHECK_FALSE(f.input.user_view());
  const Point under = f.viewport.to_schematic(CanvasPoint{150, 20});
  CanvasInput wheel;
  wheel.kind = InputKind::wheel;
  wheel.x = 150;
  wheel.y = 20;
  wheel.wheel_y = 1.0;
  const SchematicInputResult r = f.send(wheel);
  CHECK(r.consumed);
  CHECK(r.repaint);
  CHECK(r.line.empty());  // the view only: no command
  CHECK(f.viewport.zoom() == Approx(CanvasController::kWheelZoomStep));
  const Point after = f.viewport.to_schematic(CanvasPoint{150, 20});
  CHECK(after.x == Approx(under.x));
  CHECK(after.y == Approx(under.y));
  CHECK(f.input.user_view());

  CanvasInput pinch;
  pinch.kind = InputKind::pinch;
  pinch.phase = GesturePhase::begin;
  CHECK(f.send(pinch).consumed);
  pinch.phase = GesturePhase::update;
  pinch.scale = 2.0;
  pinch.x = 100;
  pinch.y = 50;
  CHECK(f.send(pinch).repaint);
  CHECK(f.viewport.zoom() == Approx(CanvasController::kWheelZoomStep * 2.0));

  pinch.scale = 0.0;  // a broken gesture changes nothing
  CHECK_FALSE(f.send(pinch).repaint);
  wheel.wheel_y = 0.0;
  CHECK_FALSE(f.send(wheel).consumed);

  f.input.reset_view();
  CHECK_FALSE(f.input.user_view());
}

TEST_CASE("a trackpad scroll and an Option or middle drag pan the schematic",
          "[cap:schematic-editing][req:Desktop and touch interaction]") {
  Fixture f;
  CanvasInput scroll;
  scroll.kind = InputKind::wheel;
  scroll.precise_wheel = true;
  scroll.wheel_x = 0.5;
  CHECK(f.send(scroll).repaint);
  // The drawing follows the fingers: the centre moves the other way.
  CHECK(f.viewport.centre().x == Approx(-0.5 * CanvasController::kTrackpadLinePixels));

  Fixture g;
  CHECK(g.send(pointer(InputKind::pointer_down, 50, 50, PointerButton::left, kModAlt)).consumed);
  CanvasInput move = pointer(InputKind::pointer_move, 60, 45);
  move.dx = 10;
  move.dy = -5;
  const SchematicInputResult r = g.send(move);
  CHECK(r.repaint);
  CHECK(r.line.empty());  // an Option-drag never selects
  CHECK(g.viewport.centre().x == Approx(-10.0));
  CHECK(g.viewport.centre().y == Approx(5.0));
  CHECK(g.send(pointer(InputKind::pointer_up, 60, 45)).consumed);
  CHECK_FALSE(g.send(move).consumed);  // the drag ended

  Fixture m;
  CHECK(m.send(pointer(InputKind::pointer_down, 50, 50, PointerButton::middle)).consumed);
  CHECK(m.send(move).repaint);
}

TEST_CASE("a click selects the instance under the pointer",
          "[cap:schematic-editing][req:Selection]") {
  Fixture f;
  const SchematicInputResult hit = f.send(pointer(InputKind::pointer_down, 100, 50));
  CHECK(hit.consumed);
  CHECK(hit.line == "schematic select instance R1");
  CHECK(f.input.selected() == "R1");
  const SchematicInputResult miss = f.send(pointer(InputKind::pointer_down, 190, 5));
  CHECK(miss.consumed);
  CHECK(miss.line.empty());
  CHECK_FALSE(f.send(pointer(InputKind::pointer_down, 100, 50, PointerButton::right)).consumed);
}

TEST_CASE("keys on the schematic canvas run the xschem-style bindings",
          "[cap:schematic-editing][req:Desktop and touch interaction]") {
  Fixture f;
  CHECK(f.send(key('R', kModShift)).line == "schematic rotate");
  CHECK(f.send(key('F', kModShift)).line == "schematic flip");
  CHECK(f.send(key('E', kModCtrl)).line == "schematic ascend");
  CHECK(f.send(key('N')).line == "schematic netlist");
  CHECK(f.send(key('W')).prompt == "schematic wire ");
  CHECK(f.send(key('M')).prompt == "schematic move ");
  CHECK(f.send(key('C')).prompt == "schematic copy ");
  CHECK(f.send(key('Q')).action == "properties");
  CHECK(f.send(key(raven_key::kInsert)).action == "place");

  // `e` descends into the selected instance; nothing is selected yet.
  const SchematicInputResult none = f.send(key('E'));
  CHECK(none.consumed);
  CHECK(none.line.empty());
  (void)f.send(pointer(InputKind::pointer_down, 100, 50));
  CHECK(f.send(key('E')).line == "schematic descend R1");

  CHECK(f.send(key('C', kModCtrl)).interrupt);  // Ctrl-C
  CHECK_FALSE(f.send(key('Z')).consumed);       // unbound

  // A rebinding through the key-binding sheet applies at once.
  REQUIRE(f.input.keys().apply(R"(macro schematic z "schematic zoom_full")"));
  CHECK(f.send(key('Z')).line == "schematic zoom_full");
}
