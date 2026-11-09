// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/options.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>

#include "hefesto/app/startup.hpp"
#include "hefesto/diag/format.hpp"

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
  } else if (name == "--pdk-manager") {
    o.show_pdk_manager = true;
  } else if (name == "--console") {
    o.show_console = true;
  } else {
    return false;
  }
  return true;
}

/// String-valued options; returns nullptr when `name` is not one of them.
std::string* string_option(ShellOptions& o, std::string_view name) {
  if (name == "--expect-window-system") return &o.expect_window_system;
  if (name == "--bench-output") return &o.bench_output;
  if (name == "--capture") return &o.capture;
  if (name == "--asset-dir") return &o.asset_dir;
  if (name == "--run") return &o.run;
  return nullptr;
}

/// Positive (> 0) or non-negative (>= 0) integer options.
bool apply_count(ShellOptions& o, std::string_view name, std::string_view value, bool& known) {
  known = true;
  if (name == "--width") return parse_value(value, o.width) && o.width > 0;
  if (name == "--height") return parse_value(value, o.height) && o.height > 0;
  if (name == "--bench-frames") return parse_value(value, o.bench_frames) && o.bench_frames >= 0;
  if (name == "--smoke-frames") return parse_value(value, o.smoke_frames) && o.smoke_frames >= 0;
  if (name == "--capture-frames") return parse_value(value, o.capture_frames) && o.capture_frames > 0;
  known = false;
  return false;
}

/// A finite number filling all of `text` (strtod: from_chars<double> needs macOS 15+ libc++).
std::optional<double> parse_double(std::string_view text) {
  const std::string copy(text);
  char* end = nullptr;
  const double value = std::strtod(copy.c_str(), &end);
  if (copy.empty() || *end != '\0' || !std::isfinite(value)) return std::nullopt;
  return value;
}

bool apply_valued(ShellOptions& o, std::string_view name, std::string_view value) {
  if (name == "--theme") return parse_theme(value, o.theme);
  if (name == "--pinch") {
    o.pinch = parse_pinch_option(value);
    return o.pinch.has_value();
  }
  if (name == "--rects") return parse_value(value, o.rects);
  if (name == "--seed") return parse_value(value, o.seed);
  if (std::string* text = string_option(o, name)) {
    *text = value;
    return true;
  }
  bool known = false;
  const bool ok = apply_count(o, name, value, known);
  return known && ok;
}

/// Magic options that take a value (app-startup "Command-line options").
bool magic_option_has_value(std::string_view name) {
  return name == "-T" || name == "-d" || name == "-m" || name == "-rcfile";
}

bool is_long_option(std::string_view arg) {
  return arg.size() > 2 && arg.substr(0, 2) == "--";
}

}  // namespace

ParseResult parse_shell_options(std::span<const std::string_view> args) {
  ShellOptions o;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string_view arg = args[i];
    if (apply_flag(o, arg)) {
      continue;
    }
    if (!is_long_option(arg)) {
      // Magic-compatible startup words and cell files go to app-startup.
      o.startup_args.emplace_back(arg);
      if (magic_option_has_value(arg) && i + 1 < args.size()) {
        o.startup_args.emplace_back(args[++i]);
      }
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

std::optional<PinchOption> parse_pinch_option(std::string_view text) {
  const std::size_t a = text.find(',');
  const std::size_t b = a == std::string_view::npos ? a : text.find(',', a + 1);
  if (b == std::string_view::npos) return std::nullopt;
  const auto x = parse_double(text.substr(0, a));
  const auto y = parse_double(text.substr(a + 1, b - a - 1));
  const auto factor = parse_double(text.substr(b + 1));
  if (!x || !y || !factor || !(*factor > 0.0)) return std::nullopt;
  return PinchOption{static_cast<float>(*x), static_cast<float>(*y), *factor};
}

std::string_view shell_usage() noexcept {
  return "usage: hefesto [-T tech] [-r] [-norcfile] [cell.mag ...]\n"
         "               [--theme dark|light] [--width PX] [--height PX] [--asset-dir DIR]\n"
         "               [--pdk-manager] [--console] [--no-vsync] [--validation]\n"
         "               [--run 'CMD; CMD ...'] [--pinch X,Y,FACTOR]\n"
         "               [--capture FILE.ppm [--capture-frames N]]\n"
         "               [--rects N] [--seed N] [--bench-frames N] [--bench-output FILE]\n"
         "               [--smoke-frames N [--expect-window-system x11|wayland|cocoa]]\n";
}

std::string startup_option_error(const diag::Diagnostic& error) {
  std::string text = diag::format_text(error);
  const std::string_view usage = app::startup_usage_line();
  if (text.find(usage) == std::string::npos) {
    text += "\n";
    text += usage;
  }
  return text;
}

}  // namespace hefesto::shell
