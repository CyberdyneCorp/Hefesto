// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-bench-canvas: the M0 canvas spike benchmark (design.md D9).
//
// Generates a deterministic synthetic layout (default 10,000,000 rectangles
// over 8 layers), uploads it, then renders offscreen through Hefesto's Vulkan
// renderer during a scripted pan at full-chip zoom and a zoom sweep from
// full chip down to a 1 um field of view, twice: drawing every rectangle
// ("brute_force") and drawing through the CanvasIndex ("indexed": tile
// culling and level of detail). It prints a JSON report:
// upload time, per-frame CPU and GPU time (median, p95, p99, max), memory,
// and the comparison with hefesto-performance "Interactive frame rate"
// (median frame time <= 16.7 ms during pan and zoom).
//
// Headless: no window and no display are needed. It does not include Raven's
// UI in the frame nor presentation; the desktop shell's --bench mode measures
// that (see docs/decisions/raven-go-no-go.md).

#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numbers>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/render/canvas_index.hpp"
#include "hefesto/render/headless.hpp"
#include "hefesto/render/synthetic_layout.hpp"

namespace {

using hefesto::render::Bounds;
using hefesto::render::Camera;
using hefesto::render::CanvasIndex;
using hefesto::render::FrameTiming;
using hefesto::render::HeadlessCanvas;
using hefesto::render::LayerBatch;
using hefesto::render::RectInstance;
using hefesto::render::RectScene;
using nlohmann::json;

constexpr double kTargetMedianMs = 16.7;  // hefesto-performance, "Interactive frame rate"
constexpr double kMicronUnits = 1000.0;   // the synthetic layout uses 1 unit = 1 nm

struct Options {
  std::uint64_t rects = 10'000'000;
  std::uint64_t seed = hefesto::render::SyntheticLayoutParams{}.seed;
  int width = 1920;
  int height = 1080;
  int warmup_frames = 10;
  int pan_frames = 600;  // 10 s at 60 fps
  int zoom_frames = 240;
  bool validation = false;
  bool skip_brute = false;
  std::string output;
};

void usage() {
  std::cerr << "usage: hefesto-bench-canvas [--rects N] [--seed N] [--width PX] [--height PX]\n"
               "                            [--warmup N] [--pan-frames N] [--zoom-frames N]\n"
               "                            [--validation] [--skip-brute-force] [--output FILE]\n";
}

bool parse_number(std::string_view text, std::uint64_t& out) {
  char* end = nullptr;
  const std::string s(text);
  const unsigned long long v = std::strtoull(s.c_str(), &end, 0);
  if (end == s.c_str() || *end != '\0') {
    return false;
  }
  out = v;
  return true;
}

bool parse_int(std::string_view text, int& out) {
  std::uint64_t v = 0;
  if (!parse_number(text, v) || v > 1'000'000) {
    return false;
  }
  out = static_cast<int>(v);
  return true;
}

/// Applies one "--name value" pair; returns false on an unknown name or bad value.
bool apply_option(Options& o, std::string_view name, std::string_view value) {
  if (name == "--rects") return parse_number(value, o.rects);
  if (name == "--seed") return parse_number(value, o.seed);
  if (name == "--width") return parse_int(value, o.width) && o.width > 0;
  if (name == "--height") return parse_int(value, o.height) && o.height > 0;
  if (name == "--warmup") return parse_int(value, o.warmup_frames);
  if (name == "--pan-frames") return parse_int(value, o.pan_frames);
  if (name == "--zoom-frames") return parse_int(value, o.zoom_frames);
  if (name == "--output") {
    o.output = value;
    return true;
  }
  return false;
}

std::optional<Options> parse_args(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "--validation" || arg == "--skip-brute-force") {
      (arg == "--validation" ? o.validation : o.skip_brute) = true;
      continue;
    }
    if (i + 1 >= argc || !apply_option(o, arg, argv[i + 1])) {
      return std::nullopt;
    }
    ++i;
  }
  return o;
}

double seconds_since(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

/// Nearest-rank percentile of an unsorted sample (p in 0..100).
double percentile(std::vector<double> values, double p) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const double rank = std::ceil(p / 100.0 * static_cast<double>(values.size()));
  const auto index = static_cast<std::size_t>(std::clamp(rank, 1.0, static_cast<double>(values.size()))) - 1;
  return values[index];
}

