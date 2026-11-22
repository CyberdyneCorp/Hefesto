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
    REQUIRE(s.run("ext2spice inv").ok());
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
  REQUIRE(s.run("ext2spice -c 0 inv").ok());
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
  REQUIRE_FALSE(r.ok());
  // EFArgs took `-F` and the cell `inv`; the read finds no inv.ext here.
  CHECK(r.diagnostics.back().text == "Cannot read extract file inv.ext");
  CHECK_FALSE(s.state.netlist.spice.no_attributes);
}

TEST_CASE("render leaves the session's settings untouched", "[cap:netlist-spice-options][req:Command options]") {
  Session s;
  REQUIRE(s.run("ext2spice format cdl").ok());
  REQUIRE(s.run("ext2spice cthresh 0.5").ok());
  s.state.netlist.names.trim_local = true;
  const spice::SpiceSettings before = s.state.netlist.spice;
  const extflat::NameFlags names = s.state.netlist.names;
  // The `lvs` preset is hierarchical (tasks.md 11.6, R3).
  const auto lvs = spice::render(s.state, "inv", spice::SpiceSettings::lvs(before));
  REQUIRE_FALSE(lvs.ok());
  CHECK(lvs.error().code == "CORE-NOTIMPL-008");
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
