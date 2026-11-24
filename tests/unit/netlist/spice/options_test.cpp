// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `ext2spice` options (capability netlist-spice-options "Command
// options"; Q-NETLIST-13 and Q-NETLIST-41 per mode). The option sequence
// is NL-SPICE-01's, pinned to the Tcl results Magic returned.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

#include "hefesto/netlist/spice/settings.hpp"
#include "hefesto/netlist/spice/spice.hpp"
#include "spice_fixture.hpp"

using spice_test::Session;
namespace spice = hefesto::netlist::spice;

TEST_CASE("ext2spice options return Magic's results (NL-SPICE-01)", "[cap:netlist-spice-options][req:Command options]") {
  Session s;
  const std::vector<std::pair<std::string, std::string>> probes{
      {"ext2spice format", "spice3"},
      {"ext2spice form hspice", ""},
      {"ext2spice format", "hspice"},
      {"ext2spice scale", "on"},
      {"ext2spice scale off", ""},
      {"ext2spice format hspice", ""},
      {"ext2spice scale", "on"},
      {"ext2spice cthresh", "2.0"},
      {"ext2spice cth 0.5", ""},
      {"ext2spice cthresh", "0.5"},
      {"ext2spice cthresh infinite", ""},
      {"ext2spice cthresh", "infinite"},
      {"ext2spice rthresh", "infinite"},
      {"ext2spice rthresh 10", ""},
      {"ext2spice merge", "none"},
      {"ext2spice merge cons", ""},
      {"ext2spice merge", "conservative"},
      {"ext2spice short", "none"},
      {"ext2spice short res", ""},
      {"ext2spice subcircuits", "on"},
      {"ext2spice subcircuits top", "auto"},
      {"ext2spice subcircuits top on", ""},
      {"ext2spice subcircuits descend auto", ""},
      {"ext2spice subcircuits top", "auto"},
      {"ext2spice hierarchy", "off"},
      {"ext2spice blackbox", "off"},
      {"ext2spice renumber", "off"},
      {"ext2spice global", "on"},
      {"ext2spice extresist", "off"},
      {"ext2spice resistor tee", "off"},
      {"ext2spice resistor tee on", ""},
      {"ext2spice resistor tee", "on"},
      {"ext2spice format bogus", "Bad format type.  Formats are:spice2, spice3, hspice, ngspice, and cdl."},
      {"ext2spice format", "hspice"},
      {"ext2spice lvs", ""},
      {"ext2spice format", "ngspice"},
      {"ext2spice hierarchy", "on"},
      {"ext2spice cthresh", "infinite"},
      {"ext2spice scale", "off"},
      {"ext2spice blackbox", "on"},
      {"ext2spice global", "off"},
      {"ext2spice default", ""},
      {"ext2spice cthresh", "2.0"},
      {"ext2spice rthresh", "infinite"},
      {"ext2spice format", "ngspice"},
  };
  for (const auto& [line, result] : probes) {
    INFO(line);
    const auto r = s.run(line);
    REQUIRE(r.ok());
    CHECK(r.value.to_string() == result);
  }
  CHECK(s.result("ext2spice short") == "resistor");
  CHECK(s.result("ext2spice rthresh 10") == "");
  CHECK(s.result("ext2spice rthresh") == "10");
  CHECK(s.result("exttospice short volt") == "");
  CHECK(s.result("ext2spice short") == "voltage source");
}

TEST_CASE("ext2spice settings persist and lvs is a value", "[cap:netlist-spice-options][req:Command options]") {
  Session s;
  REQUIRE(s.run("ext2spice merge aggressive").ok());
  REQUIRE(s.run("ext2spice renumber yes").ok());
  CHECK(s.state.netlist.spice.merge == spice::MergeMode::aggressive);
  CHECK(s.state.netlist.spice.renumber);
  REQUIRE(s.run("ext2spice lvs").ok());
  spice::SpiceSettings expected;
  expected.merge = spice::MergeMode::aggressive;
  expected.renumber = true;
  CHECK(s.state.netlist.spice == spice::SpiceSettings::lvs(expected));
  // `default` keeps the switches and resets the thresholds and name flags.
  s.state.netlist.names.trim_local = true;
  REQUIRE(s.run("ext2spice default").ok());
  CHECK(s.state.netlist.spice.hierarchy);
  CHECK(s.state.netlist.spice.cthresh == 2.0);
  CHECK_FALSE(s.state.netlist.names.trim_local);
  CHECK(s.state.netlist.names.convert_comma);
}

