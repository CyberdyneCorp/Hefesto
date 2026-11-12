// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop menu bar model (board M4-Menu-Bar; openspec/changes/
// m4-scripting tasks 10.1): Magic's wrapper menus with Magic's labels, the
// PDK items inserted through intercepted widget calls, the toolkit menus
// after a divider, and every item's logged command.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "hefesto/gui/wrapper.hpp"
#include "hefesto/shell/menu_bar_model.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

const MenuBarMenu& menu(const MenuBarView& view, std::string_view title) {
  const auto found = std::find_if(view.menus.begin(), view.menus.end(),
                                  [title](const MenuBarMenu& m) { return m.title == title; });
  REQUIRE(found != view.menus.end());
  return *found;
}

const MenuBarItem* item(const MenuBarMenu& m, std::string_view label) {
  for (const MenuBarItem& i : m.items) {
    if (i.label == label) return &i;
  }
  return nullptr;
}

std::vector<std::string> labels(const MenuBarMenu& m) {
  std::vector<std::string> out;
  for (const MenuBarItem& i : m.items) out.push_back(i.kind == MenuBarItem::Kind::separator ? "-" : i.label);
  return out;
}

/// sky130's addtechmenu: two toolkit menus on the wrapper's layout frame.
pcell::ToolkitMenus sky130_toolkit() {
  using K = pcell::ToolkitItem::Kind;
  pcell::ToolkitMenus menus;
  menus.menus.push_back({".layout1.pane.top", "pdk1", "Devices 1",
                         {{K::command, "nmos (MOSFET)", "magic::gencell sky130::sky130_fd_pr__nfet_01v8"},
                          {K::command, "pmos (MOSFET)", "magic::gencell sky130::sky130_fd_pr__pfet_01v8"},
                          {K::separator, "", ""},
                          {K::command, "n-diode", "magic::gencell sky130::sky130_fd_pr__diode_pw2nd_05v5"}}});
  menus.menus.push_back({".layout1.pane.top", "pdk2", "Devices 2",
                         {{K::command, "poly resistor - 319.8 Ohm/sq",
                           "magic::gencell sky130::sky130_fd_pr__res_generic_po"}}});
  return menus;
}

/// What the interceptor leaves after sky130.tcl's `file.toolmenu insert 4`
/// and `drc.toolmenu add command` (PDKINT 6.5).
script::MenuModel sky130_edits() {
  script::MenuModel model;
  std::vector<script::MenuItem> file = script::magic_file_menu();
  for (auto& i : file) model.append("File", i);
  model.insert("File", 4, {script::MenuItem::Kind::command, "Import SPICE", "sky130::importspice", false});
  model.insert("File", 4, {script::MenuItem::Kind::separator, "", "", false});
  model.append("Drc", {script::MenuItem::Kind::command, "DRC Routing", "drc style drc(routing)", false});
  return model;
}

MenuBarInputs inputs_with(std::map<std::string, bool> options) {
  MenuBarInputs inputs;
  inputs.option = [options = std::move(options)](std::string_view v) -> std::optional<bool> {
    const auto found = options.find(std::string{v});
    if (found == options.end()) return std::nullopt;
    return found->second;
  };
  return inputs;
}

}  // namespace

TEST_CASE("menu bar items issue the wrapper's commands", "[cap:ui-gui-managers][req:Wrapper menus]") {
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, {});
  std::vector<std::string> titles;
  for (const MenuBarMenu& m : view.menus) titles.push_back(m.title);
  CHECK(titles == std::vector<std::string>{"File", "Edit", "Cell", "Window", "Layers", "Drc", "Options"});
  CHECK(view.divider == 7);

  // Scenario "Menu dispatch": Window -> Zoom Box runs magic::findbox zoom.
  const MenuBarItem* zoom_box = item(menu(view, "Window"), "Zoom Box");
  REQUIRE(zoom_box != nullptr);
  CHECK(menu_item_script(*zoom_box) == "magic::findbox zoom");
  // Q-SCRIPT-36 (fix): Rotate 90 degree runs magic::clockwise 90.
  CHECK(item(menu(view, "Edit"), "Rotate 90 degree")->script == "magic::clockwise 90");
  // Magic's labels with double spaces collapsed.
  CHECK(item(menu(view, "Edit"), "Mirror Up/Down") != nullptr);
  CHECK(item(menu(view, "Cell"), "Up hierarchy")->script == "magic::popstack");
  CHECK(item(menu(view, "Layers"), "Unlock Base Layers")->script == "magic::tech unlock *");
  CHECK(item(menu(view, "Drc"), "DRC Fast")->script == "drc style drc(fast)");
  // Every item has a command and describes it (status bar, screen readers).
  for (const MenuBarMenu& m : view.menus) {
    for (const MenuBarItem& i : m.items) {
      if (i.kind == MenuBarItem::Kind::separator) continue;
      CHECK_FALSE(i.script.empty());
      CHECK(i.description() == i.label + " · " + i.script);
    }
  }
}

