// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shorted ports in `ext2sim`: the short mode is the one `ext2spice short`
// sets (Magic's EFOutputFlags is one session-wide flag set that both
// netlisters read; `ext2sim default` clears it), and terminal 0 of a short
// device is `dummy` with magic-compat ON and `dummy#<n>` with it OFF
// (Q-NETLIST-31, Q-NETLIST-60). The runs read a cell with two shorted port
// pairs from the session's in-memory file system, with NL-FLAT-06's nltest
// technology loaded.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "../spice/spice_fixture.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/settings/project_settings.hpp"

using spice_test::golden;

namespace {

struct ShortSim : spice_test::Session {
  explicit ShortSim(bool magic_compat) {
    write("nltest.tech", golden("NL-FLAT-06/input/nltest.tech"));
    write("top.ext",
          "timestamp 0\nversion 8.3\ntech nltest\nstyle nltest\nscale 1000 1000 5\nresistclasses 1000 5000 100\n"
          "port \"A\" 0 0 0 1 1 metal1\nport \"B\" 1 10 0 11 1 metal1\n"
          "port \"C\" 2 20 0 21 1 metal1\nport \"D\" 3 30 0 31 1 metal1\n"
          "node \"A\" 0 2 0 0 metal1 0 0 0 0 0 0\nnode \"B\" 0 2 10 0 metal1 0 0 0 0 0 0\n"
          "node \"C\" 0 2 20 0 metal1 0 0 0 0 0 0\nnode \"D\" 0 2 30 0 metal1 0 0 0 0 0 0\n"
          "equiv \"A\" \"B\"\nequiv \"C\" \"D\"\n");
    REQUIRE(hefesto::settings::set(project, "magic-compat", magic_compat ? "on" : "off").ok());
    REQUIRE(run("tech load /w/nltest -noprompt").ok());
  }

  void write(const std::string& name, const std::string& text) {
    REQUIRE(services.files().write_atomic(hefesto::platform::Path{"/w/" + name}, text).ok());
  }

  std::string sim() {
    REQUIRE(run("ext2sim -p /w top").ok());
    auto text = services.files().read(hefesto::platform::Path{"top.sim"});
    REQUIRE(text.ok());
    return text.value();
  }
};

bool has(const std::string& text, const std::string& line) { return text.find(line) != std::string::npos; }

}  // namespace

// Regression (both modes): ext2sim ignored `ext2spice short`, so the ports
// merged; Magic's efBuildEquiv reads the shared EFOutputFlags.
TEST_CASE("ext2sim builds the short devices ext2spice short asks for; ext2sim default clears it",
          "[cap:netlist-sim][req:Command options and defaults]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ShortSim s{compat};
    CHECK(has(s.sim(), "= B A\n"));
    REQUIRE(s.run("ext2spice short resistor").ok());
    const std::string shorted = s.sim();
    CHECK(has(shorted, "r A B 0.000000\nr C D 0.000000\n"));
    CHECK_FALSE(has(shorted, "= B A"));
    REQUIRE(s.run("ext2sim default").ok());
    CHECK(has(s.sim(), "= B A\n"));
  }
}

TEST_CASE("ext2sim writes the shared short node dummy with magic-compat ON",
          "[cap:netlist-sim][req:Device lines]") {
  ShortSim s{true};
  REQUIRE(s.run("ext2spice short voltage").ok());
  CHECK(has(s.sim(), "N dummy A B 0 0 0 0\nN dummy C D 0 0 0 0\n"));
}

// Q-NETLIST-60. Regression: both devices took `dummy`.
TEST_CASE("ext2sim writes each short device's own node dummy#<n> with magic-compat OFF",
          "[cap:netlist-sim][req:Device lines]") {
  ShortSim s{false};
  REQUIRE(s.run("ext2spice short voltage").ok());
  CHECK(has(s.sim(), "N dummy#0 A B 0 0 0 0\nN dummy#1 C D 0 0 0 0\n"));
}
