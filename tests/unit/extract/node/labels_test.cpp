// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Label attachment, label roles, name selection and the per-node records
// (capability extract-nodes-and-labels; openspec/changes/m7-extraction
// tasks.md 5.2, 5.3). Every emitted file is read back by extflat's reader.
// Owner: EXTNODE.

#include <catch2/catch_test_macros.hpp>

#include "node_fixture.hpp"

namespace hefesto::test::extnode {
namespace {

struct Metal {
  std::shared_ptr<const tech::Technology> t = case_tech("EX-019");
  extract::ExtStyle style = first_style(*t);
  std::unique_ptr<db::CellDef> cell = new_cell(*t);
};

TEST_CASE("A label on the boundary attaches through its first probe point",
          "[cap:extract-nodes-and-labels][req:Label attachment]") {
  Metal m;
  paint(*m.cell, *m.t, "metal1", {10, 0, 20, 20});
  paint(*m.cell, *m.t, "metal1", {-20, 0, -12, 20});
  label(*m.cell, *m.t, "b", "metal1", {10, 10, 10, 10});  // C = (10,10) is in the right shape
  label(*m.cell, *m.t, "edge", "metal1", {-12, 5, -12, 5});  // C lies in space, C+(-1,-1) in metal
  const extract::NodeRegionList nodes = nodes_of(*m.cell, *m.t, m.style);
  REQUIRE(nodes.size() == 2);
  for (const extract::NodeRegion& node : nodes) {
    REQUIRE(node.labels.size() == 1);
    CHECK(node.labels[0].text == (node.reference.x == 10 ? "b" : "edge"));
  }
}

TEST_CASE("A floating label becomes a sticky-label node with zero values",
          "[cap:extract-nodes-and-labels][req:Label attachment]") {
  Metal m;
  paint(*m.cell, *m.t, "metal1", {0, 0, 4, 4});
  label(*m.cell, *m.t, "orphan", "metal1", {40, 2, 42, 4});
  Flat flat;
  extract_flat(flat, *m.cell, *m.t, m.style);
  const auto nodes = lines_with(flat.text, "node ");
  REQUIRE(nodes.size() == 2);
  CHECK(nodes[0] == "node \"orphan\" 0 0 40 2 m1");  // prepended as created
  CHECK(nodes[1] == "node \"metal1_0_0#\" 0 0 0 0 m1");
  CHECK(read_back("ex", flat.text).empty());
}

TEST_CASE("Ports go to the head of a node's label list",
          "[cap:extract-nodes-and-labels][req:Label attachment]") {
  Metal m;
  paint(*m.cell, *m.t, "metal1", {0, 0, 10, 10});
  label(*m.cell, *m.t, "int1", "metal1", {1, 1, 1, 1});
  label(*m.cell, *m.t, "A", "metal1", {2, 2, 2, 2}, 1);
  label(*m.cell, *m.t, "B", "metal1", {3, 3, 3, 3}, 2);
  const extract::NodeRegionList nodes = nodes_of(*m.cell, *m.t, m.style);
  REQUIRE(nodes.size() == 1);
  REQUIRE(nodes[0].labels.size() == 3);
  CHECK(nodes[0].labels[0].text == "B");
  CHECK(nodes[0].labels[1].text == "A");
  CHECK(nodes[0].labels[2].text == "int1");
}

TEST_CASE("Label roles by trailing character", "[cap:extract-nodes-and-labels][req:Label types by trailing character]") {
  CHECK(extract::label_role("x@") == extract::LabelRole::node_attribute);
  CHECK(extract::label_role("x$") == extract::LabelRole::terminal_attribute);
  CHECK(extract::label_role("x^") == extract::LabelRole::gate_attribute);
  CHECK(extract::label_role("vdd") == extract::LabelRole::name);
  CHECK(extract::label_role("") == extract::LabelRole::none);
}

TEST_CASE("An attribute label is not a name and is written as an attribute",
          "[cap:extract-nodes-and-labels][req:Label types by trailing character]") {
  Metal m;
  paint(*m.cell, *m.t, "metal1", {0, 0, 10, 10});
  label(*m.cell, *m.t, "x@", "metal1", {1, 1, 1, 1});
  label(*m.cell, *m.t, "vdd", "metal1", {2, 2, 2, 2});
  Flat flat;
  extract_flat(flat, *m.cell, *m.t, m.style);
  CHECK(lines_with(flat.text, "node ") == std::vector<std::string>{"node \"vdd\" 0 0 0 0 m1"});
  CHECK(lines_with(flat.text, "attr ") == std::vector<std::string>{"attr vdd 1 1 1 1 m1 \"x\""});
  CHECK(read_back("ex", flat.text).empty());
}

TEST_CASE("A port beats an internal label and the other port is an alias",
          "[cap:extract-nodes-and-labels][req:Node name selection]") {
  Metal m;
  paint(*m.cell, *m.t, "metal1", {0, 0, 10, 10});
  label(*m.cell, *m.t, "int1", "metal1", {1, 1, 1, 1});
  label(*m.cell, *m.t, "A", "metal1", {2, 2, 2, 2}, 1);
  label(*m.cell, *m.t, "B", "metal1", {3, 3, 3, 3}, 2);
  Flat flat;
  extract_flat(flat, *m.cell, *m.t, m.style);
  CHECK(lines_with(flat.text, "node ") == std::vector<std::string>{"node \"B\" 0 0 3 3 m1"});
  CHECK(lines_with(flat.text, "equiv ") == std::vector<std::string>{"equiv \"B\" \"A\""});
  // The reader's only message is its own shorted-ports warning (EFread).
  const auto messages = read_back("ex", flat.text);
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].code == "EXTFLAT-READ-006");
}

