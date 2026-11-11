// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// capability pcell-toolkit: "Parameter dialog structure", "Dialog builder
// API", "Dialog dependencies and validation" (Q-SCRIPT-25), "Selection-driven
// dialog refresh and key binding", "Toolkit menu hooks", on the test library
// tests/data/pcell/framework/tpdk.tcl.

#include <catch2/catch_test_macros.hpp>

#include "framework_fixture.hpp"

namespace pcell = hefesto::pcell;
using hefesto::test::PcellSession;

namespace {

const pcell::DialogModel& dialog(const PcellSession& s) {
  REQUIRE(s.editor.script.gencell.dialog.has_value());
  return *s.editor.script.gencell.dialog;
}

const pcell::DialogWidget& widget(const PcellSession& s, std::string_view pname) {
  for (const auto& w : dialog(s).widgets) {
    if (w.parameter == pname) {
      return w;
    }
  }
  FAIL("no widget " << pname);
  return dialog(s).widgets.front();
}

std::vector<std::string> button_texts(const pcell::DialogModel& d) {
  std::vector<std::string> out;
  for (const auto& b : d.buttons) {
    out.push_back(b.text);
  }
  return out;
}

}  // namespace

TEST_CASE("Edit dialog for selected device", "[cap:pcell-toolkit][req:Parameter dialog structure]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::res x1 w 3") == "0");
  const std::string cell = s.value("instance list celldef x1");
  REQUIRE(s.tcl("magic::gencell_dialog x1 {} {} {}").ok());
  const auto& d = dialog(s);
  CHECK(pcell::mode_label(d.mode) == "Edit device:");
  CHECK(d.device == "res");
  CHECK(d.library == "tpdk");
  CHECK(d.cell_name == cell);
  CHECK(d.instance == "x1");
  CHECK(widget(s, "w").value == "3");
  CHECK(button_texts(d) == std::vector<std::string>{"Apply", "Okay", "Reset", "Close"});
  CHECK(d.buttons.front().script == "magic::gencell_change x1 res tpdk {}");
  CHECK(d.buttons[1].closes);
}

TEST_CASE("the new-device dialog", "[cap:pcell-toolkit][req:Parameter dialog structure]") {
  PcellSession s;
  REQUIRE(s.tcl("magic::gencell_dialog {} res tpdk {w 5}").ok());
  const auto& d = dialog(s);
  CHECK(pcell::mode_label(d.mode) == "New device:");
  CHECK(d.cell_name == "(default)");
  CHECK(d.instance == "(default)");
  CHECK(widget(s, "w").value == "5");
  CHECK(button_texts(d) == std::vector<std::string>{"Create", "Create and Close", "Reset", "Close"});
  // Re-opening reuses the window and replaces its contents.
  REQUIRE(s.tcl("magic::gencell_dialog {} res2 tpdk {}").ok());
  CHECK(dialog(s).widgets.size() == 1);
  // Create from the dialog: the button script places a device and edits it.
  REQUIRE(s.tcl("set magic::w_val 4").ok());
  REQUIRE(s.tcl(dialog(s).buttons.front().script).ok());
  CHECK(pcell::mode_label(dialog(s).mode) == "Edit device:");
  CHECK(dialog(s).device == "res2");
}

TEST_CASE("an array subscript is stripped to locate the cell", "[cap:pcell-toolkit][req:Parameter dialog structure]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::res x1") == "0");
  REQUIRE(s.tcl("magic::gencell_dialog {x1[0]} {} {} {}").ok());
  CHECK(dialog(s).device == "res");
  CHECK(dialog(s).instance == "x1");
}

TEST_CASE("Select index", "[cap:pcell-toolkit][req:Dialog builder API]") {
  PcellSession s;
  REQUIRE(s.tcl("magic::gencell_dialog {} res tpdk {}").ok());
  REQUIRE(s.tcl("magic::add_selectindex topc \"Top contact\" {none full half} {w 1}").ok());
  const auto& w = widget(s, "topc");
  CHECK(w.kind == pcell::WidgetKind::selectindex);
  CHECK(w.choices == std::vector<std::string>{"none", "full", "half"});
  CHECK(w.value == "0");
  CHECK(s.value("set magic::topc_val") == "0");
}

