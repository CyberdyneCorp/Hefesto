// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `ext2spice` run flags and output file, and render() for M9
// (capability netlist-spice-options "Run arguments and output file";
// openspec/changes/m7-extraction/design.md D18). The read and flatten are
// EXTFLAT's (extflat::parse_common_args, read, flatten); the runs here read
// small `.ext` files from the session's in-memory file system, and the
// writer is tested on hand-built inputs (flat_netlist_test.cpp,
// device_lines_test.cpp).

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <vector>

#include "hefesto/netlist/spice/run_args.hpp"
#include "hefesto/netlist/spice/spice.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "spice_fixture.hpp"

using namespace spice_test;

namespace {

struct Parsed {
  spice::SpiceSettings settings;
  spice::RunArgs run{settings, {}, {}};
  std::vector<std::size_t> next;
  bool ok = true;

  explicit Parsed(std::vector<std::string> words) {
    for (std::size_t i = 0; i < words.size();) {
      const auto after = spice::parse_spice_flag(words, i, run);
      if (!after) {
        ok = false;
        return;
      }
      next.push_back(*after);
      i = *after;
    }
  }
};

}  // namespace

TEST_CASE("Run flags set the run and the persistent switches",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  Parsed p{{"-d", "-m", "-B", "-F", "-fHSPICE", "-J", "hier"}};
  REQUIRE(p.ok);
  CHECK(p.settings.distributed_junctions);
  CHECK(p.settings.merge == spice::MergeMode::conservative);
  CHECK(p.settings.no_attributes);
  CHECK(p.run.flags.device_nodes_only);
  CHECK(p.settings.format == spice::Dialect::hspice);
  CHECK(p.settings.scale);
  CHECK(p.settings.hier_ap);
  CHECK(p.next == std::vector<std::size_t>{1, 2, 3, 4, 5, 7});
  // -M wins over -m, as Magic's two booleans do.
  Parsed m{{"-M", "-m"}};
  CHECK(m.settings.merge == spice::MergeMode::aggressive);
}

TEST_CASE("Deprecated and unknown run flags", "[cap:netlist-spice-options][req:Run arguments and output file]") {
  Parsed y{{"-y", "3"}};
  REQUIRE(y.ok);
  REQUIRE(y.run.messages.size() == 1);
  CHECK(y.run.messages[0].text == "The cap accuracy flag 'y' is deprecated.");
  Parsed bad{{"-q"}};
  CHECK_FALSE(bad.ok);
  REQUIRE(bad.run.messages.size() == 2);
  CHECK(bad.run.messages[0].text == "Unrecognized flag: -q");
  CHECK(bad.run.messages[1].text.starts_with("Usage: ext2spice [-B] [-o spicefile]"));
  Parsed missing{{"-f"}};
  CHECK_FALSE(missing.ok);
  Parsed format{{"-f", "verilog"}};
  CHECK_FALSE(format.ok);
}

namespace {

// A session whose in-memory file system holds /w/inv.ext.
struct ExtSession : Session {
  explicit ExtSession(std::string_view inv = "node a 0 0 0 0 m1\nnode b 0 0 0 0 m1\n") {
    REQUIRE(services.files().write_atomic(hefesto::platform::Path{"/w/inv.ext"}, inv).ok());
  }
  bool exists(const std::string& name) { return services.files().exists(hefesto::platform::Path{name}); }
  std::string file(const std::string& name) {
    auto text = services.files().read(hefesto::platform::Path{name});
    return text.ok() ? text.value() : std::string{};
  }
};

}  // namespace

// Regression (Q-NETLIST-42, fix in both modes): `-o` names this run's file
// only; the next run without it writes <cell>.spice.
TEST_CASE("The output file name is per run", "[cap:netlist-spice-options][req:Run arguments and output file]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ExtSession s;
    REQUIRE(hefesto::settings::set(s.project, "magic-compat", compat ? "on" : "off").ok());
    REQUIRE(s.run("ext2spice -p /w -o custom.spice inv").ok());
    CHECK(s.exists("custom.spice"));
    CHECK_FALSE(s.exists("inv.spice"));
    REQUIRE(s.services.files().remove(hefesto::platform::Path{"custom.spice"}).ok());
    REQUIRE(s.run("ext2spice -p /w inv").ok());
    CHECK(s.exists("inv.spice"));
    CHECK_FALSE(s.exists("custom.spice"));
  }
}

