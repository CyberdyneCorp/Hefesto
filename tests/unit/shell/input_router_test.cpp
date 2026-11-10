// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The canvas input path (capability hefesto-input-and-touch "Desktop input
// parity with Magic"; ui-tools-and-input "Key event normalization", "Box
// tool buttons"): native events, as the Raven wrapper translates them, go
// through the macro tables of a real core session.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/render/camera.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/input_router.hpp"
#include "hefesto/ui/state.hpp"

using namespace hefesto::shell;
namespace db = hefesto::db;
namespace ui = hefesto::ui;

namespace {

hefesto::platform::PortableOptions test_options() {
  return hefesto::platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true,
                                           .inline_tasks = true};
}

/// A session with a cell "top" in the current layout window, snap internal,
/// and the box at (0,0)-(10,10).
struct Fixture {
  CoreSession core{test_options()};
  hefesto::view::WindowId window = -1;

  Fixture() {
    auto& ed = core.editor();
    db::CellDef* top = ed.cells.create("top", ed.technology->plane_count()).value();
    ed.edit_cell = top;
    auto w = ed.windows.open(top, hefesto::view::ScreenRect{0, 0, 499, 499});
    REQUIRE(w.ok());
    window = w.value()->id();
    ed.windows.set_current(window);
    REQUIRE(run("snap internal"));
    REQUIRE(run("box 0 0 10 10"));
  }
  bool run(const std::string& line) {
    CoreCommandSink sink(core.dispatcher());
    return sink.submit(line).ok();
  }
  [[nodiscard]] PointerContext at(db::Coord x, db::Coord y) const {
    PointerContext p;
    p.state.window = window;
    p.state.point = db::Point{x, y};
    p.state.pixel_area = db::Rect{x, y, x, y};
    p.pixel = db::Point{x, y};
    p.window_name = "layout1";
    return p;
  }
  [[nodiscard]] db::Rect box() { return core.editor().box.value_or(db::Rect{}); }
  [[nodiscard]] std::vector<std::string> log() const { return core.command_log().history(); }
};

ui::KeyEvent key(int native, std::uint8_t mods = 0) {
  const auto event = key_event_for(native, mods);
  REQUIRE(event.has_value());
  return *event;
}

}  // namespace

TEST_CASE("native keys become X11 key events on a US layout",
          "[cap:ui-tools-and-input][req:Key event normalization]") {
  CHECK(key('A').keysym == 'a');
  CHECK(key('A').text == "a");
  CHECK(key('A', kModShift).keysym == 'A');
  CHECK(key('A', kModShift).state == 1U);
  CHECK(key('=', kModShift).text == "+");
  CHECK(key('1', kModShift).text == "!");
  CHECK(key('A', kModCtrl).text == "\x01");  // XLookupString's control character
  CHECK(key(raven_key::kLeft).keysym == 0xFF51U);
  CHECK(key(raven_key::kLeft).text.empty());
  CHECK(key(raven_key::kF1 + 4).keysym == 0xFFC2U);  // F5
  CHECK(key(raven_key::kKp0 + 2).keysym == 0xFFB2U);
  CHECK(key(raven_key::kEnter).text == "\r");
  CHECK(key('A', kModAlt).state == 8U);              // Option is Mod1
  CHECK(key('A', kModSuper).state == ui::kMod4);     // Command is Mod4 (Meta on macOS)
  CHECK(key(raven_key::kSpace, kModCtrl).text.empty());  // Control_XK_space keeps its keysym
  CHECK_FALSE(key_event_for(5, 0).has_value());
  // The normalized macro keys.
  CHECK(ui::normalize_key_event(key('A', kModShift), true) == ui::Key{'A', 0});
  CHECK(ui::normalize_key_event(key('A', kModCtrl), true) == ui::Key{1, 0});
  CHECK_FALSE(ui::normalize_key_event(key(raven_key::kLeftShift, kModShift), true).has_value());
}

TEST_CASE("pointer context maps canvas pixels to the root and to Magic's window pixels",
          "[cap:hefesto-input-and-touch][req:Desktop input parity with Magic]") {
  hefesto::render::Camera camera(100.0, 50.0, 2.0);  // 2 framebuffer pixels per unit
  camera.set_viewport(400, 200);
  const PointerContext at = pointer_at(camera, 2.0, 100.0, 50.0, 3, "layout3");  // logical = framebuffer / 2
  CHECK(at.state.window == 3);
  CHECK(at.state.point == db::Point{100, 50});  // the center of the view
  CHECK(at.pixel == db::Point{200, 99});       // y counts up from the bottom edge
  CHECK(at.state.pixel_area.x0 <= 100); CHECK(at.state.pixel_area.x1 >= 100);
  CHECK(at.window_name == "layout3");
}

