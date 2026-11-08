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

#include "hefesto/shell/theme_tokens.hpp"

namespace hefesto::shell {

struct ShellOptions {
  ThemeMode theme = ThemeMode::dark;
  /// Rectangles of the synthetic spike layout shown in the canvas (0: none).
  std::uint64_t rects = 1'000'000;
  std::uint64_t seed = 0x48'45'46'45'53'54'4F'00ULL;
  int width = 1440;
  int height = 900;
  /// > 0: benchmark mode. Pans the canvas for this many frames with Raven's
  /// UI drawn in the same frame, prints a JSON report and exits.
  int bench_frames = 0;
  std::string bench_output;
  /// Benchmark mode: saves the last frame here (binary PPM).
  std::string capture;
  /// > 0: smoke-test mode. Opens the main window through Raven, renders this
  /// many frames, prints the window system and exits (CI runs it under X11
  /// and Wayland).
  int smoke_frames = 0;
  /// Smoke-test mode: exit 4 unless Raven chose this window system
  /// ("x11", "wayland", "cocoa"); empty accepts any.
  std::string expect_window_system;
  /// Folder holding Inter and JetBrains Mono .ttf files; empty: Raven's
  /// bundled Roboto / Roboto Mono are used.
  std::string font_dir;
  bool vsync = true;
  bool validation = false;
  bool help = false;
};

struct ParseResult {
  std::optional<ShellOptions> options;
  std::string error;  // set when options is empty
};

[[nodiscard]] ParseResult parse_shell_options(std::span<const std::string_view> args);

[[nodiscard]] std::string_view shell_usage() noexcept;

}  // namespace hefesto::shell
