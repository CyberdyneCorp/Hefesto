// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Merged polygon output (tasks.md 10.4; gds-write "Merged polygon output",
// Q-MASK-217 replicate): point order, split points, slits, isolated tiles,
// triangles, the 200-point limit and the element bytes. The expected point
// lists are hand traces of Magic's calmaMergePaintFunc over the plane's tile
// structure; the merge and colinear passes are the same in both
// magic-compat settings, so each case holds in both. Owner: GDSW.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <random>
#include <vector>

#include "hefesto/gds/write/emit.hpp"
#include "hefesto/gds/write/merge_plane.hpp"
#include "hefesto/gds/write/merge_rings.hpp"
#include "merge_labels_fixture.hpp"

namespace gds = hefesto::gds;
namespace merge = hefesto::gds::merge;
namespace mask = hefesto::mask;
namespace db = hefesto::db;
using gdsw_test::hex;
using gdsw_test::twice_area;

namespace {

using Points = std::vector<db::Point>;

std::vector<Points> merged(const mask::MaskPlane& plane, const std::optional<db::Rect>& clip = std::nullopt) {
  return gds::merge_plane_polygons(plane, clip);
}

}  // namespace

TEST_CASE("L-shape merges into one polygon in Magic's point order",
          "[cap:gds-write][req:Merged polygon output]") {
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{0, 20, 20, 40}, {0, 0, 40, 20}});
  const auto polygons = merged(plane);
  REQUIRE(polygons.size() == 1);
  CHECK(polygons[0] == Points{{20, 40}, {0, 40}, {0, 0}, {40, 0}, {40, 20}, {20, 20}});
}

TEST_CASE("L-shape merged element bytes at paint scale 10", "[cap:gds-write][req:Merged polygon output]") {
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{0, 20, 20, 40}, {0, 0, 40, 20}});
  gdsw_test::Output out;
  REQUIRE(gds::emit_merged_boundaries(out.writer(), plane, 49, 0, 10).ok());
  CHECK(out.bytes() == hex("0004 0800 | 0006 0D02 0031 | 0006 0E02 0000 | 003C 1003"
                           " 000000C8 00000190  00000000 00000190  00000000 00000000"
                           " 00000190 00000000  00000190 000000C8  000000C8 000000C8"
                           " 000000C8 00000190 | 0004 1100"));
}

TEST_CASE("An isolated rectangle under merge is written as the unmerged rectangle",
          "[cap:gds-write][req:Merged polygon output]") {
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{0, 0, 40, 20}});
  const auto polygons = merged(plane);
  REQUIRE(polygons.size() == 1);
  CHECK(polygons[0] == Points{{0, 0}, {40, 0}, {40, 20}, {0, 20}});

  gdsw_test::Output out;
  REQUIRE(gds::emit_merged_boundaries(out.writer(), plane, 49, 0, 10).ok());
  CHECK(out.bytes() == hex("0004 0800 0006 0D02 0031 0006 0E02 0000 002C 1003"
                           " 00000000 00000000 00000190 00000000 00000190 000000C8"
                           " 00000000 000000C8 00000000 00000000 0004 1100"));
}

TEST_CASE("A square annulus is one polygon with a zero-width slit", "[cap:gds-write][req:Merged polygon output]") {
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{0, 0, 30, 10}, {0, 20, 30, 30}, {0, 10, 10, 20}, {20, 10, 30, 20}});
  const auto polygons = merged(plane);
  REQUIRE(polygons.size() == 1);
  // The slit runs along y = 20 from x = 0 to the hole: (0,20)->(20,20) out
  // and (10,20)->(0,20) back.
  CHECK(polygons[0] == Points{{30, 30}, {0, 30}, {0, 20}, {20, 20}, {20, 10}, {10, 10}, {10, 20}, {0, 20}, {0, 0},
                              {30, 0}});
  CHECK(twice_area(polygons[0]) == 2 * (900 - 100));
}

TEST_CASE("A start-only segment match makes an isolated tile go through the merge (Magic's LB_INIT result)",
          "[cap:gds-write][req:Merged polygon output]") {
  // The triangle below the rectangle splits the space under it into three
  // tiles. calmaAddSegment returns LB_INIT (2) for the first two bottom
  // segments, so the rectangle's internal-edge count is not zero: it is
  // written as a bound from its top-right corner, not as a plain rectangle.
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{0, 10, 30, 20}});
  REQUIRE(plane.paint_triangle({10, 0, 20, 10}, db::Diagonal::ne_sw, true).ok());
  const auto polygons = merged(plane);
  REQUIRE(polygons.size() == 2);
  CHECK(polygons[0] == Points{{30, 20}, {0, 20}, {0, 10}, {30, 10}});
  CHECK(polygons[1] == Points{{10, 0}, {20, 0}, {20, 10}});
}

