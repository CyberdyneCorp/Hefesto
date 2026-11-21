// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `wind3d` command table (view-3d "3D view commands"; design D9):
// `view`, `scroll`, `zoom`, `refresh`, `level`, `defaults`, `closewindow`,
// `help` and `cif` here; `render`, `see`, `cutbox` and `magic::render3d`
// in render.cpp. Argument parsing, usage messages and the `float`
// arithmetic follow Magic 8.3.694 (fc5daa9e) graphics/W3Dmain.c
// (w3dHelp ... w3dZoom, W3Dinit), Copyright (C) 1985, 1990 Regents of the
// University of California, used under Magic's permissive license (see
// THIRD_PARTY_NOTICES.md). As in Magic, a value that is not numeric leaves
// the view unchanged without a message.
//
// Owner: WIND3D (design D3).

#include <array>
#include <climits>
#include <string>
#include <utility>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "view3d/client/internal.hpp"

namespace hefesto::view3d {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using detail::Target;

// The three view values Magic returns as Tcl doubles ((double) of a float).
CommandResult triple(float a, float b, float c) {
  return CommandResult::success(cmd::Value::List{static_cast<double>(a), static_cast<double>(b),
                                                 static_cast<double>(c)});
}

// The fourth word of `view`/`scroll` and the third of `zoom`: `rel*` or
// `abs*` (strncmp of three letters); nullopt for anything else.
std::optional<bool> relative_word(const std::string& word) {
  if (detail::starts3(word, "rel")) {
    return true;
  }
  if (detail::starts3(word, "abs")) {
    return false;
  }
  return std::nullopt;
}

// Three numeric words (StrIsNumeric), or nullopt: then nothing changes.
std::optional<std::array<float, 3>> three_numbers(const std::vector<std::string>& args) {
  const auto x = detail::numeric(args[0]);
  const auto y = detail::numeric(args[1]);
  const auto z = detail::numeric(args[2]);
  if (!x || !y || !z) {
    return std::nullopt;
  }
  return std::array<float, 3>{*x, *y, *z};
}

// The shared parse of `view` and `scroll`: query, set or add three values.
// `apply` receives the numbers and the relative flag.
struct Usages {
  std::string_view keyword;  // a fourth word that is neither rel* nor abs*
  std::string_view count;    // a wrong number of words
};

template <typename Query, typename Apply>
CommandResult three_value_command(CommandContext& ctx, const Invocation& inv, const Usages& usages, Query query,
                                  Apply apply) {
  auto t = detail::target(ctx);
  if (!t.ok()) {
    return CommandResult::failure(t.error());
  }
  std::vector<std::string> args = inv.args;
  bool relative = false;
  if (args.size() == 4) {
    const std::optional<bool> rel = relative_word(args[3]);
    if (!rel) {
      return detail::usage(std::string{usages.keyword});
    }
    relative = *rel;
    args.pop_back();
  }
  if (args.size() == 3) {
    if (const auto v = three_numbers(args)) {
      apply(t.value().window, *v, relative);
      detail::redraw(t.value().editor, t.value().window);
    }
    return CommandResult::success();
  }
  if (args.empty()) {
    return query(t.value().window);
  }
  return detail::usage(std::string{usages.count});
}

// w3dView.
CommandResult view_cmd(CommandContext& ctx, const Invocation& inv) {
  return three_value_command(
      ctx, inv,
      Usages{"Usage: view angle_x angle_y angle_z absolute|relative",
             "Usage: view [angle_x angle_y angle_z [relative|absolute]]"},
      [](const WindowState& w) { return triple(w.view.view_x, w.view.view_y, w.view.view_z); },
      [](WindowState& w, const std::array<float, 3>& v, bool relative) {
        ViewState& s = w.view;
        s.view_x = relative ? s.view_x + v[0] : v[0];
        s.view_y = relative ? s.view_y + v[1] : v[1];
        s.view_z = relative ? s.view_z + v[2] : v[2];
      });
}

// w3dScroll: relative values are in screen units (divided by s_xy).
CommandResult scroll_cmd(CommandContext& ctx, const Invocation& inv) {
  return three_value_command(
      ctx, inv,
      Usages{"Usage: scroll pos_x pos_y pos_z absolute|relative",
             "Usage: scroll [pos_x pos_y pos_z [absolute|relative]]"},
      [](const WindowState& w) { return triple(w.view.trans_x, w.view.trans_y, w.view.trans_z); },
      [](WindowState& w, const std::array<float, 3>& v, bool relative) {
        ViewState& s = w.view;
        if (relative) {
          s.trans_x += v[0] / s.scale_xy;
          s.trans_y += v[1] / s.scale_xy;
          s.trans_z += v[2] / s.scale_xy;
        } else {
          s.trans_x = v[0];
          s.trans_y = v[1];
          s.trans_z = v[2];
        }
      });
}

// `zoom xy z [rel|abs]` with the two words read: positive values only.
CommandResult zoom_set(Target t, const std::string& xy_word, const std::string& z_word, bool relative) {
  const auto xy = detail::numeric(xy_word);
  const auto z = detail::numeric(z_word);
  if (!xy || !z) {
    return CommandResult::success();
  }
  if (*xy <= 0 || *z <= 0) {
    return detail::fail(detail::kZoomValue, "Error: zoom values/factors must be positive and nonzero");
  }
  ViewState& view = t.window.view;
  view.scale_xy = relative ? view.scale_xy * *xy : *xy;
  view.scale_z = relative ? view.scale_z * *z : *z;
  detail::redraw(t.editor, t.window);
  return CommandResult::success();
}

// w3dZoom.
CommandResult zoom_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = detail::target(ctx);
  if (!t.ok()) {
    return CommandResult::failure(t.error());
  }
  std::vector<std::string> args = inv.args;
  bool relative = false;
  if (args.size() == 3) {
    const std::optional<bool> rel = relative_word(args[2]);
    if (!rel) {
      return detail::usage("Usage: zoom scale_xy scale_z relative|absolute");
    }
    relative = *rel;
    args.pop_back();
  }
  if (args.size() == 2) {
    return zoom_set(t.value(), args[0], args[1], relative);
  }
  const ViewState& view = t.value().window.view;
  if (inv.args.empty()) {
    return CommandResult::success(
        cmd::Value::List{static_cast<double>(view.scale_xy), static_cast<double>(view.scale_z)});
  }
  return detail::usage("Usage: zoom [scale_xy scale_z [relative|absolute]]");
}

