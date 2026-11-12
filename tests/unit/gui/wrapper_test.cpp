// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The wrapper layout window (capability ui-gui-managers; Q-SCRIPT-13,
// Q-SCRIPT-36).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "gui_fixture.hpp"
#include "hefesto/gui/diag_codes.hpp"
#include "hefesto/gui/wrapper.hpp"

namespace gui = hefesto::gui;
using gui::test::Session;

namespace {

void tcl(Session& s, std::string_view script) {
  const auto r = s.interp().evaluate(script);
  INFO(script << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.front().text));
  REQUIRE(r.ok());
}

std::string var(Session& s, std::string_view name) {
  const auto v = s.interp().get_var(name);
  return v ? v->to_string() : std::string{"<unset>"};
}

const gui::WrapperMenuItem* find_item(const gui::WrapperMenuModel& m, std::string_view menu, std::string_view label) {
  for (const auto& mm : m.menus) {
    for (const auto& item : mm.items) {
      if (mm.title == menu && item.label == label) {
        return &item;
      }
    }
  }
  return nullptr;
}

}  // namespace

TEST_CASE("openwrapper opens .layoutN.pane.top.magic", "[cap:ui-gui-managers][req:Wrapper window construction]") {
  Session s;
  // Without the project interpreter there is no wrapper (Magic's -dnull).
  const auto none = s.run("openwrapper");
  REQUIRE_FALSE(none.ok());
  CHECK(none.diagnostics.front().code == gui::codes::kWrapper);
  (void)s.interp();
  // "First wrapper".
  const auto first = s.run("openwrapper");
  REQUIRE(first.ok());
  CHECK(first.value.to_string() == ".layout1.pane.top.magic");
  CHECK(s.editor.windows.find_named(".layout1.pane.top.magic") != nullptr);
  CHECK(var(s, "lwindow") == "1");
  CHECK(var(s, "Opts(scale)") == "1");
  // The first call turned off the engine's own window chrome.
  CHECK_FALSE(s.editor.windows.chrome_defaults().caption);
  CHECK_FALSE(s.editor.windows.chrome_defaults().scrollbars);
  CHECK_FALSE(s.editor.windows.chrome_defaults().border);
  CHECK(s.run("openwrapper").value.to_string() == ".layout2.pane.top.magic");
  CHECK(s.run("openwrapper \"\" .mine").value.to_string() == ".mine.pane.top.magic");
  CHECK(var(s, "lwindow") == "2");
  // A frame that exists is refused.
  CHECK_FALSE(s.run("openwrapper \"\" .mine").ok());
  CHECK(s.run("openwrapper a b c").raises);
  CHECK(gui::wrapper_window_name(".layout3") == ".layout3.pane.top.magic");
}

TEST_CASE("wrapper option variables get their defaults", "[cap:ui-gui-managers][req:Wrapper option variables]") {
  Session s;
  tcl(s, "set Opts(drc) 0");
  REQUIRE(s.run("openwrapper").ok());
  CHECK(var(s, "Opts(target)") == "default");
  CHECK(var(s, "Opts(autobuttontext)") == "1");
  CHECK(var(s, "Opts(toolscale)") == "1.0");
  CHECK(var(s, "Opts(drc)") == "0");  // a value set before is kept
  // "Toolbar default on".
  CHECK(var(s, "Winopts(.layout1,toolbar)") == "1");
  CHECK(var(s, "Winopts(.layout1,cmdentry)") == "0");
  CHECK(gui::wrapper_option_defaults().size() == 13);
}

TEST_CASE("Opts(callback) runs in the wrapper's scope", "[cap:ui-gui-managers][req:Wrapper hook points for scripts]") {
  Session s;
  // "PDK menu callback".
  tcl(s, "namespace eval sky130 {proc addtechmenu {f} {lappend ::menus $f}}");
  tcl(s, "set Opts(callback) {sky130::addtechmenu $framename; set ::where [list $layoutframe $winname]}");
  REQUIRE(s.run("openwrapper").ok());
  REQUIRE(s.run("openwrapper").ok());
  CHECK(var(s, "menus") == ".layout1 .layout2");
  CHECK(var(s, "where") == ".layout2.pane.top .layout2.pane.top.magic");
  // A failing callback is caught: the wrapper opens.
  tcl(s, "set Opts(callback) {error boom}");
  const auto r = s.run("openwrapper");
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == ".layout3.pane.top.magic");
  CHECK(r.diagnostics.back().code == gui::codes::kWrapperHook);
  CHECK(r.diagnostics.back().native_only);
}

TEST_CASE("Per-window callback", "[cap:ui-gui-managers][req:Wrapper hook points for scripts]") {
  Session s;
  // Q-SCRIPT-13 (fix): the key without the space runs too.
  tcl(s, "set Winopts(.layout2,callback) {set ::seen $framename}");
  REQUIRE(s.run("openwrapper").ok());
  CHECK(var(s, "seen") == "<unset>");
  REQUIRE(s.run("openwrapper").ok());
  CHECK(var(s, "seen") == ".layout2");
  // Magic's spaced key runs first.
  tcl(s, "set {Winopts(.layout3, callback)} {lappend ::order spaced}");
  tcl(s, "set Winopts(.layout3,callback) {lappend ::order plain}");
  REQUIRE(s.run("openwrapper").ok());
  CHECK(var(s, "order") == "spaced plain");
}

