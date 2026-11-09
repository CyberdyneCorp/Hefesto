// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Draws through the real Vulkan path (MoltenVK on macOS, Mesa lavapipe in the
// Linux CI job `gpu-readback-lavapipe`) into an offscreen image and reads
// pixels back. SKIPs when no Vulkan device is available, unless
// HEFESTO_REQUIRE_VULKAN_DEVICE is set (see make_canvas).

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/render/headless.hpp"

using namespace hefesto::render;

namespace {

// Returns a 64x64 canvas, or SKIPs the test when no Vulkan device exists.
// HEFESTO_REQUIRE_VULKAN_DEVICE=<name part> (set by a CI job that must run
// these tests) turns that skip into a failure and also fails when the picked
// device's name does not contain <name part> (e.g. "llvmpipe" for lavapipe).
std::unique_ptr<HeadlessCanvas> make_canvas() {
  const char* env = std::getenv("HEFESTO_REQUIRE_VULKAN_DEVICE");
  const std::string required = env != nullptr ? env : "";
  std::unique_ptr<HeadlessCanvas> canvas;
  try {
    canvas = std::make_unique<HeadlessCanvas>(HeadlessOptions{64, 64, false});
  } catch (const VulkanError& e) {
    if (!required.empty()) {
      FAIL("no Vulkan device, but HEFESTO_REQUIRE_VULKAN_DEVICE=" << required << ": " << e.what());
    }
    SKIP("no Vulkan device: " << e.what());
  }
  if (!required.empty()) {
    INFO("Vulkan device: " << canvas->device_name() << " (" << canvas->driver_info() << ")");
    REQUIRE(canvas->device_name().find(required) != std::string::npos);
  }
  return canvas;
}

std::uint8_t channel(std::uint32_t rgba, unsigned index) {
  return static_cast<std::uint8_t>((rgba >> (8U * index)) & 0xFFU);
}

}  // namespace

TEST_CASE("renderer draws instanced rectangles with layer colors",
          "[cap:hefesto-performance][req:Interactive frame rate]") {
  auto canvas = make_canvas();
  // Layer 0 covers the left half of a 64x64 world, layer 1 the top-right quarter.
  const std::vector<RectInstance> instances{{0, 0, 32, 64}, {32, 32, 64, 64}};
  const std::vector<LayerBatch> draws{{0, 0, 1}, {1, 1, 1}};
  const std::vector<LayerStyle> styles{{1.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F, 1.0F}};
  canvas->renderer().upload(instances);
  canvas->renderer().set_layer_styles(styles);
  Camera camera(32.0, 32.0, 1.0);
  camera.set_viewport(64, 64);
  const FrameTiming t = canvas->render(camera.transform(), draws);
  CHECK(t.cpu_ms > 0.0);

  const std::uint32_t left = canvas->read_pixel(10, 40);       // world (10, 24): layer 0
  const std::uint32_t top_right = canvas->read_pixel(50, 10);  // world (50, 54): layer 1
  const std::uint32_t bottom_right = canvas->read_pixel(50, 50);  // world (50, 14): clear color
  CHECK(channel(left, 0) == 255);
  CHECK(channel(left, 2) == 0);
  CHECK(channel(top_right, 2) == 255);
  CHECK(channel(top_right, 0) == 0);
  CHECK(channel(bottom_right, 0) < 20);
  CHECK(channel(bottom_right, 2) < 20);
}

TEST_CASE("sub-pixel rectangles stay visible", "[cap:hefesto-performance][req:Interactive frame rate]") {
  auto canvas = make_canvas();
  // A 1x1-unit rectangle seen at 1/100 pixel per unit is still drawn one pixel wide.
  const std::vector<RectInstance> instances{{3200, 3200, 3201, 3201}};
  const std::vector<LayerBatch> draws{{0, 0, 1}};
  const std::vector<LayerStyle> styles{{0.0F, 1.0F, 0.0F, 1.0F}};
  canvas->renderer().upload(instances);
  canvas->renderer().set_layer_styles(styles);
  Camera camera(3200.0, 3200.0, 0.01);
  camera.set_viewport(64, 64);
  (void)canvas->render(camera.transform(), draws);
  bool found = false;
  for (int y = 30; y <= 33 && !found; ++y) {
    for (int x = 30; x <= 33 && !found; ++x) {
      found = channel(canvas->read_pixel(x, y), 1) > 200;
    }
  }
  CHECK(found);
}

