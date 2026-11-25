// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-bench-view3d: the 3D mesh benchmark (openspec/changes/m10-3d-view
// design D6, D17, task 13.1; hefesto-performance "Interactive frame rate").
//
// Loads one workload (workloads.hpp), builds its layout-layer mesh over the
// cell's bounding box grown by 1 (no cut box, as an opened window without
// one) --repeat times, and prints a JSON report:
//   - build time of each run (wall clock; the mesh builder is single-threaded);
//   - the model's counts: rectangle, side and triangle instances and chunks;
//   - model_bytes: the model's CPU memory (vector capacities and chunk records,
//     excluding the database), and per meshed rectangle (D6 target <= 64);
//   - gpu_bytes_estimate: the D6 instance encoding (rectangle 20 bytes, side
//     20 bytes, triangle as stored), per meshed rectangle (D6 target <= 96),
//     information until the renderer (task 5.3) uploads real buffers;
//   - peak_rss_bytes: the process's peak resident set;
//   - frame (task 13.2, frame.hpp): with the Vulkan renderer built
//     (HEFESTO_BUILD_DESKTOP=ON), the mesh uploaded to VulkanMesh3dRenderer
//     on the headless device and --frames whole frames timed at 1280 x 800,
//     isometric view framed by frame_view, board lighting, level 0 and
//     level 1 (MSAA 4x), again with a ghosted well over the whole region
//     (level0_ghost, level1_ghost), plus the upload and
//     the first frame; frame_time_ms is level 1's median (the default
//     level), null with frame_time_note otherwise. Recorded, not gated.
// bench/view3d/view3d_gate.py checks the gated numbers against gates.json.
//
// The mask workload (task 13.3) builds in mask-layer mode instead: the
// timed build is mask::generate_flat over the bounding box plus the mesh
// (build_from_mask), clipped to that box in mask units.
//
//   hefesto-bench-view3d [--workload synthetic|mask|array|block] [--rects N] [--repeat N]
//                        [--frames N] [--pdk-root DIR] [--style-dir DIR ...] [--json FILE]
//   --frames 0 skips the frame workload (default 30).

#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "frame.hpp"
#include "hefesto/view3d/mesh/build.hpp"
#include "workloads.hpp"

namespace {

namespace bench = hefesto::bench_view3d;
namespace view3d = hefesto::view3d;
using nlohmann::json;

struct Options {
  bench::WorkloadOptions workload;
  int repeat = 3;
  int frames = 30;  // 0: no frame workload
  std::string json_path;
};

void usage() {
  std::cerr << "usage: hefesto-bench-view3d [--workload synthetic|mask|array|block] [--rects N] [--repeat N]\n"
               "                            [--frames N] [--pdk-root DIR] [--style-dir DIR]... [--json FILE]\n";
}

bool apply_option(Options& o, std::string_view flag, const char* value) {
  if (flag == "--workload") {
    o.workload.name = value;
  } else if (flag == "--rects") {
    o.workload.rects = std::strtoull(value, nullptr, 10);
  } else if (flag == "--repeat") {
    o.repeat = std::max(1, std::atoi(value));
  } else if (flag == "--frames") {
    o.frames = std::max(0, std::atoi(value));
  } else if (flag == "--pdk-root") {
    o.workload.pdk_root = value;
  } else if (flag == "--style-dir") {
    o.workload.style_dirs.emplace_back(value);
  } else if (flag == "--json") {
    o.json_path = value;
  } else {
    return false;
  }
  return true;
}

std::optional<Options> parse(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; i += 2) {
    if (i + 1 >= argc || !apply_option(o, argv[i], argv[i + 1])) {
      return std::nullopt;
    }
  }
  return o;
}

struct Counts {
  std::uint64_t rects = 0;
  std::uint64_t sides = 0;
  std::uint64_t triangles = 0;
  std::uint64_t model_bytes = 0;
};

Counts count(const view3d::MeshModel& model) {
  Counts c;
  c.model_bytes = sizeof(view3d::MeshModel) + model.chunks.capacity() * sizeof(view3d::Chunk);
  for (const view3d::Chunk& chunk : model.chunks) {
    c.rects += chunk.rects.size();
    c.sides += chunk.sides.size();
    c.triangles += chunk.triangles.size();
    c.model_bytes += chunk.rects.capacity() * sizeof(view3d::RectInstance) +
                     chunk.sides.capacity() * sizeof(view3d::SideInstance) +
                     chunk.triangles.capacity() * sizeof(view3d::Triangle);
  }
  return c;
}

// The D6 encoding: a rectangle is four float32 corners and a 16-bit layer
// (20 bytes), a side a segment plus layer and flags (20 bytes, aligned).
constexpr std::uint64_t kGpuRectBytes = 20;
constexpr std::uint64_t kGpuSideBytes = 20;

