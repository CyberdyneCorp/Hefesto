// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `ext2spice hierarchy on` (capability netlist-spice-hierarchy;
// openspec/changes/m7-extraction tasks.md 11.6): each run reads the inputs
// of a NETLIST golden case from the session's in-memory file system, with
// the case's nltest technology loaded, and is pinned to the netlist Magic
// wrote (expected/collected). The magic-compat OFF behaviours of
// Q-NETLIST-20 and Q-NETLIST-35 have no Magic output and are pinned to
// their decided text.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "hefesto/netlist/spice/spice.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "spice_fixture.hpp"

using namespace spice_test;

namespace {

// A session whose /w holds the input files of golden case `id`, with the
// case's technology loaded.
struct HierCase : Session {
  explicit HierCase(std::string_view id, bool magic_compat = true) {
    const auto dir = std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/golden/NETLIST/cases" / id / "input";
    for (const auto& entry : std::filesystem::directory_iterator{dir}) {
      std::ifstream in{entry.path(), std::ios::binary};
      std::ostringstream text;
      text << in.rdbuf();
      const std::string name = entry.path().filename().string();
      REQUIRE(services.files().write_atomic(hefesto::platform::Path{"/w/" + name}, text.str()).ok());
    }
    REQUIRE(hefesto::settings::set(project, "magic-compat", magic_compat ? "on" : "off").ok());
    REQUIRE(run("tech load /w/nltest -noprompt").ok());
    REQUIRE(run("ext2spice hierarchy on").ok());
  }

  void ext(const std::string& name, const std::string& text) {
    REQUIRE(services.files().write_atomic(hefesto::platform::Path{"/w/" + name}, text).ok());
  }

  std::string netlist(std::string_view command, const std::string& file) {
    REQUIRE(run(command).ok());
    auto text = services.files().read(hefesto::platform::Path{file});
    REQUIRE(text.ok());
    return text.value();
  }
};

std::string expected(std::string_view id, std::string_view file) {
  return golden(std::string{id} + "/expected/collected/" + std::string{file});
}

// The golden `spice-cap` comparison (Q-NETLIST-1): coupling lines `C<n> a b
// v` compare without their counter, terminals in either order, in any
// order within the netlist.
struct CapCanonical {
  std::string rest;
  std::multiset<std::string> caps;
  friend bool operator==(const CapCanonical&, const CapCanonical&) = default;
};

CapCanonical cap_canonical(const std::string& text) {
  CapCanonical out;
  std::istringstream lines{text};
  for (std::string line; std::getline(lines, line);) {
    std::istringstream words{line};
    std::string name;
    std::string a;
    std::string b;
    std::string value;
    std::string extra;
    if (line.starts_with("C") && (words >> name >> a >> b >> value) && !(words >> extra)) {
      out.caps.insert(std::min(a, b) + " " + std::max(a, b) + " " + value);
    } else {
      out.rest += line + "\n";
    }
  }
  return out;
}

}  // namespace

TEST_CASE("Hierarchical sections are written children first, the root last (NL-HIER-01)",
          "[cap:netlist-spice-hierarchy][req:Definition traversal order]") {
  HierCase c{"NL-HIER-01"};
  CHECK(c.netlist("ext2spice -p /w top", "top.spice") == expected("NL-HIER-01", "top.spice"));
}

TEST_CASE("Child nets reached by a merge or a coupling become ports (NL-HIER-02)",
          "[cap:netlist-spice-hierarchy][req:Port promotion]") {
  HierCase c{"NL-HIER-02"};
  REQUIRE(c.run("ext2spice cthresh 0").ok());
  CHECK(cap_canonical(c.netlist("ext2spice -p /w top", "top.spice")) ==
        cap_canonical(expected("NL-HIER-02", "top.cfinite.spice")));
  REQUIRE(c.run("ext2spice cthresh infinite").ok());
  CHECK(c.netlist("ext2spice -p /w top", "top.spice") == expected("NL-HIER-02", "top.cinfinite.spice"));
}

