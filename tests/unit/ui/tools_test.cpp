// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tools, buttons, cursor, setpoint, key normalization and the touch mapping
// (capabilities ui-tools-and-input, ui-command-dispatch "pushbutton and
// setpoint", ui-gui-managers "Tool modes").

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/ui/bindings.hpp"
#include "hefesto/ui/input.hpp"
#include "hefesto/ui/tools.hpp"
#include "hefesto/view/window.hpp"
#include "ui_fixture.hpp"

namespace db = hefesto::db;
namespace ui = hefesto::ui;
using hefesto::test::UiSession;

namespace {

// A layout window whose pixels are layout units (scale 1, origin 0).
hefesto::view::LayoutWindow& unit_window(UiSession& s) {
  auto* def = s.editor.cells.create("top", s.editor.technology->plane_count()).value();
  auto w = s.editor.windows.open(def, hefesto::view::ScreenRect{0, 0, 499, 499});
  REQUIRE(w.ok());
  w.value()->move_to(db::Rect{0, 0, 499, 499});
  return *w.value();
}

}  // namespace

// --- Tool registry and tool command ------------------------------------------------

TEST_CASE("the box tool is current at startup with glyph 0", "[cap:ui-tools-and-input][req:Tool registry]") {
  UiSession s;
  ui::install_defaults(s.editor);
  const auto& tools = s.editor.tools.tools();
  REQUIRE(tools.size() == 4);
  CHECK(tools[0].name == "box");
  CHECK(tools[1].name == "wiring");
  CHECK(tools[1].cursor == 19);
  CHECK(tools[2].name == "nettool");
  CHECK(tools[2].cursor == 18);
  CHECK(tools[3].name == "pick");
  CHECK(tools[3].cursor == 22);
  CHECK(s.editor.tools.current()->name == "box");
  CHECK(ui::state(s.editor).cursor_glyph == 0);
  CHECK(ui::kMaxTools == 10);
}

TEST_CASE("tool cycles box, wiring, nettool, pick and reports the current tool",
          "[cap:ui-tools-and-input][req:tool command][cap:ui-gui-managers][req:Tool modes][golden:UI-G-046]") {
  UiSession s;
  // "Cycle tools".
  CHECK(UiSession::printed(s.run("tool")) == "Switching to WIRING tool.");
  CHECK(ui::state(s.editor).cursor_glyph == 19);
  s.run("tool");
  CHECK(s.run("tool type").value.to_string() == "nettool");
  s.run("tool next");
  CHECK(s.run("tool type").value.to_string() == "pick");
  CHECK(ui::state(s.editor).cursor_glyph == 22);
  s.run("tool");
  CHECK(s.run("tool type").value.to_string() == "box");
  // "Query the tool".
  s.run("tool wiring");
  CHECK(s.run("tool type").value.to_string() == "wiring");
  // Unknown names are ignored, as by the Tcl procedure.
  const auto foo = s.run("tool foo");
  CHECK(foo.ok());
  CHECK(s.run("tool type").value.to_string() == "wiring");
  // `tool info` prints the button bindings.
  const std::string info = UiSession::printed(s.run("tool info"));
  CHECK(info.starts_with("Current tool is wiring.\nButton command bindings:\n"));
  CHECK(info.find(R"(Macro 'XK_Pointer_Button1' contains "trackwire pick")") != std::string::npos);
  // Refused while a drag is in progress.
  ui::state(s.editor).motion = "*bypass wire show";
  s.run("tool box");
  CHECK(s.run("tool type").value.is_none());  // the Tcl procedure returns at once
  ui::state(s.editor).motion.clear();
  CHECK(s.run("tool type").value.to_string() == "wiring");
  CHECK(s.run("tool a b").raises);
}

// --- Box tool buttons -------------------------------------------------------------------