TEST_CASE("the pointer's pixel area follows the core's screen-to-surface rule at any zoom",
          "[cap:hefesto-input-and-touch][req:Desktop input parity with Magic]") {
  // Regression (M2 review): zoomed in, the area was zero units wide; zoomed
  // out, two units narrower than WindScreenToSurface's units-per-pixel + 1.
  hefesto::render::Camera close(100.0, 50.0, 4.0);  // 4 pixels per unit
  close.set_viewport(400, 200);
  const PointerContext in = pointer_at(close, 1.0, 200.0, 100.0, 1, "layout1");
  CHECK(in.state.pixel_area == db::Rect{100, 49, 101, 50});
  hefesto::render::Camera far(1000.0, 500.0, 0.1);  // 10 units per pixel
  far.set_viewport(400, 200);
  const PointerContext out = pointer_at(far, 1.0, 200.0, 100.0, 1, "layout1");
  CHECK(out.state.pixel_area.width() == 11);
  CHECK(out.state.pixel_area.height() == 11);
  CHECK(out.state.pixel_area.x0 == 1000);
  CHECK(out.state.pixel_area.y0 == 490);
}

TEST_CASE("left and right buttons run the box tool's macros at the pointer",
          "[cap:hefesto-input-and-touch][req:Desktop input parity with Magic]") {
  Fixture f;
  InputRouter router(f.core.dispatcher(), f.core.editor());
  // Scenario "Box tool buttons": the left button moves the box by its lower-left corner.
  CHECK(router.button(1, true, 0, f.at(20, 30)));
  CHECK(f.box() == db::Rect{20, 30, 30, 40});
  CHECK_FALSE(router.button(1, false, 0, f.at(20, 30)));  // the macro ran on the press
  // The right button moves the upper-right corner.
  CHECK(router.button(3, true, 0, f.at(50, 60)));
  CHECK(f.box() == db::Rect{20, 30, 50, 60});
  // Shift-left moves the lower-left corner (resize).
  CHECK(router.button(1, true, kModShift, f.at(0, 0)));
  CHECK(f.box() == db::Rect{0, 0, 50, 60});
  // The command point is the pointer, and every command is logged with origin key.
  REQUIRE(f.core.editor().command_point.has_value());
  CHECK(f.core.editor().command_point->point == db::Point{0, 0});
  CHECK(router.issued() == std::vector<std::string>{"*bypass box move bl cursor", "*bypass box corner ur cursor",
                                                    "*bypass box corner bl cursor"});
}

TEST_CASE("a button without a macro goes to the tool's handler as pushbutton",
          "[cap:ui-tools-and-input][req:Box tool buttons]") {
  Fixture f;
  REQUIRE(f.run("macro Button1 \"\""));  // delete the box tool's left-button macro
  InputRouter router(f.core.dispatcher(), f.core.editor());
  CHECK(router.button(1, true, 0, f.at(20, 30)));
  CHECK(ui::state(f.core.editor()).cursor_glyph == 6);  // "box with lower-left corner"
  CHECK(router.button(1, false, 0, f.at(20, 30)));
  CHECK(f.box() == db::Rect{20, 30, 30, 40});  // released: moved, not resized
  CHECK(ui::state(f.core.editor()).cursor_glyph == 0);
  CHECK(router.issued() == std::vector<std::string>{"pushbutton left down", "pushbutton left up"});
}

TEST_CASE("keys run the current tool's macros; `:` and imacros open the command bar",
          "[cap:hefesto-input-and-touch][req:Desktop input parity with Magic]") {
  Fixture f;
  InputRouter router(f.core.dispatcher(), f.core.editor());
  std::string prompt = "unset";
  router.on_prompt([&](const std::string& text) { prompt = text; });
  // Control-Right: `box grow e 1i`.
  CHECK(router.key(key(raven_key::kRight, kModCtrl), f.at(5, 5)));
  CHECK(f.box() == db::Rect{0, 0, 11, 10});
  // `p` is an interactive macro: the command bar opens with "paint ".
  CHECK(router.key(key('P'), std::nullopt));
  CHECK(prompt == "paint ");
  CHECK(router.key(key(';', kModShift), std::nullopt));  // `:`
  CHECK(prompt.empty());
  // Space cycles the tools; Shift-space returns to the box tool.
  CHECK(router.key(key(raven_key::kSpace), std::nullopt));
  CHECK(f.core.editor().tools.current()->name == "wiring");
  CHECK(router.key(key(raven_key::kSpace, kModShift), std::nullopt));
  CHECK(f.core.editor().tools.current()->name == "box");
  CHECK(router.key(key(raven_key::kSpace, kModCtrl), std::nullopt));
  CHECK(f.core.editor().tools.current()->name == "wiring");
  // A modifier alone does nothing; an unbound key reports UI-KEY-001.
  CHECK_FALSE(router.key(key(raven_key::kLeftShift, kModShift), std::nullopt));
  CHECK(router.key(key(raven_key::kF12, kModCtrl | kModShift), std::nullopt));
  const auto log = f.log();
  CHECK(std::find(log.begin(), log.end(), "box grow e 1i") != log.end());
  CHECK(std::find(log.begin(), log.end(), "tool wiring") != log.end());
}

