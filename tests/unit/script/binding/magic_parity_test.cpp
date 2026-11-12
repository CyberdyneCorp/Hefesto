// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Engine commands run from Tcl as Magic's Tcl build runs them: regressions
// found by the M4 SCRIPT golden captures (m4-scripting tasks.md 12.5).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "binding_fixture.hpp"

using hefesto::script::test::BindingFixture;

namespace {

bool contains(const std::string& text, std::string_view part) {
  return text.find(part) != std::string::npos;
}

}  // namespace

// Regression (SCRIPT-BOX-01): tools.tcl keeps the box stack in the global
// `boxstack`, which scripts read; Hefesto had no such variable.
TEST_CASE("the box stack is the Tcl global boxstack", "[cap:ui-box-stack-and-measure][req:Box stack push]") {
  BindingFixture f;
  CHECK(f.value("info exists boxstack") == "0");
  REQUIRE(f.eval("pushbox").ok());
  CHECK(f.value("set boxstack") == "{0 0 10 20}");
  // A record set by a script is what popbox applies.
  REQUIRE(f.eval("set boxstack {{1 2 3 4}}").ok());
  CHECK(f.value("popbox") == "1 2 3 4");
  CHECK(f.value("box values") == "1 2 3 4");
  CHECK(f.value("set boxstack") == "");
  // Without a box `box values` prints "Box tool must be present" and the
  // record is empty, as `lappend boxstack [box values]` makes it.
  f.editor.box.reset();
  (void)f.take_printed();
  REQUIRE(f.eval("pushbox").ok());
  CHECK(f.value("set boxstack") == "{}");
  CHECK(contains(f.take_printed(), "Box tool must be present"));
}

// Regression (SCRIPT-BOX-03): the edit stack is the global `editstack`.
TEST_CASE("the edit stack is the Tcl global editstack", "[cap:ui-box-stack-and-measure][req:Edit stack push]") {
  BindingFixture f;
  REQUIRE(f.eval("cellname create a").ok());
  REQUIRE(f.eval("pushstack a").ok());
  CHECK(f.value("llength $editstack") == "2");
  CHECK(f.value("lindex $editstack end") == "top");
  REQUIRE(f.eval("popstack").ok());
  CHECK(f.value("set editstack") == "");
  CHECK(f.error("popstack") == "No subcell stack!");
}

// Regression (SCRIPT-PCELL-04): toolkit.tcl runs `select cell $instname`
// with the empty name of a failed getcell; DBTreeFindUse answers the
// window's root use, so the root is selected and the box covers it.
TEST_CASE("select cell with an empty name selects the window's root cell", "[cap:cmd-selection][req:Cell selection]") {
  BindingFixture f;
  CHECK(f.value("select cell {}") == "");
  CHECK(f.value("box values") == "0 0 1 1");  // the empty root cell's bounding box
  (void)f.take_printed();
  REQUIRE(f.eval("identify M1").ok());
  CHECK(contains(f.take_printed(), "Cell Topmost cell in the window (top) isn't a child of the edit cell."));
}

// Regression (SCRIPT-API-06): `magic::shownet` is a command of Magic's
// netlist window; from a layout window it is Magic's quiet dispatch failure.
TEST_CASE("netlist window commands exist and fail quietly from a layout window",
          "[cap:scripting-api][req:Command registration in the magic namespace]") {
  BindingFixture f;
  CHECK(f.value("info commands ::magic::shownet") == "::magic::shownet");
  CHECK(f.value("list [catch {magic::shownet} e] $e") == "1 {}");
}

// Regression (SCRIPT-API-06, SCRIPT-PCELL-12): `load NAME -fail -silent`
// printed "No file ... found or readable." (DBW_LOAD_QUIET prints nothing).
TEST_CASE("load -fail is quiet under -silent and -quiet", "[cap:cmd-hierarchy][req:Load command]") {
  BindingFixture f;
  (void)f.take_printed();
  REQUIRE(f.eval("load nosuch -fail -silent").ok());
  REQUIRE(f.eval("load nosuch -fail -quiet").ok());
  CHECK_FALSE(contains(f.take_printed(), "No file"));
  REQUIRE(f.eval("load nosuch -fail").ok());
  CHECK(contains(f.take_printed(), "No file \"nosuch\" found or readable."));
}

