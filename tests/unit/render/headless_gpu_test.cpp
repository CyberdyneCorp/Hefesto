// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Draws through the real Vulkan path (MoltenVK on macOS) into an offscreen
// image and reads pixels back. SKIPs when no Vulkan device is available.

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>

#include "hefesto/render/headless.hpp"

using namespace hefesto::render;

namespace {

std::unique_ptr<HeadlessCanvas> make_canvas() {
  try {
    return std::make_unique<HeadlessCanvas>(HeadlessOptions{64, 64, false});
  } catch (const VulkanError& e) {
    return nullptr;
  }
}

std::uint8_t channel(std::uint32_t rgba, unsigned index) {
  return static_cast<std::uint8_t>((rgba >> (8U * index)) & 0xFFU);
}

}  // namespace

TEST_CASE("renderer draws instanced rectangles with layer colors",
          "[cap:hefesto-performance][req:Interactive frame rate]") {
  auto canvas = make_canvas();
  if (!canvas) {
    SKIP("no Vulkan device");
  }
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
  if (!canvas) {
    SKIP("no Vulkan device");
  }
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
