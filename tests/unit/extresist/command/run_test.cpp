// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `extresist` run flow, net selection and cutoffs (capability
// extresist-command "Run flow", "Net selection and first cutoff", "Second
// cutoff after extraction", "Lumped resistance file"; extresist-output
// "Overlay file grammar and order": the atomic output; tasks.md 14.1-14.3).
// The networks are RESNET's: a run that reaches a net needs them and
// answers their CORE-NOTIMPL-008 until RESNET lands (integration).
// Owner: RESCMD.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <optional>

#include "hefesto/mask/gen/progress.hpp"
#include "resist_fixture.hpp"

using namespace resist_test;  // NOLINT(google-build-using-namespace): the fixture's aliases

namespace {

const std::string kTwoNets =
    "timestamp 0\nversion 8.3\ntech restest\nstyle restest\nscale 1 1 1\n"
    "node \"a\" 20 30000 0 0 poly\nnode \"b\" 20 100000 5 0 poly\n"
    "port \"a\" 0 0 0 1 1 poly\nport \"b\" 1 5 0 6 1 poly\n";

extresist::ResistRun request(Session& s) {
  extresist::ResistRun r;
  r.files = &s.files();
  r.emit = [&s](diag::Diagnostic d) { s.printed.emit(d); };
  return r;
}

// choose_driver on net `net` after ResSortByGate, as ResProcessNode calls it.
std::optional<extresist::Driver> driver_of(extresist::InputModel m, std::size_t net, bool magic_compat = true) {
  m.nodes[net].devices = extresist::sort_by_gate(m, m.nodes[net].devices);
  return extresist::choose_driver(m, net, magic_compat);
}

bool temporary_left(Session& s) {
  auto names = s.files().list(platform::Path{"."});
  if (!names.ok()) {
    return false;
  }
  for (const platform::Path& p : names.value()) {
    if (p.string().find("hefesto-tmp") != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("extresist with no box cell", "[cap:extresist-command][req:Run flow]") {
  Session s(true, false);
  REQUIRE(s.run("extresist").ok());
  CHECK(s.take() == std::vector<std::string>{"Couldn't find def corresponding to box"});
}

TEST_CASE("a run with every net passed over writes only the scale line", "[cap:extresist-command][req:Run flow]") {
  Session s;
  REQUIRE(s.files().write_atomic("top.ext", kTwoNets).ok());
  REQUIRE(s.run("extresist include nosuch").ok());
  REQUIRE(s.run("extresist").ok());
  CHECK(has(s.take(), "Total Nodes: 0"));
  CHECK(s.file("top.res.ext") == extresist::res_ext_scale(*s.editor.extract.current));
}

// Q-NETLIST-67 (compat): with magic-compat ON `extresist cell` runs on the box cell, as Magic's
// ToolGetBox replaces the selected cell; with it OFF on the selected cell.
TEST_CASE("extresist cell with magic-compat ON runs on the box cell", "[cap:extresist-command][req:Run flow]") {
  Session s;
  REQUIRE(s.editor.cells.create("sub", s.editor.technology->plane_count()).ok());
  extresist::ResistRun r = request(s);
  r.cell = "sub";
  REQUIRE(extresist::run(s.editor, r, nullptr).ok());
  CHECK(s.take() == std::vector<std::string>{"Cannot open file top.ext"});
}

TEST_CASE("extresist cell with magic-compat OFF runs on the selected cell", "[cap:extresist-command][req:Run flow]") {
  Session s(false);
  REQUIRE(s.editor.cells.create("sub", s.editor.technology->plane_count()).ok());
  extresist::ResistRun r = request(s);
  r.magic_compat = false;
  r.cell = "sub";
  REQUIRE(extresist::run(s.editor, r, nullptr).ok());
  CHECK(s.take() == std::vector<std::string>{"Cannot open file sub.ext"});
}

TEST_CASE("a missing .ext file is reported and the run goes on", "[cap:extresist-command][req:Run flow]") {
  Session s;
  REQUIRE(s.run("extresist").ok());
  CHECK(s.take() == std::vector<std::string>{"Cannot open file top.ext"});
  CHECK_FALSE(s.files().exists(platform::Path{"top.res.ext"}));
}

// Each processed net goes through RESNET's network (wired at the M7 R4
// integration): with no layout under the node the net is not extracted,
// Magic's "Error in extracting node" is printed and the run goes on.
TEST_CASE("a processed net with no layout is reported and the run goes on", "[cap:extresist-command][req:Run flow]") {
  Session s;
  REQUIRE(s.files().write_atomic("top.ext", kTwoNets).ok());
  REQUIRE(s.files().write_atomic("top.res.ext", "previous\n").ok());
  REQUIRE(s.run("extresist all").ok());
  const std::vector<std::string> printed = s.take();
  for (const std::string& line : printed) {
    UNSCOPED_INFO(line);
  }
  CHECK(has(printed, "Extracting b"));  // nets in reverse order of first mention
  CHECK(has(printed, "Error in extracting node b"));
  CHECK(has(printed, "Error in extracting node a"));
  CHECK(s.file("top.res.ext") == extresist::res_ext_scale(*s.editor.extract.current));
}

// Q-EXTRACT-27 (fix, both modes): a cancelled run writes nothing.
TEST_CASE("a cancelled extresist keeps the previous overlay", "[cap:extresist-output][req:Overlay file grammar and order]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(compat);
    REQUIRE(s.files().write_atomic("top.ext", kTwoNets).ok());
    REQUIRE(s.files().write_atomic("top.res.ext", "previous overlay\n").ok());
    REQUIRE(s.files().write_atomic("top.res.lump", "R a 1\n").ok());
    s.editor.extresist.options.lumped = true;
    const std::atomic<bool> cancel{true};
    hefesto::mask::Progress progress;
    progress.cancel = &cancel;
    extresist::ResistRun r = request(s);
    r.magic_compat = compat;
    const cmd::CommandResult result = extresist::run(s.editor, r, &progress);
    REQUIRE_FALSE(result.ok());
    CHECK(result.diagnostics.front().code == "EXTRESIST-CMD-004");
    CHECK(s.file("top.res.ext") == "previous overlay\n");
    CHECK(s.file("top.res.lump") == "R a 1\n");
    CHECK_FALSE(temporary_left(s));
  }
}

TEST_CASE("nets in reverse order of first mention", "[cap:extresist-command][req:Run flow]") {
  Session s;
  const auto m = read_model(s, "node \"a\" 0 0 0 0 poly\nnode \"b\" 0 0 0 0 poly\nnode \"c\" 0 0 0 0 poly\n");
  REQUIRE(m.ok());
  std::vector<std::string> order;
  for (const std::size_t i : m.value().net_order()) {
    order.push_back(m.value().nodes[i].name);
  }
  CHECK(order == std::vector<std::string>{"c", "b", "a"});
}

// --- net selection and the first cutoff -----------------------------------

TEST_CASE("the include list overrides the ignore list", "[cap:extresist-command][req:Net selection and first cutoff]") {
  extresist::ResistOptions o;
  extresist::InputNode clk;
  clk.name = "clk";
  clk.status.node_record = true;
  o.include = {"clk"};
  o.ignore = {"clk"};
  CHECK(extresist::net_skip(clk, o, false) == extresist::NetSkip::none);
  o.include.clear();
  CHECK(extresist::net_skip(clk, o, false) == extresist::NetSkip::ignored);
  o.ignore.clear();
  clk.status.skip = true;
  CHECK(extresist::net_skip(clk, o, false) == extresist::NetSkip::skipped);
  CHECK(extresist::net_skip(clk, o, true) == extresist::NetSkip::none);
  clk.status.node_record = false;
  CHECK(extresist::net_skip(clk, o, true) == extresist::NetSkip::no_record);
}

TEST_CASE("the delay cutoff", "[cap:extresist-command][req:Net selection and first cutoff]") {
  extresist::ResistOptions o;  // rthresh 10000 mohm, mindelay 1 ps = 1e9 zs
  extresist::InputNode n;
  n.name = "n";
  n.resistance = 20000;
  n.cap = 30000;  // R*C = 6e8 < 1e9
  CHECK_FALSE(extresist::first_cutoff(n, o, false));
  CHECK(extresist::first_cutoff(n, o, true));  // `all`
  o.force = {"n"};
  CHECK(extresist::first_cutoff(n, o, false));
}

TEST_CASE("the resistance cutoff passes", "[cap:extresist-command][req:Net selection and first cutoff]") {
  extresist::ResistOptions o;
  extresist::InputNode n;
  n.resistance = 20000;
  n.cap = 100000;  // R*C = 2e9
  CHECK(extresist::first_cutoff(n, o, false));
  n.cap = 0;  // the substrate: only the resistance counts
  CHECK(extresist::first_cutoff(n, o, false));
  o.threshold = 50000;
  CHECK_FALSE(extresist::first_cutoff(n, o, false));
}

TEST_CASE("the driver is the widest group of parallel devices", "[cap:extresist-command][req:Net selection and first cutoff]") {
  Session s;
  const auto m = read_model(s,
                            "node \"out\" 0 0 0 0 ndiff\nnode \"g1\" 0 0 0 0 poly\nnode \"g2\" 0 0 0 0 poly\n"
                            "node \"x\" 0 0 0 0 ndiff\n"
                            // two parallel devices on g1 (W/L 2 + 2), one wider on g2 (W/L 3)
                            "device mosfet nfet 10 0 11 1 2 4 \"GND\" \"g1\" 4 0 \"out\" 4 0 \"x\" 4 0\n"
                            "device mosfet nfet 20 0 21 1 2 4 \"GND\" \"g1\" 4 0 \"x\" 4 0 \"out\" 4 0\n"
                            "device mosfet nfet 30 0 31 1 2 6 \"GND\" \"g2\" 4 0 \"out\" 6 0 \"x\" 6 0\n");
  REQUIRE(m.ok());
  const auto out = m.value().find("out");
  REQUIRE(out.has_value());
  const auto driver = driver_of(m.value(), *out);
  REQUIRE(driver.has_value());
  CHECK_FALSE(driver->drive_only);
  CHECK((driver->location == db::Point{10, 0} || driver->location == db::Point{20, 0}));
}

namespace {

// A net driven only by narrow devices: W/L 2/8 = 0.25 each, in parallel.
const char* const kNarrow =
    "node \"out\" 0 0 0 0 ndiff\nnode \"g\" 0 0 0 0 poly\nnode \"x\" 0 0 0 0 ndiff\n"
    "device mosfet nfet 10 0 11 1 8 2 \"GND\" \"g\" 8 0 \"out\" 2 0 \"x\" 2 0\n"
    "device mosfet nfet 20 0 21 1 8 2 \"GND\" \"g\" 8 0 \"out\" 2 0 \"x\" 2 0\n";

}  // namespace

// Standing rule (proposed Q-NETLIST-71, compat): Magic sums W/L in its
// int totWL, so a group of devices below W/L 1 drives nothing (NL-RES-02:
// the active_* nets are never extracted), and the driver of a group that
// runs to the end of the list is its last-but-one device.
TEST_CASE("narrow devices drive nothing with magic-compat ON",
          "[cap:extresist-command][req:Net selection and first cutoff]") {
  Session s;
  const auto m = read_model(s, kNarrow);
  REQUIRE(m.ok());
  CHECK_FALSE(driver_of(m.value(), *m.value().find("out"), true).has_value());
}

TEST_CASE("narrow devices drive their net with magic-compat OFF",
          "[cap:extresist-command][req:Net selection and first cutoff]") {
  Session s(false);
  const auto m = read_model(s, kNarrow);
  REQUIRE(m.ok());
  const auto driver = driver_of(m.value(), *m.value().find("out"), false);
  REQUIRE(driver.has_value());
  // The group's last device in the net's list (newest first: the first record).
  CHECK(driver->location == db::Point{10, 0});
}

TEST_CASE("a parallel group at the end of the list drives from its last-but-one device with magic-compat ON",
          "[cap:extresist-command][req:Net selection and first cutoff]") {
  Session s;
  // Two parallel devices of W/L 2 each; the net's list is newest first.
  const auto m = read_model(s,
                            "node \"out\" 0 0 0 0 ndiff\nnode \"g\" 0 0 0 0 poly\nnode \"x\" 0 0 0 0 ndiff\n"
                            "device mosfet nfet 10 0 11 1 2 4 \"GND\" \"g\" 2 0 \"out\" 4 0 \"x\" 4 0\n"
                            "device mosfet nfet 20 0 21 1 2 4 \"GND\" \"g\" 2 0 \"out\" 4 0 \"x\" 4 0\n");
  REQUIRE(m.ok());
  const std::size_t out = *m.value().find("out");
  const auto on = driver_of(m.value(), out, true);
  const auto off = driver_of(m.value(), out, false);
  REQUIRE(on.has_value());
  REQUIRE(off.has_value());
  CHECK(on->location == db::Point{20, 0});   // t1 of the last step: the list's first entry
  CHECK(off->location == db::Point{10, 0});  // the group's last entry
}

// Q-NETLIST-63 (fix, both modes): Magic's ResReadDevice adds a device to
// the node of its substrate terminal without checking that the node exists,
// and a global substrate name with no node record crashes extresist
// (resis/ResReadExt.c:819-820, :725). Hefesto creates the net, which is
// counted but has no record, and the run completes.
TEST_CASE("a substrate terminal on a net with no node record does not stop the run",
          "[cap:extresist-command][req:Input model]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(compat);
    REQUIRE(s.files()
                .write_atomic("top.ext",
                              "timestamp 0\nversion 8.3\ntech restest\nstyle restest\nscale 1 1 1\n"
                              "node \"s\" 10 100 0 0 ndiff\nnode \"g\" 10 100 10 10 poly\n"
                              "device mosfet nfet 10 0 11 1 10 10 \"GND!\" \"g\" 20 0 \"s\" 10 0 \"s\" 10 0\n")
                .ok());
    extresist::ResistRun r = request(s);
    r.magic_compat = compat;
    r.all = true;
    s.editor.extresist.options.all = true;
    REQUIRE(extresist::run(s.editor, r, nullptr).ok());
    const std::vector<std::string> printed = s.take();
    for (const std::string& line : printed) {
      UNSCOPED_INFO(line);
    }
    CHECK(has_prefix(printed, "Total Nets: 2\n"));
    CHECK(s.file("top.res.ext") != "<none>");
  }
}

TEST_CASE("a net named by a FastHenry node list is not processed", "[cap:extresist-command][req:Run flow]") {
  extresist::ResistOptions o;
  extresist::InputNode n;
  n.name = "out";
  n.status.node_record = true;
  n.status.redundant = true;
  CHECK(extresist::net_skip(n, o, true) == extresist::NetSkip::redundant);
}

// Regression (NL-RES-03): PaOpen(<cell>, "w", ".res.lump", ".") writes in
// the current directory only; the cell search path is not tried.
TEST_CASE("the lump file is written in the current directory", "[cap:extresist-command][req:Lumped resistance file]") {
  Session s;
  REQUIRE(s.files().write_atomic("top.ext", kTwoNets).ok());
  REQUIRE(s.files().create_directories(platform::Path{"sub"}).ok());
  s.editor.search_paths.path = {"sub", "."};
  s.editor.extresist.options.lumped = true;
  REQUIRE(s.run("extresist").ok());
  CHECK(s.files().exists(platform::Path{"top.res.lump"}));
  CHECK_FALSE(s.files().exists(platform::Path{"sub/top.res.lump"}));
  CHECK(s.files().exists(platform::Path{"top.res.ext"}));
  CHECK_FALSE(s.files().exists(platform::Path{"sub/top.res.ext"}));
}

TEST_CASE("a drive-located net is driven from its drive point", "[cap:extresist-command][req:Net selection and first cutoff]") {
  Session s;
  const auto m = read_model(s,
                            "node \"out\" 0 0 0 0 ndiff\nnode \"g\" 0 0 0 0 poly\nnode \"x\" 0 0 0 0 ndiff\n"
                            "node \"lonely\" 0 0 0 0 poly\n"
                            "port \"out\" 0 7 8 9 10 ndiff\n"
                            "device mosfet nfet 10 0 11 1 2 4 \"GND\" \"g\" 4 0 \"out\" 4 0 \"x\" 4 0\n");
  REQUIRE(m.ok());
  const auto driver = driver_of(m.value(), *m.value().find("out"));
  REQUIRE(driver.has_value());
  CHECK(driver->location == db::Point{7, 8});
  CHECK(driver->drive_only);
  // A net with no device and no point has no driver.
  CHECK_FALSE(driver_of(m.value(), *m.value().find("lonely")).has_value());
}

// --- the second cutoff and the lump file ----------------------------------

TEST_CASE("a short network is withheld unless forced", "[cap:extresist-command][req:Second cutoff after extraction]") {
  extresist::ResistOptions o;  // rthresh 10000
  extresist::InputNode n;
  n.name = "n";
  n.cap = 100;
  CHECK(extresist::withheld(n, extresist::Extracted{8000, std::nullopt}, o));
  CHECK_FALSE(extresist::withheld(n, extresist::Extracted{12000, std::nullopt}, o));
  o.force = {"n"};
  CHECK_FALSE(extresist::withheld(n, extresist::Extracted{8000, std::nullopt}, o));
  o.force.clear();
  o.threshold = 0;
  CHECK(extresist::withheld(n, extresist::Extracted{8000, 5e8}, o));  // Tdi < 1e9 zs
  CHECK_FALSE(extresist::withheld(n, extresist::Extracted{8000, std::nullopt}, o));
}

TEST_CASE("the lumped resistance in ohms", "[cap:extresist-command][req:Lumped resistance file]") {
  extresist::ResistOptions o;
  o.min_delay_ps = 0;
  extresist::InputNode n;
  n.name = "a";
  CHECK(extresist::lumped_ohms(n, extresist::Extracted{12345, std::nullopt}, o) == 12);
  CHECK(extresist::lump_line("a", 12) == "R a 12\n");
  o.min_delay_ps = 1;
  n.cap = 1000;
  CHECK(extresist::lumped_ohms(n, extresist::Extracted{12345, 5.0e7}, o) == 50);  // (Tdi / C) / 1000
  n.cap = 0;
  CHECK(extresist::lumped_ohms(n, extresist::Extracted{12345, 5.0e7}, o) == 0);
}
