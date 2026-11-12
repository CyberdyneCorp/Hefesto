// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/shell.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <numbers>
#include <optional>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/app/startup.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/gui/wrapper.hpp"
#include "hefesto/platform/session_log.hpp"
#include "hefesto/shell/assets.hpp"
#include "hefesto/render/renderer.hpp"
#include "hefesto/render/synthetic_layout.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/options.hpp"
#include "hefesto/shell/script_consent_model.hpp"
#include "main_window.hpp"
#include "manager_panels.hpp"
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
    window.on_frame();
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
    window.on_frame();
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

/// Capture mode: renders `capture_frames` frames, saves the last one as a
/// binary PPM and exits (screenshots for the conformance record).
int run_capture(ui::Runtime& runtime, MainWindow& window, const ShellOptions& options) {
  int frames = 0;
  runtime.set_frame_callback([&](double) {
    window.on_frame();
    if (++frames >= options.capture_frames) runtime.quit();
  });
  window.set_realtime(true);
  while (runtime.wait_frame(0.0) && window.open()) {
  }
  runtime.set_frame_callback({});
  if (!window.capture(options.capture)) {
    std::fprintf(stderr, "hefesto: could not capture the window to %s\n", options.capture.c_str());
    return 1;
  }
  return 0;
}

/// Set by SIGTERM / SIGHUP: the session saves a crash backup and exits
/// without removing it (cell-io-safety "Termination request").
std::atomic<bool> g_terminate{false};

extern "C" void on_terminate_signal(int) {
  g_terminate.store(true);
}

/// The interactive loop: frames on events, the crash-backup timer polled
/// between them (it bounds the wait), and a termination request answered
/// with `crash save` before leaving.
int run_interactive(ui::Runtime& runtime, MainWindow& window, CoreSession& core) {
  std::signal(SIGTERM, on_terminate_signal);
  std::signal(SIGHUP, on_terminate_signal);
  runtime.set_frame_callback([&window](double) { window.on_frame(); });
  while (window.open() && !g_terminate.load()) {
    const std::optional<double> timer = core.backup_timer_seconds();
    // Wake at least every half second to notice a termination request.
    const double wait = std::min(timer.value_or(0.5), 0.5);
    if (!runtime.wait_frame(wait)) break;
    if (core.poll_backup_timer()) window.on_frame();
  }
  runtime.set_frame_callback({});
  if (g_terminate.load()) window.run_line("crash save");
  std::signal(SIGTERM, SIG_DFL);
  std::signal(SIGHUP, SIG_DFL);
  return 0;
}

/// The core session lives in the per-user app storage (autosave, and the
/// rotating session log in <storage>/logs/hefesto.log).
platform::PortableOptions core_options() {
  return platform::PortableOptions{.storage_root = platform::default_storage_dir(), .pdk_roots = {}};
}

/// Loads Inter and JetBrains Mono (OFL-1.1) and the icons from assets/
/// ("Typography", "Icons"); Raven's own fonts stand in when they are missing.
void load_assets(ui::Runtime& runtime, const ShellOptions& options) {
  std::error_code ec;
  std::filesystem::path exe_dir;
  if (!options.program.empty()) {
    exe_dir = std::filesystem::weakly_canonical(std::filesystem::path(options.program), ec).parent_path();
  }
#ifdef HEFESTO_SOURCE_ASSET_DIR
  const std::filesystem::path source_dir = HEFESTO_SOURCE_ASSET_DIR;
#else
  const std::filesystem::path source_dir;
#endif
  const auto dirs = asset_search_dirs(options.asset_dir, exe_dir, source_dir);
  const auto found = find_asset_dir(dirs);
  if (!found || !runtime.load_assets(*found)) {
    std::fprintf(stderr, "hefesto: the bundled fonts (assets/fonts/Inter-*.ttf, JetBrainsMono-*.ttf) were not found%s; "
                         "using Raven's fonts\n",
                 options.asset_dir.empty() ? "" : (" in " + options.asset_dir).c_str());
  }
}

class ReportSink final : public diag::DiagnosticSink {
 public:
  explicit ReportSink(cmd::Dispatcher& dispatcher) : dispatcher_(dispatcher) {}
  void emit(const diag::Diagnostic& d) override { dispatcher_.report(d); }

 private:
  cmd::Dispatcher& dispatcher_;
};

std::optional<platform::Path> env_path(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') return std::nullopt;
  return platform::Path(value);
}