TEST_CASE("Isolated triangles of the four orientations keep the unmerged point order",
          "[cap:gds-write][req:Merged polygon output]") {
  struct Case {
    db::Diagonal diagonal;
    bool right_side;
    Points expected;
  };
  const std::vector<Case> cases{
      {db::Diagonal::ne_sw, false, {{0, 0}, {0, 10}, {10, 10}}},  // k=0
      {db::Diagonal::nw_se, false, {{0, 10}, {0, 0}, {10, 0}}},   // k=1
      {db::Diagonal::ne_sw, true, {{0, 0}, {10, 0}, {10, 10}}},   // k=2
      {db::Diagonal::nw_se, true, {{0, 10}, {10, 10}, {10, 0}}},  // k=3
  };
  for (const Case& c : cases) {
    mask::MaskPlane plane;
    REQUIRE(plane.paint_triangle({0, 0, 10, 10}, c.diagonal, c.right_side).ok());
    const auto polygons = merged(plane);
    REQUIRE(polygons.size() == 1);
    CHECK(polygons[0] == c.expected);
  }
}

TEST_CASE("A triangle merges with the rectangle on its solid side", "[cap:gds-write][req:Merged polygon output]") {
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{0, 0, 10, 10}});
  REQUIRE(plane.paint_triangle({10, 0, 20, 10}, db::Diagonal::nw_se, false).ok());
  const auto polygons = merged(plane);
  REQUIRE(polygons.size() == 1);
  CHECK(polygons[0] == Points{{10, 10}, {0, 10}, {0, 0}, {20, 0}});
}

TEST_CASE("A staircase beyond the point limit is split into abutting polygons of at most 200 XY points",
          "[cap:gds-write][req:Merged polygon output]") {
  // 100 overlapping steps: each tile adds 4 points to its bound, so a bound
  // stops taking tiles before 201 points and a new one starts.
  mask::MaskPlane plane;
  for (db::Coord i = 0; i < 100; ++i) {
    REQUIRE(plane.paint({2 * i, 2 * i, 2 * i + 10, 2 * i + 2}).ok());
  }
  const auto polygons = merged(plane);
  CHECK(polygons.size() >= 2);
  std::int64_t total = 0;
  for (const Points& p : polygons) {
    CHECK(p.size() + 1 <= 200);  // XY points, closing point included
    total += twice_area(p);
  }
  CHECK(total == 2 * static_cast<std::int64_t>(plane.solid_area()));
}

TEST_CASE("The clip area bounds the enumeration and clips isolated tiles",
          "[cap:gds-write][req:Merged polygon output]") {
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{0, 0, 40, 20}, {200, 200, 210, 210}});
  const auto polygons = merged(plane, db::Rect{10, -100, 100, 100});
  REQUIRE(polygons.size() == 1);
  CHECK(polygons[0] == Points{{10, 0}, {40, 0}, {40, 20}, {10, 20}});
}

TEST_CASE("Random Manhattan planes: merged polygons cover the solid area exactly",
          "[cap:gds-write][req:Merged polygon output]") {
  std::mt19937 rng(20261114);
  std::uniform_int_distribution<int> coord(0, 60);
  std::uniform_int_distribution<int> extent(1, 25);
  for (int round = 0; round < 200; ++round) {
    mask::MaskPlane plane;
    for (int i = 0; i < 25; ++i) {
      const int x = coord(rng);
      const int y = coord(rng);
      REQUIRE(plane.paint({x, y, x + extent(rng), y + extent(rng)}).ok());
    }
    const auto polygons = merged(plane);
    std::int64_t total = 0;
    for (const Points& p : polygons) {
      REQUIRE(p.size() >= 3);
      CHECK(p.size() + 1 <= 200);
      total += twice_area(p);
    }
    CHECK(total == 2 * static_cast<std::int64_t>(plane.solid_area()));
  }
}

TEST_CASE("An empty plane writes nothing", "[cap:gds-write][req:Merged polygon output]") {
  const mask::MaskPlane plane;
  CHECK(merged(plane).empty());
  gdsw_test::Output out;
  REQUIRE(gds::emit_merged_boundaries(out.writer(), plane, 1, 0, 1).ok());
  CHECK(out.bytes().empty());
}

TEST_CASE("Merged output coordinates outside I4 fail the write", "[cap:gds-write][req:Merged polygon output]") {
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{0, 0, 1000, 1000}});
  gdsw_test::Output out;
  const auto status = gds::emit_merged_boundaries(out.writer(), plane, 1, 0, 10'000'000);
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == "GDS-WRITE-004");
  const auto huge = gds::emit_merged_boundaries(out.writer(), plane, 1, 0, std::int64_t{1} << 62);
  REQUIRE_FALSE(huge.ok());
  CHECK(huge.error().code == "GDS-WRITE-004");
}

