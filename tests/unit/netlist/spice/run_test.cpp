// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `ext2spice` run flags and output file, and render() for M9
// (capability netlist-spice-options "Run arguments and output file";
// openspec/changes/m7-extraction/design.md D18). The read and flatten are
// EXTFLAT's (extflat::parse_common_args, read, flatten); until they land
// the run answers their CORE-NOTIMPL-008 and the writer is tested on
// hand-built inputs (flat_netlist_test.cpp, device_lines_test.cpp).

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <vector>

#include "hefesto/netlist/spice/run_args.hpp"
#include "hefesto/netlist/spice/spice.hpp"
#include "spice_fixture.hpp"

using namespace spice_test;

namespace {

struct Parsed {
  spice::SpiceSettings settings;
  spice::RunArgs run{settings, {}, std::nullopt, {}};
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
  Parsed p{{"-d", "-m", "-B", "-F", "-o", "amp.out", "-fHSPICE", "-J", "hier"}};
  REQUIRE(p.ok);
  CHECK(p.settings.distributed_junctions);
  CHECK(p.settings.merge == spice::MergeMode::conservative);
  CHECK(p.settings.no_attributes);
  CHECK(p.run.flags.device_nodes_only);
  CHECK(p.run.output == "amp.out");
  CHECK(p.settings.format == spice::Dialect::hspice);
  CHECK(p.settings.scale);
  CHECK(p.settings.hier_ap);
  CHECK(p.next == std::vector<std::size_t>{1, 2, 3, 4, 6, 7, 9});
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
  Parsed missing{{"-o"}};
  CHECK_FALSE(missing.ok);
  Parsed format{{"-f", "verilog"}};
  CHECK_FALSE(format.ok);
}

TEST_CASE("The output file name is per run", "[cap:netlist-spice-options][req:Run arguments and output file]") {
  // Q-NETLIST-42 (fix): `-o` lives in the run's RunArgs, never in the
  // session settings, so a later run without it writes <cell>.spice.
  spice::SpiceSettings session;
  spice::RunArgs first{session, {}, std::nullopt, {}};
  const std::vector<std::string> words{"-o", "custom.spice"};
  REQUIRE(spice::parse_spice_flag(words, 0, first).has_value());
  CHECK(first.output == "custom.spice");
  spice::RunArgs second{session, {}, std::nullopt, {}};
  CHECK_FALSE(second.output.has_value());
  CHECK(session == spice::SpiceSettings{});
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
  REQUIRE_FALSE(flat.ok());  // EXTFLAT's read (tasks.md 10.1)
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
