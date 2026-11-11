// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// capability pcell-toolkit: the procedure contract, generated cell
// properties, gencell dispatch, the create and change algorithms
// (Q-SCRIPT-23, 24, 29) and non-placing generation, on the test library
// tests/data/pcell/framework/tpdk.tcl.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "framework_fixture.hpp"
#include "hefesto/pcell/framework/naming.hpp"

namespace pcell = hefesto::pcell;
using hefesto::test::PcellSession;

namespace {

std::string res_cell(const std::string& tcl_parameters) {
  auto p = pcell::ParameterDict::from_tcl(tcl_parameters);
  REQUIRE(p.ok());
  return pcell::generated_cell_name("res", p.value());
}

bool has_error(const PcellSession& s, std::string_view text) {
  const auto errors = s.printed_errors();
  return std::find(errors.begin(), errors.end(), text) != errors.end();
}

void make_fixlib(PcellSession& s) {
  REQUIRE(s.run("cellname create fixlib").ok());
  REQUIRE(s.run("pushstack fixlib").ok());
  REQUIRE(s.run("box values 0 0 10 10").ok());
  REQUIRE(s.run("paint m1").ok());
  REQUIRE(s.run("popstack").ok());
  REQUIRE(s.run("box values 0 0 0 0").ok());  // the box belongs to fixlib after the edit
}

}  // namespace

// ---- Library and device procedure contract ---------------------------------------

TEST_CASE("Unknown device", "[cap:pcell-toolkit][req:Library and device procedure contract]") {
  PcellSession s;
  const auto r = s.tcl("magic::gencell tpdk::nosuchdev x1");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.front().text == "No import routine for tpdk library cell nosuchdev!");
}

TEST_CASE("Default library", "[cap:pcell-toolkit][req:Library and device procedure contract]") {
  PcellSession s;
  const auto r = s.tcl("magic::gencell mycap");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.front().text == "No import routine for toolkit library cell mycap!");
  // A toolkit:: device is found once its _convert exists.
  REQUIRE(s.tcl("namespace eval toolkit { proc mycap_convert {p} {return $p}; proc mycap_defaults {} "
                "{return {c 1}}; proc mycap_check {p} {return $p}; proc mycap_draw {p} {box values 0 0 5 5; "
                "paint m1}}").ok());
  CHECK(s.value("magic::gencell mycap C1 c 2") == "0");
  CHECK(s.value("instance list celldef C1") == res_cell("c 2").replace(0, 3, "mycap"));
}

TEST_CASE("Missing check procedure", "[cap:pcell-toolkit][req:Library and device procedure contract]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::nocheck R1") == "0");
  CHECK(has_error(s, "invalid command name \"tpdk::nocheck_check\""));
  CHECK(s.value("instance list celldef R1") == pcell::generated_cell_name("nocheck", pcell::ParameterDict{{{"w", "2"}}}));
}

TEST_CASE("a family helper without _defaults is recognized and then fails",
          "[cap:pcell-toolkit][req:Library and device procedure contract]") {
  PcellSession s;
  REQUIRE(s.tcl("proc tpdk::mos_convert {p} {return $p}").ok());
  const auto r = s.tcl("magic::gencell tpdk::mos M9");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.front().text == "invalid command name \"tpdk::mos_defaults\"");
}

// ---- Parameter dictionary model ----------------------------------------------------

TEST_CASE("Type switch", "[cap:pcell-toolkit][req:Parameter dictionary model]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::res X1 gencell res2") == "0");
  const std::string cell = s.value("instance list celldef X1");
  CHECK(cell.rfind("res2_", 0) == 0);
  CHECK(s.value("cellname list property " + cell + " gencell") == "res2");
  const std::string stored = s.value("cellname list property " + cell + " parameters");
  // merge(res2_defaults, merge(res_defaults, {gencell res2})), as Magic does.
  CHECK(stored == "w 1 l 2 nf 1 compatible {res res2}");
  CHECK(stored.find("gencell") == std::string::npos);
}

// ---- Generated cell properties --------------------------------------------------------

