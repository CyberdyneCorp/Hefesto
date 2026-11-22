// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-render3d-spike: the M10 3D window check (openspec/changes/
// m10-3d-view tasks 5.1, 9.1, 13.2; design D8, risk R1). It builds a real
// layout-layer mesh with the MESH builder, draws it with
// render::VulkanMesh3dRenderer into a render::VulkanOffscreenTarget on
// Raven's Vulkan device, shows it in a Raven viewport in texture mode,
// resizes the window, and checks the window's pixels and the pick image
// after each step:
//
//   hefesto-render3d-spike [--capture PREFIX] [--expect-window-system x11|wayland|cocoa]
//   hefesto-render3d-spike --bench-rects N [--frames F]
//
// The check scene is one metal1 rectangle (0,0)-(100,50) in the default
// (top-down) view with no display styles: the layer is white and the
// background black, so the window's centre is white, its corners black and
// the pick id at the centre is an instance's.
//
// --bench-rects draws the synthetic region of bench/view3d (at least N
// rectangles, task 13.1) in the window, orbiting a little each frame, and
// prints the frame times (task 13.2): renderer draw (submit + fence) and
// the whole Raven frame.
//
// Exit codes: 0 the pixels match, 1 a check failed, 3 Raven or Vulkan could
// not start, 4 Raven picked another window system. With --capture the two
// frames are written to PREFIX-1.ppm and PREFIX-2.ppm.
//
// Owner: SHELL (the 3D tab, task 9.1; the spike was RENDER3D's, task 5.1).

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/render/mesh3d_target.hpp"
#include "hefesto/render/mesh3d_vulkan_renderer.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/view3d/camera/view_state.hpp"
#include "hefesto/view3d/mesh/build.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"
#include "raven_ui.hpp"
#include "workloads.hpp"

namespace {

using hefesto::render::VulkanMesh3dRenderer;
using hefesto::render::VulkanOffscreenTarget;
namespace db = hefesto::db;
namespace ui = hefesto::shell::ui;
namespace view3d = hefesto::view3d;

struct Options {
  std::string capture;
  std::string expect_window_system;
  std::uint64_t bench_rects = 0;
  int frames = 120;
};

Options parse(int argc, char** argv) {
  Options o;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string_view name = argv[i];
    if (name == "--capture") o.capture = argv[i + 1];
    if (name == "--expect-window-system") o.expect_window_system = argv[i + 1];
    if (name == "--bench-rects") o.bench_rects = std::strtoull(argv[i + 1], nullptr, 10);
    if (name == "--frames") o.frames = std::max(1, std::atoi(argv[i + 1]));
  }
  return o;
}

/// What the window draws: a stack, a cell and its mesh.
struct Scene {
  std::shared_ptr<const hefesto::tech::Technology> tech;
  std::unique_ptr<db::CellDef> cell;
  view3d::LayerStack stack;
  std::unique_ptr<hefesto::bench_view3d::Workload> workload;  // --bench-rects
  view3d::MeshModel mesh;

  [[nodiscard]] const db::CellDef& root() const { return workload ? workload->cell() : *cell; }
  [[nodiscard]] const view3d::LayerStack& layers() const { return workload ? workload->stack() : stack; }
};

/// The check scene: metal1 (h 0, t 10) painted at (0,0)-(100,50).
void build_check_layout(Scene& s) {
  hefesto::tech::TechnologyBuilder b;
  b.set_name("spike3d");
  (void)b.add_plane("metal1");
  const hefesto::tech::TileType metal1 = b.add_type("metal1", "metal1").value();
  s.tech = b.finalize().value();
  view3d::StackLayer layer;
  layer.name = "metal1";
  layer.type = metal1;
  layer.height = 0.0F;
  layer.thickness = 10.0F;
  layer.source = view3d::ValueSource::extract;
  std::vector<view3d::DrawHeight> heights(hefesto::tech::kMaxTileTypes);
  heights[metal1] = view3d::DrawHeight{layer.height, layer.thickness};
  s.stack = view3d::LayerStack{{layer}, std::move(heights), true, 0.01};
  s.cell = std::make_unique<db::CellDef>("spike", s.tech->plane_count());
  (void)db::paint(*s.cell, *s.tech, db::Rect{0, 0, 100, 50}, metal1);
  db::recompute_bbox(*s.cell);
}

bool build_mesh(Scene& s) {
  const db::Rect b = s.root().bbox();
  const view3d::ClipArea clip{db::Rect{b.x0 - 1, b.y0 - 1, b.x1 + 1, b.y1 + 1}, false, 0};
  view3d::BuildContext ctx;
  auto model = view3d::build_from_layout(s.layers(), s.root(), clip, ctx);
  if (!model) return false;
  s.mesh = std::move(*model);
  return true;
}

