// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `extresist` options through the dispatcher (capability
// extresist-command "Command options"; tasks.md 14.1): the thresholds
// (Q-NETLIST-28 for mindelay), the on/off settings, `skip` lists
// (Q-NETLIST-23), the net lists, `tolerance`, and the FastHenry frequency
// default (Q-NETLIST-21). Owner: RESCMD.

#include <catch2/catch_test_macros.hpp>

#include "resist_fixture.hpp"

using namespace resist_test;  // NOLINT(google-build-using-namespace): the fixture's aliases

TEST_CASE("extresist threshold, minresist and mindelay set and query", "[cap:extresist-command][req:Command options]") {
  Session s;
  CHECK(s.run("extresist threshold").value == cmd::Value{10000.0});
  REQUIRE(s.run("extresist threshold 50000").ok());
  CHECK(s.editor.extresist.options.threshold == 50000.0);
  CHECK(s.run("extresist minresist").value == cmd::Value{1000.0});
  REQUIRE(s.run("extresist minresist 250").ok());
  CHECK(s.editor.extresist.options.min_resist == 250.0);
  // Q-NETLIST-28 (replicate): given and queried in ps, stored x 1e9 as zs.
  REQUIRE(s.run("extresist mindelay 2.5").ok());
  CHECK(s.run("extresist mindelay").value == cmd::Value{2.5});
  CHECK(extresist::min_delay_zs(s.editor.extresist.options) == 2.5e9);
}

// Magic stores the negative value after the usage line.
TEST_CASE("a negative threshold with magic-compat ON is stored", "[cap:extresist-command][req:Command options]") {
  Session s;
  REQUIRE(s.run("extresist threshold -5").ok());
  CHECK(s.take() == std::vector<std::string>{"Usage:  extresist threshold [value]"});
  CHECK(s.editor.extresist.options.threshold == -5.0);
  REQUIRE(s.run("extresist minresist -1").ok());
  CHECK(s.take() == std::vector<std::string>{"Usage:  extresist minres [value]"});
}

TEST_CASE("a negative threshold with magic-compat OFF keeps the value", "[cap:extresist-command][req:Command options]") {
  Session s(false);
  REQUIRE(s.run("extresist threshold -5").ok());
  CHECK(s.take() == std::vector<std::string>{"Usage:  extresist threshold [value]"});
  CHECK(s.editor.extresist.options.threshold == 10000.0);
  REQUIRE(s.run("extresist mindelay -1").ok());
  CHECK(s.editor.extresist.options.min_delay_ps == 1.0);
}

TEST_CASE("extresist on/off settings", "[cap:extresist-command][req:Command options]") {
  Session s;
  REQUIRE(s.run("extresist simplify").ok());
  CHECK(s.take() == std::vector<std::string>{"on"});
  REQUIRE(s.run("extresist simplify of").ok());
  CHECK_FALSE(s.editor.extresist.options.simplify);
  REQUIRE(s.run("extresist lumped on").ok());
  CHECK(s.editor.extresist.options.lumped);
  REQUIRE(s.run("extresist extout off").ok());
  CHECK_FALSE(s.editor.extresist.options.extout);
  REQUIRE(s.run("extresist silent maybe").ok());
  CHECK(s.take() == std::vector<std::string>{"Value must be either \"on\" or \"off\"."});
  CHECK_FALSE(s.editor.extresist.options.silent);
  REQUIRE(s.run("extresist blackbox on").ok());
  CHECK(s.editor.extresist.options.blackbox);
}

// Magic checks no value for debug and stats: any other word turns them on.
TEST_CASE("extresist debug and stats with magic-compat ON take any word", "[cap:extresist-command][req:Command options]") {
  Session s;
  REQUIRE(s.run("extresist debug maybe").ok());
  REQUIRE(s.run("extresist stats o").ok());  // ambiguous: on as well
  CHECK(s.take().empty());
  CHECK(s.editor.extresist.options.debug);
  CHECK(s.editor.extresist.options.stats);
}

TEST_CASE("extresist debug and stats with magic-compat OFF reject other words",
          "[cap:extresist-command][req:Command options]") {
  Session s(false);
  for (const char* line : {"extresist debug maybe", "extresist stats o"}) {
    INFO(line);
    REQUIRE(s.run(line).ok());
    CHECK(s.take() == std::vector<std::string>{"Value must be either \"on\" or \"off\"."});
  }
  CHECK_FALSE(s.editor.extresist.options.debug);
  CHECK_FALSE(s.editor.extresist.options.stats);
}

