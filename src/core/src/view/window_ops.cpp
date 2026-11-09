// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Window commands `openwindow`, `closewindow`, `windownames`,
// `windowpositions`, `windowcaption`, `windowborder`, `windowscrollbars`,
// `over`, `underneath` and `crosshair` (capability ui-windows-and-views).
// Messages follow Magic 8.3.694 windows/windCmdAM.c, windows/windCmdNR.c,
// windows/windCmdSZ.c and commands/CmdCD.c, Copyright (C) 1985, 1990
// Regents of the University of California, used under Magic's permissive
// license (see THIRD_PARTY_NOTICES.md).

#include "hefesto/view/diag_codes.hpp"
#include "view/command_util.hpp"

namespace hefesto::view::detail {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

constexpr std::int32_t kCreateSize = 300;  // CREATE_WIDTH / CREATE_HEIGHT
constexpr std::string_view kUnnamed = "(UNNAMED)";

/// A 300x300 frame centred on the screen (no command point in the core).
ScreenRect create_frame() {
  const std::int32_t cx = (kNullScreen.x0 + kNullScreen.x1) / 2;
  const std::int32_t cy = (kNullScreen.y0 + kNullScreen.y1) / 2;
  return ScreenRect{cx - kCreateSize / 2, cy - kCreateSize / 2, cx + kCreateSize / 2, cy + kCreateSize / 2};
}

/// The definition named `name`, created empty and marked not found when the
/// library has none (reading it from disk is CELL's `load`).
diag::Outcome<db::CellDef*> cell_named(editor::EditorState& e, std::string_view name) {
  if (db::CellDef* def = e.cells.find(name)) {
    return def;
  }
  auto made = e.cells.create(name, e.technology->plane_count());
  if (made && name != kUnnamed) {
    made.value()->flags().not_found = true;
  }
  return made;
}

CommandResult openwindow_cmd(CommandContext& ctx, const Invocation& inv) {
  auto e = session(ctx);
  if (!e) {
    return failure(e.error());
  }
  editor::EditorState& ed = *e.value();
  const bool on_box = inv.args.empty() && ed.box && ed.edit_cell != nullptr;
  auto root = cell_named(ed, !inv.args.empty() ? std::string_view(inv.args[0])
                                               : (on_box ? std::string_view(ed.edit_cell->name()) : kUnnamed));
  if (!root) {
    return failure(root.error());
  }
  auto opened = ed.windows.open_frame(root.value(), create_frame());
  if (!opened) {
    diag::Diagnostic d = opened.error();
    d.text += "\nCould not create window";  // windOpenCmd after WindCreate fails
    return failure(d);
  }
  if (on_box) {
    opened.value()->move_to(box_frame(*ed.box));
  }
  if (inv.args.size() > 1) {
    opened.value()->set_name(inv.args[1]);
  }
  if (ed.edit_cell == nullptr && !root.value()->flags().no_edit) {
    ed.edit_cell = root.value();
  }
  refresh_captions(ed);
  return CommandResult::success();
}

LayoutWindow* window_named(const editor::EditorState& e, const std::string& name) {
  if (LayoutWindow* w = e.windows.find_named(name)) {
    return w;
  }
  const auto id = parse_int(name);
  return id ? e.windows.find(static_cast<WindowId>(*id)) : nullptr;
}

CommandResult closewindow_cmd(CommandContext& ctx, const Invocation& inv) {
  auto e = session(ctx);
  if (!e) {
    return failure(e.error());
  }
  editor::EditorState& ed = *e.value();
  LayoutWindow* w = inv.args.empty() ? ed.windows.current() : window_named(ed, inv.args[0]);
  if (w == nullptr) {
    return inv.args.empty() ? fail(codes::kNoWindow, "Point to a window first")
                            : fail(codes::kWindowUnknown, "Window named " + inv.args[0] + " cannot be found");
  }
  const auto closed = ed.windows.close(w->id());
  return closed ? CommandResult::success() : failure(closed.error());
}

// windNamesCmd, Tcl build (Magic ref: windows/windCmdSZ.c:481-560): the
// window id (no name pointer with a host-managed display), or the list of
// every window's id from the top window down.
CommandResult windownames_cmd(CommandContext& ctx, const Invocation& inv) {
  auto e = session(ctx);
  if (!e) {
    return failure(e.error());
  }
  const WindowManager& wm = e.value()->windows;
  if (inv.args.size() > 1 || (inv.args.size() == 1 && !inv.args[0].starts_with("all") &&
                              lookup(inv.args[0], {"layout"}) < 0)) {
    return fail(codes::kUsage, "Usage:  windownames [all | client_type]");
  }
  if (inv.args.empty() && wm.size() == 1) {
    return CommandResult::success(cmd::Value{std::int64_t{wm.current()->id()}});
  }
  cmd::Value::List ids;
  wm.for_each([&ids](LayoutWindow& w) { ids.emplace_back(std::int64_t{w.id()}); });
  return CommandResult::success(cmd::Value{std::move(ids)});
}

// windPositionsCmd. Magic's Tcl build returns only the last window's list;
// Hefesto returns one list per window (Q-UI-17, X-2 tier 1).
CommandResult windowpositions_cmd(CommandContext& ctx, const Invocation& inv) {
  auto e = session(ctx);
  if (!e) {
    return failure(e.error());
  }
  std::vector<std::string> args = inv.args;
  const bool frame = !args.empty() && args.front().starts_with("frame");
  if (frame) {
    args.erase(args.begin());
  }
  if (args.size() > 1) {
    return fail(codes::kUsage, "Usage:  windowpositions [file]");
  }
  std::string text;
  cmd::Value::List lists;
  e.value()->windows.for_each([&](LayoutWindow& w) {
    const ScreenRect r = frame ? w.frame() : w.mapping().screen;
    text += "specialopen " + std::to_string(r.x0) + " " + std::to_string(r.y0) + " " + std::to_string(r.x1) + " " +
            std::to_string(r.y1) + " layout\n";
    lists.emplace_back(cmd::Value::List{r.x0, r.y0, r.x1, r.y1, "layout"});
  });
  if (args.empty()) {
    return CommandResult::success(std::move(lists));
  }
  const auto written = ctx.services().files.write_atomic(args.front(), text);
  return written ? CommandResult::success() : failure(written.error());
}

/// `windowborder|windowcaption|windowscrollbars on|off`: the session default.
struct ChromeCommand {
  std::string_view name;
  bool ChromeFlags::*flag;
  std::string_view on_text;
  std::string_view off_text;
};

constexpr ChromeCommand kBorder{"windowborder", &ChromeFlags::border, "New windows will have a border.",
                                "New windows will not have a border."};
constexpr ChromeCommand kCaption{"windowcaption", &ChromeFlags::caption, "New windows will have a title caption.",
                                 "New windows will not have a title caption."};
constexpr ChromeCommand kScrollbars{"windowscrollbars", &ChromeFlags::scrollbars, "New windows will have scroll bars.",
                                    "New windows will not have scroll bars."};

CommandResult set_chrome(CommandContext& ctx, editor::EditorState& ed, const ChromeCommand& c,
                         const std::string& value) {
  const int which = lookup(value, {"on", "off"});
  if (which < 0) {
    return fail(codes::kUsage, "Usage: " + std::string(c.name) + " [on|off]");
  }
  ed.windows.chrome_defaults().*c.flag = which == 0;
  print(ctx, std::string(which == 0 ? c.on_text : c.off_text));  // TxPrintf; no Tcl result
  return CommandResult::success();
}

/// The query form (no argument) of windowborder / windowcaption.
CommandResult query_chrome(editor::EditorState& ed, const ChromeCommand& c) {
  LayoutWindow* w = ed.windows.current();
  if (w == nullptr) {
    return fail(codes::kNoWindow, "No window specified for caption command\nUsage: " + std::string(c.name) + " [on|off]");
  }
  // Tcl build: the caption itself, or "on"/"off" (windows/windCmdAM.c:81-210).
  if (c.flag == &ChromeFlags::caption) {
    refresh_captions(ed);
    return CommandResult::success(w->caption());
  }
  return CommandResult::success(std::string(w->chrome().*c.flag ? "on" : "off"));
}

cmd::Handler chrome_handler(const ChromeCommand& c) {
  return [&c](CommandContext& ctx, const Invocation& inv) {
    auto e = session(ctx);
    if (!e) {
      return failure(e.error());
    }
    const bool query_ok = c.flag != &ChromeFlags::scrollbars;
    if (inv.args.size() > 1 || (inv.args.empty() && !query_ok)) {
      return fail(codes::kUsage, "Usage: " + std::string(c.name) + (query_ok ? " [on|off]" : " on|off"));
    }
    return inv.args.empty() ? query_chrome(*e.value(), c) : set_chrome(ctx, *e.value(), c, inv.args[0]);
  };
}

cmd::Handler stacking_handler(bool front) {
  return [front](CommandContext& ctx, const Invocation& inv) {
    auto t = pointed(ctx, "Point to a window first.");
    if (!t) {
      return failure(t.error());
    }
    if (!inv.args.empty()) {
      return fail(codes::kUsage, front ? "Usage: over" : "Usage: underneath");
    }
    const WindowId id = t.value().window.id();
    front ? t.value().editor.windows.raise(id) : t.value().editor.windows.lower(id);
    t.value().editor.windows.notify(id, Change::view);
    return CommandResult::success();
  };
}

CommandResult crosshair_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = pointed(ctx);
  if (!t) {
    return failure(t.error());
  }
  WindowManager& wm = t.value().editor.windows;
  if (inv.args.size() == 1 && inv.args[0] == "off") {
    wm.set_crosshair(nullptr, std::nullopt);
    return CommandResult::success();
  }
  const auto& grid = t.value().editor.grid;
  const auto x = inv.args.size() == 2 ? parse_coord(inv.args[0], grid) : std::nullopt;
  const auto y = inv.args.size() == 2 ? parse_coord(inv.args[1], grid) : std::nullopt;
  if (!x || !y) {
    return fail(codes::kUsage, "Usage: crosshair off|x y");
  }
  wm.set_crosshair(t.value().window.root(), db::Point{*x, *y});
  return CommandResult::success();
}

}  // namespace