TEST_CASE("A merged element outside I4 writes nothing and names the cell and the value",
          "[cap:gds-write][req:Output scale factors]") {
  // Regression: the merged path wrote BOUNDARY, LAYER and DATATYPE before
  // the range check, and reported INT64_MAX on a multiply overflow.
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{0, 0, 1000, 1000}});
  gdsw_test::Output out;
  const auto status = gds::emit_merged_boundaries(out.writer(), plane, 1, 0, 10'000'000, std::nullopt, "top");
  REQUIRE_FALSE(status.ok());
  CHECK(out.bytes().empty());
  CHECK(status.error().text.find("value 10000000000 in cell top") != std::string::npos);
  const auto huge = gds::emit_merged_boundaries(out.writer(), plane, 1, 0, std::int64_t{1} << 62, std::nullopt, "top");
  REQUIRE_FALSE(huge.ok());
  CHECK(out.bytes().empty());
  CHECK(huge.error().text.find("value 4611686018427387904000 in cell top") != std::string::npos);
  const auto product = gds::checked_i4_product(-3, std::numeric_limits<std::int64_t>::max(), "c");
  REQUIRE_FALSE(product.ok());
  CHECK(product.error().text.find("value -27670116110564327421 in cell c") != std::string::npos);
  CHECK(gds::checked_i4_product(-7, 3, "c", 2).value() == -10);  // C truncation
}

TEST_CASE("LAYER and DATATYPE carry the low 16 bits as calmaOutI2", "[cap:gds-write][req:Merged polygon output]") {
  gdsw_test::Output out;
  const Points square{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  REQUIRE(gds::write_boundary(out.writer(), 63, 70000, square, 1).ok());
  const auto bytes = out.bytes();
  REQUIRE(bytes.size() > 16);
  CHECK(bytes[14] == std::byte{0x11});  // 70000 = 0x11170
  CHECK(bytes[15] == std::byte{0x70});
}

TEST_CASE("Ring passes: add_segment results and the colinear test", "[cap:gds-write][req:Merged polygon output]") {
  SECTION("start-only, end-only, complete and missing matches") {
    merge::RingArena arena;
    std::uint32_t cursor = arena.make_ring({{30, 10}, {0, 10}, {0, 0}, {30, 0}});
    // Start only: the new node keeps LB_INIT, which is returned.
    CHECK(merge::add_segment(arena, cursor, false, {30, 10}, {20, 10}) == 2);
    // End only: the new node takes the segment's kind.
    std::uint32_t left = 1;
    CHECK(merge::add_segment(arena, left, true, {0, 5}, {0, 0}) == 0);
    // Complete.
    CHECK(merge::add_segment(arena, cursor, false, {20, 10}, {0, 10}) == 1);
    // No connecting point.
    CHECK(merge::add_segment(arena, cursor, true, {7, 7}, {8, 8}) == -1);
  }
  SECTION("non-Manhattan colinear points are removed, opposite directions are not") {
    merge::RingArena arena;
    merge::BoundList bounds;
    const std::uint32_t first = arena.make_ring({{0, 0}, {2, 2}, {6, 6}, {6, 0}});
    bounds.push_back(merge::Bound{first, 4, 4});
    merge::remove_colinear(arena, bounds);
    CHECK(arena.points(bounds[0].first) == Points{{0, 0}, {6, 6}, {6, 0}});
    CHECK(bounds[0].points == 3);
    const std::uint32_t back = arena.make_ring({{0, 0}, {4, 4}, {2, 2}, {6, 0}});
    merge::BoundList opposite{merge::Bound{back, 4, 4}};
    merge::remove_colinear(arena, opposite);
    CHECK(arena.points(opposite[0].first) == Points{{0, 0}, {4, 4}, {2, 2}, {6, 0}});
    CHECK(merge::find_gcf(-4, 6) == 2);
    CHECK(merge::find_gcf(7, -7) == 7);
  }
  SECTION("degenerate and colinear passes stop on rings Magic would free") {
    merge::RingArena arena;
    merge::BoundList bounds;
    const std::uint32_t two = arena.make_ring({{0, 0}, {5, 0}});
    const std::uint32_t one = arena.make_ring({{1, 1}});
    bounds.push_back(merge::Bound{two, 2, 2});
    bounds.push_back(merge::Bound{one, 1, 1});
    merge::remove_degenerate(arena, bounds);
    merge::remove_colinear(arena, bounds);
    CHECK(bounds[1].size == 1);
    CHECK(bounds[0].size >= 1);
  }
}
