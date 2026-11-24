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
  j["p95_ms"] = runs[std::min(runs.size() - 1, (runs.size() * 95) / 100)];
  j["min_ms"] = runs.front();
  j["fps"] = median > 0.0 ? 1000.0 / median : 0.0;
  return j;
}

#ifdef HEFESTO_VIEW3D_BENCH_FRAME

/// Times `frames` draws at `level` after one warm-up draw, orbiting the
/// camera around z by 360 / frames degrees before each frame (the orbit
/// gesture), so every frame draws a new view of the whole region.
json time_level(render::VulkanMesh3dRenderer& renderer, render::VulkanOffscreenTarget& target,
                view3d::ViewState view, render::DrawParams params, int level, int frames) {
  params.level = level;
  renderer.draw(view, params, target);  // warm-up: pipelines, MSAA images, layer buffers
  const view3d::Gesture orbit{view3d::Gesture::Kind::orbit, 360.0F / static_cast<float>(frames), 0, 1, false,
                              view3d::Preset::top};
  std::vector<double> runs;
  runs.reserve(static_cast<std::size_t>(frames));
  for (int i = 0; i < frames; ++i) {
    view3d::apply(view, orbit);
    const auto start = Clock::now();
    renderer.draw(view, params, target);  // record, submit, wait (end_frame)
    runs.push_back(ms_since(start));
  }
  return summary(std::move(runs));
}

/// The region with the stack's lowest h and highest h + t (frame_view's box).
view3d::Extent3 stack_box(const view3d::LayerStack& stack, const db::Rect& bbox) {
  view3d::Extent3 box{static_cast<double>(bbox.x0), static_cast<double>(bbox.y0), static_cast<double>(bbox.x1),
                      static_cast<double>(bbox.y1), 0.0, 0.0};
  for (std::size_t i = 0; i < stack.size(); ++i) {
    const view3d::DrawHeight z = stack.draw_height(static_cast<view3d::LayerId>(i));
    box.z0 = std::min(box.z0, static_cast<double>(z.height));
    box.z1 = std::max(box.z1, static_cast<double>(z.height) + static_cast<double>(z.thickness));
  }
  return box;
}

/// `mesh` plus a well covering the whole region on a layer of its own (one
/// rectangle, h 0 and the stack's full height), and the alpha that ghosts
/// it (Q-MISC-85): the fill-rate worst case of a ghosted well.
std::pair<view3d::MeshModel, std::vector<float>> with_ghost_well(const view3d::MeshModel& mesh,
                                                                  const view3d::LayerStack& stack,
                                                                  const view3d::Extent3& box) {
  view3d::MeshModel out = mesh;
  const std::size_t layer = std::max(stack.size(), out.layer_heights.size());
  for (std::size_t i = out.layer_heights.size(); i < layer; ++i) {
    out.layer_heights.push_back(stack.draw_height(static_cast<view3d::LayerId>(i)));
  }
  out.layer_heights.push_back(view3d::DrawHeight{static_cast<float>(box.z0), static_cast<float>(box.z1 - box.z0)});
  view3d::Chunk well;
  well.id = 0xFFFFFFFFU;  // no Morton chunk of the region uses it
  well.origin_x = box.x0;
  well.origin_y = box.y0;
  well.area = db::Rect{static_cast<db::Coord>(box.x0), static_cast<db::Coord>(box.y0), static_cast<db::Coord>(box.x1),
                       static_cast<db::Coord>(box.y1)};
  well.rects.push_back(view3d::RectInstance{0.0F, 0.0F, static_cast<float>(box.x1 - box.x0),
                                            static_cast<float>(box.y1 - box.y0), static_cast<view3d::LayerId>(layer)});
  out.chunks.push_back(std::move(well));
  std::vector<float> alpha(layer + 1U, 1.0F);
  alpha[layer] = 0.2F;
  return {std::move(out), std::move(alpha)};
}

json measure(const view3d::MeshModel& mesh, const view3d::LayerStack& stack, const db::Rect& bbox,
             const FrameOptions& o) {
  render::HeadlessCanvas canvas(render::HeadlessOptions{16, 16, false});
  render::VulkanOffscreenTarget target(canvas.vulkan_device(), o.width, o.height);
  render::VulkanMesh3dRenderer renderer(target);
  view3d::ViewState view = view3d::default_view(bbox, stack.mode(), 1.0F);
  view3d::apply(view, view3d::Gesture{view3d::Gesture::Kind::preset, 0, 0, 1, false, view3d::Preset::isometric});
  const view3d::Extent3 box = stack_box(stack, bbox);
  view = view3d::frame_view(view, box, o.width, o.height);  // as the desktop tab frames it (Q-MISC-84)
  render::DrawParams params;
  params.stack = &stack;
  params.level = 0;

  json j;
  j["device"] = canvas.device_name();
  j["width"] = o.width;
  j["height"] = o.height;
  j["view"] = "isometric, framed (frame_view), orbiting 360 degrees around z over the timed frames";
  j["lighting"] = "board (Q-MISC-84)";
  j["msaa"] = renderer.multisampling();
  const auto start = Clock::now();
  renderer.upload(mesh);
  j["upload_s"] = ms_since(start) / 1000.0;
  renderer.draw(view, params, target);
  j["first_frame_ms"] = ms_since(start);  // upload of every chunk and the first frame
  j["level0"] = time_level(renderer, target, view, params, 0, o.frames);
  j["level1"] = time_level(renderer, target, view, params, 1, o.frames);

  // The same with a ghosted well over the whole region (Q-MISC-85).
  auto [ghosted, alpha] = with_ghost_well(mesh, stack, box);
  renderer.upload(ghosted);
  params.alpha = std::move(alpha);
  j["ghost_well"] = "one rectangle over the whole region on its own layer, alpha 0.2 (transparency pass)";
  j["level0_ghost"] = time_level(renderer, target, view, params, 0, o.frames);
  j["level1_ghost"] = time_level(renderer, target, view, params, 1, o.frames);
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