TEST_CASE("menus issue the listed commands; accelerators come from key macros",
          "[cap:ui-gui-managers][req:Wrapper menus]") {
  const gui::WrapperMenuModel m = gui::wrapper_menus(".layout1");
  std::vector<std::string> titles;
  for (const auto& menu : m.menus) {
    titles.push_back(menu.title);
  }
  CHECK(titles == std::vector<std::string>{"File", "Edit", "Cell", "Window", "Layers", "Drc", "Options"});
  // "Menu dispatch".
  REQUIRE(find_item(m, "Window", "Zoom Box") != nullptr);
  CHECK(find_item(m, "Window", "Zoom Box")->command == "magic::findbox zoom");
  // Q-SCRIPT-36 (fix).
  CHECK(find_item(m, "Edit", "Rotate 90 degree")->command == "magic::clockwise 90");
  CHECK(find_item(m, "Window", "Close")->command == "closewrapper .layout1");
  CHECK(find_item(m, "Window", "Set grid 10.0um")->command == "magic::grid 10.0um");
  CHECK(find_item(m, "Options", "Toolbar")->variable == "Winopts(.layout1,toolbar)");
  CHECK(find_item(m, "Options", "Maze Router") == nullptr);
  CHECK(find_item(gui::wrapper_menus(".layout1", true), "Options", "Maze Router") != nullptr);
  // "Wrapper hook points": Opts(autobuttontext) accelerator text.
  gui::WrapperMenuModel with_keys = m;
  gui::apply_accelerators(with_keys, {{" magic::findbox zoom ", "z"}, {"magic::undo", "Control_z"}, {"redo", "XK_less"}});
  CHECK(find_item(with_keys, "Window", "Zoom Box")->accelerator == "(z)");
  CHECK(find_item(with_keys, "Edit", "Undo")->accelerator == "(^z)");
  CHECK(find_item(with_keys, "Edit", "Redo")->accelerator == "(<)");
  CHECK(find_item(with_keys, "Edit", "Delete")->accelerator.empty());
  CHECK(gui::accelerator_key_name("Control_question") == "^?");
  CHECK(gui::accelerator_key_name("XK_comma") == ",");
  CHECK(gui::accelerator_key_name("more") == ">");
}

TEST_CASE("caption, position and DRC button texts", "[cap:ui-gui-managers][req:Wrapper tag callbacks and caption formats]") {
  // "Box caption".
  CHECK(gui::box_position_label(std::array<double, 4>{0, 0, 1.5, 2}) == "box (+0 +0) to (+1.5 +2) microns");
  CHECK(gui::box_position_label(std::nullopt) == "no box");
  CHECK(gui::pointer_position_label(1, 2.5, std::array<double, 2>{0.5, 0.5}) == "(+1 +2.5) +0.5 +2 microns");
  CHECK(gui::pointer_position_label(-1, 0, std::nullopt) == "(-1 +0) microns");
  CHECK(gui::wrapper_caption({"top", "EDITING", "inv"}, "box", "sky130A") ==
        "Loaded: top Editing: inv Tool: box    Technology: sky130A");
  CHECK(gui::wrapper_caption({"top"}, "wiring", "x") == "Loaded: top Editing: top Tool: wiring    Technology: x");
  CHECK(gui::drc_button(false, 3).text == "DRC=3");
  CHECK(gui::drc_button(false, 3).colour == gui::DrcButton::Colour::red);
  CHECK(gui::drc_button(false, 0).colour == gui::DrcButton::Colour::green);
  CHECK(gui::drc_button(true, 0).colour == gui::DrcButton::Colour::yellow);
}

TEST_CASE("closewrapper closes the window and resets the target; o and O are rebound",
          "[cap:ui-gui-managers][req:Window close and key overrides]") {
  Session s;
  (void)s.interp();
  s.ok("macro o openwindow");
  s.ok("macro O closewindow");
  REQUIRE(s.run("openwrapper").ok());
  REQUIRE(s.run("openwrapper").ok());
  CHECK(s.run("macro list O").value.to_string() == "closewrapper $Opts(focus)");
  CHECK(s.run("macro list o").value.to_string().starts_with("incr owindow ;"));
  tcl(s, "set Opts(target) .layout2.pane.top.magic");
  // "Close via menu".
  REQUIRE(s.run("closewrapper .layout2").ok());
  CHECK(s.editor.windows.find_named(".layout2.pane.top.magic") == nullptr);
  CHECK(var(s, "Opts(target)") == "default");
  CHECK(s.editor.windows.find_named(".layout1.pane.top.magic") != nullptr);
  CHECK_FALSE(s.run("closewrapper .layout2").ok());
  CHECK(s.run("closewrapper").raises);
}

TEST_CASE("the per-window console directs commands to its window",
          "[cap:ui-gui-managers][req:Per-window command entry]") {
  // "Directed console".
  CHECK(gui::command_entry_line(".layout1", "paint m1") == ".layout1.pane.top.magic paint m1");
  CHECK(gui::command_entry_prompt(".layout1") == ".layout1>");
}
