// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `antennacheck` command and run (antenna/antenna.hpp; capability
// antenna-check "Antenna check command", "Gate nets visited once",
// "Violation reporting"; tasks.md 15.2): hand-written `.ext` files read
// and flattened by EXTFLAT, the layout traced in a window's cell, the
// progress lines, `debug` and the feedback. Owner: ANTENNA.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "antenna_fixture.hpp"

using namespace antenna_test;  // NOLINT(google-build-using-namespace): the fixture's aliases

namespace {

// One nfet whose gate net reaches a 100 x 100 metal1 plate: A_g = 20,
// a[metal1] = 10000, a violation at metal1 (S = 25 > 20).
void violating_gate(Session& s) {
  s.paint("nfet", db::Rect{0, 0, 2, 10});
  s.paint("poly", db::Rect{2, 0, 6, 10});
  s.paint("pc", db::Rect{6, 0, 10, 10});
  s.paint("metal1", db::Rect{10, 0, 110, 100});
  s.ext("top", ext_header() +
                   "node \"g\" 0 0 0 0 poly\nnode \"s\" 0 0 0 0 ndiff\nnode \"d\" 0 0 0 0 ndiff\n"
                   "node \"gnd!\" 0 0 0 0 pwell\n"
                   "device mosfet nfet 0 0 1 1 2 10 \"gnd!\" \"g\" 4 0 \"s\" 10 0 \"d\" 10 0\n");
}

std::vector<std::string> feedback_texts(const Session& s) {
  std::vector<std::string> out;
  for (const auto& area : s.editor.feedback.areas()) {
    out.push_back(area.message());
  }
  return out;
}

}  // namespace

TEST_CASE("antennacheck reports the progress lines", "[cap:antenna-check][req:Antenna check command]") {
  Session s;
  violating_gate(s);
  std::vector<std::string> lines;
  REQUIRE(s.check({}, lines).ok());
  CHECK(lines == std::vector<std::string>{"Reading extract file.", "Building flattened netlist.",
                                          "Running antenna checks.", "antennacheck finished."});
}

TEST_CASE("antennacheck needs a plane order", "[cap:antenna-check][req:Antenna check command]") {
  Session s(tech_text(" noplaneordering\n" + kDevice));
  REQUIRE(s.run("antennacheck").ok());
  CHECK(s.take() == std::vector<std::string>{"No planeorder specified for this process:  Cannot run antenna checks!"});
}

TEST_CASE("antennacheck rejects an unknown flag", "[cap:antenna-check][req:Antenna check command]") {
  Session s;
  violating_gate(s);
  std::vector<std::string> lines;
  REQUIRE_FALSE(s.check({"-x"}, lines).ok());
  REQUIRE(lines.size() >= 2);
  CHECK(lines[0] == "Unrecognized flag: -x");
  CHECK(lines[1] == "Usage: antennacheck");
  CHECK_FALSE(has(lines, "Reading extract file."));
  CHECK(s.editor.feedback.areas().empty());
}

// Beyond Magic, both modes: Magic dereferences the missing window when a
// cell is named.
TEST_CASE("antennacheck with a cell name and no window", "[cap:antenna-check][req:Antenna check command]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(tech_text(partial_rules()), compat, false);
    std::vector<std::string> lines;
    REQUIRE(s.check({"top"}, lines, false, compat, false).ok());
    CHECK(lines == std::vector<std::string>{"Point to a window or specify a cell name."});
  }
}

// Q-MISC-91 (compat): ON, `antennacheck run` checks a cell named `run`.
TEST_CASE("antennacheck run with magic-compat ON reads run.ext", "[cap:antenna-check][req:Antenna check command]") {
  Session s;
  violating_gate(s);
  const cmd::CommandResult r = s.run("antennacheck run");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().text == "Cannot read extract file run.ext");
}

TEST_CASE("antennacheck run with magic-compat OFF reads the named cell",
          "[cap:antenna-check][req:Antenna check command]") {
  Session s(tech_text(partial_rules()), false);
  violating_gate(s);
  REQUIRE(s.run("antennacheck run top").ok());
  CHECK(has(s.take(), "antennacheck finished."));
  CHECK_FALSE(s.editor.feedback.areas().empty());
}

TEST_CASE("antennacheck debug arms the next run only", "[cap:antenna-check][req:Antenna check command]") {
  Session s;
  violating_gate(s);
  REQUIRE(s.run("antennacheck debug").ok());
  CHECK(s.take().empty());  // no check
  CHECK(s.editor.feedback.areas().empty());
  REQUIRE(s.run("antennacheck").ok());
  const std::vector<std::string> first = s.take();
  CHECK(has(first, "Cell: top"));
  CHECK(has(first, "Antenna violation detected at plane metal1"));
  CHECK(has(first, "Effective antenna ratio 500 > limit 400"));
  CHECK(has(first, "Gate rect (0 0) to (2 10)"));
  CHECK(has(first, "Antenna rect (10 0) to (110 100)"));
  REQUIRE(s.run("antennacheck").ok());
  CHECK_FALSE(has(s.take(), "Cell: top"));  // the flag was cleared
}

