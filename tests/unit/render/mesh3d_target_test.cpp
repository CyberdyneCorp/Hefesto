// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The 3D view's offscreen target (openspec/changes/m10-3d-view tasks.md 5.1,
// 5.2; design D8) through the real Vulkan path: MoltenVK on macOS, Mesa
// lavapipe in the Linux CI job gpu-readback-lavapipe. The frames are drawn
// by VulkanMesh3dRenderer (one metal1 rectangle in the top view); what the
// renderer draws is checked in mesh3d_renderer_test.cpp, here only that the
// target holds a frame in its color, depth and pick images, swaps its two
// buffers and resizes. SKIPs without a Vulkan device unless
// HEFESTO_REQUIRE_VULKAN_DEVICE is set (the same rule as
// headless_gpu_test.cpp). Owner: RENDER3D.

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../view3d/mesh/mesh_fixture.hpp"
#include "hefesto/render/headless.hpp"
#include "hefesto/render/mesh3d_target.hpp"
#include "hefesto/render/mesh3d_vulkan_renderer.hpp"
#include "hefesto/view3d/camera/view_state.hpp"

namespace db = hefesto::db;
namespace render = hefesto::render;
namespace view3d = hefesto::view3d;

namespace {

std::unique_ptr<render::HeadlessCanvas> make_device() {
  const char* env = std::getenv("HEFESTO_REQUIRE_VULKAN_DEVICE");
  const std::string required = env != nullptr ? env : "";
  std::unique_ptr<render::HeadlessCanvas> canvas;
  try {
    canvas = std::make_unique<render::HeadlessCanvas>(render::HeadlessOptions{16, 16, false});
  } catch (const render::VulkanError& e) {
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

/// One metal1 rectangle covering the middle of the top view.
struct Scene {
  view3d::test::MeshTech tech = view3d::test::build_tech();
  view3d::LayerStack stack;
  view3d::MeshModel mesh;
  view3d::ViewState view;
};

Scene metal_scene() {
  Scene s;
  view3d::StackLayer metal = view3d::test::layer("metal1", s.tech.metal1, 0.0F, 10.0F);
  std::vector<view3d::DrawHeight> heights(hefesto::tech::kMaxTileTypes);
  heights[s.tech.metal1] = view3d::DrawHeight{metal.height, metal.thickness};
  s.stack = view3d::LayerStack{{metal}, std::move(heights), false, 0.01};
  db::CellDef cell("target", s.tech.tech->plane_count());
  view3d::test::paint(cell, s.tech, db::Rect{0, 0, 100, 50}, s.tech.metal1);
  auto mesh = view3d::test::build(s.stack, cell, db::Rect{-1, -1, 101, 51}, false);
  REQUIRE(mesh.has_value());
  s.mesh = std::move(*mesh);
  s.view = view3d::default_view(cell.bbox(), view3d::Mode::layout_layer, 1.0F);
  return s;
}

/// The renderer and its scene on one target.
struct Frames {
  Scene scene = metal_scene();
  render::VulkanMesh3dRenderer renderer;

  explicit Frames(render::VulkanOffscreenTarget& target) : renderer(target) { renderer.upload(scene.mesh); }

  void draw(render::VulkanOffscreenTarget& target) {
    render::DrawParams p;
    p.stack = &scene.stack;
    renderer.draw(scene.view, p, target);
  }
};

struct Pixel {
  std::uint32_t color;
  float depth;
  std::uint32_t pick;
};

Pixel at(const render::TargetReadback& r, float fx, float fy) {
  const auto x = static_cast<std::size_t>(fx * static_cast<float>(r.width));
  const auto y = static_cast<std::size_t>(fy * static_cast<float>(r.height));
  const std::size_t i = y * static_cast<std::size_t>(r.width) + x;
  return Pixel{r.color[i], r.depth[i], r.pick[i]};
}

/// The rectangle's top face at the centre, the background in a corner (any
/// target size): the three images agree on which pixel shows the mesh.
void check_frame(const render::TargetReadback& r) {
  REQUIRE(r.color.size() == static_cast<std::size_t>(r.width) * static_cast<std::size_t>(r.height));
  REQUIRE(r.depth.size() == r.color.size());
  REQUIRE(r.pick.size() == r.color.size());
  const Pixel face = at(r, 0.5F, 0.5F);
  const Pixel corner = at(r, 0.02F, 0.02F);
  INFO("target " << r.width << "x" << r.height);
  CHECK(face.pick != view3d::kNoPick);
  CHECK(face.depth < 1.0F);
  CHECK(corner.pick == view3d::kNoPick);
  CHECK(corner.depth == 1.0F);  // cleared to the far plane
  CHECK(face.color != corner.color);
  CHECK((face.color >> 24U) == 255U);
  CHECK((corner.color >> 24U) == 255U);
}

}  // namespace

TEST_CASE("3D offscreen target holds a renderer frame in its color, depth and pick images",
          "[cap:view-3d][req:Viewing transform and shading]") {
  auto canvas = make_device();
  render::VulkanOffscreenTarget target(canvas->vulkan_device(), 64, 48);
  CHECK(target.shown().view == 0);
  CHECK(target.read_back().color.empty());

  Frames frames(target);
  frames.draw(target);
  CHECK(target.frame_count() == 1);
  const render::ShownImage shown = target.shown();
  CHECK(shown.view != 0);
  CHECK(shown.width == 64);
  CHECK(shown.height == 48);

  const render::TargetReadback r = target.read_back();
  REQUIRE(r.width == 64);
  REQUIRE(r.height == 48);
  check_frame(r);
}

TEST_CASE("3D offscreen target is double-buffered and resizes on the next frame",
          "[cap:view-3d][req:Platform mechanism of the 3D view]") {
  auto canvas = make_device();
  render::VulkanOffscreenTarget target(canvas->vulkan_device(), 32, 32);
  Frames frames(target);
  frames.draw(target);
  const std::uint64_t first = target.shown().view;
  frames.draw(target);
  const std::uint64_t second = target.shown().view;
  CHECK(first != second);  // the frame drew into the buffer that was not shown

  // A resize keeps showing the last frame until a new one is drawn.
  target.resize(80, 20);
  CHECK(target.width() == 80);
  CHECK(target.height() == 20);
  CHECK(target.shown().view == second);
  CHECK(target.shown().width == 32);

  frames.draw(target);
  CHECK(target.shown().width == 80);
  CHECK(target.shown().height == 20);
  const render::TargetReadback r = target.read_back();
  REQUIRE(r.width == 80);
  REQUIRE(r.height == 20);
  check_frame(r);

  // The other buffer, still at 32 x 32, is recreated when it is drawn into.
  frames.draw(target);
  CHECK(target.shown().width == 80);
  check_frame(target.read_back());
}

TEST_CASE("3D offscreen target rejects empty sizes and clamps resizes",
          "[cap:view-3d][req:Platform mechanism of the 3D view]") {
  auto canvas = make_device();
  CHECK_THROWS_AS(render::VulkanOffscreenTarget(canvas->vulkan_device(), 0, 10), std::invalid_argument);
  render::VulkanOffscreenTarget target(canvas->vulkan_device(), 8, 8);
  target.resize(0, -3);  // a collapsed viewport
  CHECK(target.width() == 1);
  CHECK(target.height() == 1);
  Frames frames(target);
  frames.draw(target);
  CHECK(target.read_back().pick.size() == 1U);
}