TEST_CASE("stipples use the OpenGL orientation on the GPU", "[cap:display-style-files][req:Stipple patterns]") {
  auto canvas = make_canvas();
  // A full-canvas rectangle in a checkerboard stipple whose rows differ:
  // row 0 (bottom) 0x80 -> only the leftmost pixel of each 8-pixel run.
  const std::vector<RectInstance> instances{{0, 0, 64, 64}};
  const std::vector<LayerBatch> draws{{0, 0, 1}};
  LayerStyle style{1.0F, 1.0F, 1.0F, 1.0F, true, {0x80, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
  const std::vector<LayerStyle> styles{style};
  canvas->renderer().upload(instances);
  canvas->renderer().set_layer_styles(styles);
  Camera camera(32.0, 32.0, 1.0);
  camera.set_viewport(64, 64);
  (void)canvas->render(camera.transform(), draws);
  const int bottom = canvas->height() - 1;  // read_pixel rows count from the top
  CHECK(channel(canvas->read_pixel(0, bottom), 0) == 255);   // row 0, MSB: painted
  CHECK(channel(canvas->read_pixel(1, bottom), 0) < 20);     // row 0, next bit: not painted
  CHECK(channel(canvas->read_pixel(8, bottom), 0) == 255);   // repeats every 8 pixels
  CHECK(channel(canvas->read_pixel(3, bottom - 1), 0) == 255);  // row 1 = 0xFF
  CHECK(channel(canvas->read_pixel(0, bottom - 2), 0) < 20);    // row 2 = 0x00
}

TEST_CASE("cross styles draw both diagonals on the GPU, not under 4 pixels",
          "[cap:display-style-files][req:Outline and fill rendering semantics]") {
  auto canvas = make_canvas();
  // A 40x40 rectangle (diagonals) and a 3x3 one (under GR_THRESH: nothing).
  const std::vector<RectInstance> instances{{12, 12, 52, 52}, {2, 2, 5, 5}};
  const std::vector<LayerBatch> draws{{0, 0, 2}};
  LayerStyle style{0.0F, 1.0F, 0.0F, 1.0F};
  style.diagonals = true;
  const std::vector<LayerStyle> styles{style};
  canvas->renderer().upload(instances);
  canvas->renderer().set_layer_styles(styles);
  Camera camera(32.0, 32.0, 1.0);
  camera.set_viewport(64, 64);
  (void)canvas->render(camera.transform(), draws);
  const auto green_near = [&](int x, int y) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (channel(canvas->read_pixel(x + dx, y + dy), 1) > 200) return true;
      }
    }
    return false;
  };
  // World (x, y) is pixel (x, 64 - y): both diagonals cross the center.
  CHECK(green_near(22, 64 - 22));  // on (12,12)-(52,52)
  CHECK(green_near(22, 64 - 42));  // on (12,52)-(52,12)
  CHECK(channel(canvas->read_pixel(32, 64 - 20), 1) < 20);  // inside, off the diagonals: no fill
  CHECK_FALSE(green_near(3, 64 - 3));  // the small rectangle is skipped
}

TEST_CASE("outline line patterns repeat every 8 pixels from the line's left end",
          "[cap:display-style-files][req:Outline and fill rendering semantics]") {
  auto canvas = make_canvas();
  // A horizontal zero-height line from x = 0 to 64 at y = 32, pattern 0x0F:
  // pixels 0-3 of each 8 painted, 4-7 not.
  const std::vector<RectInstance> instances{{0, 32, 64, 32}};
  const std::vector<LayerBatch> draws{{0, 0, 1}};
  LayerStyle style{1.0F, 1.0F, 1.0F, 1.0F};
  style.dash = 0x0F;
  const std::vector<LayerStyle> styles{style};
  canvas->renderer().upload(instances);
  canvas->renderer().set_layer_styles(styles);
  Camera camera(32.0, 32.0, 1.0);
  camera.set_viewport(64, 64);
  (void)canvas->render(camera.transform(), draws);
  int row = -1;
  for (int y = 30; y <= 34 && row < 0; ++y) {
    if (channel(canvas->read_pixel(1, y), 0) > 200) row = y;
  }
  REQUIRE(row >= 0);
  CHECK(channel(canvas->read_pixel(2, row), 0) > 200);
  CHECK(channel(canvas->read_pixel(5, row), 0) < 20);
  CHECK(channel(canvas->read_pixel(9, row), 0) > 200);
  CHECK(channel(canvas->read_pixel(14, row), 0) < 20);
  // 0xFF and 0 are solid.
  style.dash = 0xFF;
  const std::vector<LayerStyle> solid{style};
  canvas->renderer().set_layer_styles(solid);
  (void)canvas->render(camera.transform(), draws);
  CHECK(channel(canvas->read_pixel(5, row), 0) > 200);
}

TEST_CASE("polygons draw triangles and glyph fills merged in layer order",
          "[cap:hefesto-rendering][req:Style-file-faithful 2D drawing]") {
  auto canvas = make_canvas();
  // Layer 0: a red rectangle over the whole canvas. Layer 1: a blue
  // lower-left triangle of (0,0)-(64,64), relative to base (100, 100).
  // Layer 2: a green rectangle over the top-right corner, drawn after it.
  const std::vector<RectInstance> instances{{100, 100, 164, 164}, {150, 150, 164, 164}};
  const std::vector<LayerBatch> draws{{0, 0, 1}, {2, 1, 1}};
  const std::vector<PolygonVertex> vertices{{0.0F, 0.0F}, {0.0F, 64.0F}, {64.0F, 0.0F}};
  const std::vector<LayerBatch> polygon_draws{{1, 0, 3}};
  const std::vector<LayerStyle> styles{
      {1.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F, 1.0F}, {0.0F, 1.0F, 0.0F, 1.0F}};
  canvas->renderer().upload(instances);
  canvas->renderer().upload_polygons(vertices, 100, 100);
  canvas->renderer().set_layer_styles(styles);
  Camera camera(132.0, 132.0, 1.0);
  camera.set_viewport(64, 64);
  (void)canvas->render(camera.transform(), draws, polygon_draws);
  const std::uint32_t lower_left = canvas->read_pixel(8, 56);   // world (108, 108): triangle
  const std::uint32_t upper_right = canvas->read_pixel(40, 30);  // world (140, 134): above the diagonal
  const std::uint32_t corner = canvas->read_pixel(60, 4);        // world (160, 160): layer 2 on top
  CHECK(channel(lower_left, 2) == 255);
  CHECK(channel(lower_left, 0) == 0);
  CHECK(channel(upper_right, 0) == 255);
  CHECK(channel(upper_right, 2) == 0);
  CHECK(channel(corner, 1) == 255);
}