/// Steps 6-11 of app-startup "Startup sequence" on the window's session:
/// technology, settings layers, startup files, file arguments. Problems are
/// reported to the Diagnostics panel; the window stays open.
void run_startup(CoreSession& core, const app::StartupOptions& startup,
                 std::function<void(script::Interpreter&)> configure_interpreter = {}) {
  app::StartupEnvironment env;
  env.install_root = env_path("CAD_ROOT").value_or(platform::Path());
  env.home = env_path("HOME").value_or(platform::Path());
  std::error_code ec;
  env.working_directory = std::filesystem::current_path(ec);
  env.pdk_root = env_path("PDK_ROOT");
  // hefesto-files-and-pdks: environment and links through the session's
  // install host; the recent-projects list guards `pdk uninstall`.
  env.host = core.install_host();
  env.recent_projects = platform::default_storage_dir() / "recent-projects.json";
  // M4: the window's Tcl console and manager procedures (SHELL-B).
  env.configure_interpreter = std::move(configure_interpreter);
  ReportSink sink(core.dispatcher());
  auto result = app::run_startup(core.dispatcher(), core.project(), core.files(), startup, env, sink);
  if (!result) core.dispatcher().report(result.error());
  // The window is the wrapper frame .layout1: the PDK's wrapper hooks run on
  // it (its device menus, the intercepted File and DRC menu items), as in
  // Magic's Tk wrapper (SHELL-A round 2, open item 1).
  if (core.editor().script.interp != nullptr) {
    with_command_context(core, [](cmd::CommandContext& c) { (void)gui::adopt_wrapper(c, ".layout1"); });
  }
}

/// True when `script` lies under the managed PDK store (a Trust answer is offered).
bool in_pdk_store(CoreSession& core, const platform::Path& script) {
  const pdk::PdkManager* manager = core.pdk_manager();
  if (manager == nullptr || manager->root().empty()) return false;
  // Canonical forms (security finding S-09): a link or `..` must not make a
  // file outside the store look like one of its scripts.
  std::error_code ec;
  const std::string root = std::filesystem::weakly_canonical(manager->root(), ec).string();
  const std::string path = std::filesystem::weakly_canonical(script, ec).string();
  return path.size() > root.size() && path.compare(0, root.size(), root) == 0 &&
         (root.back() == '/' || path[root.size()] == '/');
}

int run_window(const ShellOptions& options) {
  // Magic-compatible startup words: a bad option exits 1 before any window
  // (app-startup "Command-line options").
  std::vector<std::string_view> words(options.startup_args.begin(), options.startup_args.end());
  auto startup = app::parse_startup_options(words);
  if (!startup) {
    std::fprintf(stderr, "%s\n", startup_option_error(startup.error()).c_str());
    return 1;
  }
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
  load_assets(*runtime, options);
  int status = 0;
  {
    CoreSession core(core_options(), platform::default_storage_dir() / "logs", HostServices{true, std::nullopt});
    // M4 consent (hefesto-security "Consent before implicit script
    // execution"; board M4-Script-Consent): PDK scripts run after the
    // window's prompt. Outlives the window, which unregisters it first.
    MainWindow* asking = nullptr;
    DesktopConsentHost consent(
        [&asking](const ConsentPromptView& view) {
          return asking != nullptr ? asking->ask_consent(view) : script::ConsentDecision::decline;
        },
        [&core](const platform::Path& script) { return in_pdk_store(core, script); });
    if (!startup.value().files.empty()) consent.set_document(startup.value().files.front().name);
    MainWindow window(*runtime, options, queue_mutex, core);
    asking = &window;
    window.set_consent_host(&consent);
    run_startup(core, startup.value(), [&window](script::Interpreter& interp) { window.attach_interpreter(interp); });
    // The background DRC checker (m3-drc design D8 "Hosts"): started once
    // the editor is set up; its ready callback posts to Raven's event loop,
    // which commits the square between commands. Stopped before the window.
    DrcPump drc_pump(
        core.editor(), [](std::function<void()> task) { ui::post_task(std::move(task)); },
        [&window](std::size_t committed) { window.on_drc_service(committed); });
    if (!options.run.empty()) window.run_when_ready(options.run);
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
    } else if (!options.capture.empty()) {
      status = run_capture(*runtime, window, options);
    } else {
      status = run_interactive(*runtime, window, core);
    }
    if (!g_terminate.load()) core.end_crash_session();  // normal exit
  }
  return status;
}

}  // namespace

int run(int argc, char** argv) {
  std::vector<std::string_view> args(argv + 1, argv + argc);
  ParseResult parsed = parse_shell_options(args);
  if (parsed.options && argc > 0) parsed.options->program = argv[0];
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