json summarize(const std::vector<double>& ms) {
  double sum = 0.0;
  for (double v : ms) {
    sum += v;
  }
  const double mean = ms.empty() ? 0.0 : sum / static_cast<double>(ms.size());
  return json{{"frames", ms.size()},
              {"mean_ms", mean},
              {"median_ms", percentile(ms, 50.0)},
              {"p95_ms", percentile(ms, 95.0)},
              {"p99_ms", percentile(ms, 99.0)},
              {"max_ms", percentile(ms, 100.0)}};
}

/// Chooses what to draw for a camera position: every layer batch (brute
/// force) or the visible ranges of the CanvasIndex (culling + LOD).
class DrawPlanner {
 public:
  explicit DrawPlanner(const RectScene& scene) : scene_(&scene) {}
  explicit DrawPlanner(const CanvasIndex& index) : index_(&index) {}

  std::span<const LayerBatch> plan(const Camera& camera) {
    if (index_ == nullptr) {
      return scene_->batches();
    }
    index_->visible_ranges(camera.visible_window(), camera.units_per_pixel(), ranges_);
    return ranges_;
  }

 private:
  const RectScene* scene_ = nullptr;
  const CanvasIndex* index_ = nullptr;
  std::vector<LayerBatch> ranges_;
};

struct PhaseSamples {
  std::vector<double> cpu_ms;
  std::vector<double> gpu_ms;
  std::vector<double> instances;
  std::vector<double> draws;

  /// Renders one frame; CPU time includes planning the draw ranges.
  void frame(HeadlessCanvas& canvas, DrawPlanner& planner, const Camera& camera) {
    const auto start = std::chrono::steady_clock::now();
    const std::span<const LayerBatch> plan = planner.plan(camera);
    const double plan_ms = seconds_since(start) * 1e3;
    const FrameTiming t = canvas.render(camera.transform(), plan);
    cpu_ms.push_back(plan_ms + t.cpu_ms);
    if (t.gpu_ms) {
      gpu_ms.push_back(*t.gpu_ms);
    }
    double n = 0.0;
    for (const LayerBatch& b : plan) {
      n += b.count;
    }
    instances.push_back(n);
    draws.push_back(static_cast<double>(plan.size()));
  }

  [[nodiscard]] json report() const {
    const double median = percentile(cpu_ms, 50.0);
    json out{{"cpu", summarize(cpu_ms)},
             {"instances_per_frame_median", percentile(instances, 50.0)},
             {"instances_per_frame_max", percentile(instances, 100.0)},
             {"draw_calls_per_frame_max", percentile(draws, 100.0)},
             {"target_median_ms", kTargetMedianMs},
             {"meets_target", median <= kTargetMedianMs}};
    out["gpu"] = gpu_ms.empty() ? json(nullptr) : summarize(gpu_ms);
    return out;
  }
};

/// Pan at full-chip zoom (every rectangle on screen: the worst case), along a
/// closed loop so the view stays over the die.
PhaseSamples run_pan(HeadlessCanvas& canvas, DrawPlanner& planner, Camera camera, int frames) {
  PhaseSamples samples;
  const double radius_px = 0.25 * canvas.width();
  const double n = std::max(frames, 1);
  for (int i = 0; i < frames; ++i) {
    const double a0 = 2.0 * std::numbers::pi * i / n;
    const double a1 = 2.0 * std::numbers::pi * (i + 1) / n;
    camera.pan_pixels(radius_px * (std::cos(a1) - std::cos(a0)), radius_px * (std::sin(a1) - std::sin(a0)));
    samples.frame(canvas, planner, camera);
  }
  return samples;
}

/// Geometric zoom sweep from the full-chip fit to a 1 um wide field of view.
PhaseSamples run_zoom(HeadlessCanvas& canvas, DrawPlanner& planner, Camera camera, int frames) {
  PhaseSamples samples;
  const double start = camera.pixels_per_unit();
  const double end = canvas.width() / kMicronUnits;
  const double step = std::pow(end / start, 1.0 / std::max(frames - 1, 1));
  for (int i = 0; i < frames; ++i) {
    samples.frame(canvas, planner, camera);
    camera.zoom_at(step, 0.5 * canvas.width(), 0.5 * canvas.height());
  }
  return samples;
}