TEST_CASE("With no ports the first name label wins", "[cap:extract-nodes-and-labels][req:Node name selection]") {
  Metal m;
  paint(*m.cell, *m.t, "metal1", {0, 0, 10, 10});
  label(*m.cell, *m.t, "alpha", "metal1", {1, 1, 1, 1});
  label(*m.cell, *m.t, "beta", "metal1", {2, 2, 2, 2});
  const extract::NodeRegionList nodes = nodes_of(*m.cell, *m.t, m.style);
  REQUIRE(nodes.size() == 1);
  CHECK(extract::node_name(nodes[0], *m.t, compat_mode(true)) == "alpha");
}

void attribute_then_two_names(Metal& m) {
  paint(*m.cell, *m.t, "metal1", {0, 0, 10, 10});
  label(*m.cell, *m.t, "cap=1f@", "metal1", {1, 1, 1, 1});
  label(*m.cell, *m.t, "alpha", "metal1", {2, 2, 2, 2});
  label(*m.cell, *m.t, "beta", "metal1", {3, 3, 3, 3});
}

TEST_CASE("Q-EXTRACT-4: an attribute attached before two names, magic-compat ON: the later name wins",
          "[cap:extract-nodes-and-labels][req:Node name selection]") {
  Metal m;
  attribute_then_two_names(m);
  const extract::NodeRegionList nodes = nodes_of(*m.cell, *m.t, m.style, true);
  REQUIRE(nodes.size() == 1);
  CHECK(extract::node_name(nodes[0], *m.t, compat_mode(true)) == "beta");
}

TEST_CASE("Q-EXTRACT-4: an attribute attached before two names, magic-compat OFF: the first name wins",
          "[cap:extract-nodes-and-labels][req:Node name selection]") {
  Metal m;
  attribute_then_two_names(m);
  const extract::NodeRegionList nodes = nodes_of(*m.cell, *m.t, m.style, false);
  REQUIRE(nodes.size() == 1);
  CHECK(extract::node_name(nodes[0], *m.t, compat_mode(false)) == "alpha");
}

TEST_CASE("A port named like its node moves the node's reference to the label",
          "[cap:extract-nodes-and-labels][req:Port records]") {
  Metal m;
  paint(*m.cell, *m.t, "metal1", {0, 0, 50, 10});
  label(*m.cell, *m.t, "OUT", "metal1", {40, 2, 42, 4}, 3);
  Flat flat;
  extract_flat(flat, *m.cell, *m.t, m.style);
  CHECK(lines_with(flat.text, "port ") == std::vector<std::string>{"port \"OUT\" 3 40 2 42 4 m1"});
  CHECK(lines_with(flat.text, "node ") == std::vector<std::string>{"node \"OUT\" 0 0 40 2 m1"});
  CHECK(flat.text.find("port ") < flat.text.find("node "));
  CHECK(read_back("ex", flat.text).empty());
}