TEST_CASE("Properties after creation", "[cap:pcell-toolkit][req:Generated cell properties]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::res x1 w 1 l 2 nf 1") == "0");
  const std::string cell = res_cell("w 1 l 2 nf 1 compatible {res res2}");
  CHECK(s.value("instance list celldef x1") == cell);
  CHECK(s.value("cellname list property " + cell + " library") == "tpdk");
  CHECK(s.value("cellname list property " + cell + " gencell") == "res");
  CHECK(s.value("cellname list property " + cell + " parameters") == "w 1 l 2 nf 1 compatible {res res2}");
}

TEST_CASE("the stored parameters keep the text Tcl keeps",
          "[cap:pcell-toolkit][req:Generated cell properties]") {
  PcellSession s;
  // A _check returning its argument unchanged and no arguments: Magic's
  // dict merge with an empty dictionary keeps the literal defaults text.
  REQUIRE(s.tcl("proc tpdk::lit_defaults {} {return {w  1\n l 2}}; proc tpdk::lit_convert {p} {return $p}; "
                "proc tpdk::lit_check {p} {return $p}; proc tpdk::lit_draw {p} {}").ok());
  CHECK(s.value("magic::gencell_makecell tpdk::lit").rfind("lit_", 0) == 0);
  const std::string name = s.value("magic::gencell_makecell tpdk::lit");
  CHECK(s.value("cellname list property " + name + " parameters") == "w  1\n l 2");
}

TEST_CASE("a selected cell without gencell property derives its type from the name",
          "[cap:pcell-toolkit][req:Generated cell properties]") {
  PcellSession s;
  REQUIRE(s.run("cellname create res_12").ok());
  REQUIRE(s.run("box values 0 0 0 0").ok());
  REQUIRE(s.run("getcell res_12").ok());
  REQUIRE(s.run("select cell res_12_0").ok());
  // Library toolkit, type res: no toolkit::res_defaults exists.
  const auto r = s.tcl("magic::gencell {}");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.front().text == "invalid command name \"toolkit::res_defaults\"");
  // A name without the _<digits> form is not a device.
  REQUIRE(s.run("cellname create plain").ok());
  REQUIRE(s.run("getcell plain").ok());
  REQUIRE(s.run("select cell plain_0").ok());
  const auto none = s.tcl("magic::gencell {}");
  REQUIRE_FALSE(none.ok());
  CHECK(none.diagnostics.front().text == "No gencell device is selected!");
}

// ---- gencell entry point dispatch ---------------------------------------------------

TEST_CASE("Scripted creation", "[cap:pcell-toolkit][req:gencell entry point dispatch]") {
  PcellSession s;
  REQUIRE(s.run("box values 100 200 150 260").ok());
  CHECK(s.value("magic::gencell tpdk::res M1 w 2") == "0");
  CHECK(s.value("instance list celldef M1") == res_cell("w 2 l 2 nf 1 compatible {res res2}"));
  REQUIRE(s.run("select cell M1").ok());
  const auto box = s.value("units internal; instance list abutment");
  CHECK(box.rfind("100 200 ", 0) == 0);
}

TEST_CASE("SPICE parameters", "[cap:pcell-toolkit][req:gencell entry point dispatch]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::res M2 -spice w 1.0 l 0.15 mult 2") == "0");
  CHECK(s.value("set tpdk::converted") == "w 1.0 l 0.15 mult 2");
  CHECK(s.value("instance list celldef M2") == res_cell("w 1.0 l 0.15 nf 2 compatible {res res2}"));
}

TEST_CASE("an odd argument count is the dict create error", "[cap:pcell-toolkit][req:gencell entry point dispatch]") {
  PcellSession s;
  const auto r = s.tcl("magic::gencell tpdk::res M3 w");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.front().text == "wrong # args: should be \"dict create ?key value ...?\"");
}

