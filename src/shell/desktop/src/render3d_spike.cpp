// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-render3d-spike: the M10 renderer spike (openspec/changes/
// m10-3d-view tasks.md 5.1, design D8, risk R1). It renders the depth-tested
// test mesh (hefesto/render/mesh3d_spike.hpp) offscreen on Raven's Vulkan
// device, shows it in a Raven viewport in texture mode, resizes the window,
// and checks the window's pixels after each step:
//
//   hefesto-render3d-spike [--capture PREFIX] [--expect-window-system x11|wayland|cocoa]
//
// Exit codes: 0 the pixels match, 1 a pixel check failed, 3 Raven or Vulkan
// could not start, 4 Raven picked another window system. With --capture the
// two frames are written to PREFIX-1.ppm and PREFIX-2.ppm.
//
// Owner: RENDER3D (spike; the 3D tab itself belongs to SHELL, task 9.1).

#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/render/mesh3d_spike.hpp"
#include "hefesto/render/mesh3d_target.hpp"
#include "raven_ui.hpp"

namespace {

using hefesto::render::Mesh3dSpike;
using hefesto::render::VulkanOffscreenTarget;
namespace ui = hefesto::shell::ui;

struct Options {
  std::string capture;
  std::string expect_window_system;
};

Options parse(int argc, char** argv) {
  Options o;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string_view name = argv[i];
    if (name == "--capture") o.capture = argv[i + 1];
    if (name == "--expect-window-system") o.expect_window_system = argv[i + 1];
  }
  return o;
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

// The spike mesh in window fractions (clip x, y -> (c + 1) / 2). The UNORM
// view passes the sRGB-encoded values to the UI unchanged, so the background
// (linear 0.2158605) reads 128.
constexpr Probe kProbes[] = {
    {"near red only", 0.125F, 0.625F, {255, 0, 0}},
    {"red over far green (depth test)", 0.5F, 0.625F, {255, 0, 0}},
    {"far green only", 0.875F, 0.625F, {0, 255, 0}},
    {"coplanar blue over red (LESS_OR_EQUAL)", 0.25F, 0.125F, {0, 0, 255}},
    {"background (back face culled)", 0.5F, 0.95F, {128, 128, 128}},
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

/// The spike's window: a viewport in texture mode that redraws the test mesh
/// whenever the viewport's pixel size changes (the resize callback).
class SpikeWindow {
 public:
  SpikeWindow(ui::Runtime& runtime, const hefesto::render::QueueLock& lock)
      : runtime_(runtime),
        window_("Hefesto 3D renderer spike", 640, 400),
        viewport_(ui::Viewport::create()),
        target_(runtime.vulkan_device(), 1, 1, lock),
        spike_(target_) {
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

  /// Runs `frames` Raven frames, redrawing the 3D image when needed.
  void run(int frames) {
    for (int i = 0; i < frames; ++i) {
      if (dirty_) redraw();
      runtime_.wait_frame(0.05);
    }
  }

  [[nodiscard]] std::uint64_t frames_drawn() const noexcept { return target_.frame_count(); }
  [[nodiscard]] int target_width() const noexcept { return target_.width(); }
  [[nodiscard]] int target_height() const noexcept { return target_.height(); }

 private:
  void redraw() {
    spike_.draw(target_);
    // Re-point the viewport before the next frame draws into the buffer it showed.
    const hefesto::render::ShownImage shown = target_.shown();
    viewport_->show_vulkan_image(shown.view, shown.width, shown.height);
    dirty_ = false;
  }

  ui::Runtime& runtime_;
  ui::Window window_;
  std::unique_ptr<ui::Viewport> viewport_;
  VulkanOffscreenTarget target_;
  Mesh3dSpike spike_;
  bool dirty_ = true;
};

bool capture_and_check(SpikeWindow& spike, const Options& o, int step) {
  int width = 0;
  int height = 0;
  const std::vector<std::uint8_t> rgba = spike.window().capture_rgba(width, height);
  if (!o.capture.empty() && !rgba.empty()) {
    write_ppm(o.capture + "-" + std::to_string(step) + ".ppm", rgba, width, height);
  }
  const bool ok = check_capture(rgba, width, height, step);
  std::printf("render3d spike: step %d: window %dx%d, target %dx%d, %llu frames drawn: %s\n", step, width, height,
              spike.target_width(), spike.target_height(), static_cast<unsigned long long>(spike.frames_drawn()),
              ok ? "ok" : "FAILED");
  return ok;
}

int run_spike(const Options& o) {
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
  SpikeWindow spike(*runtime, lock);
  const std::string system = spike.window().window_system();
  if (!o.expect_window_system.empty() && system != o.expect_window_system) {
    std::fprintf(stderr, "render3d spike: window system %s, expected %s\n", system.c_str(),
                 o.expect_window_system.c_str());
    return 4;
  }
  spike.run(20);
  const bool first = capture_and_check(spike, o, 1);
  spike.window().set_size(900, 300);
  spike.run(20);
  const bool second = capture_and_check(spike, o, 2);
  return first && second ? 0 : 1;
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
