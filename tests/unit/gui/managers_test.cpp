// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The technology, cell and library managers and the text helper
// (capability ui-gui-managers; Q-SCRIPT-27, Q-SCRIPT-31, Q-SCRIPT-33).

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "gui_fixture.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/gui/managers.hpp"
#include "hefesto/script/binding/bind.hpp"

namespace gui = hefesto::gui;
namespace db = hefesto::db;
using gui::test::Session;
using Words = std::vector<std::string>;
using Commands = std::vector<Words>;

namespace {

bool has_entry(const gui::TechManagerModel& m, const gui::TechMenuEntry& e) {
  return std::find(m.tech_menu.begin(), m.tech_menu.end(), e) != m.tech_menu.end();
}

}  // namespace

TEST_CASE("PDK listed in tech menu", "[cap:ui-gui-managers][req:Technology manager]") {
  Session s;
  s.write("/pdk/sky130A/libs.tech/magic/sky130A.magicrc", "");
  s.write("/techs/scmos.tech", "tech\n");
  s.write("/techs/old.tech27", "tech\n");
  s.editor.search_paths.system = {"/techs"};
  // PDK_ROOT from the path-variable table (the environment) ...
  s.editor.path_variables.variable = [](std::string_view name) -> std::optional<std::string> {
    return name == "PDK_ROOT" ? std::optional<std::string>{"/pdk"} : std::nullopt;
  };
  gui::TechManagerModel model;
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) {
               model = gui::tech_manager_model(ctx);
               return hefesto::cmd::CommandResult::success();
             })
              .ok());
  CHECK(model.technology == "small");
  CHECK(has_entry(model, {"sky130A", "source /pdk/sky130A/libs.tech/magic/sky130A.magicrc"}));
  CHECK(has_entry(model, {"scmos", "tech load scmos"}));
  CHECK(has_entry(model, {"old", "tech load old"}));
  CHECK(model.lambda_numerator == "1");
  CHECK(model.lambda_denominator == "1");
  // ... the interpreter's PDK_ROOT first.
  s.write("/other/gf180mcuD/x", "");
  REQUIRE(s.interp().evaluate("set PDK_ROOT /other").ok());
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) {
               model = gui::tech_manager_model(ctx);
               return hefesto::cmd::CommandResult::success();
             })
              .ok());
  CHECK(has_entry(model, {"gf180mcuD", "source /other/gf180mcuD/libs.tech/magic/gf180mcuD.magicrc"}));
  CHECK_FALSE(has_entry(model, {"sky130A", "source /pdk/sky130A/libs.tech/magic/sky130A.magicrc"}));
  // Units-per-lambda entries, in the order shown.
  CHECK(gui::tech_manager_scalegrid(2, 3, 1, 2) == Words{"scalegrid", "2", "6"});
  // Every option is accepted; `techmanager initall` is a no-op (Q-SCRIPT-33).
  CHECK(s.run("techmanager initall").ok());
  CHECK(s.run("techmanager create").ok());
  CHECK(s.run("techmanager a b").raises);
}

TEST_CASE("the cell manager tree and its buttons", "[cap:ui-gui-managers][req:Cell manager]") {
  Session s;
  db::CellDef* top = s.cell("top");
  db::CellDef* inv = s.cell("inv");
  db::PlaceOptions o;
  o.id = "inv_0";
  REQUIRE(db::place(*top, *inv, o).ok());
  gui::CellManagerModel model;
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) {
               model = gui::cell_manager_model(ctx);
               return hefesto::cmd::CommandResult::success();
             })
              .ok());
  CHECK(model.window == "default");
  REQUIRE(model.rows.size() == 2);
  CHECK(model.rows[0] == gui::CellManagerRow{"top", "top", "", 0});
  CHECK(model.rows[1] == gui::CellManagerRow{"top/inv", "inv", "inv_0", 1});
  CHECK(gui::cell_manager_commands(gui::CellManagerAction::load, model.rows[1], "inv_0") == Commands{{"load", "inv"}});
  CHECK(gui::cell_manager_commands(gui::CellManagerAction::place, model.rows[1], "inv_0") ==
        Commands{{"getcell", "inv"}});
  CHECK(gui::cell_manager_commands(gui::CellManagerAction::edit, model.rows[1], "inv_0") ==
        Commands{{"select", "cell", "inv_0"}, {"expand"}, {"edit"}});
  CHECK(gui::cell_manager_commands(gui::CellManagerAction::zoom, model.rows[0], "") == Commands{{"view"}});
  CHECK(gui::cell_manager_commands(gui::CellManagerAction::expand, model.rows[1], "inv_0").back() ==
        Words{"expand", "toggle"});
}