TEST_CASE("box tool buttons choose a corner and move or resize on release",
          "[cap:ui-tools-and-input][req:Box tool buttons]") {
  hefesto::editor::EditorState editor;
  const db::Rect box{0, 0, 10, 10};
  // "Left click moves box".
  CHECK_FALSE(ui::box_tool_button(editor, ui::Button::left, true, box, {20, 30}).has_value());
  CHECK(ui::state(editor).cursor_glyph == 6);
  auto cmd = ui::box_tool_button(editor, ui::Button::left, false, box, {20, 30});
  REQUIRE(cmd.has_value());
  CHECK(*cmd == std::vector<std::string>{"box", "move", "bl", "cursor"});
  CHECK(ui::state(editor).cursor_glyph == 0);
  // "Right click resizes".
  (void)ui::box_tool_button(editor, ui::Button::right, true, box, {25, 5});
  CHECK(ui::state(editor).cursor_glyph == 5);
  cmd = ui::box_tool_button(editor, ui::Button::right, false, box, {25, 5});
  CHECK(*cmd == std::vector<std::string>{"box", "corner", "tr", "cursor"});
  // "Move by upper-right corner": left, right near UR, release right, release left.
  (void)ui::box_tool_button(editor, ui::Button::left, true, box, {1, 1});
  (void)ui::box_tool_button(editor, ui::Button::right, true, box, {9, 9});
  CHECK(ui::state(editor).cursor_glyph == ui::corner_glyph(ui::Corner::tr, true));
  CHECK_FALSE(ui::box_tool_button(editor, ui::Button::right, false, box, {9, 9}).has_value());
  CHECK(ui::state(editor).cursor_glyph == 9);
  cmd = ui::box_tool_button(editor, ui::Button::left, false, box, {50, 50});
  CHECK(*cmd == std::vector<std::string>{"box", "move", "tr", "cursor"});
  // The middle button paints by example on press only.
  cmd = ui::box_tool_button(editor, ui::Button::middle, true, box, {5, 5});
  CHECK(*cmd == std::vector<std::string>{"paint", "cursor"});
  CHECK_FALSE(ui::box_tool_button(editor, ui::Button::middle, false, box, {5, 5}).has_value());
  // The glyph table.
  CHECK(ui::corner_glyph(ui::Corner::bl, false) == 2);
  CHECK(ui::corner_glyph(ui::Corner::tl, true) == 8);
  CHECK(ui::corner_name(ui::Corner::br) == "br");
}

// --- pushbutton and setpoint ----------------------------------------------------------------

TEST_CASE("setpoint forces the point and pushbutton clicks there",
          "[cap:ui-command-dispatch][req:pushbutton and setpoint][golden:UI-G-018][golden:UI-G-019]") {
  UiSession s;
  unit_window(s);
  s.editor.box = db::Rect{0, 0, 10, 10};
  // "Scripted click".
  const std::string wid = std::to_string(s.editor.windows.current()->id());
  REQUIRE(s.run("setpoint 100 120 " + wid).ok());
  REQUIRE(s.editor.command_point.has_value());
  CHECK(s.editor.command_point->point == db::Point{100, 120});
  CHECK(s.run("setpoint").value.to_string() == "100 120 100 120");
  REQUIRE(s.run("pushbutton left down").ok());
  REQUIRE(s.run("pushbutton l up").ok());
  CHECK(s.calls_of("box") == std::vector<std::string>{"box move bl cursor"});
  // Usage errors.
  CHECK(UiSession::first_error(s.run("pushbutton left")) == "UI-USAGE-001: Usage: pushbutton button action");
  CHECK(UiSession::first_error(s.run("pushbutton up left")) == "UI-USAGE-001: Usage: pushbutton button action");
  CHECK(UiSession::first_error(s.run("setpoint 1.5 2")) == "UI-USAGE-001: Usage: setpoint [x y [window ID|name]]");
  // A script keeps the point; an interactive command drops it.
  s.run("box", hefesto::cmd::Origin::script);
  CHECK(s.editor.command_point.has_value());
  s.run("box", hefesto::cmd::Origin::gui);
  CHECK_FALSE(s.editor.command_point.has_value());
}

TEST_CASE("setpoint without a window reports screen coordinates",
          "[cap:ui-command-dispatch][req:pushbutton and setpoint]") {
  UiSession s;
  s.run("setpoint 7 8");
  CHECK(UiSession::printed(s.run("setpoint")) == "Point is at screen coordinates (7, 8).");
}