TEST_CASE("Scripted change of a fixed device", "[cap:pcell-toolkit][req:gencell entry point dispatch]") {
  PcellSession s;
  make_fixlib(s);
  CHECK(s.value("magic::gencell tpdk::fix Q1") == "0");
  CHECK(s.value("instance list celldef Q1") == "fixlib");
  // Q-SCRIPT-23 (fixed in both magic-compat settings): no undefined
  // delx/dely error; the instance becomes a 2 x 1 array.
  for (const char* compat : {"on", "off"}) {
    INFO(compat);
    PcellSession t;
    make_fixlib(t);
    REQUIRE(t.run(std::string{"setting set magic-compat "} + compat).ok());
    CHECK(t.value("magic::gencell tpdk::fix Q1") == "0");
    CHECK(t.value("magic::gencell tpdk::fix Q1 nx 2") == "0");
    REQUIRE(t.run("select cell Q1").ok());
    // xlo xhi ylo yhi (a one-element list of them with magic-compat off).
    CHECK(t.value("array -list count").find("0 1 0 0") != std::string::npos);
    // nx comes from the instance (xhi-xlo+1) when not given.
    CHECK(t.value("magic::gencell tpdk::fix Q1 ny 2") == "0");
    REQUIRE(t.run("select cell Q1").ok());
    CHECK(t.value("array -list count").find("0 1 0 1") != std::string::npos);
  }
}

TEST_CASE("an existing instance without arguments opens the edit dialog",
          "[cap:pcell-toolkit][req:gencell entry point dispatch]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::res x1") == "0");
  CHECK_FALSE(s.editor.script.gencell.dialog.has_value());
  CHECK(s.value("magic::gencell tpdk::res x1") == "0");
  REQUIRE(s.editor.script.gencell.dialog.has_value());
  CHECK(s.editor.script.gencell.dialog->mode == pcell::DialogMode::edit_device);
  CHECK(s.editor.script.gencell.dialog->instance == "x1");
  // No instance name: the new-device dialog.
  CHECK(s.value("magic::gencell tpdk::res") == "0");
  CHECK(s.editor.script.gencell.dialog->mode == pcell::DialogMode::new_device);
}

// ---- Create algorithm -----------------------------------------------------------------

TEST_CASE("Draw errors do not abort", "[cap:pcell-toolkit][req:Create algorithm]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::bad B1") == "0");
  CHECK(has_error(s, "bad layer"));
  const std::string cell = pcell::generated_cell_name("bad", pcell::ParameterDict{{{"w", "1"}}});
  CHECK(s.value("instance list celldef B1") == cell);
  CHECK(s.value("cellname list property " + cell + " library") == "tpdk");
  CHECK(s.value("cellname list property " + cell + " gencell") == "bad");
  CHECK(s.value("cellname list property " + cell + " parameters") == "w 1");
}

TEST_CASE("Box on the new instance", "[cap:pcell-toolkit][req:Create algorithm]") {
  PcellSession s;
  // The box is restored before `getcell` (toolkit.tcl:1266-1268), which
  // then sets it to the new instance (CmdGetcell, commands/CmdFI.c:1106):
  // the instance sits at the saved box's lower-left and _draw's box
  // changes do not leak.
  REQUIRE(s.run("box values 100 200 150 260").ok());
  CHECK_FALSE(s.value("magic::gencell_create res tpdk {w 2}").empty());
  CHECK(s.value("box values") == "100 200 120 210");
  // A fixed device: the box is restored after _draw.
  make_fixlib(s);
  REQUIRE(s.run("box values 100 200 150 260").ok());
  CHECK(s.value("magic::gencell_create fix tpdk {nocell 1}") == "fixlib_0");
  CHECK(s.value("box values") == "100 200 150 260");
  CHECK(s.editor.suspend_depth == 0);
}

TEST_CASE("an existing generated cell is reused and drawn into again (Q-SCRIPT-24)",
          "[cap:pcell-toolkit][req:Create algorithm]") {
  PcellSession s;
  const std::string a = s.value("magic::gencell_create res tpdk {w 2}");
  REQUIRE(s.run("box values 100 0 100 0").ok());
  const std::string b = s.value("magic::gencell_create res tpdk {w 2.0}");
  CHECK(a != b);
  CHECK(s.value("instance list celldef " + a) == s.value("instance list celldef " + b));
}

// ---- Change algorithm -----------------------------------------------------------------