// Regression: the flattening used the default 2 fF threshold instead of the
// run's, so `-c 0` hid efFlatSingleCap's message for a 1 fF coupling.
TEST_CASE("The run's cap threshold reaches the flattening",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  ExtSession s{"node a 0 0 0 0 m1\ncap a nosuch 1000\n"};
  REQUIRE(s.run("ext2spice -p /w inv").ok());
  CHECK(s.take_printed().find("cap(2): no such node nosuch") == std::string::npos);
  REQUIRE(s.run("ext2spice -p /w -c 0 inv").ok());
  CHECK(s.take_printed().find("cap(2): no such node nosuch\n") != std::string::npos);
}

// EFArgs runs spcParseArgs for -h first (its usage text), then prints its
// own; -T is a flag of the standalone EFArgs only.
TEST_CASE("ext2spice -h prints both usage texts; -T is unrecognized",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  Session s;
  const auto help = s.run("ext2spice -h");
  REQUIRE_FALSE(help.ok());
  CHECK(s.take_printed().find("Usage: ext2spice [-B] [-o spicefile]") != std::string::npos);
  REQUIRE_FALSE(help.diagnostics.empty());
  CHECK(help.diagnostics.back().text.find("Standard arguments:") != std::string::npos);
  const auto tech = s.run("ext2spice -T scmos inv");
  REQUIRE_FALSE(tech.ok());
  CHECK(s.take_printed().find("Unrecognized flag: -T") != std::string::npos);
  CHECK_FALSE(s.state.netlist.spice.tech.has_value());
}

TEST_CASE("The run reaches EFArgs with the run arguments", "[cap:netlist-spice-options][req:Run arguments and output file]") {
  Session s;
  const auto r = spice::run(s.state, spice::SpiceRun{{"-F", "inv"}});
  REQUIRE(r.ok());
  // EFArgs took `-F` and the cell `inv`; the read finds no inv.ext here.
  CHECK(r.value.to_string() == "Cannot read extract file inv.ext\n");
  CHECK_FALSE(s.state.netlist.spice.no_attributes);
}

TEST_CASE("render leaves the session's settings untouched", "[cap:netlist-spice-options][req:Command options]") {
  Session s;
  REQUIRE(s.run("ext2spice format cdl").ok());
  REQUIRE(s.run("ext2spice cthresh 0.5").ok());
  s.state.netlist.names.trim_local = true;
  const spice::SpiceSettings before = s.state.netlist.spice;
  const extflat::NameFlags names = s.state.netlist.names;
  // The `lvs` preset is hierarchical: it reaches the read, which finds no
  // inv.ext here (hier_test.cpp renders it on a golden case's files).
  const auto lvs = spice::render(s.state, "inv", spice::SpiceSettings::lvs(before));
  REQUIRE_FALSE(lvs.ok());
  CHECK(lvs.error().code == "EXTFLAT-READ-001");
  // Simulation overrides: flat, ngspice, every parasitic.
  spice::SpiceSettings sim = before;
  sim.format = spice::Dialect::ngspice;
  sim.cthresh = 0.0;
  const auto flat = spice::render(s.state, "inv", sim);
  REQUIRE_FALSE(flat.ok());  // no inv.ext here
  CHECK(s.state.netlist.spice == before);
  CHECK(s.state.netlist.names == names);
  CHECK(s.result("ext2spice format") == "cdl");
}

TEST_CASE("Rendering a value never changes the caller's flags", "[cap:netlist-spice-options][req:Command options]") {
  // render()'s writer step: the settings are a value and the name flags a
  // copy, so the same input renders the same text twice.
  spice::SpiceInput in = nltest_input("inv");
  in.nodes = {node("out", 5.0F)};
  spice::SpiceSettings sim;
  sim.format = spice::Dialect::hspice;
  hefesto::compat::CompatSettings compat;
  extflat::NameFlags names;
  const extflat::NameFlags before = names;
  extflat::NameFlags copy = names;
  const std::string first = spice::write_flat(in, sim, {}, compat, copy, kMagicNames).text;
  extflat::NameFlags again = names;
  CHECK(spice::write_flat(in, sim, {}, compat, again, kMagicNames).text == first);
  CHECK(names == before);
  CHECK(copy.trim_local);  // the run's flags went to the copy
}

