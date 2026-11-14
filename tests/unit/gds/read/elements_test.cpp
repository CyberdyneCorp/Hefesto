// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The pure element arithmetic of the GDS reader (hefesto/gds/read/elements.hpp):
// angle rounding and STRANS transforms, magnification fractions and copy
// names, path end extensions and the path outline, and the TEXT
// presentation anchors. The outline values are pinned on every leg (design
// D22: Magic's float operation order, 1.5708 for 90 degrees with
// magic-compat ON). Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <vector>

#include "hefesto/gds/read/elements.hpp"

namespace gds = hefesto::gds;
namespace db = hefesto::db;
using gds::PathPoint;

TEST_CASE("ANGLE values are rounded to quarter turns", "[cap:gds-read][req:Structure reference transforms]") {
  CHECK(gds::manhattan_angle(90.0).degrees == 90);
  CHECK_FALSE(gds::manhattan_angle(90.0).rounded);
  CHECK(gds::manhattan_angle(360.0).degrees == 0);
  CHECK(gds::manhattan_angle(-90.0).degrees == 270);
  const auto a = gds::manhattan_angle(44.9);
  CHECK(a.truncated == 44);
  CHECK(a.degrees == 0);
  CHECK(a.rounded);
  CHECK(gds::manhattan_angle(45.0).degrees == 90);
  CHECK(gds::manhattan_angle(134.0).degrees == 90);
  CHECK(gds::manhattan_angle(135.0).degrees == 180);
  CHECK(gds::manhattan_angle(315.0).degrees == 0);
  CHECK(gds::manhattan_angle(764.0).truncated == 44);
  CHECK(gds::manhattan_angle(std::numeric_limits<double>::quiet_NaN()).degrees == 0);
  CHECK(gds::manhattan_angle(1e300).degrees % 90 == 0);
}

TEST_CASE("STRANS reflects, then rotates counterclockwise", "[cap:gds-read][req:Structure reference transforms]") {
  const db::Transform t = gds::strans_transform(0x8000, 90);
  CHECK(db::apply(t, db::Point{1, 2}) == db::Point{2, 1});
  CHECK(db::apply(gds::strans_transform(0, 90), db::Point{1, 0}) == db::Point{0, 1});
  CHECK(db::apply(gds::strans_transform(0, 270), db::Point{1, 0}) == db::Point{0, -1});
  CHECK(db::apply(gds::strans_transform(0x8000, 0), db::Point{1, 2}) == db::Point{1, -2});
  CHECK(gds::strans_transform(0x4000, 0) == db::kIdentity);  // absolute flags are ignored
}

TEST_CASE("magnifications as fractions", "[cap:gds-read][req:Magnified instances]") {
  CHECK(gds::mag_fraction(1.5) == gds::MagFraction{3, 2, true});
  CHECK(gds::mag_fraction(2.0) == gds::MagFraction{2, 1, true});
  CHECK(gds::mag_fraction(0.25) == gds::MagFraction{1, 4, true});
  const auto third = gds::mag_fraction(0.333333);
  CHECK(third.num == 1);
  CHECK(third.den == 3);
  CHECK_FALSE(third.exact);
  const auto tiny = gds::mag_fraction(1e-9);
  CHECK(tiny.num == 1);
  CHECK(tiny.den == gds::kMaxMagDenominator);
  CHECK(gds::mag_fraction(1e12).den == 1);
  CHECK(gds::magnified_name("cell", 3, 2) == "cell_mag1p5");
  CHECK(gds::magnified_name("cell", 2, 1) == "cell_mag2");
  CHECK(gds::magnified_name("c", 1, 3) == "c_mag0p333333");
}

