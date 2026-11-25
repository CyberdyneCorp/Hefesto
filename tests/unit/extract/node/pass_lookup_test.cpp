// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The node lookups EXTCAP and EXTDEV use during a production pass read
// RegionFinder's encoded client slots (regions.hpp), not a plain index
// (openspec/changes/m7-extraction design D6, D8; M7 R3 integration).
// Owner: INTEGRATION.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/extract/cap/caps.hpp"
#include "hefesto/extract/device/devices.hpp"
#include "node_fixture.hpp"

namespace hefesto::test::extnode {
namespace {

TEST_CASE("The capacitance and device lookups name a tile's node from RegionFinder's kept slots",
          "[cap:extract-nodes-and-labels][req:Node connectivity]") {
  const auto t = case_tech("EX-019");
  const extract::ExtStyle style = first_style(*t, false);
  auto cell = new_cell(*t);
  paint(*cell, *t, "metal1", {0, 0, 10, 10});
  paint(*cell, *t, "metal1", {20, 0, 30, 10});
  REQUIRE(db::paint_triangle(*cell, *t, {40, 0, 44, 4}, db::Diagonal::nw_se, false, type_of(*t, "poly")).ok());
  REQUIRE(db::paint_triangle(*cell, *t, {40, 0, 44, 4}, db::Diagonal::nw_se, true, type_of(*t, "ndiff")).ok());

  extract::RegionFinder finder(*t);
  finder.keep_clients = true;
  extract::NodePassOptions options;
  options.magic_compat = false;
  auto found = finder.find_nodes(*cell, style, options);
  REQUIRE(found.ok());
  const extract::NodeRegionList& nodes = found.value();
  REQUIRE(nodes.size() == 4);

  const extract::NodeLookup cap_lookup = extract::region_node_lookup(finder, *cell, nodes.size());
  const extract::DeviceNodeLookup dev_lookup = extract::region_slot_lookup(finder, *cell, nodes.size());
  const tech::PlaneId metal1 = t->find_plane("metal1").id.value();
  const tech::PlaneId active = t->find_plane("active").id.value();

  const db::TileView left_shape = cell->plane(metal1).tile_at({5, 5});
  const db::TileView right_shape = cell->plane(metal1).tile_at({25, 5});
  const auto left = cap_lookup(metal1, left_shape, false);
  const auto right = cap_lookup(metal1, right_shape, false);
  REQUIRE(left.has_value());
  REQUIRE(right.has_value());
  CHECK(*left != *right);
  CHECK(nodes[*left].reference == db::Point{0, 0});
  CHECK(nodes[*right].reference == db::Point{20, 0});
  CHECK(dev_lookup(metal1, left_shape) == left);
  CHECK(dev_lookup(metal1, right_shape) == right);

  // Both sides of the split tile: each side asked about has its own node.
  const db::TileView split = cell->plane(active).tile_at({41, 1});
  REQUIRE(split.split);
  const auto poly_side = cap_lookup(active, split, false);
  const auto ndiff_side = cap_lookup(active, split, true);
  REQUIRE(poly_side.has_value());
  REQUIRE(ndiff_side.has_value());
  CHECK(*poly_side != *ndiff_side);
  CHECK_FALSE(nodes[*poly_side].right_side);
  CHECK(nodes[*ndiff_side].right_side);
  db::TileView right_view = split;
  right_view.right_side = true;
  CHECK(dev_lookup(active, right_view) == ndiff_side);

  extract::reset(*cell);
  CHECK(extract::clients_clear(*cell));
}

}  // namespace
}  // namespace hefesto::test::extnode