std::uint64_t peak_rss_bytes() {
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
#if defined(__APPLE__)
  return static_cast<std::uint64_t>(usage.ru_maxrss);  // bytes on macOS
#else
  return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024U;  // KiB on Linux
#endif
}

json upload_report(const hefesto::render::UploadStats& upload) {
  const double rate = upload.seconds > 0 ? static_cast<double>(upload.bytes) / upload.seconds / double(1U << 30U) : 0.0;
  return json{{"seconds", upload.seconds}, {"bytes", upload.bytes}, {"gib_per_s", rate}};
}

/// Uploads `instances`, then measures the pan and the zoom sweep.
json run_mode(HeadlessCanvas& canvas, std::span<const RectInstance> instances, DrawPlanner planner,
              const Bounds& bounds, const Options& o) {
  json out{{"upload", upload_report(canvas.renderer().upload(instances))},
           {"device_bytes", canvas.renderer().device_bytes()}};
  Camera camera;
  camera.set_viewport(o.width, o.height);
  camera.fit(bounds);
  for (int i = 0; i < o.warmup_frames; ++i) {
    (void)canvas.render(camera.transform(), planner.plan(camera));
  }
  out["pan_full_chip"] = run_pan(canvas, planner, camera, o.pan_frames).report();
  out["zoom_sweep"] = run_zoom(canvas, planner, camera, o.zoom_frames).report();
  out["meets_target"] =
      out["pan_full_chip"]["meets_target"].get<bool>() && out["zoom_sweep"]["meets_target"].get<bool>();
  return out;
}

json run(const Options& o) {
  json report{{"benchmark", "hefesto-canvas"},
              {"spec", "hefesto-performance: Interactive frame rate"},
              {"mode", "headless-offscreen"},
              {"viewport", {{"width", o.width}, {"height", o.height}}}};

  auto t0 = std::chrono::steady_clock::now();
  hefesto::render::SyntheticLayoutParams params;
  params.count = o.rects;
  params.seed = o.seed;
  const auto rects = hefesto::render::generate_synthetic_layout(params);
  const double generate_s = seconds_since(t0);
  t0 = std::chrono::steady_clock::now();
  const auto scene = RectScene::from_layered(rects);
  const double scene_s = seconds_since(t0);
  t0 = std::chrono::steady_clock::now();
  const auto index = CanvasIndex::build(rects);
  const double index_s = seconds_since(t0);
  report["dataset"] = {{"rectangles", scene.size()},
                       {"layers", scene.batches().size()},
                       {"seed", o.seed},
                       {"fingerprint", hefesto::render::fingerprint(rects)},
                       {"instance_bytes", scene.byte_size()},
                       {"generate_s", generate_s},
                       {"scene_build_s", scene_s},
                       {"index_build_s", index_s},
                       {"index_lod_cells", index.lod_cell_count()},
                       {"index_instance_bytes", index.byte_size()}};

  HeadlessCanvas canvas({o.width, o.height, o.validation});
  report["device"] = {
      {"name", canvas.device_name()}, {"driver", canvas.driver_info()}, {"vulkan_api", canvas.api_version()}};
  if (!o.skip_brute) {
    report["brute_force"] = run_mode(canvas, scene.instances(), DrawPlanner(scene), scene.bounds(), o);
  }
  report["indexed"] = run_mode(canvas, index.instances(), DrawPlanner(index), index.bounds(), o);
  report["meets_target"] = report["indexed"]["meets_target"];

  const auto budget = canvas.memory_budget();
  report["memory"] = {
      {"peak_rss_bytes", peak_rss_bytes()},
      {"device_local_usage_bytes", budget.available ? json(budget.device_local_usage) : json(nullptr)},
      {"device_local_budget_bytes", budget.available ? json(budget.device_local_budget) : json(nullptr)}};
  return report;
}

}  // namespace

int main(int argc, char** argv) {
  const auto options = parse_args(argc, argv);
  if (!options) {
    usage();
    return 2;
  }
  json report;
  int status = 0;
  try {
    report = run(*options);
  } catch (const std::exception& e) {
    report = json{{"benchmark", "hefesto-canvas"}, {"status", "error"}, {"error", e.what()}};
    status = 1;
  }
  const std::string text = report.dump(2);
  std::cout << text << '\n';
  if (!options->output.empty()) {
    std::ofstream(options->output) << text << '\n';
  }
  return status;
}