TEST_CASE("Abstract, primitive and wire-only cells are stubbed or omitted (NL-HIER-03)",
          "[cap:netlist-spice-hierarchy][req:Omitted, stubbed and absorbed definitions]") {
  HierCase c{"NL-HIER-03"};
  CHECK(c.netlist("ext2spice -p /w top", "top.spice") == expected("NL-HIER-03", "top.bboff.spice"));
  REQUIRE(c.run("ext2spice blackbox on").ok());
  CHECK(c.netlist("ext2spice -p /w top", "top.spice") == expected("NL-HIER-03", "top.bbon.spice"));
}

TEST_CASE("A root without ports is a top-level circuit under top auto (NL-HIER-04)",
          "[cap:netlist-spice-hierarchy][req:Section layout]") {
  HierCase c{"NL-HIER-04"};
  CHECK(c.netlist("ext2spice -p /w topnp", "topnp.spice") == expected("NL-HIER-04", "topnp.hier.spice"));
  CHECK(c.netlist("ext2spice -p /w topp", "topp.spice") == expected("NL-HIER-04", "topp.hier.spice"));
  REQUIRE(c.run("ext2spice lvs").ok());
  CHECK(c.netlist("ext2spice -p /w topnp", "topnp.spice") == expected("NL-HIER-04", "topnp.lvs.spice"));
  CHECK(c.netlist("ext2spice -p /w topp", "topp.spice") == expected("NL-HIER-04", "topp.lvs.spice"));
}

TEST_CASE("Array uses are called once per element, x outer and y inner (NL-HIER-05)",
          "[cap:netlist-spice-hierarchy][req:Subcircuit calls in hierarchical mode]") {
  HierCase c{"NL-HIER-05"};
  REQUIRE(c.run("ext2spice renumber off").ok());
  CHECK(c.netlist("ext2spice -p /w top", "top.spice") == expected("NL-HIER-05", "top.renumoff.spice"));
  REQUIRE(c.run("ext2spice renumber on").ok());
  CHECK(c.netlist("ext2spice -p /w top", "top.spice") == expected("NL-HIER-05", "top.renumon.spice"));
}

// Q-NETLIST-35 and Q-NETLIST-19 with magic-compat ON: Magic's text.
TEST_CASE("Hierarchical tee rsubckt, negative couplings and shorted ports with magic-compat ON (NL-HIER-06)",
          "[cap:netlist-spice-hierarchy][req:Hierarchical device and parasitic differences]") {
  HierCase c{"NL-HIER-06"};
  REQUIRE(c.run("ext2spice resistor tee on").ok());
  REQUIRE(c.run("ext2spice short voltage").ok());
  REQUIRE(c.run("ext2spice cthresh 0").ok());
  CHECK(cap_canonical(c.netlist("ext2spice -p /w top", "top.spice")) ==
        cap_canonical(expected("NL-HIER-06", "top.spice")));
}

// Q-NETLIST-35 (compat): two sections with a shorted port pair each.
TEST_CASE("The voltage-source counter runs on across sections with magic-compat ON",
          "[cap:netlist-spice-hierarchy][req:Section layout]") {
  HierCase c{"NL-HIER-06"};
  c.ext("rcell2.ext", golden("NL-HIER-06/input/rcell.ext"));
  c.ext("top.ext", "tech nltest\nstyle nltest\nscale 1000 1000 5\nresistclasses 1000 5000 100\n"
        "use rcell c1 1 0 0 0 1 0\nuse rcell2 c2 1 0 100 0 1 0\nnode \"n\" 0 1 0 50 metal1 0 0 0 0 0 0\n"
        "merge \"c1/A\" \"n\" 0\nmerge \"c2/A\" \"n\" 0\n");
  REQUIRE(c.run("ext2spice short voltage").ok());
  const std::string text = c.netlist("ext2spice -p /w top", "top.spice");
  CHECK(text.find("V0 A B 0.0\n") != std::string::npos);
  CHECK(text.find("V1 A B 0.0\n") != std::string::npos);
}

