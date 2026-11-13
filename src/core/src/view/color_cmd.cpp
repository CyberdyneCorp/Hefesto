// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `color` client's commands `color`, `load`, `pushbutton` and `save`
// (capability ui-color-window, requirement "Color window commands"; Q-UI-37
// press-only `pushbutton`). Messages follow Magic 8.3.694
// cmwind/CMWcmmnds.c, Copyright (C) 1985, 1990 Regents of the University of
// California, used under Magic's permissive license (see
// THIRD_PARTY_NOTICES.md). Owner: VIEW.

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "hefesto/view/color_window.hpp"
#include "hefesto/view/diag_codes.hpp"
#include "view/command_util.hpp"

namespace hefesto::view {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using detail::fail;
using detail::failure;
using detail::print;

constexpr std::string_view kMonitor = "std";  // MainMonType

/// The session and its active color window.
struct ColorTarget {
  editor::EditorState& editor;
  ColorWindow& window;
};

diag::Outcome<ColorTarget> target(const CommandContext& ctx) {
  auto e = detail::session(ctx);
  if (!e) {
    return e.error();
  }
  editor::EditorState& ed = *e.value();
  const auto& point = ed.command_point;
  ColorWindow* w = ed.color_map.active(point ? std::optional<WindowId>(point->window) : std::nullopt);
  if (w == nullptr) {
    return diag::make(codes::kNoColorWindow, "No color window is open.");
  }
  return ColorTarget{ed, *w};
}

/// sscanf("%i"): a C integer prefix (decimal, 0x hex, leading-0 octal).
std::optional<long> c_integer(const std::string& text) {
  char* end = nullptr;
  const long v = std::strtol(text.c_str(), &end, 0);
  return end == text.c_str() ? std::nullopt : std::optional<long>(v);
}

std::array<int, 3> rgb_of(const editor::EditorState& e, int color) {
  if (!e.styles || color < 0 || static_cast<std::size_t>(color) >= e.styles->colors.entries.size()) {
    return {0, 0, 0};
  }
  const auto& c = e.styles->colors.entries[static_cast<std::size_t>(color)];
  return {c.r, c.g, c.b};
}

CommandResult color_usage() {
  return fail(codes::kUsage, "Usage: color [#|next|last|get|rgb]");
}

CommandResult color_report(CommandContext& ctx, const ColorTarget& t) {
  const auto rgb = rgb_of(t.editor, t.window.color);
  char buf[160];
  std::snprintf(buf, sizeof(buf), "Current color is %o octal (%d decimal) (red = %d, green = %d, blue = %d)",
                static_cast<unsigned>(t.window.color), t.window.color, rgb[0], rgb[1], rgb[2]);
  print(ctx, buf);
  return CommandResult::success();
}

/// The `color` keyword forms; nullopt for an index to select.
std::optional<CommandResult> color_keyword(const ColorTarget& t, const std::string& word, long& color) {
  const auto n = static_cast<long>(color_count(t.editor));
  if (std::strncmp(word.c_str(), "next", 4) == 0) {
    color = t.window.color + 1 >= n ? 0 : t.window.color + 1;
  } else if (std::strncmp(word.c_str(), "last", 4) == 0) {
    color = t.window.color - 1 < 0 ? n - 1 : t.window.color - 1;
  } else if (std::strncmp(word.c_str(), "get", 3) == 0) {
    return CommandResult::success(cmd::Value{std::int64_t{t.window.color}});
  } else if (std::strncmp(word.c_str(), "rgb", 3) == 0) {
    const auto rgb = rgb_of(t.editor, t.window.color);
    return CommandResult::success(cmd::Value{cmd::Value::List{rgb[0], rgb[1], rgb[2]}});
  } else {
    return color_usage();
  }
  return std::nullopt;
}

CommandResult color_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = target(ctx);
  if (!t) {
    return failure(t.error());
  }
  if (inv.args.empty()) {
    return color_report(ctx, t.value());
  }
  if (inv.args.size() > 1) {
    return color_usage();
  }
  long color = 0;
  if (const auto n = c_integer(inv.args[0])) {
    color = *n;
  } else if (auto done = color_keyword(t.value(), inv.args[0], color)) {
    return std::move(*done);
  }
  const auto count = static_cast<long>(color_count(t.value().editor));
  if (color < 0 || color >= count) {
    return fail(codes::kColorRange,
                "The colormap only has values from 0 to " + std::to_string(count - 1) + " (decimal).");
  }
  load_color(t.value().window, static_cast<int>(color));
  return CommandResult::success();
}