TEST_CASE("later-milestone menu items are present and disabled", "[cap:ui-gui-managers][req:Wrapper menus]") {
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, {});
  const MenuBarItem* read_gds = item(menu(view, "File"), "Read GDS");
  REQUIRE(read_gds != nullptr);
  CHECK_FALSE(read_gds->enabled);
  CHECK(read_gds->tooltip == "Read GDS: arrives in M5");
  CHECK_FALSE(item(menu(view, "Options"), "Netlist Window")->enabled);
  CHECK(item(menu(view, "Options"), "3D Display")->tooltip == "3D Display: arrives in M10");
  CHECK(item(menu(view, "Options"), "Cell Manager")->enabled);
  CHECK(later_milestone("magic::flush") == std::nullopt);
}

TEST_CASE("menu bar check items follow and toggle their option variables",
          "[cap:ui-gui-managers][req:Wrapper menus]") {
  const MenuBarView view =
      menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, inputs_with({{"Opts(drcmgr)", true}}));
  const MenuBarItem* drc = item(menu(view, "Options"), "DRC Manager");
  REQUIRE(drc != nullptr);
  CHECK(drc->kind == MenuBarItem::Kind::check);
  CHECK(drc->checked);
  // Tk's checkbutton: the variable takes the toggled value, then the command runs.
  CHECK(menu_item_script(*drc) == "set ::Opts(drcmgr) 0 ; magic::drcmanager create");
  const MenuBarItem* cells = item(menu(view, "Options"), "Cell Manager");
  CHECK_FALSE(cells->checked);
  CHECK(menu_item_script(*cells) == "set ::Opts(cellmgr) 1 ; magic::cellmanager create");
}

TEST_CASE("accelerators are shown only for an exact key macro match", "[cap:ui-gui-managers][req:Wrapper menus]") {
  gui::WrapperMenuModel wrapper = gui::wrapper_menus(".layout1");
  // `z` is `zoom .5`, which does not equal Zoom In's `zoom 0.5` (board note 6).
  gui::apply_accelerators(wrapper, {{"delete", "d"}, {"zoom .5", "z"}, {"magic::redraw", "Control_L"}});
  const MenuBarView view = menu_bar_view(wrapper, {}, {}, {});
  CHECK(item(menu(view, "Edit"), "Delete")->accelerator == "d");
  CHECK(item(menu(view, "Window"), "Zoom In")->accelerator.empty());
  CHECK(item(menu(view, "Window"), "Redraw")->accelerator == "^L");
}

TEST_CASE("PDK widget edits land in the wrapper menus",
          "[cap:scripting-pdk-integration][req:Wrapper widget operations in PDK menu callbacks]") {
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), sky130_edits(), {}, {});
  // Import SPICE between two separators after Flush changes (board note 7).
  CHECK(labels(menu(view, "File")) == std::vector<std::string>{"Open...", "Save...", "-", "Flush changes", "-",
                                                               "Import SPICE", "-", "Read GDS", "Write GDS", "-",
                                                               "Save All and Quit", "Quit"});
  const MenuBarItem* spice = item(menu(view, "File"), "Import SPICE");
  CHECK(spice->pdk);
  CHECK(spice->script == "sky130::importspice");
  // The native entries keep the wrapper's state (Read GDS still waits for M5).
  CHECK_FALSE(item(menu(view, "File"), "Read GDS")->enabled);
  CHECK_FALSE(item(menu(view, "File"), "Quit")->pdk);
  // DRC Routing appended after the wrapper's Drc items.
  const MenuBarMenu& drc = menu(view, "Drc");
  CHECK(drc.items.back().label == "DRC Routing");
  CHECK(drc.items.back().pdk);
  CHECK(item(drc, "DRC update") != nullptr);
}

