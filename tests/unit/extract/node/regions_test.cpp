// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Node connectivity and the node reference point (capability
// extract-nodes-and-labels; openspec/changes/m7-extraction tasks.md 5.1,
// 5.2). Values pinned to the captured EX cases. Owner: EXTNODE.

#include <catch2/catch_test_macros.hpp>

#include "node_fixture.hpp"

namespace hefesto::test::extnode {
namespace {

TEST_CASE("EX-025: a pwell and an abutting psd tap are one node with the summed class area and perimeter",
          "[cap:extract-nodes-and-labels][req:Node connectivity]") {
  const auto t = case_tech("EX-025");
  const extract::ExtStyle style = first_style(*t);
  auto cell = case_cell("EX-025", *t);
  const extract::NodeRegionList nodes = nodes_of(*cell, *t, style);
  REQUIRE(nodes.size() == 1);
  // expected/collected/input/ex.ext: node "well_0_0#" 3228 0 0 0 pw 416 96
  REQUIRE(nodes[0].classes.size() == 1);
  CHECK(nodes[0].classes[0].area == 416);
  CHECK(nodes[0].classes[0].perimeter == 96);
  CHECK(extract::generated_name(nodes[0], *t) == "well_0_0#");
  CHECK(t->type_name(nodes[0].type, tech::NameForm::short_name) == "pw");
  const std::string expected = case_expected("EX-025");
  CHECK(expected.find("node \"well_0_0#\" 3228 0 0 0 pw 416 96\n") != std::string::npos);
}

TEST_CASE("Two unconnected shapes with the same label are two nodes",
          "[cap:extract-nodes-and-labels][req:Node connectivity]") {
  const auto t = case_tech("EX-019");
  const extract::ExtStyle style = first_style(*t);
  auto cell = new_cell(*t);
  paint(*cell, *t, "metal1", {0, 0, 10, 10});
  paint(*cell, *t, "metal1", {20, 0, 30, 10});
  label(*cell, *t, "n1", "metal1", {5, 5, 5, 5});
  label(*cell, *t, "n1", "metal1", {25, 5, 25, 5});
  const extract::NodeRegionList nodes = nodes_of(*cell, *t, style);
  REQUIRE(nodes.size() == 2);
  CHECK(nodes[0].labels.size() == 1);
  CHECK(nodes[1].labels.size() == 1);
}

TEST_CASE("A contact joins its planes and the lowest plane gives the reference",
          "[cap:extract-nodes-and-labels][req:Node reference point and generated name]") {
  const auto t = case_tech("EX-019");
  const extract::ExtStyle style = first_style(*t);
  auto cell = new_cell(*t);
  paint(*cell, *t, "poly", {5, 5, 15, 15});
  paint(*cell, *t, "m1c", {5, 5, 10, 10});
  paint(*cell, *t, "metal1", {-100, 0, 10, 10});
  const extract::NodeRegionList nodes = nodes_of(*cell, *t, style);
  REQUIRE(nodes.size() == 1);
  CHECK(nodes[0].reference == db::Point{5, 5});
  CHECK(t->plane_name(nodes[0].plane) == "active");
}

TEST_CASE("Every client slot is back at kClientDefault after a pass",
          "[cap:extract-nodes-and-labels][req:Node connectivity]") {
  const auto t = case_tech("EX-025");
  const extract::ExtStyle style = first_style(*t);
  auto cell = case_cell("EX-025", *t);
  extract::RegionFinder finder(*t);
  auto found = finder.find_nodes(*cell, style, {});
  REQUIRE(found.ok());
  CHECK(extract::clients_clear(*cell));  // keep_clients off: reset before return

  finder.keep_clients = true;
  found = finder.find_nodes(*cell, style, {});
  REQUIRE(found.ok());
  CHECK_FALSE(extract::clients_clear(*cell));
  // The slot names the node: the psd tile is in the pwell's node.
  const db::Plane& active = cell->plane(t->find_plane("active").id.value());
  const db::TileView psd = active.tile_at({21, 6});
  REQUIRE(finder.node_of(active, psd, found.value().size()) == std::optional<std::size_t>{0});
  extract::reset(*cell);
  CHECK(extract::clients_clear(*cell));
}

TEST_CASE("RegionFinder::find floods the generic connectivity with the caller's callbacks",
          "[cap:extract-nodes-and-labels][req:Node connectivity]") {
  const auto t = case_tech("EX-025");
  const extract::ExtStyle style = first_style(*t);
  auto cell = case_cell("EX-025", *t);
  extract::RegionFinder finder(*t);
  int firsts = 0;
  int tiles = 0;
  extract::RegionCallbacks callbacks;
  callbacks.first = [&](const db::Rect&, tech::TileType, tech::PlaneId) { ++firsts; };
  callbacks.each = [&](const db::Rect&, tech::TileType, tech::PlaneId) { ++tiles; };
  auto found = finder.find(*cell, db::whole_plane(), style.active, extract::ConnectTable{style.node_conn}, callbacks);
  REQUIRE(found.ok());
  CHECK(found.value().size() == 1);
  CHECK(firsts == 1);
  CHECK(tiles == 2);
  CHECK(extract::clients_clear(*cell));
}

TEST_CASE("EX-019: generated names with negative coordinates and the right side of a split tile",
          "[cap:extract-nodes-and-labels][req:Node reference point and generated name]") {
  const auto t = case_tech("EX-019");
  const extract::ExtStyle style = first_style(*t);
  auto cell = case_cell("EX-019", *t);
  Flat flat;
  extract_flat(flat, *cell, *t, style);
  CHECK(flat.text == case_expected("EX-019"));
  CHECK(read_back("ex", flat.text).empty());
}

// A split tile with material on both sides whose right side touches
// nothing else: DBSrPaintClient tests the tile's one slot, so Magic never
// starts a region at the right side once the left side has one.
TEST_CASE("Second side of a split tile as a seed: magic-compat ON skips it",
          "[cap:extract-nodes-and-labels][req:Node connectivity]") {
  const auto t = case_tech("EX-019");
  const extract::ExtStyle style = first_style(*t);
  auto cell = new_cell(*t);
  REQUIRE(db::paint_triangle(*cell, *t, {0, 0, 4, 4}, db::Diagonal::nw_se, false, type_of(*t, "poly")).ok());
  REQUIRE(db::paint_triangle(*cell, *t, {0, 0, 4, 4}, db::Diagonal::nw_se, true, type_of(*t, "ndiff")).ok());
  CHECK(nodes_of(*cell, *t, style, true).size() == 1);
}

TEST_CASE("Second side of a split tile as a seed: magic-compat OFF gives it its node",
          "[cap:extract-nodes-and-labels][req:Node connectivity]") {
  const auto t = case_tech("EX-019");
  const extract::ExtStyle style = first_style(*t, false);
  auto cell = new_cell(*t);
  REQUIRE(db::paint_triangle(*cell, *t, {0, 0, 4, 4}, db::Diagonal::nw_se, false, type_of(*t, "poly")).ok());
  REQUIRE(db::paint_triangle(*cell, *t, {0, 0, 4, 4}, db::Diagonal::nw_se, true, type_of(*t, "ndiff")).ok());
  const extract::NodeRegionList nodes = nodes_of(*cell, *t, style, false);
  REQUIRE(nodes.size() == 2);
  CHECK(nodes[0].right_side);
  CHECK(extract::generated_name(nodes[0], *t) == "active_0_0r#");
}

TEST_CASE("The clip area limits the class area and perimeter",
          "[cap:extract-nodes-and-labels][req:Node connectivity]") {
  const auto t = case_tech("EX-025");
  const extract::ExtStyle style = first_style(*t);
  auto cell = new_cell(*t);
  paint(*cell, *t, "pwell", {0, 0, 20, 20});
  extract::NodePassOptions options;
  options.clip = db::Rect{0, 0, 10, 20};
  const extract::NodeRegionList nodes = nodes_of(*cell, *t, style, true, options);
  REQUIRE(nodes.size() == 1);
  CHECK(nodes[0].classes[0].area == 200);
  CHECK(nodes[0].classes[0].perimeter == 10 + 20 + 10);  // the right side is outside the clip
}

TEST_CASE("Without lumped resistance the class values stay zero",
          "[cap:extract-nodes-and-labels][req:Node connectivity]") {
  const auto t = case_tech("EX-025");
  const extract::ExtStyle style = first_style(*t);
  auto cell = case_cell("EX-025", *t);
  extract::NodePassOptions options;
  options.resistance = false;
  const extract::NodeRegionList nodes = nodes_of(*cell, *t, style, true, options);
  REQUIRE(nodes.size() == 1);
  CHECK(nodes[0].classes == std::vector<extract::ClassArea>{extract::ClassArea{}});
}

TEST_CASE("Substrate seeds make the first region and a tile-less substrate is a node of its own",
          "[cap:extract-nodes-and-labels][req:Node connectivity]") {
  const auto t = case_tech("EX-025");
  const extract::ExtStyle style = first_style(*t);
  auto cell = case_cell("EX-025", *t);
  const tech::PlaneId well = t->find_plane("well").id.value();
  extract::NodePassOptions seeded;
  seeded.substrate_seeds.push_back({well, {1, 1}, false});
  extract::RegionFinder finder(*t);
  auto found = finder.find_nodes(*cell, style, seeded);
  REQUIRE(found.ok());
  REQUIRE(found.value().size() == 1);
  CHECK(finder.substrate_region() == std::optional<std::size_t>{0});

  extract::NodePassOptions tileless;
  tileless.substrate_without_tiles = well;
  found = finder.find_nodes(*cell, style, tileless);
  REQUIRE(found.ok());
  REQUIRE(found.value().size() == 2);
  CHECK(found.value().back().reference == db::Point{db::kMinusInfinity + 3, db::kMinusInfinity + 3});
  CHECK(found.value().back().type == tech::kSpace);
}

}  // namespace
}  // namespace hefesto::test::extnode
