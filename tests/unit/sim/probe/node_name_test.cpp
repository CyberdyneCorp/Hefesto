// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// sim::node_name: irsim-interface "Hierarchical node names" and "Node name
// preference order" brought forward for probes (Q-SIM-24 (a); task 14.2
// naming portion), with no length limit (simulation-ui-and-crossprobe
// "Probe names without length limits").
//
// Owner: SIMPROBE.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/db/transform.hpp"
#include "hefesto/sim/probe/probe.hpp"
#include "probe_fixture.hpp"

namespace sim = hefesto::sim;
namespace db = hefesto::db;
using hefesto::test::simprobe::Layout;

TEST_CASE("Fewer path components win", "[cap:irsim-interface][req:Node name preference order]") {
  CHECK(sim::prefer_node_name("top_net", "u1/net3", true));
  CHECK_FALSE(sim::prefer_node_name("u1/net3", "top_net", true));
}

TEST_CASE("Lexicographic tie-break", "[cap:irsim-interface][req:Node name preference order]") {
  CHECK(sim::prefer_node_name("u1/b", "u1/a", true));
  CHECK_FALSE(sim::prefer_node_name("u1/a", "u1/b", true));
}

TEST_CASE("Preference rules: terminal names, globals, labels over generated names",
          "[cap:irsim-interface][req:Node name preference order]") {
  // Rule 1: an `@=` name wins against anything.
  CHECK(sim::prefer_node_name("@=g100,50", "vdd!", true));
  CHECK_FALSE(sim::prefer_node_name("vdd!", "@=g100,50", true));
  // Rule 2: with globals on, a global name wins; between globals, fewer `/`.
  CHECK(sim::prefer_node_name("u1/u2/vdd!", "out", true));
  CHECK(sim::prefer_node_name("u1/vdd!", "u1/u2/vdd!", true));
  CHECK(sim::prefer_node_name("u1/vd!", "u1/gnd!", true));   // shorter
  CHECK(sim::prefer_node_name("u1/vdd!", "u1/gnd!", true));  // same length: greater
  // With globals off, `!` is an ordinary character: fewer `/` wins.
  CHECK(sim::prefer_node_name("out", "u1/u2/vdd!", false));
  // Rule 3: a label beats a generated name, whatever its path.
  CHECK(sim::prefer_node_name("u1/u2/mid", "metal1_0_0#", true));
  // Rule 4: shorter, then greater.
  CHECK(sim::prefer_node_name("ab", "abc", true));
}

TEST_CASE("A labelled node is named by its label", "[cap:irsim-interface][req:Hierarchical node names]") {
  Layout l;
  db::CellDef& top = l.cell("top");
  l.paint(top, "metal1", {0, 0, 10, 2});
  l.label(top, "out", "metal1", {1, 1, 1, 1});
  CHECK(l.name(l.select(top, {}, {5, 1})) == "out");
}

TEST_CASE("An unlabelled node gets the extractor's generated name",
          "[cap:irsim-interface][req:Hierarchical node names]") {
  Layout l;
  db::CellDef& top = l.cell("top");
  l.paint(top, "metal1", {0, 0, 10, 2});
  const std::string name = l.name(l.select(top, {}, {5, 1}));
  CHECK(name.starts_with("metal1_"));
  CHECK(name.ends_with("#"));
}

TEST_CASE("The preferred label of a region names it", "[cap:irsim-interface][req:Hierarchical node names]") {
  Layout l;
  db::CellDef& top = l.cell("top");
  l.paint(top, "metal1", {0, 0, 10, 2});
  l.label(top, "zz_long_name", "metal1", {1, 1, 1, 1});
  l.label(top, "b", "metal1", {8, 1, 8, 1});
  l.label(top, "a", "metal1", {6, 1, 6, 1});
  CHECK(l.name(l.select(top, {}, {5, 1})) == "b");  // shortest, then greater
}

TEST_CASE("A node inside an instance takes the instance path", "[cap:irsim-interface][req:Hierarchical node names]") {
  Layout l;
  db::CellDef& inv = l.cell("inv");
  l.paint(inv, "metal1", {0, 0, 10, 2});
  l.label(inv, "mid", "metal1", {1, 1, 1, 1});
  db::CellDef& buf = l.cell("buf");
  Layout::place(buf, inv, "inv_0", db::translation(100, 0));
  CHECK(l.name(l.select(buf, {"inv_0"}, {5, 1})) == "inv_0/mid");
}