TEST_CASE("path end extensions", "[cap:gds-read][req:PATH elements]") {
  SECTION("Manhattan paths agree") {
    for (const bool magic_compat : {true, false}) {
      std::vector<PathPoint> p{{0, 0}, {200, 0}};
      gds::extend_path(p, 6, 14, magic_compat);
      CHECK(p == std::vector<PathPoint>{{-6, 0}, {214, 0}});
    }
  }
  SECTION("diagonal end") {
    std::vector<PathPoint> on{{0, 0}, {60, 80}};
    gds::extend_path(on, 0, 10, true);
    CHECK(on.back() == PathPoint{70, 90});
    std::vector<PathPoint> off{{0, 0}, {60, 80}};
    gds::extend_path(off, 0, 10, false);
    CHECK(off.back() == PathPoint{66, 88});
  }
  SECTION("negative extensions shorten only with magic-compat OFF") {
    std::vector<PathPoint> on{{0, 0}, {200, 0}};
    gds::extend_path(on, -10, 0, true);
    CHECK(on.front() == PathPoint{0, 0});
    std::vector<PathPoint> off{{0, 0}, {200, 0}};
    gds::extend_path(off, -10, 0, false);
    CHECK(off.front() == PathPoint{10, 0});
  }
  SECTION("a single point is not extended") {
    std::vector<PathPoint> p{{5, 5}};
    gds::extend_path(p, 10, 10, true);
    CHECK(p == std::vector<PathPoint>{{5, 5}});
  }
}

TEST_CASE("path outlines", "[cap:gds-read][req:Path-to-polygon conversion]") {
  SECTION("45-degree segment, pinned values") {
    const auto out = gds::path_outline({{0, 0}, {200, 200}}, 10, false, true);
    REQUIRE(out.polygons.size() == 1);
    CHECK(out.polygons.front() == hefesto::mask::Polygon{{96, 104}, {-4, 4}, {4, -4}, {104, 96}});
  }
  SECTION("flush ends with H = 1.5708 and pi/2") {
    const auto on = gds::path_outline({{0, 0}, {200, 0}}, 5, false, true);
    const auto off = gds::path_outline({{0, 0}, {200, 0}}, 5, false, false);
    REQUIRE(on.polygons.size() == 1);
    REQUIRE(off.polygons.size() == 1);
    CHECK(on.polygons.front() == hefesto::mask::Polygon{{100, 2}, {0, 2}, {0, -2}, {100, -2}});
    CHECK(off.polygons.front() == hefesto::mask::Polygon{{100, 3}, {0, 3}, {0, -3}, {100, -3}});
  }
  SECTION("direction reversal splits the outline") {
    const auto out = gds::path_outline({{0, 0}, {200, 0}, {0, 0}}, 10, true, true);
    CHECK(out.polygons.size() == 2);
    REQUIRE(out.warnings.size() == 1);
    CHECK(out.warnings.front() == "Warning: direction reversal in path at (100, 0).");
  }
  SECTION("mitre limit truncates") {
    const auto out = gds::path_outline({{0, 0}, {200, 0}, {0, 10}}, 10, false, true);
    CHECK(out.truncated);
    CHECK(out.mitre_message == "Error: mitre limit exceeded at wire junction at (200, 0).");
    CHECK(out.polygons.empty());
  }
  SECTION("duplicate head point (Q-MASK-116)") {
    const auto on = gds::path_outline({{0, 0}, {0, 0}, {-200, 0}}, 10, false, true);
    const auto off = gds::path_outline({{0, 0}, {0, 0}, {-200, 0}}, 10, false, false);
    REQUIRE(on.warnings.size() == 1);  // the head duplicate stays: its angle 0 reverses against pi
    CHECK(on.warnings.front() == "Warning: duplicate point in path at (0, 0).");
    CHECK(off.warnings.empty());
    CHECK(off.polygons.size() == 1);
  }
  SECTION("out-of-plane outlines are dropped") {
    const auto out = gds::path_outline({{0, 0}, {std::int64_t{1} << 40, 0}}, 10, false, true);
    CHECK(out.out_of_range);
    CHECK(out.polygons.empty());
  }
}

TEST_CASE("TEXT presentation anchors", "[cap:gds-read][req:TEXT element parsing]") {
  CHECK(gds::presentation_position(0x0) == db::kSouthEast);
  CHECK(gds::presentation_position(0x1) == db::kSouth);
  CHECK(gds::presentation_position(0x2) == db::kSouthWest);
  CHECK(gds::presentation_position(0x4) == db::kEast);
  CHECK(gds::presentation_position(0x5) == db::kCenter);
  CHECK(gds::presentation_position(0x6) == db::kWest);
  CHECK(gds::presentation_position(0x8) == db::kNorthEast);
  CHECK(gds::presentation_position(0x9) == db::kNorth);
  CHECK(gds::presentation_position(0xA) == db::kNorthWest);
  CHECK(gds::presentation_position(0x3) == db::kSouthEast);
  CHECK(gds::presentation_position(0x15) == db::kCenter);  // the font bits are not the anchor
}