TEST_CASE("the cell manager's tags clear the edit stack on load", "[cap:ui-gui-managers][req:Cell manager]") {
  // "Load clears edit stack": `cellmanager create` installs cellmgr.tcl's tags
  // (the `load` tag runs `clearstack`, run by BINDING's tag callbacks).
  Session s;
  s.editor.script.tags.set("load", "magic::captions");
  REQUIRE(s.run("cellmanager create").ok());
  REQUIRE(s.run("cellmanager create").ok());  // installed once
  CHECK(s.editor.script.tags.get("load") == "magic::captions ; catch {magic::clearstack}; magic::cellmanager");
  CHECK(s.editor.script.tags.get("select") == "magic::mgrselect %r");
  CHECK(s.editor.script.tags.get("getcell") == "magic::cellmanager");
  REQUIRE(s.run("libmanager create").ok());
  CHECK(s.editor.script.tags.get("writeall") == "magic::libmanager");
  CHECK(s.editor.script.tags.get("select") == "magic::mgrselect %r ; magic::libmanager %1");
  REQUIRE(s.run("make_texthelper").ok());
  REQUIRE(s.run("drcmanager create").ok());
  CHECK(s.editor.script.tags.get("addpath") == "magic::libmanager ; magic::drcmanager");
  CHECK(s.run("cellmanager update").ok());
  CHECK(gui::gui_script_tags(gui::GuiScript::wrapper).size() == 17);
}

TEST_CASE("Load clears edit stack", "[cap:ui-gui-managers][req:Cell manager]") {
  // End to end through the Tcl binding: `load foo` runs the `load` tag the
  // cell manager installed (`catch {magic::clearstack}; magic::cellmanager`).
  Session s;
  auto& tcl = s.interp();
  REQUIRE(hefesto::script::bind_registry(tcl, s.registry, *s.dispatcher).ok());
  s.cell("top");
  s.ok("load top");
  REQUIRE(tcl.evaluate("magic::cellmanager create").ok());
  s.editor.edit_stack = {"{top 0 0 10 10}", "{top 0 0 20 20}"};
  const auto loaded = tcl.evaluate("load foo");
  INFO((loaded.diagnostics.empty() ? std::string{} : loaded.diagnostics.front().text));
  REQUIRE(loaded.ok());
  CHECK(s.editor.edit_stack.empty());
  REQUIRE(s.editor.cells.find("foo") != nullptr);
  // Without the cell manager's tag the stack survives a load.
  REQUIRE(tcl.evaluate("tag load {}").ok());
  s.editor.edit_stack = {"{foo 0 0 10 10}"};
  REQUIRE(tcl.evaluate("load top").ok());
  CHECK(s.editor.edit_stack.size() == 1);
}

TEST_CASE("Filter by technology", "[cap:ui-gui-managers][req:Library manager]") {
  Session s;
  s.write("/work/lib/a.mag", "magic\ntech small\ntimestamp 0\n");
  s.write("/work/lib/b.mag", "magic\ntech scmos\n");
  s.write("/work/lib/c.mag", "magic\r\ntech small extra\n");
  s.write("/work/lib/notes.txt", "magic\ntech small\n");
  s.write("/work/more/d.mag", "magic\ntech small\n");
  s.editor.search_paths.path = {"lib", "./lib"};
  s.editor.search_paths.library = {"$LIBS"};
  s.editor.path_variables.variable = [](std::string_view name) -> std::optional<std::string> {
    return name == "LIBS" ? std::optional<std::string>{"more"} : std::nullopt;
  };
  gui::LibraryManagerModel filtered;
  gui::LibraryManagerModel all;
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) {
               filtered = gui::library_manager_model(ctx);
               all = gui::library_manager_model(ctx, false);
               return hefesto::cmd::CommandResult::success();
             })
              .ok());
  // ./lib is lib again (Q-SCRIPT-27: normalized directories).
  CHECK(filtered.directories == Words{"lib/", "more/"});
  REQUIRE(filtered.rows.size() == 2);
  CHECK(filtered.rows[0] == gui::LibraryManagerRow{"lib/", "a", "small"});
  CHECK(filtered.rows[1] == gui::LibraryManagerRow{"more/", "d", "small"});
  REQUIRE(all.rows.size() == 3);
  CHECK(all.rows[1] == gui::LibraryManagerRow{"lib/", "b", "scmos"});
  CHECK(gui::mag_file_technology("magic\ntech x") == "x");
  CHECK_FALSE(gui::mag_file_technology("magic").has_value());
  CHECK_FALSE(gui::mag_file_technology("magic \ntech x").has_value());
  CHECK_FALSE(gui::mag_file_technology("magic\ntech ").has_value());
  CHECK(gui::library_manager_commands(gui::LibraryManagerAction::place, "a") ==
        Commands{{"getcell", "a", "child", "0", "0"}});
  CHECK(gui::library_manager_commands(gui::LibraryManagerAction::pick, "a").front() == Words{"tool", "pick"});
}