// Q-NETLIST-54 (compat, M7-14): spcParseArgs' -M -m -f -B -d -J are Magic
// globals, so with magic-compat ON they hold for later runs.
TEST_CASE("Run flags persist for later runs with magic-compat ON",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  ExtSession s;
  REQUIRE(hefesto::settings::set(s.project, "magic-compat", "on").ok());
  REQUIRE(s.run("ext2spice -p /w -f hspice -B -d -m -J hier inv").ok());
  CHECK(s.state.netlist.spice.format == spice::Dialect::hspice);
  CHECK(s.state.netlist.spice.no_attributes);
  CHECK(s.state.netlist.spice.distributed_junctions);
  CHECK(s.state.netlist.spice.merge == spice::MergeMode::conservative);
  CHECK(s.state.netlist.spice.hier_ap);
  REQUIRE(s.run("ext2spice -p /w inv").ok());
  CHECK(s.file("inv.spice").starts_with("* HSPICE file created from inv.ext"));
}

// Q-NETLIST-54 (compat, M7-14): with magic-compat OFF the flags apply to
// the run that names them and leave the session's settings unchanged.
// Regression: ext2spice kept them in both modes.
TEST_CASE("Run flags apply to one run with magic-compat OFF",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  ExtSession s;
  REQUIRE(hefesto::settings::set(s.project, "magic-compat", "off").ok());
  const spice::SpiceSettings before = s.state.netlist.spice;
  REQUIRE(s.run("ext2spice -p /w -f hspice -B -d -m -J hier inv").ok());
  CHECK(s.file("inv.spice").starts_with("* HSPICE file created from inv.ext"));
  CHECK(s.state.netlist.spice.format == before.format);
  CHECK(s.state.netlist.spice.no_attributes == before.no_attributes);
  CHECK(s.state.netlist.spice.distributed_junctions == before.distributed_junctions);
  CHECK(s.state.netlist.spice.merge == before.merge);
  CHECK(s.state.netlist.spice.hier_ap == before.hier_ap);
  CHECK(s.state.netlist.spice.scale == before.scale);
  REQUIRE(s.run("ext2spice -p /w inv").ok());
  CHECK(s.file("inv.spice").starts_with("* SPICE3 file created from inv.ext"));
}

// Regression (tasks.md 11.3, NL-SPICE-05): the root's ports were all
// numbered as implicit, in reverse node order, because the flattened
// netlist did not carry the `port` records' indices.
TEST_CASE("The subcircuit wrapper lists the root's ports by their indices",
          "[cap:netlist-spice-options][req:Subcircuit header line]") {
  ExtSession s{"port in 0 0 0 1 1 m1\nport out 1 0 10 1 11 m1\nnode in 0 0 0 0 m1\nnode out 0 0 0 10 m1\n"
               "node mid 0 0 0 20 m1\n"};
  REQUIRE(s.run("ext2spice subcircuits top on").ok());
  REQUIRE(s.run("ext2spice -p /w inv").ok());
  CHECK(s.file("inv.spice").find(".subckt inv in out\n") != std::string::npos);
}

// Q-NETLIST-61 (compat-neutral): Magic's EFDone frees EFSearchPath at the
// end of every run, so `-p` holds for the run that names it. Regression:
// ext2spice kept it for later runs (ext2sim did not).
TEST_CASE("ext2spice -p holds for one run", "[cap:netlist-spice-options][req:Run arguments and output file]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ExtSession s;
    REQUIRE(hefesto::settings::set(s.project, "magic-compat", compat ? "on" : "off").ok());
    REQUIRE(s.run("ext2spice -p /w inv").ok());
    REQUIRE(s.services.files().remove(hefesto::platform::Path{"inv.spice"}).ok());
    const auto again = s.run("ext2spice inv");
    CHECK(again.ok() == compat);  // Q-NETLIST-59: a missing .ext is the result with magic-compat ON
    CHECK_FALSE(s.exists("inv.spice"));
  }
}

// Q-NETLIST-61: a run that stops at "Point to a window" returns before
// EFDone in Magic, so its -p is left for the next run (magic-compat ON).
TEST_CASE("ext2spice -p of a run without a cell is left for the next run with magic-compat ON",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  ExtSession s;
  REQUIRE(hefesto::settings::set(s.project, "magic-compat", "on").ok());
  CHECK_FALSE(s.run("ext2spice -p /w").ok());
  REQUIRE(s.run("ext2spice inv").ok());
  CHECK(s.exists("inv.spice"));
}

TEST_CASE("ext2spice -p of a run without a cell is dropped with magic-compat OFF",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  ExtSession s;
  REQUIRE(hefesto::settings::set(s.project, "magic-compat", "off").ok());
  CHECK_FALSE(s.run("ext2spice -p /w").ok());
  CHECK_FALSE(s.run("ext2spice inv").ok());
  CHECK_FALSE(s.exists("inv.spice"));
}