TEST_CASE("the builders, getparams and setparams", "[cap:pcell-toolkit][req:Dialog builder API]") {
  PcellSession s;
  REQUIRE(s.tcl("magic::gencell_dialog {} res tpdk {}").ok());
  const auto& d = dialog(s);
  REQUIRE(d.widgets.size() == 6);
  CHECK(widget(s, "gencell").kind == pcell::WidgetKind::selectlist);
  CHECK(widget(s, "gencell").value == "res");
  CHECK(widget(s, "guard").value.empty());  // fallback empty
  CHECK(widget(s, "info").kind == pcell::WidgetKind::message);
  CHECK(widget(s, "info").color == "blue");
  // getparams: entries, checkboxes and selects in grid order.
  CHECK(s.value("magic::gencell_getparams") == "w 1 l 2 gencell res guard {} nf 1");
  // setparams writes every value widget except gencell_sel.
  REQUIRE(s.tcl("magic::gencell_setparams {w 7 l 8 guard 1 nf 2 gencell res2}").ok());
  CHECK(s.value("set magic::w_val") == "7");
  CHECK(widget(s, "w").value == "7");
  CHECK(widget(s, "gencell").value == "res");
  const auto missing = s.tcl("magic::gencell_setparams {w 1}");
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.diagnostics.front().text == "key \"l\" not known in dictionary");
  // _check message variables are plain variables in the magic namespace.
  REQUIRE(s.tcl("set magic::ctot_val 12").ok());
  CHECK(s.value("set magic::ctot_val") == "12");
}

TEST_CASE("builders and setparams without a dialog", "[cap:pcell-toolkit][req:Dialog builder API]") {
  PcellSession s;
  CHECK(s.tcl("magic::gencell_setparams {w 1}").ok());  // does nothing
  const auto r = s.tcl("magic::add_entry w Width {w 1}");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.front().text == "bad window path name \".params.body.area.edits\"");
  const auto usage = s.tcl("magic::add_entry w");
  REQUIRE_FALSE(usage.ok());
  CHECK(usage.diagnostics.front().text == "wrong # args: should be \"magic::add_entry pname ptext parameters\"");
}

TEST_CASE("Dependent recomputation", "[cap:pcell-toolkit][req:Dialog dependencies and validation]") {
  PcellSession s;
  REQUIRE(s.tcl("magic::gencell_dialog {} res tpdk {}").ok());
  const auto& nf = widget(s, "nf");
  // The dependency first, then the two bounds-check bindings of the row.
  REQUIRE(nf.handlers.size() == 3);
  CHECK(nf.handlers[0] == "magic::update_dialog tpdk::res_dep nf res tpdk");
  CHECK(nf.handlers[1] == "magic::update_dialog {} nf res tpdk");
  CHECK(widget(s, "w").handlers.size() == 2);
  CHECK(widget(s, "info").handlers.empty());
  // The user changes nf and presses Return.
  REQUIRE(s.tcl("set tpdk::calls {}; set magic::nf_val 4").ok());
  REQUIRE(s.tcl(nf.handlers[0]).ok());
  CHECK(s.value("set tpdk::calls") == "check {dep nf}");
  CHECK(widget(s, "l").value == "8");
  CHECK(widget(s, "info").value == "checked");
  // _check corrects an invalid value.
  REQUIRE(s.tcl("set magic::w_val 0.5").ok());
  REQUIRE(s.tcl(widget(s, "w").handlers[0]).ok());
  CHECK(widget(s, "w").value == "1");
}

TEST_CASE("add_dependency without names binds every parameter of _defaults (Q-SCRIPT-25)",
          "[cap:pcell-toolkit][req:Dialog dependencies and validation]") {
  for (const char* compat : {"on", "off"}) {
    INFO(compat);
    PcellSession s;
    REQUIRE(s.run(std::string{"setting set magic-compat "} + compat).ok());
    REQUIRE(s.tcl("magic::gencell_dialog {} res tpdk {}").ok());
    REQUIRE(s.tcl("magic::add_dependency tpdk::res_dep res tpdk").ok());
    for (const char* p : {"w", "l", "nf"}) {
      INFO(p);
      CHECK(widget(s, p).handlers.back() == std::string{"magic::update_dialog tpdk::res_dep "} + p + " res tpdk");
    }
    CHECK(dialog(s).dependencies.back().parameters ==
          std::vector<std::string>{"w", "l", "nf", "compatible"});
  }
}

TEST_CASE("the gencell selection switches the device type in update_dialog",
          "[cap:pcell-toolkit][req:Dialog dependencies and validation]") {
  PcellSession s;
  REQUIRE(s.tcl("magic::gencell_dialog {} res tpdk {}").ok());
  REQUIRE(s.tcl("set magic::gencell_val res2").ok());
  REQUIRE(s.tcl("magic::update_dialog {} gencell res tpdk").ok());
  // res2's defaults merged with the dialog: res2 has no `compatible`.
  CHECK(widget(s, "w").value == "1");
}