TEST_CASE("pointer motion runs the wiring preview and the pick tool's drag",
          "[cap:hefesto-input-and-touch][req:Desktop input parity with Magic]") {
  Fixture f;
  InputRouter router(f.core.dispatcher(), f.core.editor());
  CHECK_FALSE(router.motion(f.at(1, 1)));  // nothing tracks the pointer
  CHECK(router.issued().empty());
  f.core.editor().wire.tracking = true;
  (void)router.motion(f.at(4, 6));
  REQUIRE(router.issued().size() == 1U);
  CHECK(router.issued().back() == "*bypass wire show");
  CHECK(f.core.editor().command_point->point == db::Point{4, 6});
  f.core.editor().wire.tracking = false;
  // tools.tcl startselect: `*bypass select move` by the pointer's offset from the origin.
  ui::UiState& ui = ui::state(f.core.editor());
  ui.motion = "*bypass select move %Xi %Yi";
  ui.drag_origin = db::Point{10, 10};
  (void)router.motion(f.at(15, 7));
  CHECK(router.issued().back() == "*bypass select move 5i -3i");
}

TEST_CASE("the pick tool's buttons substitute the window name",
          "[cap:ui-tools-and-input][req:Other tools' buttons]") {
  Fixture f;
  REQUIRE(f.run("tool pick"));
  InputRouter router(f.core.dispatcher(), f.core.editor());
  CHECK(router.button(1, true, 0, f.at(40, 40)));
  CHECK(router.issued().back() == "keepselect layout1");
  CHECK(f.box() == db::Rect{40, 40, 50, 50});  // no drag: keepselect moves the box
  CHECK(router.has_button_macro(2, 0));
  CHECK_FALSE(router.has_button_macro(1, kModAlt));  // Option-drag pans
}

TEST_CASE("paint, label and measure modes sweep the box and issue commands on release",
          "[cap:hefesto-input-and-touch][req:Desktop input parity with Magic]") {
  Fixture f;
  InputRouter router(f.core.dispatcher(), f.core.editor());
  std::string prompt;
  router.on_prompt([&](const std::string& text) { prompt = text; });
  router.set_mode(EditMode::paint);
  router.set_paint_layer("m1");
  CHECK(router.button(1, true, 0, f.at(30, 40)));
  CHECK(router.motion(f.at(5, 8)));
  REQUIRE(router.sweep().has_value());
  CHECK(*router.sweep() == db::Rect{5, 8, 30, 40});
  CHECK(router.button(1, false, 0, f.at(5, 8)));
  CHECK_FALSE(router.sweep().has_value());
  CHECK(f.box() == db::Rect{5, 8, 30, 40});
  CHECK(router.issued().back() == "box values 5i 8i 30i 40i ; paint m1");
  // Option erases.
  (void)router.button(1, true, 0, f.at(0, 0));
  (void)router.button(1, false, kModAlt, f.at(2, 2));
  CHECK(router.issued().back() == "box values 0i 0i 2i 2i ; erase m1");
  // Escape cancels a sweep.
  (void)router.button(1, true, 0, f.at(0, 0));
  CHECK(router.key(key(raven_key::kEscape), std::nullopt));
  CHECK_FALSE(router.button(1, false, 0, f.at(9, 9)));
  // Label: the command bar opens with "label " on the swept box.
  router.set_mode(EditMode::label);
  (void)router.button(1, true, 0, f.at(1, 1));
  (void)router.button(1, false, 0, f.at(3, 3));
  CHECK(prompt == "label ");
  CHECK(f.box() == db::Rect{1, 1, 3, 3});
  router.set_mode(EditMode::measure);
  (void)router.button(1, true, 0, f.at(0, 0));
  (void)router.button(1, false, 0, f.at(4, 0));
  CHECK(router.issued().back() == "box values 0i 0i 4i 0i ; measure");
  // The other buttons keep the box tool's macros in a mode.
  CHECK(router.button(3, true, 0, f.at(20, 20)));
  CHECK(router.issued().back() == "*bypass box corner ur cursor");
}
