// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The 3D view's offscreen target and the renderer spike (openspec/changes/
// m10-3d-view tasks.md 5.1, 5.2; design D8) through the real Vulkan path:
// MoltenVK on macOS, Mesa lavapipe in the Linux CI job gpu-readback-lavapipe.
// SKIPs without a Vulkan device unless HEFESTO_REQUIRE_VULKAN_DEVICE is set
// (the same rule as headless_gpu_test.cpp). Owner: RENDER3D.

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include "hefesto/render/headless.hpp"
#include "hefesto/render/mesh3d_spike.hpp"
#include "hefesto/render/mesh3d_target.hpp"

using namespace hefesto::render;

namespace {

std::unique_ptr<HeadlessCanvas> make_device() {
  const char* env = std::getenv("HEFESTO_REQUIRE_VULKAN_DEVICE");
  const std::string required = env != nullptr ? env : "";
  std::unique_ptr<HeadlessCanvas> canvas;
  try {
    canvas = std::make_unique<HeadlessCanvas>(HeadlessOptions{16, 16, false});
  } catch (const VulkanError& e) {
    if (!required.empty()) {
      FAIL("no Vulkan device, but HEFESTO_REQUIRE_VULKAN_DEVICE=" << required << ": " << e.what());
    }
    SKIP("no Vulkan device: " << e.what());
  }
  if (!required.empty()) {
    INFO("Vulkan device: " << canvas->device_name());
    REQUIRE(canvas->device_name().find(required) != std::string::npos);
  }
  return canvas;
}

struct Pixel {
  std::uint32_t color;
  float depth;
  std::uint32_t pick;
};

Pixel at(const TargetReadback& r, int x, int y) {
  const auto i = static_cast<std::size_t>(y) * static_cast<std::size_t>(r.width) + static_cast<std::size_t>(x);
  return Pixel{r.color[i], r.depth[i], r.pick[i]};
}

std::uint32_t channel(std::uint32_t rgba, unsigned index) {
  return (rgba >> (8U * index)) & 0xFFU;
}

std::uint32_t id(SpikeQuad q) {
  return static_cast<std::uint32_t>(q);
}

struct Expected {
  const char* what;
  float clip_x;
  float clip_y;
  std::uint32_t pick;
  std::uint32_t rgb;  // 0xRRGGBB, sRGB-encoded
  float depth;
};

// The background is linear 0.2158605, which the sRGB view stores as 128 (the
// shown UNORM view then passes 128 to the UI unchanged).
const Expected kSpikeFrame[] = {
    {"near red only", -0.75F, 0.25F, id(SpikeQuad::near_red), 0xFF0000, 0.25F},
    // Red was drawn first and is nearer: the depth test keeps it over green.
    {"red over far green", 0.0F, 0.25F, id(SpikeQuad::near_red), 0xFF0000, 0.25F},
    {"far green only", 0.75F, 0.25F, id(SpikeQuad::far_green), 0x00FF00, 0.75F},
    // Blue is coplanar with red and drawn later: LESS_OR_EQUAL lets it win.
    {"coplanar blue over red", -0.5F, -0.75F, id(SpikeQuad::coplanar_blue), 0x0000FF, 0.25F},
    // Nothing here but the culled back face: background, no pick, far depth.
    {"background", 0.0F, 0.9F, 0, 0x808080, 1.0F},
};

bool near(std::uint32_t a, std::uint32_t b) {
  return a + 1 >= b && b + 1 >= a;
}

/// Checks the spike's arrangement at clip-space points (any target size).
void check_spike_frame(const TargetReadback& r) {
  for (const Expected& e : kSpikeFrame) {
    const auto x = static_cast<int>((e.clip_x + 1.0F) * 0.5F * static_cast<float>(r.width));
    const auto y = static_cast<int>((e.clip_y + 1.0F) * 0.5F * static_cast<float>(r.height));
    const Pixel p = at(r, x, y);
    INFO(e.what << " at (" << x << ", " << y << ") of " << r.width << "x" << r.height);
    CHECK(p.pick == e.pick);
    CHECK(p.depth == e.depth);
    CHECK(near(channel(p.color, 0), (e.rgb >> 16U) & 0xFFU));
    CHECK(near(channel(p.color, 1), (e.rgb >> 8U) & 0xFFU));
    CHECK(near(channel(p.color, 2), e.rgb & 0xFFU));
    CHECK(channel(p.color, 3) == 255);
  }
}

}  // namespace

TEST_CASE("3D offscreen target renders the depth-tested spike mesh into color, depth and pick images",
          "[cap:view-3d][req:Viewing transform and shading]") {
  auto canvas = make_device();
  VulkanOffscreenTarget target(canvas->vulkan_device(), 64, 48);
  CHECK(target.shown().view == 0);
  CHECK(target.read_back().color.empty());

  Mesh3dSpike spike(target);
  spike.draw(target);
  CHECK(target.frame_count() == 1);
  const ShownImage shown = target.shown();
  CHECK(shown.view != 0);
  CHECK(shown.width == 64);
  CHECK(shown.height == 48);

  const TargetReadback r = target.read_back();
  REQUIRE(r.width == 64);
  REQUIRE(r.height == 48);
  REQUIRE(r.color.size() == 64U * 48U);
  check_spike_frame(r);
}

TEST_CASE("3D offscreen target is double-buffered and resizes on the next frame",
          "[cap:view-3d][req:Platform mechanism of the 3D view]") {
  auto canvas = make_device();
  VulkanOffscreenTarget target(canvas->vulkan_device(), 32, 32);
  Mesh3dSpike spike(target);
  spike.draw(target);
  const std::uint64_t first = target.shown().view;
  spike.draw(target);
  const std::uint64_t second = target.shown().view;
  CHECK(first != second);  // the frame drew into the buffer that was not shown

  // A resize keeps showing the last frame until a new one is drawn.
  target.resize(80, 20);
  CHECK(target.width() == 80);
  CHECK(target.height() == 20);
  CHECK(target.shown().view == second);
  CHECK(target.shown().width == 32);

  spike.draw(target);
  CHECK(target.shown().width == 80);
  CHECK(target.shown().height == 20);
  const TargetReadback r = target.read_back();
  REQUIRE(r.width == 80);
  REQUIRE(r.height == 20);
  REQUIRE(r.depth.size() == 80U * 20U);
  REQUIRE(r.pick.size() == 80U * 20U);
  check_spike_frame(r);

  // The other buffer, still at 32 x 32, is recreated when it is drawn into.
  spike.draw(target);
  CHECK(target.shown().width == 80);
  check_spike_frame(target.read_back());
}

TEST_CASE("3D offscreen target rejects empty sizes and clamps resizes",
          "[cap:view-3d][req:Platform mechanism of the 3D view]") {
  auto canvas = make_device();
  CHECK_THROWS_AS(VulkanOffscreenTarget(canvas->vulkan_device(), 0, 10), std::invalid_argument);
  VulkanOffscreenTarget target(canvas->vulkan_device(), 8, 8);
  target.resize(0, -3);  // a collapsed viewport
  CHECK(target.width() == 1);
  CHECK(target.height() == 1);
  Mesh3dSpike spike(target);
  spike.draw(target);
  CHECK(target.read_back().pick.size() == 1U);
}