TEST_CASE("Candidates across the hierarchy: fewer components, labels over generated names",
          "[cap:irsim-interface][req:Hierarchical node names]") {
  Layout l;
  db::CellDef& inv = l.cell("inv");
  l.paint(inv, "metal1", {0, 0, 10, 2});
  l.label(inv, "x", "metal1", {1, 1, 1, 1});
  db::CellDef& top = l.cell("top");
  Layout::place(top, inv, "inv_0", db::translation(10, 0));
  l.paint(top, "metal1", {0, 0, 12, 2});  // touches the instance's metal1

  SECTION("an unlabelled top region loses to the instance's label") {
    CHECK(l.name(l.select(top, {}, {1, 1})) == "inv_0/x");
  }
  SECTION("a top label has fewer components") {
    l.label(top, "net_top", "metal1", {1, 1, 1, 1});
    CHECK(l.name(l.select(top, {}, {1, 1})) == "net_top");
    // The same node, selected inside the instance.
    CHECK(l.name(l.select(top, {"inv_0"}, {5, 1})) == "net_top");
  }
}

TEST_CASE("Global name stripped", "[cap:irsim-interface][req:Hierarchical node names]") {
  Layout l;
  db::CellDef& leaf = l.cell("leaf");
  l.paint(leaf, "metal1", {0, 0, 10, 2});
  l.label(leaf, "vdd!", "metal1", {1, 1, 1, 1});
  db::CellDef& mid = l.cell("mid");
  Layout::place(mid, leaf, "u2", db::kIdentity);
  db::CellDef& top = l.cell("top");
  Layout::place(top, mid, "u1", db::kIdentity);
  l.paint(top, "metal1", {0, 0, 4, 2});
  l.label(top, "out", "metal1", {1, 1, 1, 1});

  CHECK(l.name(l.select(top, {}, {1, 1})) == "vdd");
  // Globals off: `!` is ordinary and the top label has fewer components.
  CHECK(l.name(l.select(top, {}, {1, 1}, false)) == "out");
}

TEST_CASE("Array elements are named with their subscripts", "[cap:irsim-interface][req:Hierarchical node names]") {
  Layout l;
  db::CellDef& bit = l.cell("bit");
  l.paint(bit, "metal1", {0, 0, 10, 2});
  l.label(bit, "q", "metal1", {1, 1, 1, 1});
  db::CellDef& top = l.cell("top");
  db::ArrayInfo array;
  array.xlo = 0;
  array.xhi = 3;
  array.xsep = 20;
  Layout::place(top, bit, "b", db::kIdentity, array);
  CHECK(l.name(l.select(top, {"b[2]"}, {5, 1})) == "b[2]/q");
  const auto missing = sim::node_name(l.select(top, {"b[7]"}, {5, 1}));
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == "SIM-PROBE-003");
}

TEST_CASE("A node name longer than 256 bytes is returned whole in both magic-compat modes",
          "[cap:irsim-interface][req:Hierarchical node names]"
          "[cap:simulation-ui-and-crossprobe][req:Probe names without length limits]") {
  // Regression: Magic's SimGetNodeName builds the name in 256-byte static
  // buffers (sim/SimExtract.c:734-767) and overflows them on a longer path
  // or label; Hefesto fixes the crash in both modes.
  Layout l;
  const std::string label(300, 'n');
  db::CellDef& leaf = l.cell("leaf");
  l.paint(leaf, "metal1", {0, 0, 10, 2});
  l.label(leaf, label, "metal1", {1, 1, 1, 1});
  db::CellDef* child = &leaf;
  std::vector<std::string> path;
  std::string expected = label;
  for (int level = 0; level < 12; ++level) {  // "Deep hierarchy": 12 levels
    db::CellDef& parent = l.cell("level" + std::to_string(level));
    const std::string id = "instance_with_a_long_name_" + std::to_string(level);
    Layout::place(parent, *child, id, db::kIdentity);
    path.insert(path.begin(), id);
    expected = id + "/" + expected;
    child = &parent;
  }
  REQUIRE(expected.size() > 600);
  for (const bool compat : {true, false}) {
    sim::LayoutSelection s = l.select(*child, path, {5, 1});
    s.naming.magic_compat = compat;
    CHECK(l.name(s) == expected);
  }
}

TEST_CASE("Naming errors are diagnostics", "[cap:irsim-interface][req:Hierarchical node names]") {
  Layout l;
  db::CellDef& top = l.cell("top");
  l.paint(top, "metal1", {0, 0, 10, 2});
  const auto none = sim::node_name(l.select(top, {}, {50, 50}));
  REQUIRE_FALSE(none.ok());
  CHECK(none.error().code == "SIM-PROBE-004");
  sim::LayoutSelection no_tech = l.select(top, {}, {5, 1});
  no_tech.naming.technology = nullptr;
  CHECK(sim::node_name(no_tech).error().code == "SIM-PROBE-002");
}