TEST_CASE("Headless change keeps the instance name", "[cap:pcell-toolkit][req:Change algorithm]") {
  for (const char* compat : {"on", "off"}) {
    INFO(compat);
    PcellSession s;
    REQUIRE(s.run(std::string{"setting set magic-compat "} + compat).ok());
    CHECK(s.value("magic::gencell tpdk::res M1") == "0");
    CHECK_FALSE(s.editor.script.gencell.dialog.has_value());
    CHECK(s.value("magic::gencell tpdk::res M1 w 2.0") == "0");
    CHECK(s.value("instance list celldef M1") == res_cell("w 2.0 l 2 nf 1 compatible {res res2}"));
    CHECK(s.editor.suspend_depth == 0);
  }
}

TEST_CASE("Shared cell retained", "[cap:pcell-toolkit][req:Change algorithm]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::res x1") == "0");
  REQUIRE(s.run("box values 100 0 100 0").ok());  // Magic refuses an exact copy at the same place
  CHECK(s.value("magic::gencell tpdk::res x2") == "0");
  const std::string old_cell = s.value("instance list celldef x1");
  CHECK(s.value("instance list celldef x2") == old_cell);
  CHECK(s.value("magic::gencell tpdk::res x1 w 2") == "0");
  CHECK(s.value("cellname list exists " + old_cell) != "0");
  CHECK(s.value("instance list celldef x1") == res_cell("w 2 l 2 nf 1 compatible {res res2}"));
}

TEST_CASE("Last user removes old cell", "[cap:pcell-toolkit][req:Change algorithm]") {
  PcellSession s;
  CHECK(s.value("magic::gencell tpdk::res x1") == "0");
  const std::string old_cell = s.value("instance list celldef x1");
  CHECK(s.value("magic::gencell tpdk::res x1 w 2") == "0");
  CHECK(s.value("cellname list exists " + old_cell) == "0");
  CHECK(s.value("instance list celldef x1") == res_cell("w 2 l 2 nf 1 compatible {res res2}"));
}

TEST_CASE("Orientation preserved", "[cap:pcell-toolkit][req:Change algorithm]") {
  PcellSession s;
  REQUIRE(s.run("box values 40 50 40 50").ok());
  const std::string placed = s.value("magic::gencell_create res tpdk {w 1} 90h");
  REQUIRE(s.run("select cell " + placed).ok());
  REQUIRE(s.run("identify x1").ok());
  const std::string before = s.value("units internal; select cell x1; instance list abutment");
  CHECK(s.value("magic::gencell tpdk::res x1 w 2") == "0");
  REQUIRE(s.run("select cell x1").ok());
  CHECK(s.value("instance list orientation") == "90h");
  const std::string after = s.value("units internal; instance list abutment");
  CHECK(after.substr(0, after.find(' ', after.find(' ') + 1)) == before.substr(0, before.find(' ', before.find(' ') + 1)));
}

TEST_CASE("a default instance name follows the new cell", "[cap:pcell-toolkit][req:Change algorithm]") {
  PcellSession s;
  const std::string inst = s.value("magic::gencell_create res tpdk {w 1}");
  const std::string old_cell = s.value("instance list celldef " + inst);
  CHECK(inst.rfind(old_cell, 0) == 0);
  REQUIRE(s.tcl("magic::gencell_change " + inst + " res tpdk {w 2 l 2 nf 1 compatible {res res2}}").ok());
  const std::string new_cell = res_cell("w 2 l 2 nf 1 compatible {res res2}");
  CHECK(s.value("cellname list exists " + old_cell) == "0");
  CHECK(s.value("instance list exists " + new_cell + "_0") == new_cell + "_0");
}

// ---- Non-placing cell generation --------------------------------------------------------

TEST_CASE("Makecell returns name only", "[cap:pcell-toolkit][req:Non-placing cell generation]") {
  PcellSession s;
  const std::string name = s.value("magic::gencell_makecell tpdk::res w 1 l 2 nf 1");
  CHECK(name == res_cell("w 1 l 2 nf 1 compatible {res res2}"));
  CHECK(s.value("cellname list exists " + name) != "0");
  CHECK(s.value("cellname list property " + name + " gencell") == "res");
  CHECK(s.value("cellname list children top").empty());
  // No _check: w 0.5 is not corrected.
  const std::string unchecked = s.value("magic::gencell_makecell tpdk::res w 0.5");
  CHECK(unchecked == res_cell("w 0.5 l 2 nf 1 compatible {res res2}"));
}
