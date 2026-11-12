// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The wiring tool's HUD, mode chip and Wire inspector (wiring_hud.hpp;
// board M2-Wiring; M4 CARRY task 11.7).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/shell/wiring_hud.hpp"
#include "hefesto/tech/loader.hpp"

using namespace hefesto::shell;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace wiring = hefesto::wiring;

namespace {

std::shared_ptr<const tech::Technology> tiny() {
  hefesto::platform::LocalFileSystem local;
  hefesto::diag::CollectingSink sink;
  auto loaded = tech::load_technology(local, std::string{HEFESTO_SOURCE_DIR} + "/tests/data/tech/tiny.tech",
                                      tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  return loaded.value();
}

tech::TileType type(const hefesto::editor::EditorState& ed, std::string_view name) {
  return *ed.technology->find_type(name).id;
}

const UnitScale kLambda{1, 1, std::nullopt};

}  // namespace

TEST_CASE("the wiring HUD shows type, width, direction and leg length while tracking",
          "[cap:hefesto-input-and-touch][req:Desktop input parity with Magic]") {
  hefesto::editor::EditorState ed;
  ed.technology = tiny();
  const db::Rect box{0, 0, 4, 4};
  CHECK_FALSE(wiring_hud_text(ed, box, {20, 2}, kLambda).has_value());  // not tracking
  ed.wire.tracking = true;
  CHECK_FALSE(wiring_hud_text(ed, box, {20, 2}, kLambda).has_value());  // no type yet
  ed.wire.type = type(ed, "metal2");
  ed.wire.width = 4;
  const auto east = wiring_hud_text(ed, box, {20, 2}, kLambda);
  REQUIRE(east.has_value());
  const auto leg = wiring::plan_leg(box, {20, 2}, 4, wiring::LegMode::choose);
  REQUIRE(leg.has_value());
  CHECK(*east == "metal2 · 4 · east · " + std::to_string(leg->area.x1 - leg->area.x0));
  const auto north = wiring_hud_text(ed, box, {2, 30}, kLambda);
  REQUIRE(north.has_value());
  CHECK(north->find("north") != std::string::npos);
  // Inside the box's span: no leg, type and width only.
  CHECK(wiring_hud_text(ed, box, {2, 2}, kLambda) == "metal2 · 4");
  // Microns when the technology has a micron scale.
  const UnitScale microns{1, 1, 0.01};
  CHECK(wiring_hud_text(ed, box, {2, 2}, microns) == "metal2 · 0.040");
}

TEST_CASE("the HUD follows the cursor and flips near the canvas edges",
          "[cap:hefesto-input-and-touch][req:Desktop input parity with Magic]") {
  CHECK(wiring_hud_point(100.0, 100.0, 180.0, 24.0, 1000.0, 700.0) == HudPoint{114.0, 120.0});
  CHECK(wiring_hud_point(900.0, 100.0, 180.0, 24.0, 1000.0, 700.0) == HudPoint{900.0 - 14.0 - 180.0, 120.0});
  CHECK(wiring_hud_point(100.0, 690.0, 180.0, 24.0, 1000.0, 700.0) == HudPoint{114.0, 690.0 - 20.0 - 24.0});
}

TEST_CASE("the Wiring tracking chip shows while tracking and cancels with trackwire",
          "[cap:hefesto-input-and-touch][req:Desktop input parity with Magic]") {
  hefesto::editor::EditorState ed;
  CHECK_FALSE(wiring_chip(ed).has_value());
  ed.wire.tracking = true;
  const auto chip = wiring_chip(ed);
  REQUIRE(chip.has_value());
  CHECK(chip->text == "Wiring · tracking");
  CHECK(chip->cancel == "trackwire cancel");
  CHECK_FALSE(chip->accessible_name.empty());
}

TEST_CASE("the Wire inspector edits the wire through wire commands and lists the wiring stack",
          "[cap:hefesto-input-and-touch][req:Desktop input parity with Magic]") {
  hefesto::editor::EditorState ed;
  ed.technology = tiny();
  wiring::WiringSection section;
  // Reverse file order: `contact via metal1 metal2` was read after
  // `contact pcontact poly metal1`.
  wiring::WiringContact via;
  via.type = type(ed, "m2contact");
  via.layer1 = type(ed, "metal1");
  via.layer2 = type(ed, "metal2");
  wiring::WiringContact pc;
  pc.type = type(ed, "pcontact");
  pc.layer1 = type(ed, "polysilicon");
  pc.layer2 = type(ed, "metal1");
  section.contacts = {via, pc};
  CHECK(wiring_stack(section, *ed.technology) == std::vector<std::string>{"polysilicon", "metal1", "metal2"});

  WireInspectorView v = wire_inspector(ed, section, kLambda);
  CHECK(v.type == "no material");
  ed.wire.type = type(ed, "metal1");
  ed.wire.width = 3;
  v = wire_inspector(ed, section, kLambda);
  CHECK(v.type == "metal1");
  CHECK(v.width == "3");
  CHECK(v.units == "lambda");
  CHECK(v.width_command == "wire width ");
  REQUIRE(v.actions.size() == 3U);
  CHECK(v.actions[0].command == "wire decrement width");
  CHECK(v.actions[1].command == "wire increment width");
  CHECK(v.actions[2].command == "wire switch");
  REQUIRE(v.stack.size() == 3U);
  CHECK(v.stack[1].current);
  CHECK(v.stack[2].command == "wire type metal2");
}