struct Rgb {
  int r;
  int g;
  int b;
};

struct Probe {
  const char* what;
  float fx;  // fraction of the width
  float fy;  // fraction of the height
  Rgb expected;
};

// The UNORM view passes the sRGB-encoded values to the UI unchanged.
constexpr Probe kProbes[] = {
    {"metal1 top face at the centre", 0.5F, 0.5F, {255, 255, 255}},
    {"background, top-left", 0.03F, 0.05F, {0, 0, 0}},
    {"background, bottom-right", 0.97F, 0.95F, {0, 0, 0}},
};

bool near(int a, int b) {
  return a - b <= 3 && b - a <= 3;
}

/// Checks the probes against a window capture; prints each failure.
bool check_capture(const std::vector<std::uint8_t>& rgba, int width, int height, int step) {
  if (rgba.empty() || width <= 0 || height <= 0) {
    std::fprintf(stderr, "render3d spike: step %d: the window could not be captured\n", step);
    return false;
  }
  bool ok = true;
  for (const Probe& p : kProbes) {
    const auto x = static_cast<std::size_t>(p.fx * static_cast<float>(width));
    const auto y = static_cast<std::size_t>(p.fy * static_cast<float>(height));
    const std::size_t i = (y * static_cast<std::size_t>(width) + x) * 4U;
    const Rgb got{rgba[i], rgba[i + 1], rgba[i + 2]};
    if (!near(got.r, p.expected.r) || !near(got.g, p.expected.g) || !near(got.b, p.expected.b)) {
      std::fprintf(stderr, "render3d spike: step %d: %s at (%zu, %zu): got %d %d %d, expected %d %d %d\n", step,
                   p.what, x, y, got.r, got.g, got.b, p.expected.r, p.expected.g, p.expected.b);
      ok = false;
    }
  }
  return ok;
}

void write_ppm(const std::string& path, const std::vector<std::uint8_t>& rgba, int width, int height) {
  std::FILE* file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) return;
  std::fprintf(file, "P6\n%d %d\n255\n", width, height);
  for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
    std::fwrite(&rgba[i], 1, 3, file);
  }
  std::fclose(file);
}

/// The window: a viewport in texture mode that redraws the mesh whenever
/// the viewport's pixel size or the view changes.
class SpikeWindow {
 public:
  SpikeWindow(ui::Runtime& runtime, const hefesto::render::QueueLock& lock, const Scene& scene)
      : runtime_(runtime),
        scene_(scene),
        window_("Hefesto 3D view check", 640, 400),
        viewport_(ui::Viewport::create()),
        target_(runtime.vulkan_device(), 1, 1, lock),
        renderer_(target_),
        view_(view3d::default_view(scene.root().bbox(), view3d::Mode::layout_layer, 1.0F)) {
    renderer_.upload(scene_.mesh);
    window_.set_content(*viewport_);
    viewport_->on_resize([this](int w, int h) {
      target_.resize(w, h);
      dirty_ = true;
    });
  }
  ~SpikeWindow() {
    window_.destroy();  // no frame renders after this
    viewport_.reset();  // destroys the wrapped texture before the target's images
  }
  SpikeWindow(const SpikeWindow&) = delete;
  SpikeWindow& operator=(const SpikeWindow&) = delete;

  [[nodiscard]] ui::Window& window() noexcept { return window_; }
  [[nodiscard]] VulkanMesh3dRenderer& renderer() noexcept { return renderer_; }

  /// Runs `frames` Raven frames, redrawing the 3D image when needed.
  void run(int frames) {
    for (int i = 0; i < frames; ++i) {
      if (dirty_) redraw();
      runtime_.wait_frame(0.05);
    }
  }

  /// Orbits by `degrees` per frame for `frames` frames; returns the mean
  /// draw time and the mean whole-frame time, in ms.
  std::pair<double, double> orbit(int frames, float degrees) {
    double draw_ms = 0.0;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < frames; ++i) {
      view_.view_z += degrees;
      draw_ms += redraw();
      runtime_.wait_frame(0.0);
    }
    const double total = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return {draw_ms / frames, total / frames};
  }

  [[nodiscard]] std::uint64_t frames_drawn() const noexcept { return target_.frame_count(); }
  [[nodiscard]] int target_width() const noexcept { return target_.width(); }
  [[nodiscard]] int target_height() const noexcept { return target_.height(); }

 private:
  double redraw() {
    hefesto::render::DrawParams params;
    params.stack = &scene_.layers();
    params.level = 1;
    const auto start = std::chrono::steady_clock::now();
    renderer_.draw(view_, params, target_);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    // Re-point the viewport before the next frame draws into the buffer it showed.
    const hefesto::render::ShownImage shown = target_.shown();
    viewport_->show_vulkan_image(shown.view, shown.width, shown.height);
    dirty_ = false;
    return ms;
  }

  ui::Runtime& runtime_;
  const Scene& scene_;
  ui::Window window_;
  std::unique_ptr<ui::Viewport> viewport_;
  VulkanOffscreenTarget target_;
  VulkanMesh3dRenderer renderer_;
  view3d::ViewState view_;
  bool dirty_ = true;
};