TEST_CASE("Node records in list order with their attribute records and class values",
          "[cap:extract-nodes-and-labels][req:Node and attribute records]") {
  const auto t = case_tech("EX-025");
  const extract::ExtStyle style = first_style(*t);
  auto cell = case_cell("EX-025", *t);
  label(*cell, *t, "n5", "pwell", {1, 1, 1, 1});
  label(*cell, *t, "cap=10f@", "pwell", {2, 2, 2, 2});
  extract::NodeRecordOptions options;
  options.resistance = {3228};
  Flat flat;
  extract_flat(flat, *cell, *t, style, true, options);
  CHECK(lines_with(flat.text, "node ") == std::vector<std::string>{"node \"n5\" 3228 0 0 0 pw 416 96"});
  CHECK(lines_with(flat.text, "attr ") == std::vector<std::string>{"attr n5 2 2 2 2 pw \"cap=10f\""});
  CHECK(read_back("ex", flat.text).empty());
}

TEST_CASE("The substrate name without the substrate record names a plain node (an abstract view's node)",
          "[cap:extract-nodes-and-labels][req:Node and attribute records]") {
  // extMakeNodeNumPrint names the node at -infinity with extSubsName even
  // when glob_subsnode is NULL (an abstract view); extOutputNodes then
  // writes it as `node`, not `substrate`.
  const auto t = case_tech("EX-025");
  const extract::ExtStyle style = first_style(*t);
  for (const bool record : {true, false}) {
    auto cell = case_cell("EX-025", *t);
    extract::NodeRecordOptions options;
    options.substrate = extract::SubstrateName{0, "SUBX", record};
    Flat flat;
    extract_flat(flat, *cell, *t, style, true, options);
    INFO("record " << record);
    CHECK(lines_with(flat.text, "substrate \"SUBX\"").size() == (record ? 1U : 0U));
    CHECK(lines_with(flat.text, "node \"SUBX\"").size() == (record ? 0U : 1U));
  }
}

TEST_CASE("Aliases off: non-port aliases are not written and are marked",
          "[cap:extract-nodes-and-labels][req:Alias output]") {
  Metal m;
  paint(*m.cell, *m.t, "metal1", {0, 0, 10, 10});
  label(*m.cell, *m.t, "a", "metal1", {1, 1, 1, 1});
  label(*m.cell, *m.t, "b", "metal1", {2, 2, 2, 2});
  Flat flat;
  extract_flat(flat, *m.cell, *m.t, m.style);
  CHECK(lines_with(flat.text, "node ") == std::vector<std::string>{"node \"a\" 0 0 0 0 m1"});
  CHECK(lines_with(flat.text, "equiv ").empty());
  CHECK(flat.records.non_exported == extract::AliasMarks{false, true});

  // A later attachment in the same extraction skips the marked label.
  extract::RegionFinder finder(*m.t);
  finder.keep_clients = true;
  auto nodes = finder.find_nodes(*m.cell, m.style, {});
  REQUIRE(nodes.ok());
  extract::LabelPassOptions skip;
  skip.skip = &flat.records.non_exported;
  extract::attach_labels(*m.cell, finder, nodes.value(), m.style, *m.t, skip, compat_mode(true));
  extract::reset(*m.cell);
  REQUIRE(nodes.value().size() == 1);
  REQUIRE(nodes.value()[0].labels.size() == 1);
  CHECK(nodes.value()[0].labels[0].text == "a");
}

TEST_CASE("Aliases on: each later name is an equiv record",
          "[cap:extract-nodes-and-labels][req:Alias output]") {
  Metal m;
  paint(*m.cell, *m.t, "metal1", {0, 0, 10, 10});
  label(*m.cell, *m.t, "a", "metal1", {1, 1, 1, 1});
  label(*m.cell, *m.t, "b", "metal1", {2, 2, 2, 2});
  extract::NodeRecordOptions options;
  options.aliases = true;
  Flat flat;
  extract_flat(flat, *m.cell, *m.t, m.style, true, options);
  CHECK(lines_with(flat.text, "equiv ") == std::vector<std::string>{"equiv \"a\" \"b\""});
  CHECK(read_back("ex", flat.text).empty());
}

void two_ports(Metal& m) {
  paint(*m.cell, *m.t, "metal1", {0, 0, 10, 10});
  label(*m.cell, *m.t, "P", "metal1", {1, 1, 1, 1}, 1);
  label(*m.cell, *m.t, "Q", "metal1", {2, 2, 2, 2}, 2);
}

