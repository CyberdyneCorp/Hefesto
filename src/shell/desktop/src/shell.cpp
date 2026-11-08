// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/shell.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/platform/session_log.hpp"
#include "hefesto/render/renderer.hpp"
#include "hefesto/render/synthetic_layout.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/options.hpp"
#include "main_window.hpp"
#include "raven_ui.hpp"

namespace hefesto::shell {

namespace {

using Clock = std::chrono::steady_clock;

double percentile(std::vector<double> v, double p) {
  if (v.empty()) {
    return 0.0;
  }
  std::sort(v.begin(), v.end());
  const double rank = std::ceil(p / 100.0 * static_cast<double>(v.size()));
  return v[static_cast<std::size_t>(std::clamp(rank, 1.0, static_cast<double>(v.size()))) - 1];
}

nlohmann::json summarize(const std::vector<double>& ms) {
  return {{"frames", ms.size()},
          {"median_ms", percentile(ms, 50.0)},
          {"p95_ms", percentile(ms, 95.0)},
          {"p99_ms", percentile(ms, 99.0)},
          {"max_ms", percentile(ms, 100.0)}};
}

/// Benchmark mode: pans along a loop for `frames` frames with the whole UI
/// drawn every frame, and records the frame-to-frame interval and Raven's
/// own per-frame timings (hefesto-performance, "Interactive frame rate").
struct UiBench {
  MainWindow& window;
  ui::Runtime& runtime;
  int frames = 0;
  int done = 0;
  Clock::time_point last;
  std::vector<double> interval_ms;
  std::vector<double> raven_render_ms;
  std::vector<double> raven_layout_paint_ms;

  void on_frame() {
    const Clock::time_point now = Clock::now();
    if (done > 0) {
      interval_ms.push_back(std::chrono::duration<double, std::milli>(now - last).count());
      const ui::FrameStats s = window.frame_stats();
      raven_render_ms.push_back(s.render_ms);
      raven_layout_paint_ms.push_back(s.layout_ms + s.paint_ms);
    }
    last = now;
    const double a0 = 2.0 * std::numbers::pi * done / frames;
    const double a1 = 2.0 * std::numbers::pi * (done + 1) / frames;
    window.pan_pixels(300.0 * (std::cos(a1) - std::cos(a0)), 300.0 * (std::sin(a1) - std::sin(a0)));
    if (++done > frames) {
      runtime.quit();
    }
  }

  [[nodiscard]] nlohmann::json report(const ShellOptions& o) const {
    const double median = percentile(interval_ms, 50.0);
    return {{"benchmark", "hefesto-shell-canvas"},
            {"mode", "raven-viewport-draw-callback"},
            {"raven", runtime.version()},
            {"rectangles", o.rects},
            {"canvas_instances", window.canvas_instances()},
            {"vsync", o.vsync},
            {"window", {{"width", o.width}, {"height", o.height}}},
            {"frame_interval", summarize(interval_ms)},
            {"raven_render", summarize(raven_render_ms)},
            {"raven_layout_paint", summarize(raven_layout_paint_ms)},
            {"target_median_ms", 16.7},
            {"meets_target", median <= 16.7}};
  }
};

int run_bench(ui::Runtime& runtime, MainWindow& window, const ShellOptions& options) {
  UiBench bench{window, runtime, options.bench_frames, 0, Clock::now(), {}, {}, {}};
  window.set_realtime(true);
  runtime.set_frame_callback([&bench](double) { bench.on_frame(); });
  while (runtime.wait_frame(0.0) && window.open()) {
  }
  runtime.set_frame_callback({});
  if (!options.capture.empty() && !window.capture(options.capture)) {
    std::fprintf(stderr, "hefesto: could not capture the window to %s\n", options.capture.c_str());
  }
  const std::string text = bench.report(options).dump(2);
  std::cout << text << '\n';
  if (!options.bench_output.empty()) {
    std::ofstream(options.bench_output) << text << '\n';
  }
  return 0;
}

/// Smoke-test mode: renders `options.smoke_frames` frames of the main window
/// and checks which window system Raven chose. Exit 0, or 4 on a mismatch.
int run_smoke(ui::Runtime& runtime, MainWindow& window, const ShellOptions& options) {
  int frames = 0;
  runtime.set_frame_callback([&](double) {
    if (++frames >= options.smoke_frames) {
      runtime.quit();
    }
  });
  window.set_realtime(true);
  while (runtime.wait_frame(0.0) && window.open()) {
  }
  runtime.set_frame_callback({});
  const std::string system = window.window_system();
  std::printf("hefesto: smoke test: %d frames on %s (Raven %s)\n", frames, system.c_str(),
              runtime.version().c_str());
  if (frames < options.smoke_frames) {
    std::fprintf(stderr, "hefesto: smoke test: the window closed after %d of %d frames\n", frames,
                 options.smoke_frames);
    return 4;
  }
  if (!options.expect_window_system.empty() && system != options.expect_window_system) {
    std::fprintf(stderr, "hefesto: smoke test: expected window system %s, Raven chose %s\n",
                 options.expect_window_system.c_str(), system.c_str());
    return 4;
  }
  return 0;
}

/// The core session lives in the per-user app storage (autosave, and the
/// rotating session log in <storage>/logs/hefesto.log).
platform::PortableOptions core_options() {
  return platform::PortableOptions{.storage_root = platform::default_storage_dir(), .pdk_roots = {}};
}

int run_window(const ShellOptions& options) {
  // Shared by Raven's render thread and the canvas renderer's uploads.
  std::mutex queue_mutex;
  ui::RuntimeOptions rt_options;
  rt_options.vsync = options.vsync;
  rt_options.validation = options.validation;
  rt_options.queue_lock = [&queue_mutex] { queue_mutex.lock(); };
  rt_options.queue_unlock = [&queue_mutex] { queue_mutex.unlock(); };
  std::string error;
  auto runtime = ui::Runtime::start(std::move(rt_options), error);
  if (!runtime) {
    std::fprintf(stderr, "hefesto: %s\n", error.c_str());
    return 3;
  }
  int status = 0;
  {
    CoreSession core(core_options(), platform::default_storage_dir() / "logs");
    MainWindow window(*runtime, options, queue_mutex, core);
    if (options.rects > 0) {
      render::SyntheticLayoutParams params;
      params.count = options.rects;
      params.seed = options.seed;
      window.show_layout(render::generate_synthetic_layout(params));
    }
    if (options.smoke_frames > 0) {
      status = run_smoke(*runtime, window, options);
    } else if (options.bench_frames > 0) {
      status = run_bench(*runtime, window, options);
    } else {
      while (runtime->wait_frame(-1.0) && window.open()) {
      }
    }
  }
  return status;
}

}  // namespace

int run(int argc, char** argv) {
  std::vector<std::string_view> args(argv + 1, argv + argc);
  const ParseResult parsed = parse_shell_options(args);
  if (!parsed.options) {
    std::fprintf(stderr, "hefesto: %s\n%s", parsed.error.c_str(), shell_usage().data());
    return 2;
  }
  if (parsed.options->help) {
    std::fputs(shell_usage().data(), stdout);
    return 0;
  }
  try {
    return run_window(*parsed.options);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "hefesto: %s\n", e.what());
    return 1;
  }
}

}  // namespace hefesto::shell