/// `T.D.M`: the given names, or the technology's style family, the display
/// class of the loaded styles and the monitor type.
std::string cmap_name(const editor::EditorState& e, const std::vector<std::string>& args) {
  if (args.size() == 3) {
    return args[0] + "." + args[1] + "." + args[2];
  }
  const display::DisplayClass cls = e.styles ? e.styles->display_class : display::DisplayClass::opengl;
  return e.technology->styles().style_family + "." + std::string(display::display_class_name(cls)) + "." +
         std::string(kMonitor);
}

bool bad_file_args(const Invocation& inv) {
  return !inv.args.empty() && inv.args.size() != 3;
}

CommandResult file_usage(const Invocation& inv) {
  return fail(codes::kUsage, "Usage: " + inv.name + " [techStyle displayStyle monitorType]");
}

CommandResult load_cmd(CommandContext& ctx, const Invocation& inv) {
  if (bad_file_args(inv)) {
    return file_usage(inv);
  }
  auto t = target(ctx);
  if (!t) {
    return failure(t.error());
  }
  editor::EditorState& e = t.value().editor;
  if (!check_color_map_written(e, ctx.project().compat.magic_compat)) {
    return CommandResult::success();
  }
  const std::string name = cmap_name(e, inv.args);
  std::vector<platform::Path> dirs{"."};
  for (const std::string& d : e.technology->styles().search_dirs) {
    dirs.emplace_back(d);
  }
  for (const platform::Path& dir : dirs) {
    auto text = ctx.services().files.read(dir / (name + ".cmap"));
    if (!text) {
      continue;
    }
    auto map = display::parse_color_map(text.value(), name + ".cmap", ctx.project().compat.magic_compat);
    if (!map) {
      return failure(map.error());
    }
    if (!e.styles) {
      e.styles.emplace();
    }
    e.styles->colors = std::move(map).value();
    e.color_map.modified = false;
    return CommandResult::success();
  }
  return fail(codes::kColorFile, "Couldn't open color map file \"" + name + ".cmap\"");
}

CommandResult save_cmd(CommandContext& ctx, const Invocation& inv) {
  if (bad_file_args(inv)) {
    return file_usage(inv);
  }
  auto t = target(ctx);
  if (!t) {
    return failure(t.error());
  }
  editor::EditorState& e = t.value().editor;
  const std::string file = cmap_name(e, inv.args) + ".cmap";
  const display::ColorMap empty;
  const auto written = ctx.services().files.write_atomic(file, display::write_color_map(e.styles ? e.styles->colors : empty));
  if (!written) {
    return fail(codes::kColorFile, "Can't write color map file \"" + file + "\"");
  }
  e.color_map.modified = false;
  return CommandResult::success();
}

CommandResult pushbutton_cmd(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() != 1) {
    return fail(codes::kUsage, "Usage: pushbutton <button>");
  }
  static constexpr std::array<std::string_view, 3> kButtons = {"left", "middle", "right"};
  const int button = detail::lookup_nocase(inv.args[0], kButtons);
  if (button < 0) {
    return fail(codes::kUsage, "Argument \"button\" must be one of \"left\", \"middle\", or \"right\".");
  }
  auto t = target(ctx);
  if (!t) {
    return failure(t.error());
  }
  // Press only: no release is delivered (Q-UI-37).
  if (const auto& point = t.value().editor.command_point) {
    (void)press_button(t.value().editor, t.value().window, point->point, static_cast<Button>(button));
  }
  return CommandResult::success();
}

}  // namespace

std::vector<cmd::CommandSpec> color_client_commands() {
  return {
      cmd::CommandSpec{"color", "color [color-#]", "specify color to edit, or print current intensities", color_cmd},
      cmd::CommandSpec{"load", "load [techStyle displayStyle monitorType]",
                       "load new color map techStyle.displayStyle.monitorType", load_cmd},
      cmd::CommandSpec{"pushbutton", "pushbutton button", "invoke a button press in the color window",
                       pushbutton_cmd},
      cmd::CommandSpec{"save", "save [techStyle displayStyle monitorType]",
                       "save color map to techStyle.displayStyle.monitorType", save_cmd},
  };
}

}  // namespace hefesto::view
