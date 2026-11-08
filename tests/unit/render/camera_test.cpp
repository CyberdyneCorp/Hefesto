// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "hefesto/render/camera.hpp"

using Catch::Approx;
using hefesto::render::Bounds;
using hefesto::render::Camera;

namespace {

/// NDC of a world point, computed the way the vertex shader does.
double ndc_x(const Camera& c, std::int32_t x) {
  const auto t = c.transform();
  return static_cast<double>(static_cast<float>(x - t.origin_x) * t.scale_x + t.offset_x);
}
double ndc_y(const Camera& c, std::int32_t y) {
  const auto t = c.transform();
  return static_cast<double>(static_cast<float>(y - t.origin_y) * t.scale_y + t.offset_y);
}

}  // namespace

TEST_CASE("zoom keeps the point under the cursor fixed", "[cap:hefesto-performance][req:Interactive frame rate]") {
  Camera cam(1000.0, 2000.0, 0.5);
  cam.set_viewport(800, 600);
  const double wx = cam.world_x(100.0);
  const double wy = cam.world_y(450.0);
  cam.zoom_at(3.0, 100.0, 450.0);
  CHECK(cam.pixels_per_unit() == Approx(1.5));
  CHECK(cam.world_x(100.0) == Approx(wx));
  CHECK(cam.world_y(450.0) == Approx(wy));
}

TEST_CASE("pan follows the drag; screen y points down", "[cap:hefesto-performance][req:Interactive frame rate]") {
  Camera cam(0.0, 0.0, 2.0);
  cam.set_viewport(100, 100);
  cam.pan_pixels(10.0, 20.0);
  CHECK(cam.center_x() == Approx(-5.0));
  CHECK(cam.center_y() == Approx(10.0));
}

TEST_CASE("fit centers the bounds and shows all of them", "[cap:hefesto-performance][req:Interactive frame rate]") {
  Camera cam;
  cam.set_viewport(1000, 500);
  cam.fit(Bounds{0, 0, 4000, 1000, true}, 0.0);
  CHECK(cam.center_x() == Approx(2000.0));
  CHECK(cam.center_y() == Approx(500.0));
  CHECK(cam.pixels_per_unit() == Approx(0.25));  // width-limited: 1000 px / 4000 units
  CHECK(ndc_x(cam, 0) == Approx(-1.0).margin(1e-6));
  CHECK(ndc_x(cam, 4000) == Approx(1.0).margin(1e-6));
  CHECK(ndc_y(cam, 1000) == Approx(-0.5).margin(1e-6));  // world up = NDC down
}

TEST_CASE("transform keeps sub-unit precision far from the origin",
          "[cap:hefesto-performance][req:Interactive frame rate]") {
  // 1.9e9 units from the origin, zoomed so that one unit is 100 pixels.
  Camera cam(1'900'000'000.5, -1'900'000'000.25, 100.0);
  cam.set_viewport(1000, 1000);
  // The world point at the viewport center maps to NDC 0 within a pixel.
  CHECK(ndc_x(cam, 1'900'000'000) == Approx(-0.1).margin(2e-3));
  CHECK(ndc_x(cam, 1'900'000'001) == Approx(0.1).margin(2e-3));
  CHECK(ndc_y(cam, -1'900'000'000) == Approx(-0.05).margin(2e-3));
}

TEST_CASE("zoom is clamped", "[cap:hefesto-performance][req:Interactive frame rate]") {
  Camera cam;
  cam.zoom_at(1e30, 0.0, 0.0);
  CHECK(cam.pixels_per_unit() == Camera::kMaxPixelsPerUnit);
  cam.zoom_at(0.0, 0.0, 0.0);  // ignored
  CHECK(cam.pixels_per_unit() == Camera::kMaxPixelsPerUnit);
}