TEST_CASE("New port label", "[cap:ui-gui-managers][req:Text helper]") {
  gui::TextHelperModel m;
  m.text = "VDD";
  m.font = "FreeSans";
  m.size = "0.5";
  m.layer = "m1";
  m.default_layer = false;
  m.port = true;
  m.port_index = "3";
  CHECK(gui::new_label_commands(m) ==
        Commands{{"label", "VDD", "FreeSans", "0.5um", "0", "0 0", "center", "m1"}, {"port", "make", "3"}});
  // Sticky layers get a `-`; the default layer drops the layer and a default justification.
  m.sticky = true;
  m.justification = "ne";
  CHECK(gui::new_label_commands(m).front() == Words{"label", "VDD", "FreeSans", "0.5um", "0", "0 0", "ne", "-m1"});
  m.default_layer = true;
  m.justification = "default";
  m.size = "0";
  m.port = false;
  CHECK(gui::new_label_commands(m) ==
        Commands{{"label", "VDD", "FreeSans", "1um", "0", "0 0"}, {"port", "remove"}});
  // Q-SCRIPT-31: the default font gives `label <text> <just>` and nothing else.
  m.font = "default";
  CHECK(gui::new_label_commands(m) == Commands{{"label", "VDD", "default"}});
  m.text.clear();
  CHECK(gui::new_label_commands(m).empty());
  // The defaults of the helper.
  const gui::TextHelperModel d;
  CHECK((d.font == "FreeSans" && d.size == "1" && d.justification == "center" && d.rotation == "0" &&
         d.offset == "0 0" && d.port_index == "0"));
}

TEST_CASE("the text helper changes the selected label", "[cap:ui-gui-managers][req:Text helper]") {
  gui::TextHelperModel m;
  m.text = "A";
  m.font = "default";
  m.justification = "default";
  m.port = true;
  m.port_index = "2";
  CHECK(gui::change_label_commands(m, false) ==
        Commands{{"setlabel", "text", "A"},     {"setlabel", "font", "-1"},     {"setlabel", "justify", "-1"},
                 {"setlabel", "size", "1"},     {"setlabel", "offset", "0 0"},  {"setlabel", "rotate", "0"},
                 {"setlabel", "sticky", "0"},   {"port", "make", "2"}});
  CHECK(gui::change_label_commands(m, true).back() == Words{"port", "index", "2"});
  m.port = false;
  CHECK(gui::change_label_commands(m, true).back() == Words{"port", "remove"});
  CHECK(gui::change_label_commands(m, false).back() == Words{"setlabel", "sticky", "0"});

  // Apply with no label selected creates one; the port entry advances.
  Session s;
  s.editor.edit_cell = s.cell("top");
  s.ok("snap internal");
  s.ok("box 0 0 10 10");
  s.ok("paint metal1");
  s.ok("box 5 5 5 5");
  gui::TextHelperModel add;
  add.text = "B";
  add.font = "default";
  add.port = true;
  add.port_index = "3";
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::text_helper_apply(ctx, add); }).ok());
  REQUIRE(s.editor.edit_cell->labels().size() == 1);
  CHECK(s.editor.edit_cell->labels()[0].text == "B");
  Words fonts;
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) {
               fonts = gui::text_helper_fonts(ctx);
               return hefesto::cmd::CommandResult::success();
             })
              .ok());
  CHECK(fonts.front() == "default");
}

// Regression (SHELL-B round 2): Magic's technology manager shows
// `[lindex [tech version] 1]`, the description without its list braces.
TEST_CASE("the technology manager shows the description as a list element",
          "[cap:ui-gui-managers][req:Technology manager]") {
  Session s;
  std::ifstream in{std::string{HEFESTO_SOURCE_DIR} + "/tests/data/tech/tiny.tech"};
  std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
  const std::string from = "\"Tiny demo process\"";
  REQUIRE(text.find(from) != std::string::npos);
  text.replace(text.find(from), from.size(), "\"SkyWater SKY130: {demo} process\"");
  hefesto::diag::CollectingSink sink;
  auto loaded = hefesto::tech::load_technology_text(text, "/t/tiny.tech", s.services.files(),
                                                    hefesto::tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  s.editor.technology = loaded.value();
  gui::TechManagerModel model;
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) {
               model = gui::tech_manager_model(ctx);
               return hefesto::cmd::CommandResult::success();
             })
              .ok());
  CHECK(model.version == "(version 1.0)");
  CHECK(model.description == "SkyWater SKY130: {demo} process");
}
