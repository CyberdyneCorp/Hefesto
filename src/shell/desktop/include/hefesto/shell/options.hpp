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

#include "hefesto/shell/theme_tokens.hpp"

namespace hefesto::shell {

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

}  // namespace hefesto::shell