TEST_CASE("The voltage-source counter restarts in each section with magic-compat OFF",
          "[cap:netlist-spice-hierarchy][req:Section layout]") {
  HierCase c{"NL-HIER-06", false};
  c.ext("rcell2.ext", golden("NL-HIER-06/input/rcell.ext"));
  c.ext("top.ext", "tech nltest\nstyle nltest\nscale 1000 1000 5\nresistclasses 1000 5000 100\n"
        "use rcell c1 1 0 0 0 1 0\nuse rcell2 c2 1 0 100 0 1 0\nnode \"n\" 0 1 0 50 metal1 0 0 0 0 0 0\n"
        "merge \"c1/A\" \"n\" 0\nmerge \"c2/A\" \"n\" 0\n");
  REQUIRE(c.run("ext2spice short voltage").ok());
  const std::string text = c.netlist("ext2spice -p /w top", "top.spice");
  const std::size_t first = text.find("V0 A B 0.0\n");
  REQUIRE(first != std::string::npos);
  CHECK(text.find("V0 A B 0.0\n", first + 1) != std::string::npos);
  CHECK(text.find("V1 ") == std::string::npos);
}

// Q-NETLIST-20 (compat): node lines of a section read without a
// `substrate` record (NL-HIER-01's files have none).
TEST_CASE("Hierarchical node lines need a substrate record with magic-compat ON",
          "[cap:netlist-spice-hierarchy][req:Section layout]") {
  HierCase c{"NL-HIER-01"};
  REQUIRE(c.run("ext2spice cthresh 0").ok());
  const std::string text = c.netlist("ext2spice -p /w top", "top.spice");
  CHECK(text.find("C0 out 0 ") == std::string::npos);
}

TEST_CASE("Hierarchical node lines are written without a substrate record with magic-compat OFF",
          "[cap:netlist-spice-hierarchy][req:Section layout]") {
  HierCase c{"NL-HIER-01", false};
  REQUIRE(c.run("ext2spice cthresh 0").ok());
  const std::string text = c.netlist("ext2spice -p /w top", "top.spice");
  CHECK(text.find("C1 out 0 5f\n") != std::string::npos);
}

TEST_CASE("Hierarchical HSPICE names and gate-attribute device names (NL-SPICE-04, NL-SPICE-13)",
          "[cap:netlist-spice-hierarchy][req:Hierarchical device and parasitic differences]") {
  HierCase hs{"NL-SPICE-04"};
  REQUIRE(hs.run("ext2spice format hspice").ok());
  CHECK(hs.netlist("ext2spice -p /w top", "top.spice") == expected("NL-SPICE-04", "top.hier.spice"));
  HierCase attr{"NL-SPICE-13"};
  CHECK(attr.netlist("ext2spice -p /w top", "top.spice") == expected("NL-SPICE-13", "top.hier.spice"));
}

// Regression: with `hierarchy on` the run answered CORE-NOTIMPL-008; a
// missing `.ext` file is EFReadFile's Tcl result with magic-compat ON
// (NL-SPICE-01) and a failure with it OFF.
TEST_CASE("A missing .ext file is the command's result with magic-compat ON",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  HierCase c{"NL-HIER-01"};
  const auto result = c.run("ext2spice -p /w YES");
  REQUIRE(result.ok());
  CHECK(result.value.to_string() == "Cannot read extract file YES.ext\n");
}

TEST_CASE("A missing .ext file fails the run with magic-compat OFF",
          "[cap:netlist-spice-options][req:Run arguments and output file]") {
  HierCase c{"NL-HIER-01", false};
  CHECK_FALSE(c.run("ext2spice -p /w YES").ok());
}

// tasks.md 11.7: the `lvs` preset renders hierarchically, writes no file
// and leaves the session's settings and name flags as they were.
TEST_CASE("render with the lvs preset writes the hierarchical netlist and changes nothing",
          "[cap:netlist-spice-options][req:Command options]") {
  HierCase c{"NL-HIER-04"};
  const spice::SpiceSettings before = c.state.netlist.spice;
  const extflat::NameFlags names = c.state.netlist.names;
  spice::SpiceSettings lvs = spice::SpiceSettings::lvs(before);
  lvs.search_path = {(std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/golden/NETLIST/cases/NL-HIER-04/input").string()};
  const auto text = spice::render(c.state, "topnp", lvs);
  REQUIRE(text.ok());
  CHECK(text.value() == expected("NL-HIER-04", "topnp.lvs.spice"));
  CHECK(c.state.netlist.spice == before);
  CHECK(c.state.netlist.names == names);
  CHECK_FALSE(c.services.files().exists(hefesto::platform::Path{"topnp.spice"}));
}