// Regression: the flat path applied EFArgs' -t after the read, so with
// magic-compat ON a failed read dropped it; Magic sets EFOutputFlags while
// parsing, and the hierarchical path already did.
TEST_CASE("A failed read keeps -t with magic-compat ON",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  ExtSession s;
  REQUIRE(hefesto::settings::set(s.project, "magic-compat", "on").ok());
  REQUIRE_FALSE(s.state.netlist.names.trim_local);
  REQUIRE(s.run("ext2spice -p /w -t # nosuch").ok());
  CHECK(s.state.netlist.names.trim_local);
}

TEST_CASE("A -t never reaches the session's flags with magic-compat OFF",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  ExtSession s;
  REQUIRE(hefesto::settings::set(s.project, "magic-compat", "off").ok());
  CHECK_FALSE(s.run("ext2spice -p /w -t # nosuch").ok());
  CHECK_FALSE(s.state.netlist.names.trim_local);
}

// Regression (both modes): the wrapper sized its port list by the highest
// port number of the `.ext` file; the list is sparse now, and the read
// caps the number (EXTFLAT-READ-009).
TEST_CASE("Sparse port numbers keep their order; a number past the cap fails the read",
          "[cap:netlist-spice-options][req:Subcircuit header line]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ExtSession s{"port in 1048576 0 0 1 1 m1\nport out 7 0 10 1 11 m1\nnode in 0 0 0 0 m1\nnode out 0 0 0 10 m1\n"};
    REQUIRE(hefesto::settings::set(s.project, "magic-compat", compat ? "on" : "off").ok());
    REQUIRE(s.run("ext2spice subcircuits top on").ok());
    REQUIRE(s.run("ext2spice -p /w inv").ok());
    CHECK(s.file("inv.spice").find(".subckt inv out in\n") != std::string::npos);
    ExtSession hostile{"port in 2147483647 0 0 1 1 m1\nnode in 0 0 0 0 m1\n"};
    REQUIRE(hefesto::settings::set(hostile.project, "magic-compat", compat ? "on" : "off").ok());
    const auto r = hostile.run("ext2spice -p /w inv");
    REQUIRE_FALSE(r.ok());
    REQUIRE_FALSE(r.diagnostics.empty());
    CHECK(r.diagnostics.back().code == "EXTFLAT-READ-009");
  }
}

// Q-NETLIST-61: EFSearchPath is shared by both tools, so the path an
// ext2spice run without a cell left is read by the next ext2sim run.
TEST_CASE("ext2sim reads with the -p left by ext2spice without a cell (magic-compat ON)",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  ExtSession s;
  REQUIRE(hefesto::settings::set(s.project, "magic-compat", "on").ok());
  CHECK_FALSE(s.run("ext2spice -p /w").ok());
  REQUIRE(s.run("ext2sim inv").ok());
  CHECK(s.exists("inv.sim"));
}

// Q-NETLIST-59 (compat): EFReadFile leaves `Cannot read extract file
// <cell>.ext` as the Tcl result and ext2spice returns OK in Magic
// (NL-SPICE-01); with magic-compat OFF the run fails with EXTFLAT-READ-001.
// Flat and hierarchical runs alike; no netlist is written.
TEST_CASE("ext2spice of a missing .ext returns the message and succeeds with magic-compat ON",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  for (const char* hierarchy : {"off", "on"}) {
    INFO("hierarchy " << hierarchy);
    ExtSession s;
    REQUIRE(hefesto::settings::set(s.project, "magic-compat", "on").ok());
    REQUIRE(s.run(std::string{"ext2spice hierarchy "} + hierarchy).ok());
    const auto r = s.run("ext2spice -p /w nosuch");
    REQUIRE(r.ok());
    CHECK(r.value.to_string() == "Cannot read extract file nosuch.ext\n");
    CHECK_FALSE(s.exists("nosuch.spice"));
  }
}

TEST_CASE("ext2spice of a missing .ext fails with EXTFLAT-READ-001 with magic-compat OFF",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  for (const char* hierarchy : {"off", "on"}) {
    INFO("hierarchy " << hierarchy);
    ExtSession s;
    REQUIRE(hefesto::settings::set(s.project, "magic-compat", "off").ok());
    REQUIRE(s.run(std::string{"ext2spice hierarchy "} + hierarchy).ok());
    const auto r = s.run("ext2spice -p /w nosuch");
    REQUIRE_FALSE(r.ok());
    REQUIRE_FALSE(r.diagnostics.empty());
    CHECK(r.diagnostics.back().code == "EXTFLAT-READ-001");
    CHECK(r.diagnostics.back().text == "Cannot read extract file nosuch.ext");
    CHECK_FALSE(s.exists("nosuch.spice"));
  }
}
