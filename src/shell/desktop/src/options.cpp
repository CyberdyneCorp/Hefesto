// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/options.hpp"

#include <charconv>

namespace hefesto::shell {

namespace {

template <typename T>
bool parse_value(std::string_view text, T& out) {
  T value{};
  const auto* end = text.data() + text.size();
  const auto [ptr, ec] = std::from_chars(text.data(), end, value);
  if (ec != std::errc{} || ptr != end) {
    return false;
  }
  out = value;
  return true;
}

bool parse_theme(std::string_view text, ThemeMode& out) {
  if (text == "dark" || text == "light") {
    out = text == "light" ? ThemeMode::light : ThemeMode::dark;
    return true;
  }
  return false;
}

/// Options without a value; returns false when `name` is not one of them.
bool apply_flag(ShellOptions& o, std::string_view name) {
  if (name == "--help" || name == "-h") {
    o.help = true;
  } else if (name == "--no-vsync") {
    o.vsync = false;
  } else if (name == "--validation") {
    o.validation = true;
  } else {
    return false;
  }
  return true;
}

bool apply_valued(ShellOptions& o, std::string_view name, std::string_view value) {
  if (name == "--theme") return parse_theme(value, o.theme);
  if (name == "--rects") return parse_value(value, o.rects);
  if (name == "--seed") return parse_value(value, o.seed);
  if (name == "--width") return parse_value(value, o.width) && o.width > 0;
  if (name == "--height") return parse_value(value, o.height) && o.height > 0;
  if (name == "--bench-frames") return parse_value(value, o.bench_frames) && o.bench_frames >= 0;
  if (name == "--smoke-frames") return parse_value(value, o.smoke_frames) && o.smoke_frames >= 0;
  if (name == "--expect-window-system") {
    o.expect_window_system = value;
    return true;
  }
  if (name == "--bench-output") {
    o.bench_output = value;
    return true;
  }
  if (name == "--capture") {
    o.capture = value;
    return true;
  }
  if (name == "--font-dir") {
    o.font_dir = value;
    return true;
  }
  return false;
}

}  // namespace

ParseResult parse_shell_options(std::span<const std::string_view> args) {
  ShellOptions o;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string_view arg = args[i];
    if (apply_flag(o, arg)) {
      continue;
    }
    if (i + 1 >= args.size()) {
      return {std::nullopt, "missing value or unknown option: " + std::string(arg)};
    }
    if (!apply_valued(o, arg, args[i + 1])) {
      return {std::nullopt, "bad option: " + std::string(arg) + " " + std::string(args[i + 1])};
    }
    ++i;
  }
  return {o, {}};
}

std::string_view shell_usage() noexcept {
  return "usage: hefesto [--theme dark|light] [--rects N] [--seed N] [--width PX] [--height PX]\n"
         "               [--font-dir DIR] [--no-vsync] [--validation]\n"
         "               [--bench-frames N] [--bench-output FILE] [--capture FILE.ppm]\n"
         "               [--smoke-frames N [--expect-window-system x11|wayland|cocoa]]\n";
}

}  // namespace hefesto::shell
