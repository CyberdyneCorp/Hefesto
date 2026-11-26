// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shorted-port devices (Q-NETLIST-31, Q-NETLIST-60): terminal 0 of the 0 Ω
// resistor or 0 V source built for two shorted ports is the cell's shared
// node `dummy` with magic-compat ON, as in Magic, and a node of its own per
// device with it OFF, written `dummy#<n>` (numbered per cell from 0). The
// runs read a cell with two shorted port pairs from the session's in-memory
// file system, with NL-FLAT-06's nltest technology loaded.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "spice_fixture.hpp"

using namespace spice_test;

namespace {

// Ports A/B and C/D shorted by `equiv` in one cell, then `extra`.
std::string two_pairs(const std::string& extra = {}) {
  return "timestamp 0\nversion 8.3\ntech nltest\nstyle nltest\nscale 1000 1000 5\nresistclasses 1000 5000 100\n"
         "port \"A\" 0 0 0 1 1 metal1\nport \"B\" 1 10 0 11 1 metal1\n"
         "port \"C\" 2 20 0 21 1 metal1\nport \"D\" 3 30 0 31 1 metal1\n"
         "node \"A\" 0 2 0 0 metal1 0 0 0 0 0 0\nnode \"B\" 0 2 10 0 metal1 0 0 0 0 0 0\n"
         "node \"C\" 0 2 20 0 metal1 0 0 0 0 0 0\nnode \"D\" 0 2 30 0 metal1 0 0 0 0 0 0\n"
         "equiv \"A\" \"B\"\nequiv \"C\" \"D\"\n" +
         extra;
}

struct ShortCase : Session {
  explicit ShortCase(bool magic_compat) {
    write("nltest.tech", golden("NL-FLAT-06/input/nltest.tech"));
    REQUIRE(hefesto::settings::set(project, "magic-compat", magic_compat ? "on" : "off").ok());
    REQUIRE(run("tech load /w/nltest -noprompt").ok());
  }

  void write(const std::string& name, const std::string& text) {
    REQUIRE(services.files().write_atomic(hefesto::platform::Path{"/w/" + name}, text).ok());
  }

  std::string netlist(std::string_view command, const std::string& file) {
    const auto r = run(command);
    INFO(take_printed());
    REQUIRE(r.ok());
    auto text = services.files().read(hefesto::platform::Path{file});
    REQUIRE(text.ok());
    return text.value();
  }
};

bool has(const std::string& text, const std::string& line) { return text.find(line) != std::string::npos; }

// A root `top` with one use `m1` of the two-pair cell `mid`.
void hierarchy(ShortCase& c) {
  c.write("mid.ext", two_pairs());
  c.write("top.ext", "timestamp 0\nversion 8.3\ntech nltest\nstyle nltest\nscale 1000 1000 5\n"
                     "resistclasses 1000 5000 100\nuse mid m1 1 0 0 0 1 0\n");
  REQUIRE(c.run("ext2spice hierarchy on").ok());
}

std::string tee_netlist(ShortCase& c) {
  REQUIRE(c.run("ext2spice short resistor").ok());
  REQUIRE(c.run("ext2spice resistor tee on").ok());
  return c.netlist("ext2spice -p /w top", "top.spice");
}

}  // namespace

// Q-NETLIST-31 (compat), magic-compat ON: Magic's shared `dummy` joins the
// two pairs through the tee midpoints.
TEST_CASE("Shorted-port resistor tees share the midpoint dummy with magic-compat ON",
          "[cap:netlist-spice-devices][req:Resistor lines]") {
  ShortCase c{true};
  c.write("top.ext", two_pairs());
  const std::string text = tee_netlist(c);
  CHECK(has(text, "R0A dummy A 0.000000\nR0B dummy B 0.000000\nR1A dummy C 0.000000\nR1B dummy D 0.000000\n"));
}

// Q-NETLIST-31 and Q-NETLIST-60, magic-compat OFF: each short device has its
// own midpoint, `dummy#<n>` numbered per cell. Regression: both used `dummy`.
TEST_CASE("Shorted-port resistor tees get their own midpoint dummy#<n> with magic-compat OFF",
          "[cap:netlist-spice-devices][req:Resistor lines]") {
  ShortCase c{false};
  c.write("top.ext", two_pairs("node \"dummy\" 5 2 50 0 metal1 0 0 0 0 0 0\n"));
  REQUIRE(c.run("ext2spice cthresh 0").ok());
  const std::string text = tee_netlist(c);
  CHECK(has(text, "R0A dummy#0 A 0.000000\nR0B dummy#0 B 0.000000\nR1A dummy#1 C 0.000000\nR1B dummy#1 D 0.000000\n"));
  // A user net named `dummy` stays a net of its own.
  CHECK(has(text, " dummy 0 2f"));
}

TEST_CASE("Hierarchical shorted-port resistor tees share dummy with magic-compat ON",
          "[cap:netlist-spice-devices][req:Resistor lines]") {
  ShortCase c{true};
  hierarchy(c);
  const std::string text = tee_netlist(c);
  CHECK(has(text, ".subckt mid A B C D\nR0A dummy A 0.000000\nR0B dummy B 0.000000\nR1A dummy C 0.000000\n"
                  "R1B dummy D 0.000000\n"));
}

TEST_CASE("Hierarchical shorted-port resistor tees get dummy#<n> per cell with magic-compat OFF",
          "[cap:netlist-spice-devices][req:Resistor lines]") {
  ShortCase c{false};
  hierarchy(c);
  const std::string text = tee_netlist(c);
  CHECK(has(text, ".subckt mid A B C D\nR0A dummy#0 A 0.000000\nR0B dummy#0 B 0.000000\nR1A dummy#1 C 0.000000\n"
                  "R1B dummy#1 D 0.000000\n"));
}
