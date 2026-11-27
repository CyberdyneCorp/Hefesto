// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pointer and key input of the schematic canvas (schematic_input.hpp; M8
// task 13.2): wheel, pinch and drag change only the view, a click selects
// the instance under the pointer, keys run the xschem-style bindings.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/shell/input_router.hpp"
#include "hefesto/shell/schematic_input.hpp"
#include "hefesto/ui/commands.hpp"

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
  CHECK(f.send(key('R', kModShift)).line == "schematic rotate 0 0");  // the view's centre, no pointer yet
  CHECK(f.send(key('F', kModShift)).line == "schematic flip 0 0");
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

  f.input.set_running(true);
  CHECK(f.send(key('C', kModCtrl)).interrupt);  // Ctrl-C while a netlist runs
  f.input.set_running(false);
  CHECK_FALSE(f.send(key('Z')).consumed);       // unbound

  // A rebinding through the core's `macro schematic ...` (the key-binding
  // sheet's lines) applies at once to the editor's table the canvas reads.
  hefesto::editor::EditorState editor;
  f.input.keys().attach(editor.macros);
  const std::vector<std::string> args{"schematic", "z", "schematic zoom_full"};
  REQUIRE(hefesto::ui::macro_command(editor, args, false, "layout").ok());
  CHECK(f.send(key('Z')).line == "schematic zoom_full");
}

TEST_CASE("Copy and Paste on the schematic canvas use the platform's chord",
          "[cap:schematic-editing][req:Clipboard]") {
  SECTION("Linux: Ctrl-C copies unless a netlist runs, Ctrl-V pastes at the pointer") {
    Fixture f;
    const SchematicInputResult copy = f.send(key('C', kModCtrl));
    CHECK(copy.copy);
    CHECK_FALSE(copy.interrupt);
    f.input.set_running(true);
    const SchematicInputResult stop = f.send(key('C', kModCtrl));
    CHECK(stop.interrupt);
    CHECK_FALSE(stop.copy);
    f.input.set_running(false);

    // Before the pointer came in: the view's centre.
    const SchematicInputResult centre = f.send(key('V', kModCtrl));
    REQUIRE(centre.paste_at.has_value());
    CHECK(centre.paste_at->x == Approx(f.viewport.centre().x));
    CHECK(centre.paste_at->y == Approx(f.viewport.centre().y));
    // Then where the pointer last was.
    CanvasInput move = pointer(InputKind::pointer_move, 150, 20);
    (void)f.send(move);
    const SchematicInputResult at = f.send(key('V', kModCtrl));
    REQUIRE(at.paste_at.has_value());
    const Point expected = f.viewport.to_schematic(CanvasPoint{150, 20});
    CHECK(at.paste_at->x == Approx(expected.x));
    CHECK(at.paste_at->y == Approx(expected.y));

    // Command-C is no copy off macOS; Ctrl-Shift-C is no copy either.
    CHECK_FALSE(f.send(key('C', kModSuper)).copy);
    CHECK_FALSE(f.send(key('C', kModCtrl | kModShift)).copy);
  }
  SECTION("macOS: Command-C copies and Command-V pastes; Ctrl-C stays the interrupt") {
    SchematicScene scene;
    SchematicViewport viewport;
    viewport.set_size(200, 100);
    SchematicCanvasInput input{true};
    CHECK(input.handle(key('C', kModSuper), viewport, scene).copy);
    CHECK(input.handle(key('V', kModSuper), viewport, scene).paste_at.has_value());
    const SchematicInputResult ctrl_c = input.handle(key('C', kModCtrl), viewport, scene);
    CHECK(ctrl_c.interrupt);
    CHECK_FALSE(ctrl_c.copy);
  }
}

namespace {

void bind(hefesto::editor::EditorState& editor, const std::string& key_name, const std::string& text) {
  const std::vector<std::string> args{"schematic", key_name, text};
  REQUIRE(hefesto::ui::macro_command(editor, args, false, "layout").ok());
}

}  // namespace

// Regression (M8 round 6 review): `R` and `F` ran `schematic rotate` and
// `schematic flip` with no pivot, which a run without a pointer turns
// around the origin with xschem-compat ON; in the editor the pivot is the
// snapped pointer position ("Rotate and flip").
TEST_CASE("R and F on the canvas name the snapped pointer position as the pivot",
          "[cap:schematic-editing][req:Rotate and flip]") {
  Fixture f;
  (void)f.send(pointer(InputKind::pointer_move, 150, 20));
  const Point at = f.viewport.to_schematic(CanvasPoint{150, 20});
  const auto snapped = [](double v) { return std::to_string(std::llround(v / 10.0) * 10); };
  const std::string pivot = snapped(at.x) + " " + snapped(at.y);
  CHECK(f.send(key('R', kModShift)).line == "schematic rotate " + pivot);
  CHECK(f.send(key('F', kModShift)).line == "schematic flip " + pivot);
  // A rebound line is run as written.
  hefesto::editor::EditorState editor;
  f.input.keys().attach(editor.macros);
  bind(editor, "R", "schematic rotate -inplace");
  CHECK(f.send(key('R', kModShift)).line == "schematic rotate -inplace");
}

// Regression (M8 round 6 review): the Copy and Paste chords were checked
// before the `schematic` table, so a binding of Control_c, Control_v or (on
// macOS) Meta_c, Meta_v was ignored although the keys are rebindable (D15).
TEST_CASE("a binding of the Copy and Paste chords comes before the clipboard",
          "[cap:schematic-editing][req:Desktop and touch interaction]") {
  SECTION("Linux") {
    Fixture f;
    hefesto::editor::EditorState editor;
    f.input.keys().attach(editor.macros);
    bind(editor, "Control_c", "schematic select all");
    bind(editor, "Control_v", "schematic undo");
    const SchematicInputResult c = f.send(key('C', kModCtrl));
    CHECK(c.line == "schematic select all");
    CHECK_FALSE(c.copy);
    const SchematicInputResult v = f.send(key('V', kModCtrl));
    CHECK(v.line == "schematic undo");
    CHECK_FALSE(v.paste_at.has_value());
    f.input.set_running(true);  // the interrupt is not rebindable
    CHECK(f.send(key('C', kModCtrl)).interrupt);
  }
  SECTION("macOS") {
    SchematicScene scene;
    SchematicViewport viewport;
    viewport.set_size(200, 100);
    SchematicCanvasInput input{true};
    hefesto::editor::EditorState editor;
    input.keys().attach(editor.macros);
    bind(editor, "Meta_c", "schematic select all");
    const SchematicInputResult c = input.handle(key('C', kModSuper), viewport, scene);
    CHECK(c.line == "schematic select all");
    CHECK_FALSE(c.copy);
    CHECK(input.handle(key('V', kModSuper), viewport, scene).paste_at.has_value());  // unbound: Paste
    CHECK(input.handle(key('C', kModCtrl), viewport, scene).interrupt);
  }
}