TEST_CASE("feedback for a violation", "[cap:antenna-check][req:Violation reporting]") {
  Session s;
  violating_gate(s);
  std::vector<std::string> lines;
  REQUIRE(s.check({}, lines).ok());
  // The gate tile, then the non-space tiles of the check plane metal1 (the
  // poly contact and the plate); none at metal2 or metal3.
  std::vector<db::Rect> rects;
  for (const auto& area : s.editor.feedback.areas()) {
    rects.push_back(area.area);
  }
  CHECK(rects == std::vector<db::Rect>{db::Rect{0, 0, 2, 10}, db::Rect{6, 0, 10, 10}, db::Rect{10, 0, 110, 100}});
  for (const std::string& text : feedback_texts(s)) {
    CHECK(text == "Antenna error at plane metal1\n");
  }
  CHECK_FALSE(has(lines, "Cell: top"));  // no debug lines without `debug`
}

TEST_CASE("no feedback without a violation", "[cap:antenna-check][req:Violation reporting]") {
  Session s(tech_text(kOrder + " model partial surface\n antenna metal1 surface 600 none\n" + kDevice));
  violating_gate(s);  // S = 10000 / 600 < 20
  std::vector<std::string> lines;
  REQUIRE(s.check({}, lines).ok());
  CHECK(s.editor.feedback.areas().empty());
}

TEST_CASE("two transistors sharing a gate are checked once", "[cap:antenna-check][req:Gate nets visited once]") {
  Session s;
  violating_gate(s);
  // A second device on the same gate node at the same place: the feedback
  // of one evaluation only.
  s.ext("top", ext_header() +
                   "node \"g\" 0 0 0 0 poly\nnode \"s\" 0 0 0 0 ndiff\nnode \"d\" 0 0 0 0 ndiff\n"
                   "node \"gnd!\" 0 0 0 0 pwell\n"
                   "device mosfet nfet 0 0 1 1 2 10 \"gnd!\" \"g\" 4 0 \"s\" 10 0 \"d\" 10 0\n"
                   "device mosfet nfet 1 1 2 2 2 10 \"gnd!\" \"g\" 4 0 \"d\" 10 0 \"s\" 10 0\n");
  std::vector<std::string> lines;
  REQUIRE(s.check({}, lines).ok());
  CHECK(s.editor.feedback.areas().size() == 3);
}

// Regression (Q-MISC-93, both modes): Magic indexes the plane table with
// extGetDevType's -1 for a model with no style device line; without the
// guard Hefesto dereferences the empty type (a libc++ hardening abort). The
// device is skipped; the violating nfet is still checked.
TEST_CASE("a device whose model has no style device line is skipped",
          "[cap:antenna-check][req:Gate nets visited once]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(tech_text(partial_rules()), compat);
    violating_gate(s);
    s.ext("top", ext_header() +
                     "node \"g\" 0 0 0 0 poly\nnode \"h\" 0 0 0 0 poly\nnode \"s\" 0 0 0 0 ndiff\n"
                     "node \"d\" 0 0 0 0 ndiff\nnode \"gnd!\" 0 0 0 0 pwell\n"
                     "device mosfet nostyle 0 0 1 1 2 10 \"gnd!\" \"h\" 4 0 \"s\" 10 0 \"d\" 10 0\n"
                     "device mosfet nfet 0 0 1 1 2 10 \"gnd!\" \"g\" 4 0 \"s\" 10 0 \"d\" 10 0\n");
    std::vector<std::string> lines;
    REQUIRE(s.check({}, lines, false, compat).ok());
    CHECK(has(lines, "antennacheck finished."));
    CHECK(s.editor.feedback.areas().size() == 3);
  }
}

TEST_CASE("the progress line every 100 distinct gate nets", "[cap:antenna-check][req:Gate nets visited once]") {
  Session s;
  std::string text = ext_header() + "node \"gnd!\" 0 0 0 0 pwell\nnode \"s\" 0 0 0 0 ndiff\n";
  for (int i = 0; i < 100; ++i) {
    text += "node \"g" + std::to_string(i) + "\" 0 0 0 0 poly\n";
  }
  // 200 devices on 100 gate nodes, each node twice.
  for (int i = 0; i < 200; ++i) {
    const std::string x = std::to_string(1000 + 10 * i);
    const std::string x1 = std::to_string(1001 + 10 * i);
    text += "device mosfet nfet " + x + " 0 " + x1 + " 1 2 10 \"gnd!\" \"g" + std::to_string(i % 100) +
            "\" 4 0 \"s\" 10 0 \"s\" 10 0\n";
  }
  s.ext("top", text);
  std::vector<std::string> lines;
  REQUIRE(s.check({}, lines).ok());
  CHECK(has(lines, "   100 gates analyzed."));
  CHECK_FALSE(has(lines, "   200 gates analyzed."));
}

// Magic clears efAntennaDebug only at the end of a run; a run that returns
// early (here: no window) leaves it armed.
TEST_CASE("antennacheck debug survives a run that stops early", "[cap:antenna-check][req:Antenna check command]") {
  Session s(tech_text(partial_rules()), true, false);
  violating_gate(s);
  REQUIRE(s.run("antennacheck debug").ok());
  REQUIRE(s.run("antennacheck").ok());
  CHECK(s.take() == std::vector<std::string>{"Point to a window or specify a cell name."});
  REQUIRE(s.editor.windows.open(s.top, view::ScreenRect{0, 0, 999, 999}).ok());
  REQUIRE(s.run("antennacheck").ok());
  CHECK(has(s.take(), "Cell: top"));
}