std::uint64_t peak_rss_bytes() {
  rusage usage{};
  (void)getrusage(RUSAGE_SELF, &usage);
#if defined(__APPLE__)
  return static_cast<std::uint64_t>(usage.ru_maxrss);  // bytes
#else
  return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024U;  // kilobytes
#endif
}

double per_rect(std::uint64_t bytes, std::uint64_t rects) {
  return rects == 0 ? 0.0 : static_cast<double>(bytes) / static_cast<double>(rects);
}

/// The clip area of a build: the bounding box grown by 1 (layout layers),
/// or the mask source's box in mask units (mask_clip).
view3d::ClipArea clip_of(const bench::Workload& w) {
  if (const view3d::MaskSource* source = w.mask_source()) {
    return view3d::mask_clip(*source, false);
  }
  const hefesto::db::Rect b = w.cell().bbox();
  return view3d::ClipArea{hefesto::db::Rect{b.x0 - 1, b.y0 - 1, b.x1 + 1, b.y1 + 1}, false, 0};
}

/// One timed build (for the mask workload: generation and mesh); nullopt
/// when the builder returned no model.
std::optional<view3d::MeshModel> timed_build(const bench::Workload& w, double& seconds) {
  const view3d::ClipArea clip = clip_of(w);
  view3d::BuildContext ctx;
  const auto start = std::chrono::steady_clock::now();
  auto model = w.mask_source() != nullptr ? view3d::build_from_mask(w.stack(), w.cell(), *w.mask_source(), clip, ctx)
                                          : view3d::build_from_layout(w.stack(), w.cell(), clip, ctx);
  seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  return model;
}

json timing(std::vector<double> runs) {
  json j;
  j["runs"] = runs;
  std::sort(runs.begin(), runs.end());
  j["min"] = runs.front();
  j["median"] = runs[runs.size() / 2];
  return j;
}

json report(const Options& o, const bench::Workload& w, const view3d::MeshModel& model, std::vector<double> runs) {
  const Counts c = count(model);
  const std::uint64_t gpu =
      c.rects * kGpuRectBytes + c.sides * kGpuSideBytes + c.triangles * sizeof(view3d::Triangle);
  json j;
  j["workload"] = o.workload.name;
  j["description"] = w.description();
  j["layers"] = w.stack().size();
  j["rects"] = c.rects;
  j["sides"] = c.sides;
  j["triangles"] = c.triangles;
  j["chunks"] = model.chunks.size();
  j["build_s"] = timing(std::move(runs));
  j["model_bytes"] = c.model_bytes;
  j["model_bytes_per_rect"] = per_rect(c.model_bytes, c.rects);
  j["gpu_bytes_estimate"] = gpu;
  j["gpu_bytes_per_rect_estimate"] = per_rect(gpu, c.rects);
  j["peak_rss_bytes"] = peak_rss_bytes();
  return j;
}

/// The frame workload's fields of the report (task 13.2).
void add_frames(json& j, const Options& o, const bench::Workload& w, const view3d::MeshModel& model) {
  if (o.frames == 0) {
    j["frame_time_ms"] = nullptr;
    j["frame_time_note"] = "not measured: --frames 0";
    return;
  }
  json frame = bench::measure_frames(model, w.stack(), clip_of(w).area, bench::FrameOptions{o.frames, 1280, 800});
  if (frame.contains("error")) {
    j["frame_time_ms"] = nullptr;
    j["frame_time_note"] = frame["error"];
    return;
  }
  j["frame_time_ms"] = frame["level1"]["median_ms"];
  j["frame"] = std::move(frame);
}

int run(const Options& o) {
  std::string error;
  const auto workload = bench::load_workload(o.workload, error);
  if (!workload) {
    std::cerr << "hefesto-bench-view3d: " << error << "\n";
    return 2;
  }
  std::vector<double> runs;
  std::optional<view3d::MeshModel> last;
  for (int i = 0; i < o.repeat; ++i) {
    double seconds = 0.0;
    last = timed_build(*workload, seconds);
    if (!last) {
      std::cerr << "hefesto-bench-view3d: the build returned no model\n";
      return 1;
    }
    runs.push_back(seconds);
  }
  json result = report(o, *workload, *last, runs);
  add_frames(result, o, *workload, *last);
  json out = {{"tool", "hefesto-bench-view3d"}, {"results", json::array({std::move(result)})}};
  std::cout << out.dump(2) << "\n";
  if (!o.json_path.empty()) {
    std::ofstream(o.json_path) << out.dump(2) << "\n";
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const auto options = parse(argc, argv);
  if (!options) {
    usage();
    return 2;
  }
  return run(*options);
}
