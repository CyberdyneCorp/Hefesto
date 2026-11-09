// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Nonzero-winding triangulation of glyph contours and split-tile triangles
// (capability font-labels rendering; hefesto-rendering "Style-file-faithful
// 2D drawing").

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "hefesto/render/polygon_fill.hpp"

namespace render = hefesto::render;
using hefesto::display::PointF;
using Contours = std::vector<std::vector<PointF>>;

namespace {

/// Total area of a triangle list.
double area(const std::vector<render::PolygonVertex>& v) {
  double total = 0.0;
  for (std::size_t i = 0; i + 2 < v.size(); i += 3) {
    const double ax = static_cast<double>(v[i + 1].x - v[i].x);
    const double ay = static_cast<double>(v[i + 1].y - v[i].y);
    const double bx = static_cast<double>(v[i + 2].x - v[i].x);
    const double by = static_cast<double>(v[i + 2].y - v[i].y);
    total += std::abs(ax * by - ay * bx) / 2.0;
  }
  return total;
}

std::vector<PointF> square(double x0, double y0, double size, bool clockwise = false) {
  std::vector<PointF> s{{x0, y0}, {x0 + size, y0}, {x0 + size, y0 + size}, {x0, y0 + size}};
  if (clockwise) std::reverse(s.begin(), s.end());
  return s;
}

}  // namespace

TEST_CASE("nonzero fill: a square, a hole and an overlap", "[cap:font-labels][req:Font label rendering]") {
  std::vector<render::PolygonVertex> out;
  REQUIRE(render::fill_nonzero(Contours{square(0, 0, 10)}, 0.0, 0.0, out));
  CHECK(area(out) == Catch::Approx(100.0));

  out.clear();  // an opposite-orientation inner contour is a hole
  REQUIRE(render::fill_nonzero(Contours{square(0, 0, 10), square(2, 2, 4, true)}, 0.0, 0.0, out));
  CHECK(area(out) == Catch::Approx(84.0));

  out.clear();  // same orientation: nonzero keeps the overlap filled once
  REQUIRE(render::fill_nonzero(Contours{square(0, 0, 10), square(5, 5, 10)}, 0.0, 0.0, out));
  CHECK(area(out) == Catch::Approx(175.0));
}

TEST_CASE("nonzero fill: crossing edges and the base point", "[cap:font-labels][req:Font label rendering]") {
  std::vector<render::PolygonVertex> out;
  // A bow tie: the edges cross at (5, 5); each half has area 25.
  const Contours bow{{{0, 0}, {10, 10}, {10, 0}, {0, 10}}};
  REQUIRE(render::fill_nonzero(bow, 0.0, 0.0, out));
  CHECK(area(out) == Catch::Approx(50.0));

  out.clear();
  REQUIRE(render::fill_nonzero(Contours{square(1000000, 2000000, 2)}, 1000000.0, 2000000.0, out));
  for (const auto& v : out) {
    CHECK(v.x >= 0.0F);
    CHECK(v.x <= 2.0F);
    CHECK(v.y >= 0.0F);
    CHECK(v.y <= 2.0F);
  }
}

TEST_CASE("nonzero fill ignores degenerate and non-finite input", "[cap:font-labels][req:Font label rendering]") {
  std::vector<render::PolygonVertex> out;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  REQUIRE(render::fill_nonzero(Contours{{{0, 0}, {1, 1}}, {{0, 0}, {nan, 1}, {2, 2}}, {}}, 0.0, 0.0, out));
  CHECK(out.empty());
  // Too many edges: refused, nothing appended.
  std::vector<PointF> huge;
  for (std::size_t i = 0; i <= render::kMaxFillEdges; ++i) {
    huge.push_back({static_cast<double>(i % 2), static_cast<double>(i)});
  }
  CHECK_FALSE(render::fill_nonzero(Contours{huge}, 0.0, 0.0, out));
  CHECK(out.empty());
}
