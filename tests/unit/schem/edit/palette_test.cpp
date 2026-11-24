// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The headless palette, context menu and desktop binding view model
// (capability schematic-editing "Desktop and touch interaction",
// Q-SCHEM-24; tasks.md 9.3, command level only: the iPad presentation is
// M14).
//
// Owner: EDIT.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "edit_fixture.hpp"
#include "hefesto/schem/edit/palette.hpp"

namespace schem = hefesto::schem;
using hefesto::test::active;
using hefesto::test::new_schematic;
using hefesto::test::run;
using hefesto::test::SchemSession;

namespace {

constexpr const char* kTouch = "[cap:schematic-editing][req:Desktop and touch interaction]";

bool contains(std::span<const schem::EditAction> actions, schem::EditAction action) {
  return std::find(actions.begin(), actions.end(), action) != actions.end();
}

}  // namespace

TEST_CASE("Rotate on iPad", kTouch) {
  // Long press on the instance, then Rotate: the same action and commands as `R`.
  REQUIRE(contains(schem::context_menu_actions(), schem::EditAction::rotate));
  const auto key = schem::desktop_binding("R");
  REQUIRE(key == schem::EditAction::rotate);
  schem::ActionInput touch;
  touch.pointer = {101.0, 98.0};  // a finger, not on the grid
  schem::ActionInput mouse;
  mouse.pointer = {99.0, 102.0};
  const auto from_menu = schem::action_commands(schem::EditAction::rotate, touch);
  CHECK(from_menu == schem::action_commands(*key, mouse));
  CHECK(from_menu == std::vector<std::vector<std::string>>{{"rotate", "100", "100"}});

  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "100", "100"});
  run(s, {"select", "instance", "R1"});
  run(s, from_menu.front());
  CHECK(active(s).instances[0].rotation == 1);
}

TEST_CASE("Wire from the palette on iPad", kTouch) {
  REQUIRE(contains(schem::palette_actions(), schem::EditAction::wire));
  REQUIRE(schem::desktop_binding("w") == schem::EditAction::wire);
  // A drag from pin (0,-30) to pin (50,0) with H-V routing, the finger a
  // little off the pins; the desktop clicks the same snapped points.
  schem::ActionInput drag;
  drag.start = {1.5, -28.0};
  drag.pointer = {48.0, 2.0};
  schem::ActionInput clicks;
  clicks.start = {0.0, -30.0};
  clicks.pointer = {50.0, 0.0};
  const auto commands = schem::action_commands(schem::EditAction::wire, drag);
  CHECK(commands == schem::action_commands(*schem::desktop_binding("w"), clicks));
  CHECK(commands == std::vector<std::vector<std::string>>{{"wire", "0", "-30", "50", "-30"},
                                                          {"wire", "50", "-30", "50", "0"}});
}

TEST_CASE("the palette offers every common operation and the bindings are xschem's", kTouch) {
  using enum schem::EditAction;
  for (const auto action : {select, wire, place_symbol, move, copy, rotate, flip, delete_selection, edit_properties,
                            descend, ascend, highlight, undo, redo, netlist}) {
    CHECK(contains(schem::palette_actions(), action));
    CHECK_FALSE(schem::action_label(action).empty());
  }
  for (const auto action : {move, copy, rotate, flip, delete_selection, edit_properties, descend, highlight}) {
    CHECK(contains(schem::context_menu_actions(), action));
  }
  CHECK(schem::desktop_binding("m") == move);
  CHECK(schem::desktop_binding("c") == copy);
  CHECK(schem::desktop_binding("F") == flip);
  CHECK(schem::desktop_binding("e") == descend);
  CHECK(schem::desktop_binding("Ctrl+e") == ascend);
  CHECK(schem::desktop_binding("q") == edit_properties);
  CHECK(schem::desktop_binding("k") == highlight);
  CHECK(schem::desktop_binding("u") == undo);
  CHECK(schem::desktop_binding("U") == redo);
  CHECK(schem::desktop_binding("Insert") == place_symbol);
  CHECK(schem::desktop_binding("n") == netlist);
  CHECK_FALSE(schem::desktop_binding("z").has_value());

  schem::ActionInput input;
  input.start = {0, 0};
  input.pointer = {21, -9};
  input.symbol = "res.sym";
  input.instance = "x1";
  CHECK(schem::action_commands(move, input) == std::vector<std::vector<std::string>>{{"move", "20", "-10"}});
  CHECK(schem::action_commands(copy, input) == std::vector<std::vector<std::string>>{{"copy", "20", "-10"}});
  CHECK(schem::action_commands(place_symbol, input) ==
        std::vector<std::vector<std::string>>{{"place", "res.sym", "20", "-10"}});
  CHECK(schem::action_commands(descend, input) == std::vector<std::vector<std::string>>{{"descend", "x1"}});
  CHECK(schem::action_commands(select, input).empty());
}
