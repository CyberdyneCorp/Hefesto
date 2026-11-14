// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CIF wire outlines (capability cif-format-io "Wire command"; Q-MASK-333
// square ends, replicate). Paths are in doubled read units. Owner: CIF.

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "hefesto/cif/read/wire.hpp"

using hefesto::cif::ReadPoint;
using hefesto::cif::wire_outline;
using hefesto::mask::Polygon;

namespace {

Polygon poly(std::initializer_list<std::pair<int, int>> points) {
  Polygon out;
  for (const auto& [x, y] : points) {
    out.push_back(hefesto::db::Point{x, y});
  }
  return out;
}

}  // namespace

TEST_CASE("A wire has square half-width end extensions", "[cap:cif-format-io][req:Wire command]") {
  const auto out = wire_outline({{0, 0}, {200, 0}}, 10);
  REQUIRE(out.polygons.size() == 1);
  CHECK(out.polygons[0] == poly({{105, 5}, {-5, 5}, {-5, -5}, {105, -5}}));
  CHECK(out.warnings.empty());
  CHECK_FALSE(out.truncated);
}

TEST_CASE("A single-point wire is a square around the point", "[cap:cif-format-io][req:Wire command]") {
  const auto out = wire_outline({{10, 10}}, 10);
  REQUIRE(out.polygons.size() == 1);
  CHECK(out.polygons[0] == poly({{10, 10}, {0, 10}, {0, 0}, {10, 0}}));
}

TEST_CASE("A vertical wire", "[cap:cif-format-io][req:Wire command]") {
  const auto out = wire_outline({{0, 0}, {0, 100}}, 4);
  REQUIRE(out.polygons.size() == 1);
  CHECK(out.polygons[0] == poly({{-2, 52}, {-2, -2}, {2, -2}, {2, 52}}));
}

TEST_CASE("A reversal closes the outline and starts a new one", "[cap:cif-format-io][req:Wire command]") {
  const auto out = wire_outline({{0, 0}, {200, 0}, {0, 0}}, 10);
  REQUIRE(out.polygons.size() == 2);
  CHECK(out.polygons[0] == poly({{105, 5}, {-5, 5}, {-5, -5}, {105, -5}}));
  REQUIRE(out.warnings.size() == 1);
  CHECK(out.warnings[0] == "Warning: direction reversal in path at (100, 0).");
}

TEST_CASE("A repeated first point is kept and reported as a duplicate at a reversal",
          "[cap:cif-format-io][req:Wire command]") {
  // Magic never compares the first two points, so (0,0) (0,0) stays; the
  // segment between them has angle 0 and the next one turns back.
  const auto out = wire_outline({{0, 0}, {0, 0}, {-200, 0}}, 10);
  REQUIRE(out.warnings.size() == 1);
  CHECK(out.warnings[0] == "Warning: duplicate point in path at (0, 0).");
  CHECK(out.polygons.size() == 2);
}

TEST_CASE("An acute junction truncates the route", "[cap:cif-format-io][req:Wire command]") {
  const auto out = wire_outline({{0, 0}, {200, 0}, {0, 20}}, 10);
  CHECK(out.truncated);
  CHECK(out.mitre_message == "Error: mitre limit exceeded at wire junction at (200, 0).");
  CHECK(out.polygons.empty());  // the outline being built is dropped
}

TEST_CASE("A wire leaving the usable plane is flagged", "[cap:cif-format-io][req:Wire command]") {
  CHECK(wire_outline({{0, 0}, {200, 0}}, std::int64_t{1} << 40).out_of_range);
  CHECK(wire_outline({{0, 0}, {std::int64_t{1} << 40, 0}}, 10).out_of_range);
  CHECK_FALSE(wire_outline({{0, 0}, {200, 0}}, 10).out_of_range);
}

TEST_CASE("A 45-degree joint keeps its mitre", "[cap:cif-format-io][req:Wire command]") {
  // Width 10 along (0,0) -> (200,0) -> (400,200): the joint offset is
  // width / cos(22.5 deg), rounded half away from zero.
  const auto out = wire_outline({{0, 0}, {200, 0}, {400, 200}}, 10);
  REQUIRE(out.polygons.size() == 1);
  CHECK(out.polygons[0] == poly({{200, 107}, {98, 5}, {-5, 5}, {-5, -5}, {102, -5}, {207, 100}}));
}