bool register_window_ops(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "openwindow", "openwindow [cell] [name]", "Opens a layout window.", openwindow_cmd);
  ok = add(registry, "closewindow", "closewindow [name]", "Closes a layout window.", closewindow_cmd) && ok;
  ok = add(registry, "windownames", "windownames [all|type]", "Lists window ids and names.", windownames_cmd) && ok;
  ok = add(registry, "windowpositions", "windowpositions [frame] [file]", "Lists window positions.",
           windowpositions_cmd) &&
       ok;
  ok = add(registry, "windowcaption", "windowcaption [on|off]",
           "Reports the window caption, or sets whether new windows have one.", chrome_handler(kCaption)) &&
       ok;
  ok = add(registry, "windowborder", "windowborder [on|off]", "Reports or sets the border of new windows.",
           chrome_handler(kBorder)) &&
       ok;
  ok = add(registry, "windowscrollbars", "windowscrollbars on|off", "Sets whether new windows have scroll bars.",
           chrome_handler(kScrollbars)) &&
       ok;
  ok = add(registry, "over", "over", "Moves the window to the front.", stacking_handler(true)) && ok;
  ok = add(registry, "underneath", "underneath", "Moves the window to the back.", stacking_handler(false)) && ok;
  ok = add(registry, "crosshair", "crosshair x y | off", "Shows or removes the crosshair.", crosshair_cmd) && ok;
  return ok;
}

}  // namespace hefesto::view::detail