// A command of no arguments: `run` on the window, else Magic's usage line.
template <typename Run>
CommandResult no_argument_command(CommandContext& ctx, const Invocation& inv, std::string_view name, Run run) {
  auto t = detail::target(ctx);
  if (!t.ok()) {
    return CommandResult::failure(t.error());
  }
  if (!inv.args.empty()) {
    return detail::usage("Usage: " + std::string{name});
  }
  return run(t.value());
}

CommandResult refresh_cmd(CommandContext& ctx, const Invocation& inv) {
  return no_argument_command(ctx, inv, "refresh", [](Target t) {
    detail::redraw(t.editor, t.window);
    return CommandResult::success();
  });
}

// Set3DDefaults: the default view of the current mode (Q-MISC-78), both
// visibility sets reset (Q-MISC-71), no cut box.
CommandResult defaults_cmd(CommandContext& ctx, const Invocation& inv) {
  const bool compat = detail::magic_compat(ctx);
  return no_argument_command(ctx, inv, "defaults", [compat](Target t) {
    detail::set_defaults(t.editor, t.window, compat);
    detail::redraw(t.editor, t.window);
    return CommandResult::success();
  });
}

// w3dClose: the window's `closewindow` (the window takes no window-client
// commands, so the layout one never applies).
CommandResult closewindow_cmd(CommandContext& ctx, const Invocation& inv) {
  return no_argument_command(ctx, inv, "closewindow", [](Target t) {
    static_cast<void>(close_wind3d(t.editor));
    return CommandResult::success();
  });
}

// W3Dinit's table, in WindAddCommand's alphabetical order.
constexpr std::array<std::string_view, 12> kHelp = {
    "cif\t\t\tswitch to/from CIF layers display",
    "closewindow\t\tclose the 3D display",
    "cutbox [none|box|llx lly urx ury]\n\t\t\tset clipping rectangle for 3D view",
    "defaults\t\trevert to defaults",
    "help\t\tprint this command list",
    "level [<n>|up|down]\tset rendering level",
    "refresh\t\trefresh 3D display",
    "render name [height thick [style]]\n\t\t\tproperties of CIF layer rendering",
    "scroll [x y z]\tspecify viewpoint position",
    "see [no] layer\tview or hide layers from the 3D view",
    "view [x y z]\tspecify viewpoint angle",
    "zoom [xy z]\t\tspecify render volume scale",
};

// w3dHelp.
CommandResult help_cmd(CommandContext& ctx, const Invocation& inv) {
  return no_argument_command(ctx, inv, "help", [&ctx](Target) {
    std::string text = "\nWind3D command summary:";
    for (const std::string_view line : kHelp) {
      text += "\n    " + std::string{line};
    }
    text += "\n\nType '?' in the window to get a key macro summary.";
    detail::print(ctx, std::move(text));
    return CommandResult::success();
  });
}

