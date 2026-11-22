// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Frame-time workload of hefesto-bench-view3d (task 13.2). See frame.hpp.
// Owner: RENDER3D.

#include "frame.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

#ifdef HEFESTO_VIEW3D_BENCH_FRAME
#include <exception>
#include <memory>

#include "hefesto/render/headless.hpp"
#include "hefesto/render/mesh3d_target.hpp"
#include "hefesto/render/mesh3d_vulkan_renderer.hpp"
#include "hefesto/view3d/camera/view_state.hpp"
#endif

namespace hefesto::bench_view3d {

namespace {

using nlohmann::json;
using Clock = std::chrono::steady_clock;

[[maybe_unused]] double ms_since(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

[[maybe_unused]] json summary(std::vector<double> runs) {
  json j;
  j["runs_ms"] = runs;
  std::sort(runs.begin(), runs.end());
  const double median = runs[runs.size() / 2];
  j["median_ms"] = median;
  j["min_ms"] = runs.front();
  j["fps"] = median > 0.0 ? 1000.0 / median : 0.0;
  return j;
}

#ifdef HEFESTO_VIEW3D_BENCH_FRAME

/// Times `frames` draws at `level` after one warm-up draw.
json time_level(render::VulkanMesh3dRenderer& renderer, render::VulkanOffscreenTarget& target,
                const view3d::ViewState& view, render::DrawParams params, int level, int frames) {
  params.level = level;
  renderer.draw(view, params, target);  // warm-up: pipelines, MSAA images, layer buffers
  std::vector<double> runs;
  runs.reserve(static_cast<std::size_t>(frames));
  for (int i = 0; i < frames; ++i) {
    const auto start = Clock::now();
    renderer.draw(view, params, target);  // record, submit, wait (end_frame)
    runs.push_back(ms_since(start));
  }
  return summary(std::move(runs));
}

json measure(const view3d::MeshModel& mesh, const view3d::LayerStack& stack, const db::Rect& bbox,
             const FrameOptions& o) {
  render::HeadlessCanvas canvas(render::HeadlessOptions{16, 16, false});
  render::VulkanOffscreenTarget target(canvas.vulkan_device(), o.width, o.height);
  render::VulkanMesh3dRenderer renderer(target);
  view3d::ViewState view = view3d::default_view(bbox, stack.mode(), 1.0F);
  view3d::apply(view, view3d::Gesture{view3d::Gesture::Kind::preset, 0, 0, 1, false, view3d::Preset::isometric});
  render::DrawParams params;
  params.stack = &stack;
  params.level = 0;

  json j;
  j["device"] = canvas.device_name();
  j["width"] = o.width;
  j["height"] = o.height;
  j["view"] = "isometric, whole region";
  j["msaa"] = renderer.multisampling();
  const auto start = Clock::now();
  renderer.upload(mesh);
  j["upload_s"] = ms_since(start) / 1000.0;
  renderer.draw(view, params, target);
  j["first_frame_ms"] = ms_since(start);  // upload of every chunk and the first frame
  j["level0"] = time_level(renderer, target, view, params, 0, o.frames);
  j["level1"] = time_level(renderer, target, view, params, 1, o.frames);
  return j;
}

#endif

}  // namespace

json measure_frames(const view3d::MeshModel& mesh, const view3d::LayerStack& stack, const db::Rect& bbox,
                    const FrameOptions& options) {
#ifdef HEFESTO_VIEW3D_BENCH_FRAME
  try {
    return measure(mesh, stack, bbox, options);
  } catch (const std::exception& e) {
    return json{{"error", std::string("no frame measured: ") + e.what()}};
  }
#else
  (void)mesh;
  (void)stack;
  (void)bbox;
  (void)options;
  return json{{"error", "not measured: built without the Vulkan renderer (HEFESTO_BUILD_DESKTOP=OFF)"}};
#endif
}

}  // namespace hefesto::bench_view3d