bool capture_and_check(SpikeWindow& spike, const Options& o, int step) {
  int width = 0;
  int height = 0;
  const std::vector<std::uint8_t> rgba = spike.window().capture_rgba(width, height);
  if (!o.capture.empty() && !rgba.empty()) {
    write_ppm(o.capture + "-" + std::to_string(step) + ".ppm", rgba, width, height);
  }
  bool ok = check_capture(rgba, width, height, step);
  const view3d::PickId centre = spike.renderer().read_pick(spike.target_width() / 2, spike.target_height() / 2);
  const view3d::PickId corner = spike.renderer().read_pick(1, 1);
  if (centre == view3d::kNoPick || corner != view3d::kNoPick) {
    std::fprintf(stderr, "render3d spike: step %d: pick ids centre %u, corner %u (expected an instance, then 0)\n",
                 step, centre, corner);
    ok = false;
  }
  std::printf("render3d spike: step %d: window %dx%d, target %dx%d, %llu frames drawn, centre pick %u: %s\n", step,
              width, height, spike.target_width(), spike.target_height(),
              static_cast<unsigned long long>(spike.frames_drawn()), centre, ok ? "ok" : "FAILED");
  return ok;
}

int run_bench(SpikeWindow& spike, const Scene& scene, const Options& o) {
  std::size_t rects = 0;
  for (const view3d::Chunk& c : scene.mesh.chunks) rects += c.rects.size();
  spike.run(10);  // the first frames lay the window out
  const auto [draw_ms, frame_ms] = spike.orbit(o.frames, 1.0F);
  std::printf("render3d bench: %s; %zu rectangles, %zu chunks; target %dx%d; %d frames: draw %.2f ms (%.1f fps), "
              "whole frame %.2f ms (%.1f fps)\n",
              scene.workload->description().c_str(), rects, scene.mesh.chunks.size(), spike.target_width(),
              spike.target_height(), o.frames, draw_ms, 1000.0 / draw_ms, frame_ms, 1000.0 / frame_ms);
  return 0;
}

int run_check(SpikeWindow& spike, const Options& o) {
  spike.run(20);
  const bool first = capture_and_check(spike, o, 1);
  spike.window().set_size(900, 300);
  spike.run(20);
  const bool second = capture_and_check(spike, o, 2);
  return first && second ? 0 : 1;
}

bool load_scene(Scene& scene, const Options& o) {
  if (o.bench_rects > 0) {
    hefesto::bench_view3d::WorkloadOptions w;
    w.rects = o.bench_rects;
    std::string error;
    scene.workload = hefesto::bench_view3d::load_workload(w, error);
    if (!scene.workload) {
      std::fprintf(stderr, "render3d spike: %s\n", error.c_str());
      return false;
    }
  } else {
    build_check_layout(scene);
  }
  return build_mesh(scene);
}

int run_spike(const Options& o) {
  Scene scene;
  if (!load_scene(scene, o)) {
    std::fprintf(stderr, "render3d spike: the mesh could not be built\n");
    return 1;
  }
  std::mutex queue_mutex;
  ui::RuntimeOptions options;
  options.app_name = "hefesto-render3d-spike";
  options.vsync = false;
  options.queue_lock = [&queue_mutex] { queue_mutex.lock(); };
  options.queue_unlock = [&queue_mutex] { queue_mutex.unlock(); };
  std::string error;
  const std::unique_ptr<ui::Runtime> runtime = ui::Runtime::start(options, error);
  if (!runtime) {
    std::fprintf(stderr, "render3d spike: %s\n", error.c_str());
    return 3;
  }
  const hefesto::render::QueueLock lock{options.queue_lock, options.queue_unlock};
  SpikeWindow spike(*runtime, lock, scene);
  const std::string system = spike.window().window_system();
  if (!o.expect_window_system.empty() && system != o.expect_window_system) {
    std::fprintf(stderr, "render3d spike: window system %s, expected %s\n", system.c_str(),
                 o.expect_window_system.c_str());
    return 4;
  }
  return o.bench_rects > 0 ? run_bench(spike, scene, o) : run_check(spike, o);
}

}  // namespace

int main(int argc, char** argv) {
  try {
    return run_spike(parse(argc, argv));
  } catch (const std::exception& e) {
    std::fprintf(stderr, "render3d spike: %s\n", e.what());
    return 3;
  }
}