// w3dLevel: `level` returns it; `level n|up|down` sets it, clamped at 0
// (`up` saturates at INT_MAX, where Magic's increment overflows).
CommandResult level_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = detail::target(ctx);
  if (!t.ok()) {
    return CommandResult::failure(t.error());
  }
  int& level = t.value().window.level;
  if (inv.args.empty()) {
    return CommandResult::success(std::int64_t{level});
  }
  if (inv.args.size() != 1) {
    return detail::usage("Usage: level [n]");
  }
  const std::string& word = inv.args[0];
  if (const auto n = detail::integer(word)) {
    level = *n;
  } else if (word == "up") {
    level = level == INT_MAX ? level : level + 1;
  } else if (word == "down") {
    level = level == INT_MIN ? level : level - 1;
  } else {
    return detail::usage("Usage: level [<n>|up|down]");
  }
  level = std::max(level, 0);
  detail::redraw(t.value().editor, t.value().window);
  return CommandResult::success();
}

// w3dToggleCIF with the Q-MISC-73, 76 and 78 rules: the mode is the
// window's own; entering mask-layer mode needs a current output style and
// rescales by its f; leaving it divides by the f of exit_mask_factor.
CommandResult cif_cmd(CommandContext& ctx, const Invocation& inv) {
  const bool compat = detail::magic_compat(ctx);
  return no_argument_command(ctx, inv, "cif", [compat](Target t) {
    WindowState& w = t.window;
    CommandResult result = CommandResult::success();
    if (w.mode == Mode::layout_layer && detail::current_style(t.editor) != nullptr) {
      w.mode = Mode::mask_layer;
      w.applied_factor = current_factor(t.editor);
      rescale(w.view, w.applied_factor);
    } else if (w.mode == Mode::mask_layer) {
      rescale(w.view, inverse_factor(w.exit_mask_factor(compat, current_factor(t.editor))));
      w.mode = Mode::layout_layer;
      w.applied_factor = 1.0F;
    } else {
      result = detail::fail(detail::kNoStyle, std::string{detail::kNoStyleText});
    }
    detail::redraw(t.editor, w);
    return result;
  });
}

struct Entry {
  std::string_view name;
  std::string_view synopsis;
  std::string_view help;
  cmd::Handler handler;
};

}  // namespace

bool register_wind3d_commands(cmd::CommandRegistry& registry) {
  const std::array<Entry, 12> entries = {{
      {"cif", "cif", "Switches the 3D window between mask-layer and layout-layer mode.", cif_cmd},
      {"closewindow", "closewindow", "Closes the 3D window.", closewindow_cmd},
      {"cutbox", "cutbox [none|box|llx lly urx ury]", "Reports or sets the 3D window's clipping rectangle.",
       detail::cutbox_cmd},
      {"defaults", "defaults", "Restores the 3D window's default view.", defaults_cmd},
      {"help", "help", "Lists the 3D window's commands.", help_cmd},
      {"level", "level [<n>|up|down]", "Reports or sets the rendering level.", level_cmd},
      {"refresh", "refresh", "Redraws the 3D window.", refresh_cmd},
      {"render", "render name [height thick [style]]", "Reports or sets the 3D rendering of a mask layer.",
       detail::render_cmd},
      {"scroll", "scroll [x y z [absolute|relative]]", "Reports or sets the 3D viewpoint position.", scroll_cmd},
      {"see", "see [no] layer", "Shows or hides layers in the 3D window.", detail::see_cmd},
      {"view", "view [x y z [absolute|relative]]", "Reports or sets the 3D viewpoint angles.", view_cmd},
      {"zoom", "zoom [xy z [absolute|relative]]", "Reports or sets the 3D render volume scale.", zoom_cmd},
  }};
  bool ok = true;
  for (const Entry& e : entries) {
    ok = !registry
              .add(cmd::CommandSpec{std::string{e.name}, std::string{e.synopsis}, std::string{e.help}, e.handler},
                   kWind3dClient)
              .has_value() &&
         ok;
  }
  ok = !registry
            .add(cmd::CommandSpec{"render3d", "render3d [cell]",
                                  "Opens the 3D window on a cell in its own top-level window (wrapper.tcl).",
                                  detail::render3d_cmd},
                 cmd::kScriptClient)
            .has_value() &&
       ok;
  return ok;
}

}  // namespace hefesto::view3d