// --- cursor command -------------------------------------------------------------------------

TEST_CASE("cursor sets the glyph or reports the pointer", "[cap:ui-tools-and-input][req:cursor command][golden:UI-G-048]") {
  UiSession s;
  // "Set glyph".
  REQUIRE(s.run("cursor 22").ok());
  CHECK(ui::state(s.editor).cursor_glyph == 22);
  const auto bogus = s.run("cursor bogus");
  REQUIRE(bogus.diagnostics.size() == 2);
  CHECK(bogus.diagnostics[0].text == "Usage: cursor glyphnum");
  CHECK(bogus.diagnostics[1].text == " (or): cursor [internal | lambda | microns | user | window | units]");
  // No pointer: nothing to report (the null display).
  CHECK(s.run("cursor").value.is_none());
  const std::string wid = std::to_string(unit_window(s).id());
  s.editor.snap = hefesto::editor::SnapMode::internal;
  s.run("setpoint 12 34 " + wid);
  CHECK(s.run("cursor").value.to_string() == "12 34");
  CHECK(s.run("cursor internal").value.to_string() == "12 34");
  CHECK(s.run("cursor window").value.to_string() == "12 34");
  // Lambda snapping with 3 internal units per lambda.
  s.editor.grid = hefesto::tech::GridScale{3, 1};
  s.editor.snap = hefesto::editor::SnapMode::lambda;
  s.run("setpoint 13 34 " + wid);
  CHECK(s.run("cursor internal").value.to_string() == "12 33");
}

// --- Other tools' buttons ------------------------------------------------------------------

TEST_CASE("the wiring, nettool and pick tables override the box buttons",
          "[cap:ui-tools-and-input][req:Other tools' buttons]") {
  UiSession s;
  ui::install_defaults(s.editor);
  const auto text = [&](std::string_view table, std::string_view k) {
    const ui::Macro* m = s.editor.macros.find(table, ui::parse_key_name(k).value());
    return m == nullptr ? std::string{"<none>"} : m->text;
  };
  CHECK(text("wiring", "Button1") == "trackwire pick");
  CHECK(text("wiring", "Shift_Button2") == "wire switch");
  CHECK(text("wiring", "Button4") == "wire incr width ; wire show");
  CHECK(text("wiring", "s") == "select");  // copied from the box table
  CHECK(text("nettool", "Button2") == "netlist join");
  CHECK(text("nettool", "Shift_Button1") == "<none>");
  CHECK(text("pick", "Button1") == "keepselect %W");
  CHECK(text("pick", "Shift_Button2") == "startselect %W copy");
  CHECK(text("pick", "Shift_Button3") == "box move ur cursor");
  CHECK(ui::substitute_window("keepselect %W", ".layout0") == "keepselect .layout0");
  // Netlist actions report the missing module.
  CHECK(UiSession::first_error(s.run("netlist select")).starts_with("UI-CLIENT-002:"));
  // The pick tool helpers: box gestures outside a drag, drag start and end.
  s.run(ui::substitute_window(text("pick", "Button1"), "0"));
  CHECK(s.calls_of("box").back() == "box move bl cursor");
  s.run("startselect 0 pick");
  CHECK(s.calls_of("select").back() == "select pick");
  CHECK_FALSE(ui::state(s.editor).motion.empty());
  CHECK(ui::state(s.editor).cursor_glyph == 21);
  s.run("startselect 0 pick");
  CHECK(s.calls_of("copy").back() == "copy center 0");
  CHECK(ui::state(s.editor).motion.empty());
  s.run("cancelselect 0");
  CHECK(s.calls_of("box").back() == "box corner ur cursor");
  s.run("startselect 0 copy");
  CHECK(s.calls_of("select").back() == "select nocycle");
  s.run("keepselect 0");
  CHECK(s.calls_of("select").back() == "select keep");
  s.run("cancelselect 0");
  CHECK(s.calls_of("select").back() == "select clear");
}

// --- Key event normalization ----------------------------------------------------------------

