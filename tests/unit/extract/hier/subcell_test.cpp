// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The subcell parts of extract-devices and extract-substrate
// (openspec/changes/m7-extraction tasks.md 7.7) and the node table's
// adjustment values when two nodes with values are unified (both modes).
// They need hierarchical cells, so they live with the extract/hier tests.
// Owner: EXTHIER (EXTDEV's subcell parts).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

#include "hefesto/extract/device/devices.hpp"
#include "hefesto/extract/hier/node_table.hpp"
#include "hier_fixture.hpp"

namespace hefesto::test::exthier {
namespace {

TEST_CASE("EX-043: a capacitor duplicated in a subcell is written by the subcell only",
          "[cap:extract-devices][req:Subcell device priority]") {
  HierCase c("EX-043", true);
  db::CellDef& top = c.cell("ex");
  auto regions = extract::find_device_regions(top, c.tech(), c.style());
  REQUIRE(regions.size() == 1);
  extract::disable_subcell_devices(top, regions, c.tech(), c.style());
  CHECK(regions[0].disabled);

  db::CellDef& child = c.cell("cap");
  auto own = extract::find_device_regions(child, c.tech(), c.style());
  REQUIRE(own.size() == 1);
  extract::disable_subcell_devices(child, own, c.tech(), c.style());
  CHECK_FALSE(own[0].disabled);
}

TEST_CASE("A device away from every subcell keeps its record", "[cap:extract-devices][req:Subcell device priority]") {
  HierCase c("EX-043", true);
  db::CellDef& top = c.cell("ex");
  const auto mim = c.tech().find_type("mimcap");
  REQUIRE(mim.ok());
  REQUIRE(db::paint(top, c.tech(), db::Rect{40, 0, 50, 10}, *mim.id).ok());
  auto regions = extract::find_device_regions(top, c.tech(), c.style());
  REQUIRE(regions.size() == 2);
  extract::disable_subcell_devices(top, regions, c.tech(), c.style());
  const auto disabled = std::count_if(regions.begin(), regions.end(), [](const auto& r) { return r.disabled; });
  CHECK(disabled == 1);
}

TEST_CASE("EX-047: each array element's substrate merges with the parent's",
          "[cap:extract-substrate][req:Subcell substrate connections]") {
  HierCase c("EX-047", true);
  const auto run = c.run("ex");
  for (const std::string name : {"x[0]/VSUB", "x[1]/VSUB", "x[2]/VSUB", "y/VSUB"}) {
    CHECK(run.text.find("\"" + name + "\"") != std::string::npos);
  }
  CHECK(run.text.find("\"VSUB\"") != std::string::npos);
}

TEST_CASE("EX-047: a labelled square becomes an interaction area without uses, so x[0] and y are not merged",
          "[cap:extract-substrate][req:Subcell substrate connections]") {
  HierCase c("EX-047", true);
  const auto run = c.run("exw");
  // Captured: only the array pass's x[2]/x[1] group, which Hefesto does not
  // reproduce yet (tasks.md 8.5 follow-up).
  const std::string captured = captured_ext("EX-047", "exw");
  CHECK(captured.find("\"x[0]/VSUB\"") == std::string::npos);
  CHECK(captured.find("\"y/VSUB\"") == std::string::npos);
  CHECK(run.text.find("\"x[0]/VSUB\"") == std::string::npos);
  CHECK(run.text.find("\"y/VSUB\"") == std::string::npos);
}

TEST_CASE("Unifying two nodes with adjustments: magic-compat ON keeps the survivor's only",
          "[cap:extract-hierarchy][req:Merge records and pass-2 adjustments]") {
  extract::HierNodeTable t(1);
  const std::size_t a = t.node_of("a");
  const std::size_t b = t.node_of("b");
  t.node(a).cap = -10.0;
  t.node(b).cap = -5.0;
  t.node(b).classes[0] = {-4, -2};
  const std::size_t kept = t.unify(a, b, false);
  CHECK(kept == a);
  CHECK(t.node(kept).cap == -10.0);
  CHECK(t.node(kept).classes[0] == extract::ClassArea{0, 0});
}

TEST_CASE("Unifying two nodes with adjustments: magic-compat OFF sums them",
          "[cap:extract-hierarchy][req:Merge records and pass-2 adjustments]") {
  extract::HierNodeTable t(1);
  const std::size_t a = t.node_of("a");
  const std::size_t b = t.node_of("b");
  t.node(a).cap = -10.0;
  t.node(b).cap = -5.0;
  t.node(b).classes[0] = {-4, -2};
  const std::size_t kept = t.unify(a, b, true);
  CHECK(t.node(kept).cap == -15.0);
  CHECK(t.node(kept).classes[0] == extract::ClassArea{-4, -2});
}

}  // namespace
}  // namespace hefesto::test::exthier
