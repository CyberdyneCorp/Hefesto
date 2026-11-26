// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The frame-time workload of hefesto-bench-view3d (openspec/changes/
// m10-3d-view design D6, D17, task 13.2; hefesto-performance "Interactive
// frame rate"): uploads a mesh to render::VulkanMesh3dRenderer on the
// headless Vulkan device and times whole frames (record, submit and wait,
// the renderer's draw, which ends in the target's end_frame) in the
// isometric view framed by view3d::frame_view (the whole region visible, as
// the desktop tab frames it), orbiting the camera around z before each
// frame, at level 0 and at level 1 (MSAA 4x: the geometry is drawn twice),
// then again with a ghosted well (alpha 0.2) over the whole region
// (Q-MISC-85), and with a net overlay prism over the whole region (task
// 10.4).
//
// Built only when the Vulkan renderer is (HEFESTO_BUILD_DESKTOP=ON, which
// defines HEFESTO_VIEW3D_BENCH_FRAME); otherwise measure_frames reports why
// nothing was measured. Owner: RENDER3D (task 13.2).

#pragma once

#include <nlohmann/json.hpp>

#include "hefesto/db/geometry.hpp"
#include "hefesto/view3d/mesh/mesh_model.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::bench_view3d {

struct FrameOptions {
  int frames = 30;      // timed frames per level (after one warm-up frame)
  int width = 1280;     // the offscreen target, pixels
  int height = 800;
};

/// {"device", "width", "height", "upload_s", "first_frame_ms",
///  "level0": {"median_ms", "p95_ms", "min_ms", "fps", "runs_ms"}, "level1": {...},
///  "level0_ghost": {...}, "level1_ghost": {...}, "level0_overlay": {...}, "level1_overlay": {...}},
/// or {"error": "..."} when no Vulkan device or renderer is available.
/// `bbox`: the region the default view fits.
[[nodiscard]] nlohmann::json measure_frames(const view3d::MeshModel& mesh, const view3d::LayerStack& stack,
                                            const db::Rect& bbox, const FrameOptions& options);

}  // namespace hefesto::bench_view3d