TEST_CASE("native key events become macro keys", "[cap:ui-tools-and-input][req:Key event normalization]") {
  using ui::KeyEvent;
  // Modifier-only presses are ignored.
  CHECK_FALSE(ui::normalize_key_event(KeyEvent{0xFFE1, ui::kShift, ""}, false).has_value());
  // A lone Shift or Control is dropped from ASCII keys; Ctrl+letter is a control code.
  CHECK(*ui::normalize_key_event(KeyEvent{'A', ui::kShift, "A"}, false) == ui::Key{'A', 0});
  CHECK(*ui::normalize_key_event(KeyEvent{'a', ui::kControl, "\x01"}, false) == ui::Key{0x01, 0});
  CHECK(*ui::normalize_key_event(KeyEvent{'a', ui::kControl | ui::kShift, "\x01"}, false) ==
        ui::parse_key_name("Control_Shift_XK_a").value());
  // "Keypad distinguished": Shift dropped, keysym kept.
  const auto kp4 = ui::normalize_key_event(KeyEvent{0xFFB4, ui::kShift, "4"}, false);
  CHECK(*kp4 == ui::parse_key_name("XK_KP_4").value());
  CHECK(*kp4 != ui::parse_key_name("4").value());
  // Keys without ASCII keep every modifier.
  CHECK(*ui::normalize_key_event(KeyEvent{0xFF51, ui::kControl | ui::kShift, ""}, false) ==
        ui::parse_key_name("Control_Shift_XK_Left").value());
  // macOS: Mod1-Mod5 count as Meta.
  CHECK(ui::normalize_key_event(KeyEvent{0xFF51, ui::kMod4, ""}, true)->modifiers == ui::kMeta);
  CHECK(ui::normalize_key_event(KeyEvent{0xFF51, ui::kMod4, ""}, false)->modifiers == 0);
  // Buttons are Pointer_Button keys with their modifiers.
  CHECK(*ui::normalize_key_event(KeyEvent{0xFEEC, ui::kControl, ""}, false) == ui::button_key(4, ui::kControl));
  // Ctrl-C is the interrupt; a second one while pending is reported.
  CHECK(ui::is_interrupt_key(KeyEvent{'c', ui::kControl, "\x03"}));
  hefesto::editor::EditorState editor;
  CHECK_FALSE(ui::request_interrupt(editor));
  CHECK(ui::request_interrupt(editor));
  CHECK(ui::is_modifier_keysym(0xFFE5));
  CHECK_FALSE(ui::is_modifier_keysym('a'));
}

// --- Touch and pointer gesture mapping --------------------------------------------------------

TEST_CASE("touch gestures reach the same bindings", "[cap:ui-tools-and-input][req:Touch and pointer gesture mapping]") {
  UiSession s;
  ui::install_defaults(s.editor);
  // "Tap moves the box": a tap is Button1 of the current tool.
  const ui::Key tap = ui::gesture_button(ui::TouchModifiers{});
  CHECK(ui::handle_key(s.editor, tap).text == "*bypass box move bl cursor");
  // The floating modifier picks Button2/3 and Shift/Control variants.
  CHECK(ui::gesture_button(ui::TouchModifiers{3, true, false}) == ui::button_key(3, ui::kShift));
  CHECK(ui::handle_key(s.editor, ui::gesture_button(ui::TouchModifiers{2, false, false})).text ==
        "*bypass paint cursor");
  // A long press lists the secondary and tertiary actions.
  const auto menu = ui::long_press_actions(s.editor);
  REQUIRE(menu.size() == 6);
  CHECK(menu[0] == std::pair<std::string, std::string>{"XK_Pointer_Button2", "*bypass paint cursor"});
  // "Pinch zoom" and pan.
  CHECK(ui::pinch_command(2.0) == "zoom 0.5");
  CHECK(ui::pinch_command(0.0).empty());
  CHECK(ui::pan_command(10, 0) == "scroll l 10i");
  CHECK(ui::pan_command(0, -4) == "scroll u 4i");
  CHECK(ui::pan_command(0, 0).empty());
  CHECK(ui::gesture_table().size() == 6);
  // Rebinding through the macro tables changes the gesture.
  s.run(R"(macro Button1 "box")");
  CHECK(ui::handle_key(s.editor, tap).text == "box");
}