TEST_CASE("Q-EXTRACT-22: shorted ports with lumped resistance on, magic-compat ON: no warning",
          "[cap:extract-nodes-and-labels][req:Alias output]") {
  Metal m;
  two_ports(m);
  Flat flat;
  extract_flat(flat, *m.cell, *m.t, m.style, true);
  CHECK(lines_with(flat.text, "equiv ") == std::vector<std::string>{"equiv \"Q\" \"P\""});
  CHECK(flat.sink.snapshot().empty());
}

TEST_CASE("Q-EXTRACT-22: shorted ports with lumped resistance on, magic-compat OFF: the warning",
          "[cap:extract-nodes-and-labels][req:Alias output]") {
  Metal m;
  two_ports(m);
  Flat flat;
  extract_flat(flat, *m.cell, *m.t, m.style, false);
  CHECK(lines_with(flat.text, "equiv ") == std::vector<std::string>{"equiv \"Q\" \"P\""});
  const auto messages = flat.sink.snapshot();
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].code == "EXTRACT-NODE-001");
  CHECK(messages[0].text == "Warning:  Ports \"Q\" and \"P\" are electrically shorted.");
}

TEST_CASE("Unique names of the same port are not reported as shorted",
          "[cap:extract-nodes-and-labels][req:Alias output]") {
  Metal m;
  paint(*m.cell, *m.t, "metal1", {0, 0, 10, 10});
  label(*m.cell, *m.t, "P_uq0", "metal1", {1, 1, 1, 1}, 1);
  label(*m.cell, *m.t, "P", "metal1", {2, 2, 2, 2}, 2);
  extract::NodeRecordOptions options;
  options.resistance_on = false;
  Flat flat;
  extract_flat(flat, *m.cell, *m.t, m.style, true, options);
  CHECK(flat.sink.snapshot().empty());
}

TEST_CASE("A name on two unconnected nodes leaves one feedback entry per occurrence",
          "[cap:extract-nodes-and-labels][req:Duplicate label warning]") {
  Metal m;
  paint(*m.cell, *m.t, "metal1", {0, 0, 10, 10});
  paint(*m.cell, *m.t, "metal1", {20, 0, 30, 10});
  label(*m.cell, *m.t, "x", "metal1", {5, 5, 5, 5});
  label(*m.cell, *m.t, "x", "metal1", {25, 5, 25, 5});
  const extract::NodeRegionList nodes = nodes_of(*m.cell, *m.t, m.style);
  const auto feedback = extract::duplicate_label_feedback(*m.cell, nodes, *m.t, {});
  REQUIRE(feedback.size() == 2);
  CHECK(feedback[0].area == db::Rect{24, 4, 26, 6});
  CHECK(feedback[0].text == "Label \"x\" attached to more than one unconnected node: metal1_20_0#");
  CHECK(feedback[1].text == "Label \"x\" attached to more than one unconnected node: metal1_0_0#");

  m.cell->properties().put("LEFview", std::string{"TRUE"});
  CHECK(extract::duplicate_label_feedback(*m.cell, nodes, *m.t, {}).empty());
}

// Q-EXTRACT-36 (b), both modes: with magic-compat ON the left side of a
// two-sided split tile stays unvisited once its right side was flooded
// from a neighbour (Q-EXTRACT-31). ExtLabelRegions then dereferences
// CLIENTDEFAULT for a label probed on that side; the label is dropped.
TEST_CASE("Q-EXTRACT-36 (b): a label on the unvisited side of a two-sided split tile is dropped",
          "[cap:extract-nodes-and-labels][req:Label attachment]") {
  const auto t = case_tech("EX-019");
  const extract::ExtStyle style = first_style(*t);
  auto cell = new_cell(*t);
  paint(*cell, *t, "ndiff", {0, 4, 4, 8});  // touches the right (ndiff) side's top edge
  REQUIRE(db::paint_triangle(*cell, *t, {0, 0, 4, 4}, db::Diagonal::nw_se, false, type_of(*t, "poly")).ok());
  REQUIRE(db::paint_triangle(*cell, *t, {0, 0, 4, 4}, db::Diagonal::nw_se, true, type_of(*t, "ndiff")).ok());
  label(*cell, *t, "p", "poly", {1, 1, 1, 1});
  const extract::NodeRegionList nodes = nodes_of(*cell, *t, style, true);
  REQUIRE(nodes.size() == 1);
  CHECK(nodes[0].type == type_of(*t, "ndiff"));
  CHECK(nodes[0].labels.empty());
}

}  // namespace
}  // namespace hefesto::test::extnode