// Regression (SCRIPT-API-20): `cellname list self` names the selected
// cells (else the edit root), and an arrayed use is named with its first
// indices as dbGetUseName writes them.
TEST_CASE("cellname list self and the names of arrayed instances",
          "[cap:cmd-hierarchy][req:Cellname and instance commands]") {
  BindingFixture f;
  REQUIRE(f.eval("cellname create inv").ok());
  REQUIRE(f.eval("box values 0 0 0 0").ok());
  const std::string use = f.value("getcell inv");
  CHECK(f.value("cellname list self") == "inv");
  REQUIRE(f.eval("array 2 1").ok());
  CHECK(f.value("cellname list instances inv") == "{" + use + "\\[0\\]}");
  REQUIRE(f.eval("select clear").ok());
  CHECK(f.value("cellname list self") == "top");
}

// Regression (SCRIPT-API-18): grid spacings were always lambda; Magic's
// cmdParseCoord reads them in the current units.
TEST_CASE("grid spacings are read in the current units",
          "[cap:ui-windows-and-views][req:Grid display and grid command]") {
  BindingFixture f;
  REQUIRE(f.eval("scalegrid 1 2").ok());
  REQUIRE(f.eval("units internal").ok());
  REQUIRE(f.eval("grid 10 20").ok());
  CHECK(f.value("grid box") == "0 0 10 20");
  REQUIRE(f.eval("units lambda").ok());
  REQUIRE(f.eval("grid 10 20").ok());
  CHECK(f.value("grid box") == "0 0 20 40");
  (void)f.take_printed();
  REQUIRE(f.eval("grid foo").ok());
  const std::string printed = f.take_printed();
  CHECK(contains(printed, "Coordinate value cannot be parsed:  assuming 0"));
  CHECK(contains(printed, "Grid spacing must be greater than zero."));
}

// Regression (SCRIPT-PDK-01): the grid limit ignored the session's grid
// scaling, which Magic applies to the CIF output style.
TEST_CASE("the grid limit applies to the scaled CIF output style",
          "[cap:tech-loading-and-scaling][req:Grid limit check]") {
  BindingFixture f;  // cifoutput style lambda: scalefactor 1
  REQUIRE(f.eval("scalegrid 1 2").ok());
  (void)f.take_printed();
  REQUIRE(f.eval("scalegrid 1 10").ok());
  CHECK(contains(f.take_printed(), "Grid scaling is finer than limit set by the process!"));
  CHECK(f.value("tech lambda") == "1 2");
}

// Regression (SCRIPT-PDK-10, -18): a missing technology file named only
// "." as the searched path; Magic searches and prints its system path.
TEST_CASE("tech load names the system path it searched", "[cap:tech-loading-and-scaling][req:File search rules]") {
  BindingFixture f;
  f.editor.search_paths.system = {".", "$CAD_ROOT/magic/sys", "$CAD_ROOT/magic/sys/current"};
  (void)f.take_printed();
  (void)f.eval("tech load nosuch -noprompt");
  CHECK(contains(f.take_printed(), "Could not find file 'nosuch.tech' in any of these directories:\n"
                                   "         . $CAD_ROOT/magic/sys $CAD_ROOT/magic/sys/current"));
}

// Regression (SCRIPT-PDK-07, SCRIPT-PCELL-12): a refused `save NAME` asks
// for another file name (cmdCheckNewName) before "Cell not saved.".
TEST_CASE("save asks for another file name when the name is taken", "[cap:cmd-hierarchy][req:Save command]") {
  BindingFixture f{true, true, true};  // a console answers through magic::dialog
  REQUIRE(f.eval("proc magic::dialog {args} { lappend ::asked $args; return {} }").ok());
  REQUIRE(f.eval("cellname create other").ok());
  (void)f.take_printed();
  REQUIRE(f.eval("save other").ok());
  CHECK(f.value("set ::asked") == "{{} {File for cell top: [hit return to abort save] }}");
  const std::string printed = f.take_printed();
  CHECK(contains(printed, "Can't rename cell 'top' to 'other' because that cell already exists."));
  CHECK(contains(printed, "Cell not saved."));
}

// Q-CELL-47: cmdCheckNewName reads the file name with `sizeof returnname`,
// the size of a pointer, so Magic keeps the first 7 characters of the
// answer; with magic-compat OFF the whole line is the name.
TEST_CASE("the file name answered to save keeps 7 characters with magic-compat ON",
          "[cap:cmd-hierarchy][req:Save command]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    BindingFixture f{compat, true, true};
    REQUIRE(f.eval("proc magic::dialog {args} { return \"abcdefghij\\n\" }").ok());
    REQUIRE(f.eval("cellname create other").ok());
    (void)f.take_printed();
    REQUIRE(f.eval("save other").ok());
    // The fixture's working directory is not writable: the name shows in the
    // write error.
    CHECK(contains(f.take_printed(), compat ? "writing file abcdefg.mag" : "writing file abcdefghij.mag"));
  }
}
