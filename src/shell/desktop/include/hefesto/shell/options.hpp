// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Command-line options of the desktop app "hefesto".

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/shell/theme_tokens.hpp"

namespace hefesto::shell {

/// A trackpad pinch injected after --run (scripted captures and tests).
struct PinchOption {
  float x = 0.0F;  // window point, logical pixels
  float y = 0.0F;
  double factor = 1.0;  // > 1 spreads the fingers (zooms in)
};

/// A left-button drag injected after --run (scripted captures and tests):
/// press at (x0, y0), move to (x1, y1), release.
struct DragOption {
  float x0 = 0.0F;  // window points, logical pixels
  float y0 = 0.0F;
  float x1 = 0.0F;
  float y1 = 0.0F;
};

struct ShellOptions {
  ThemeMode theme = ThemeMode::dark;
  /// Rectangles of the synthetic spike layout shown in the canvas instead of
  /// a cell (0: none, the default; the benchmark passes --rects).
  std::uint64_t rects = 0;
  std::uint64_t seed = 0x48'45'46'45'53'54'4F'00ULL;
  int width = 1440;
  int height = 900;
  /// > 0: benchmark mode. Pans the canvas for this many frames with Raven's
  /// UI drawn in the same frame, prints a JSON report and exits.
  int bench_frames = 0;
  std::string bench_output;
  /// Saves a rendered frame here (binary PPM) and exits: after the benchmark
  /// in benchmark mode, otherwise after `capture_frames` frames.
  std::string capture;
  int capture_frames = 30;
  /// --capture-running: the capture is taken in the frame a running script
  /// draws when its Interrupt card first shows (board M4-Script-Consent
  /// tile 7), instead of after `capture_frames` frames.
  bool capture_running = false;
  /// > 0: smoke-test mode. Opens the main window through Raven, renders this
  /// many frames, prints the window system and exits (CI runs it under X11
  /// and Wayland).
  int smoke_frames = 0;
  /// Smoke-test mode: exit 4 unless Raven chose this window system
  /// ("x11", "wayland", "cocoa"); empty accepts any.
  std::string expect_window_system;
  /// The app's asset folder (fonts/, icons/); empty: found next to the
  /// executable (see find_asset_dir).
  std::string asset_dir;
  /// Command lines run after startup as if typed in the command bar
  /// (Console echo, Dispatcher, origin `gui`); `;` separates commands
  /// ("load sky130_fd_sc_hd__inv_1; see no metal1; theme light"). Scripted
  /// checks and captures of interactive flows.
  std::string run;
  /// --pinch X,Y,FACTOR: after `run`, a trackpad pinch at window point
  /// (X, Y) through Raven's event injection, as the system sends one; the
  /// app prints the zoom before and after it.
  std::optional<PinchOption> pinch;
  /// --drag X0,Y0,X1,Y1: after `run`, a left-button drag through Raven's
  /// event injection (a panel header to a dock); the app prints the panel
  /// command the drop issued.
  std::optional<DragOption> drag;
  /// Opens the PDK manager panel at startup.
  bool show_pdk_manager = false;
  /// Opens the Console / Diagnostics panel at startup (collapsed otherwise).
  bool show_console = false;
  /// Magic-compatible startup words (capability app-startup "Command-line
  /// options", "Command-line file arguments"): -T tech, -r, -norcfile,
  /// -d type, -m type, -rcfile file, and cell files, in order. Parsed by
  /// app::parse_startup_options.
  std::vector<std::string> startup_args;
  bool vsync = true;
  bool validation = false;
  bool help = false;
  /// The executable's path (argv[0]; not an option): locates assets/.
  std::string program;
};

struct ParseResult {
  std::optional<ShellOptions> options;
  std::string error;  // set when options is empty
};

[[nodiscard]] ParseResult parse_shell_options(std::span<const std::string_view> args);

[[nodiscard]] std::string_view shell_usage() noexcept;

/// "X,Y,FACTOR" (X, Y finite, FACTOR > 0); nullopt otherwise.
[[nodiscard]] std::optional<PinchOption> parse_pinch_option(std::string_view text);
/// `X0,Y0,X1,Y1` (four numbers) for --drag.
[[nodiscard]] std::optional<DragOption> parse_drag_option(std::string_view text);

/// What the app prints for a bad Magic-compatible startup option: the
/// diagnostic as text, followed by Magic's usage line unless the
/// diagnostic's text already ends with it (printed once).
[[nodiscard]] std::string startup_option_error(const diag::Diagnostic& error);

}  // namespace hefesto::shell
