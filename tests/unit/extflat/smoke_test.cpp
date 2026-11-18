// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// read_flat over the smoke `.ext` files Magic 8.3.694 wrote in the
// capture image (tests/data/extflat/smoke/, m6-lef-def-oasis task 1.6):
// `top` uses `leaf` twice (leaf_1 at x = 20, leaf_0 at 0), joins leaf_0/A
// to its port IN, leaf_0/Y to leaf_1/A and leaf_1/A to the wire `mid`, and
// has the rail `vdd!`. The expected list follows EFReadFile, EFFlatBuild
// (EF_FLATNODES | EF_NOFLATSUBCKT) and EFVisitNodes by hand. Owner: EXTSUB.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "ext_fixture.hpp"
#include "hefesto/platform/file_system.hpp"

namespace extflat = hefesto::extflat;
namespace cell = hefesto::cell;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;
using extflat::test::all_names;
using extflat::test::names;
using extflat::test::node_named;

namespace {

struct Smoke {
  platform::LocalFileSystem fs;
  cell::SearchPaths paths{{std::string{HEFESTO_TEST_DATA_DIR} + "/smoke"}, {}, {}};
  cell::PathVariables vars;
  diag::CollectingSink sink;
  cell::LookupContext ctx{fs, paths, vars, sink};
};

}  // namespace

TEST_CASE("the smoke .ext files of Magic flatten to four nodes in EFVisitNodes order",
          "[cap:ext-flattening-hierarchy][req:Visit orders for output]") {
  Smoke s;
  extflat::ReadOptions options;
  options.timestamps = true;
  const auto netlist = extflat::read_flat(s.ctx, "top", options);
  REQUIRE(netlist.ok());
  CHECK(s.sink.snapshot().empty());
  // leaf_0 and leaf_1 share bucket 26 of the use table, so leaf_0 (inserted
  // last) is flattened first; the list is the reverse of creation order.
  CHECK(names(netlist.value()) == std::vector<std::string>{"vdd!", "mid", "leaf_1/Y", "IN"});
}

TEST_CASE("smoke: a subcircuit contributes its ports and merges join them",
          "[cap:ext-flattening-hierarchy][req:Flat node construction]") {
  Smoke s;
  const auto netlist = extflat::read_flat(s.ctx, "top", {});
  REQUIRE(netlist.ok());
  const auto& n = netlist.value();

  const auto* mid = node_named(n, "mid");
  REQUIRE(mid != nullptr);
  CHECK(all_names(*mid) == std::vector<std::string>{"mid", "leaf_0/Y", "leaf_1/A"});
  CHECK(mid->location == hefesto::db::Point{10, 8});  // taken from `mid`, which won the name
  CHECK(mid->layer == "m1");
  CHECK(mid->port);
  CHECK_FALSE(mid->top_port);
  CHECK(mid->locations == std::vector<extflat::NodeLocation>{{{10, 8}, "m1"}});

  const auto* y = node_named(n, "leaf_1/Y");
  REQUIRE(y != nullptr);
  CHECK(y->location == hefesto::db::Point{20, 8});  // leaf's (0, 8) through leaf_1's transform
  CHECK(y->locations.empty());
}

TEST_CASE("smoke: the top-level port keeps its name and Magic's truncated location",
          "[cap:ext-flattening-hierarchy][req:Node merge semantics]") {
  Smoke s;
  const auto netlist = extflat::read_flat(s.ctx, "top", {});
  REQUIRE(netlist.ok());
  const auto* in = node_named(netlist.value(), "IN");
  REQUIRE(in != nullptr);
  CHECK(all_names(*in) == std::vector<std::string>{"IN", "leaf_0/A"});
  CHECK(in->top_port);
  CHECK(in->port);
  // (int)(0.5 + -6.0) truncates toward zero: -5.
  CHECK(in->location == hefesto::db::Point{-5, 0});
  CHECK(in->locations == std::vector<extflat::NodeLocation>{{{-5, 0}, "m1"}});

  const auto* vdd = node_named(netlist.value(), "vdd!");
  REQUIRE(vdd != nullptr);
  CHECK(vdd->location == hefesto::db::Point{0, 20});
  CHECK_FALSE(vdd->port);
}

TEST_CASE("smoke: each .ext file is read once and its timestamp recorded in read order",
          "[cap:ext-flattening-read][req:Locating and recursively reading .ext files]") {
  Smoke s;
  extflat::ReadOptions options;
  options.timestamps = true;
  const auto netlist = extflat::read_flat(s.ctx, "top", options);
  REQUIRE(netlist.ok());
  const auto& stamps = netlist.value().stamps();
  REQUIRE(stamps.size() == 2);
  CHECK(stamps[0].cell == "top");
  CHECK(stamps[0].timestamp == 100);
  CHECK(stamps[1].cell == "leaf");
}