TEST_CASE("ext2spice bad values print the usage or Magic's message", "[cap:netlist-spice-options][req:Command options]") {
  Session s;
  (void)s.take_printed();
  REQUIRE(s.run("ext2spice scale maybe").ok());
  std::string usage;
  for (const std::string_view entry : spice::spice_options()) {
    usage += "    " + std::string{entry} + "\n";
  }
  CHECK(s.take_printed() == usage);
  CHECK(s.result("ext2spice scale") == "on");
  REQUIRE(s.run("ext2spice cthresh x").ok());
  CHECK(s.take_printed() == "exttospice: numeric value or \"infinite\" expected.\n");
  REQUIRE(s.run("ext2spice merge sometimes").ok());
  CHECK(s.take_printed() ==
        "Merge types are:\n    none\t\t\tdon't merge parallel devices\n    conservative\t\tmerge devices with "
        "same L, W\n    aggressive\t\tmerge devices with same L\n");
  REQUIRE(s.run("ext2spice subcircuits auto").ok());
  CHECK(s.take_printed() == usage);
}

TEST_CASE("HSPICE turns scale mode on", "[cap:netlist-spice-options][req:Command options]") {
  Session s;
  REQUIRE(s.run("ext2spice scale off").ok());
  REQUIRE(s.run("ext2spice format hspice").ok());
  CHECK(s.result("ext2spice scale") == "on");
}

TEST_CASE("subcircuits descend auto sets top auto with magic-compat ON",
          "[cap:netlist-spice-options][req:Command options]") {
  Session s;
  REQUIRE(s.run("ext2spice subcircuits top on").ok());
  REQUIRE(s.run("ext2spice subcircuits descend auto").ok());
  CHECK(s.result("ext2spice subcircuits top") == "auto");
}

TEST_CASE("subcircuits descend auto is a usage error with magic-compat OFF",
          "[cap:netlist-spice-options][req:Command options]") {
  Session s;
  s.project.compat.magic_compat = false;
  REQUIRE(s.run("ext2spice subcircuits top on").ok());
  (void)s.take_printed();
  REQUIRE(s.run("ext2spice subcircuits descend auto").ok());
  CHECK(s.take_printed().starts_with("    [run] [options]"));
  CHECK(s.result("ext2spice subcircuits top") == "on");
}

TEST_CASE("ext2spice run is the cell name with magic-compat ON", "[cap:netlist-spice-options][req:Command options]") {
  Session s;
  // The run reaches EFArgs with `run foo`: `foo` is the cell, whose
  // `.ext` file does not exist here (EFReadFile's Tcl result).
  const auto r = s.run("ext2spice run foo");
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == "Cannot read extract file foo.ext\n");
  // An ambiguous word (resistor, renumber) is a run argument too.
  (void)s.take_printed();
  const auto ambiguous = s.run("ext2spice re");
  REQUIRE(ambiguous.ok());
  CHECK(ambiguous.value.to_string() == "Cannot read extract file re.ext\n");
  CHECK(s.take_printed().find("Ambiguous option") == std::string::npos);
}

TEST_CASE("ext2spice run is a keyword with magic-compat OFF", "[cap:netlist-spice-options][req:Command options]") {
  Session s;
  s.project.compat.magic_compat = false;
  (void)s.take_printed();
  REQUIRE(s.run("ext2spice re").ok());
  CHECK(s.take_printed() == "Ambiguous option: re\n");
  CHECK(s.result("ext2spice renumber") == "off");
}

TEST_CASE("ext2spice scale is the PDK startup setting", "[cap:netlist-spice-options][req:Command options]") {
  // User decision M7-3: M4's `ext2spice scale` is SpiceSettings::scale.
  Session s;
  REQUIRE(s.run("ext2spice scale off").ok());
  CHECK_FALSE(s.state.netlist.spice.scale);
  CHECK(s.result("exttospice scale") == "off");
  REQUIRE(s.run("exttospice scale true").ok());
  CHECK(s.state.netlist.spice.scale);
  CHECK_FALSE(s.state.settings.effective("ext2spice-scale").has_value());
}