TEST_CASE("toolkit menus follow the wrapper menus after a divider", "[cap:pcell-toolkit][req:Toolkit menu hooks]") {
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), {}, sky130_toolkit(), {});
  REQUIRE(view.menus.size() == 9);
  CHECK(view.divider == 7);
  CHECK(view.menus[7].title == "Devices 1");
  CHECK(view.menus[7].pdk);
  CHECK(view.menus[7].source == "Devices 1 · pdk1");
  CHECK(view.menus[8].title == "Devices 2");
  // Scenario "PDK device menu entry": the item runs the gencell command.
  const MenuBarItem* nmos = item(view.menus[7], "nmos (MOSFET)");
  REQUIRE(nmos != nullptr);
  CHECK(menu_item_script(*nmos) == "magic::gencell sky130::sky130_fd_pr__nfet_01v8");
  CHECK(labels(view.menus[7]) == std::vector<std::string>{"nmos (MOSFET)", "pmos (MOSFET)", "-", "n-diode"});
  // Another frame's menus are not shown when a frame is given.
  MenuBarInputs other;
  other.frame = ".layout2";
  CHECK(menu_bar_view(gui::wrapper_menus(".layout2"), {}, sky130_toolkit(), other).menus.size() == 7);
}

TEST_CASE("declined PDK scripts leave no PDK menus", "[cap:hefesto-security][req:Consent before implicit script execution]") {
  MenuBarInputs declined;
  declined.scripts_declined = true;
  const MenuBarView view = menu_bar_view(gui::wrapper_menus(".layout1"), sky130_edits(), sky130_toolkit(), declined);
  CHECK(view.menus.size() == 7);
  CHECK(item(menu(view, "File"), "Import SPICE") == nullptr);
  CHECK(item(menu(view, "Drc"), "DRC Routing") == nullptr);
}

TEST_CASE("menu bar signature changes with the menus", "[cap:ui-gui-managers][req:Wrapper menus]") {
  const auto plain = menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, {});
  const auto pdk = menu_bar_view(gui::wrapper_menus(".layout1"), sky130_edits(), sky130_toolkit(), {});
  CHECK(menu_bar_signature(plain) == menu_bar_signature(menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, {})));
  CHECK(menu_bar_signature(plain) != menu_bar_signature(pdk));
  const auto checked =
      menu_bar_view(gui::wrapper_menus(".layout1"), {}, {}, inputs_with({{"Opts(cellmgr)", true}}));
  CHECK(menu_bar_signature(plain) != menu_bar_signature(checked));
}

TEST_CASE("skipped widget statements become Diagnostics rows and one status message",
          "[cap:scripting-pdk-integration][req:Wrapper widget operations in PDK menu callbacks]") {
  const script::SkippedStatement a{"wm geometry .layout1 900x700", {"/pdk/libs.tech/magic/mymenus.tcl", 57}};
  const script::SkippedStatement b{".layout1.titlebar.mbuttons.cell.toolmenu add command -label X -command y",
                                   {"/pdk/libs.tech/magic/mymenus.tcl", 42}};
  CHECK(skipped_statement_row(a) == "▲ SCRIPT-PDK-004 skipped widget statement mymenus.tcl:57\nwm geometry .layout1 900x700");
  CHECK(skipped_statements_message({a, b}) == "▲ 2 menu statements of mymenus.tcl were skipped");
  CHECK(skipped_statements_message({a}) == "▲ 1 menu statement of mymenus.tcl was skipped");
  CHECK(skipped_statements_message({}).empty());
  CHECK(collapse_spaces("Lock   Cell") == "Lock Cell");
}