TEST_CASE("Selecting refreshes the dialog", "[cap:pcell-toolkit][req:Selection-driven dialog refresh and key binding]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::res x1") == "0");
  CHECK(s.value("magic::gencell tpdk::res x2 w 2") == "0");
  REQUIRE(s.tcl("magic::gencell_dialog x1 {} {} {}").ok());
  // The `select` tag (magic::gencell_update %1) after `select cell x2`.
  REQUIRE(s.run("select cell x2").ok());
  REQUIRE(s.tcl("magic::gencell_update cell").ok());
  CHECK(dialog(s).instance == "x2");
  CHECK(widget(s, "w").value == "2");
  // Other select forms and a closed dialog do nothing.
  REQUIRE(s.tcl("magic::gencell_update box").ok());
  CHECK(dialog(s).instance == "x2");
  s.editor.script.gencell.dialog.reset();
  REQUIRE(s.tcl("magic::gencell_update cell").ok());
  CHECK_FALSE(s.editor.script.gencell.dialog.has_value());
}

TEST_CASE("a select run by a device procedure does not refresh the dialog",
          "[cap:pcell-toolkit][req:Selection-driven dialog refresh and key binding]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::res x1") == "0");
  CHECK(s.value("magic::gencell tpdk::res x2 w 2") == "0");
  REQUIRE(s.tcl("magic::gencell_dialog x1 {} {} {}").ok());
  REQUIRE(s.tcl("proc tpdk::sel_defaults {} {return {}}; proc tpdk::sel_convert {p} {return $p}; "
                "proc tpdk::sel_check {p} {return $p}; "
                "proc tpdk::sel_draw {p} {select cell x2; magic::gencell_update cell}").ok());
  REQUIRE(s.tcl("magic::gencell_makecell tpdk::sel").ok());
  CHECK(dialog(s).instance == "x1");
}

TEST_CASE("Control-P edits the selected device", "[cap:pcell-toolkit][req:Selection-driven dialog refresh and key binding]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::res x1") == "0");
  REQUIRE(s.run("select cell x1").ok());
  // The ^P macro runs `magic::gencell {}` (then raises .params).
  REQUIRE(s.tcl("magic::gencell {}").ok());
  CHECK(dialog(s).instance == "x1");
  CHECK(dialog(s).mode == pcell::DialogMode::edit_device);
}

TEST_CASE("PDK device menu entry", "[cap:pcell-toolkit][req:Toolkit menu hooks]") {
  PcellSession s;
  REQUIRE(s.tcl("magic::add_toolkit_menu .layout1.pane.top \"Devices 1\" pdk1").ok());
  REQUIRE(s.tcl("magic::add_toolkit_command .layout1.pane.top \"nmos (MOSFET)\" "
                "\"magic::gencell tpdk::res\" pdk1").ok());
  REQUIRE(s.tcl("magic::add_toolkit_separator .layout1.pane.top pdk1").ok());
  REQUIRE(s.tcl("magic::add_toolkit_button .layout1.pane.top \"Resistor\" res2 pdk1").ok());
  const auto& menus = s.editor.script.gencell.menus.menus;
  REQUIRE(menus.size() == 1);
  CHECK(menus[0].frame == ".layout1.pane.top");
  CHECK(menus[0].library == "pdk1");
  CHECK(menus[0].text == "Devices 1");
  REQUIRE(menus[0].items.size() == 3);
  CHECK(menus[0].items[0].text == "nmos (MOSFET)");
  CHECK(menus[0].items[0].command == "magic::gencell tpdk::res ");
  CHECK(menus[0].items[1].kind == pcell::ToolkitItem::Kind::separator);
  CHECK(menus[0].items[2].command == "magic::gencell pdk1::res2 {} ");
  // The item opens the new-device dialog for that type.
  REQUIRE(s.tcl(menus[0].items[0].command).ok());
  CHECK(dialog(s).mode == pcell::DialogMode::new_device);
  CHECK(dialog(s).device == "res");
  // An item for a menu that does not exist fails as Tk does.
  const auto r = s.tcl("magic::add_toolkit_separator .layout1.pane.top pdk2");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.front().text == "invalid command name \".layout1.pane.top.titlebar.mbuttons.pdk2.toolmenu\"");
}