// Q-NETLIST-23 (fix, both modes): a comma list; a bad type changes nothing.
TEST_CASE("extresist skip takes a list of types", "[cap:extresist-command][req:Command options]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(compat);
    REQUIRE(s.run("extresist skip pc,metal1").ok());
    tech::TypeMask both = tech::TypeMask::of(s.type("pc"));
    both.set(s.type("metal1"));
    CHECK(s.editor.extresist.options.skip == both);
    REQUIRE(s.run("extresist skip").ok());
    CHECK(s.editor.extresist.options.skip.empty());
  }
}

// Regression (Q-NETLIST-23): Magic writes out of bounds for a bad type.
TEST_CASE("extresist skip with an unknown type leaves the masks unchanged",
          "[cap:extresist-command][req:Command options]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(compat);
    REQUIRE(s.run("extresist skip pc").ok());
    (void)s.take();
    REQUIRE(s.run("extresist skip metal1,nosuchtype").ok());
    CHECK(s.take() == std::vector<std::string>{"Unrecognized layer (type) name \"nosuchtype\""});
    CHECK(s.editor.extresist.options.skip == tech::TypeMask::of(s.type("pc")));
  }
}

TEST_CASE("extresist force, ignore and include lists", "[cap:extresist-command][req:Command options]") {
  Session s;
  REQUIRE(s.run("extresist force clk").ok());
  REQUIRE(s.run("extresist force clk").ok());
  REQUIRE(s.run("extresist force rst").ok());
  CHECK(s.editor.extresist.options.force == std::vector<std::string>{"clk", "rst"});
  REQUIRE(s.run("extresist force").ok());
  const std::vector<std::string> listed = s.take();
  REQUIRE(listed.size() == 1);
  CHECK((listed[0] == "clk rst " || listed[0] == "rst clk "));
  REQUIRE(s.run("extresist force NONE").ok());
  CHECK(s.editor.extresist.options.force.empty());
  REQUIRE(s.run("extresist include clk").ok());
  REQUIRE(s.run("extresist ignore clk").ok());
  CHECK(s.editor.extresist.options.include == std::vector<std::string>{"clk"});
  CHECK(s.editor.extresist.options.ignore == std::vector<std::string>{"clk"});
  REQUIRE(s.run("extresist include all").ok());
  CHECK(s.editor.extresist.options.include.empty());
}

TEST_CASE("extresist tolerance is deprecated", "[cap:extresist-command][req:Command options]") {
  Session s;
  REQUIRE(s.run("extresist tolerance 3").ok());
  CHECK(s.take() == std::vector<std::string>{"Note:  This option has been deprecated and is unused."});
}

// Q-NETLIST-21 (fix): the 10 MHz default is stored; Magic prints an
// uninitialised value.
TEST_CASE("extresist fasthenry with a bad frequency assumes 10 MHz", "[cap:extresist-command][req:Command options]") {
  Session s(true, false);
  (void)s.run("extresist fasthenry abc");
  const std::vector<std::string> lines = s.take();
  REQUIRE(lines.size() >= 2);
  CHECK(lines[0] == "Cannot parse frequency value.  Assuming default");
  CHECK(lines[1] == "Frequency = 10000000.0 Hz");
  CHECK_FALSE(s.editor.extresist.options.fasthenry_frequency.has_value());
  (void)s.run("extresist fasthenry 2e9");
  CHECK(s.editor.extresist.options.fasthenry_frequency == 2e9);
}

TEST_CASE("geometry and fasthenry restore the settings after the run", "[cap:extresist-command][req:Command options]") {
  Session s(true, false);
  REQUIRE(s.run("extresist lumped on").ok());
  REQUIRE(s.run("extresist geometry").ok());  // no box: the run stops at once
  CHECK(s.take() == std::vector<std::string>{"Couldn't find def corresponding to box"});
  CHECK(s.editor.extresist.options.lumped);
  CHECK(s.editor.extresist.options.simplify);
  CHECK(s.editor.extresist.options.extout);
  CHECK_FALSE(s.editor.extresist.options.all);
}

TEST_CASE("extresist cell needs a selected cell", "[cap:extresist-command][req:Command options]") {
  Session s;
  REQUIRE(s.run("extresist cell").ok());
  CHECK(s.take() == std::vector<std::string>{"No cell selected"});
}

TEST_CASE("extresist box needs a box and a type", "[cap:extresist-command][req:Command options]") {
  Session s(true, false);
  REQUIRE(s.run("extresist box poly").ok());
  CHECK(s.take() == std::vector<std::string>{"Sorry, the box must appear in one of the windows."});
}
