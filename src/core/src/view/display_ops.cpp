// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Display-state commands `expand`, `unexpand`, `see`, `grid`, `redraw`,
// `reset` and `updatedisplay` (capability ui-windows-and-views). Messages
// and argument rules follow Magic 8.3.694 commands/CmdE.c (CmdExpand),
// commands/CmdTZ.c (CmdUnexpand), commands/CmdRS.c (CmdSee),
// commands/CmdFI.c (CmdGrid) and windows/windCmdNR.c / windCmdSZ.c,
// Copyright (C) 1985, 1990 Regents of the University of California, used
// under Magic's permissive license (see THIRD_PARTY_NOTICES.md).

#include <array>
#include <sstream>

#include "hefesto/db/transform.hpp"
#include "hefesto/view/diag_codes.hpp"
#include "view/command_util.hpp"

namespace hefesto::view::detail {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

// --- expand / unexpand -------------------------------------------------------------

enum class Mode : std::uint8_t { selection, surround, overlap, all };

struct ExpandWalk {
  LayoutWindow& window;
  db::Rect box;
  Mode mode;
  bool expand;  // false: unexpand
  bool toggle;
  std::vector<std::string> unavailable;

  [[nodiscard]] bool matches(const db::Rect& bbox) const {
    switch (mode) {
      case Mode::all: return true;
      case Mode::overlap: return db::overlap(bbox, box);
      case Mode::surround: return db::surround(box, bbox);
      case Mode::selection: return false;  // no selection before M2
    }
    return false;
  }

  void act(const db::CellUse& use) {
    if (expand && use.def->flags().not_found) {
      unavailable.push_back(use.def->name());
      return;
    }
    const bool now = window.is_expanded(use);
    window.set_expanded(use, toggle ? !now : expand);
  }

  /// DBExpandAll: acts on every matching instance at any depth, descending
  /// through the instances that are expanded after the action.
  void walk(const db::CellDef& def, const db::Transform& to_root) {
    for (const auto& use : def.uses()) {
      if (!use || use->def == nullptr) {
        continue;
      }
      if (matches(db::apply(to_root, use->bbox))) {
        act(*use);
      }
      if (window.is_expanded(*use)) {
        walk(*use->def, db::compose(to_root, use->transform));
      }
    }
  }
};

struct ExpandArgs {
  Mode mode;
  bool toggle = false;
};

std::optional<ExpandArgs> parse_expand_args(const std::vector<std::string>& args, Mode default_mode, bool allow_toggle) {
  ExpandArgs out{default_mode, false};
  std::vector<std::string> rest = args;
  // Any prefix of "toggle" as the last argument means toggle (Q-UI-19).
  if (allow_toggle && !rest.empty() && !rest.back().empty() && std::string_view("toggle").starts_with(rest.back())) {
    out.toggle = true;
    rest.pop_back();
  }
  if (rest.size() > 1) {
    return std::nullopt;
  }
  if (rest.size() == 1) {
    const int m = lookup(rest[0], {"selection", "surround", "overlap", "all"});
    if (m < 0) {
      return std::nullopt;
    }
    out.mode = static_cast<Mode>(m);
  }
  return out;
}

CommandResult expansion(CommandContext& ctx, const Invocation& inv, bool expand) {
  const auto args = parse_expand_args(inv.args, expand ? Mode::overlap : Mode::surround, expand);
  if (!args) {
    return fail(codes::kUsage, expand ? "Usage: expand [selection|surround|overlap|all] [toggle]"
                                      : "Usage: unexpand [selection|surround|overlap|all]");
  }
  auto t = pointed(ctx);
  if (!t) {
    return failure(t.error());
  }
  LayoutWindow& w = t.value().window;
  const auto box = box_in_window(t.value().editor, w);
  if (!box && args->mode != Mode::all) {
    return fail(codes::kBox, "The box isn't in the same window as the cursor.");
  }
  if (w.root() == nullptr) {
    return CommandResult::success();
  }
  ExpandWalk walk{w, box.value_or(db::Rect{}), args->mode, expand, args->toggle, {}};
  walk.walk(*w.root(), db::kIdentity);
  for (const auto& name : walk.unavailable) {
    ctx.emit(diag::make(codes::kExpand, "Cell " + name + " is unavailable.  It could not be expanded."));
  }
  t.value().editor.windows.notify(w.id(), Change::expansion);
  return CommandResult::success();
}

// --- see ---------------------------------------------------------------------------

struct SeeRequest {
  tech::TypeMask types;
  bool labels = false;
  bool cells = false;
};

diag::Outcome<SeeRequest> parse_see_layers(const tech::Technology& tech, std::string_view list) {
  SeeRequest req;
  std::string rest;
  std::istringstream in{std::string(list)};
  for (std::string word; std::getline(in, word, ',');) {
    if (word == "labels") {
      req.labels = true;
    } else if (word == "subcells") {
      req.cells = true;
    } else {
      rest += (rest.empty() ? "" : ",") + word;
    }
  }
  if (!rest.empty()) {
    auto mask = tech.parse_type_list(rest);
    if (!mask) {
      return diag::make(codes::kSee, mask.error().text);
    }
    req.types = mask.value();
  }
  return req;
}

/// "Each user type SHALL be expanded to its layer mask": the contacts having
/// a requested type as a residue join it.
tech::TypeMask with_contacts(const tech::Technology& tech, tech::TypeMask mask) {
  for (const auto& info : tech.contacts()) {
    if (!info.stacked && !(info.residues & mask).empty()) {
      mask.set(info.type);
    }
  }
  return mask;
}

/// A stacked type follows its component contact on its own home plane.
void follow_stacked(const tech::Technology& tech, tech::TypeMask& visible) {
  for (const auto& info : tech.contacts()) {
    if (!info.stacked) {
      continue;
    }
    const auto home = tech.home_plane(info.type);
    for (const tech::TileType c : {info.component_a, info.component_b}) {
      if (tech.home_plane(c) == home) {
        visible.set(info.type, visible.test(c));
      }
    }
  }
}

void apply_see(const tech::Technology& tech, LayoutWindow& w, const SeeRequest& req, bool show) {
  tech::TypeMask visible = w.visible_types();
  const tech::TypeMask mask = with_contacts(tech, req.types);
  visible = show ? (visible | mask) : (visible & ~mask);
  follow_stacked(tech, visible);
  visible.set(tech::kSpace);
  w.set_visible_types(visible);
  if (req.labels) {
    w.set_labels_visible(show);
  }
  if (req.cells) {
    w.set_cells_visible(show);
  }
}

CommandResult see_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = pointed(ctx, "Point to a layout window first.");
  if (!t) {
    return failure(t.error());
  }
  std::vector<std::string> args = inv.args;
  const bool show = args.empty() || args.front() != "no";
  if (!show) {
    args.erase(args.begin());
  }
  if (args.size() > 1) {
    return fail(codes::kUsage, "Usage: see [no] layers|allSame");
  }
  LayoutWindow& w = t.value().window;
  if (args.size() == 1 && args[0] == "allSame") {
    w.set_all_same(show);
  } else {
    const auto req = args.empty() ? diag::Outcome<SeeRequest>(SeeRequest{tech::TypeMask::all(), true, true})
                                  : parse_see_layers(*t.value().editor.technology, args[0]);
    if (!req) {
      return failure(req.error());
    }
    apply_see(*t.value().editor.technology, w, req.value(), show);
  }
  t.value().editor.windows.notify(w.id(), Change::visibility);
  return CommandResult::success();
}

// --- grid --------------------------------------------------------------------------

constexpr std::string_view kGridUsage = "Usage: grid [xSpacing [ySpacing [xOrig yOrig]]]]";

CommandResult grid_multiple(GridState& g, const Invocation& inv) {
  if (inv.args.size() == 1) {
    return CommandResult::success(g.multiple.value_or(1));
  }
  if (inv.args[1] == "off") {
    g.multiple.reset();
    return CommandResult::success();
  }
  const auto m = parse_int(inv.args[1]);
  if (!m || *m < 1 || *m > 255) {  // rejected without storing (Q-UI-21)
    return fail(codes::kGrid, "Usage: grid multiple <integer value 1-255>");
  }
  g.multiple = static_cast<std::int32_t>(*m);
  return CommandResult::success();
}

CommandResult grid_set(Target& t, const std::vector<std::string>& values) {
  if (values.size() == 3 || values.size() > 4) {
    return fail(codes::kUsage, std::string(kGridUsage));
  }
  const auto& grid = t.editor.grid;
  std::array<db::Coord, 4> v{0, 0, 0, 0};
  for (std::size_t i = 0; i < values.size(); ++i) {
    const auto c = parse_coord(values[i], grid);
    if (!c) {
      return fail(codes::kCoordinate, "Bad coordinate \"" + values[i] + "\"");
    }
    v[i] = *c;
  }
  const db::Coord sx = v[0];
  const db::Coord sy = values.size() >= 2 ? v[1] : sx;
  if (sx <= 0 || sy <= 0) {
    return fail(codes::kGrid, "Grid spacing must be greater than zero.");
  }
  GridState& g = t.window.grid();
  g.spacing = db::Rect{v[2], v[3], v[2] + sx, v[3] + sy};
  g.visible = true;
  return CommandResult::success();
}

CommandResult grid_option(Target& t, int option, const Invocation& inv) {
  GridState& g = t.window.grid();
  const db::Rect& b = g.spacing;
  switch (option) {
    case 0:  // box
    case 7:  // what
      if (option == 0 && inv.args.size() > 1) {
        return grid_set(t, std::vector<std::string>(inv.args.begin() + 1, inv.args.end()));
      }
      return CommandResult::success("Grid unit box is (" + std::to_string(b.x0) + ", " + std::to_string(b.y0) +
                                    ") to (" + std::to_string(b.x1) + ", " + std::to_string(b.y1) + ")");
    case 1:  // help
      return CommandResult::success(std::string(kGridUsage) +
                                    "\nor     grid <option>\nwhere <option> is one of: on, off, state, box, what, "
                                    "help, or multiple.");
    case 2:
      return grid_multiple(g, inv);
    case 3:
    case 4:
      g.visible = option == 4;
      return CommandResult::success();
    case 5:
      return CommandResult::success(std::string("Grid is ") + (g.visible ? "on" : "off"));
    default:  // toggle
      g.visible = !g.visible;
      return CommandResult::success();
  }
}

CommandResult grid_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = pointed(ctx);
  if (!t) {
    return failure(t.error());
  }
  int option = 6;  // toggle
  if (inv.args.size() == 1 && inv.args[0] == "0") {
    option = 3;  // off
  } else if (!inv.args.empty()) {
    option = lookup(inv.args[0], {"box", "help", "multiple", "off", "on", "state", "toggle", "what"});
  }
  const CommandResult result = option < 0 ? grid_set(t.value(), inv.args) : grid_option(t.value(), option, inv);
  t.value().editor.windows.notify(t.value().window.id(), Change::grid);
  return result;
}

// --- redraw / reset / updatedisplay ----------------------------------------------------

void notify_all(editor::EditorState& ed, Change change) {
  ed.windows.for_each([&](LayoutWindow& w) { ed.windows.notify(w.id(), change); });
}

CommandResult redraw_cmd(CommandContext& ctx, const Invocation& inv) {
  auto e = session(ctx);
  if (!e) {
    return failure(e.error());
  }
  if (!inv.args.empty()) {
    return fail(codes::kUsage, "Usage: redraw");
  }
  notify_all(*e.value(), Change::content);
  return CommandResult::success();
}

CommandResult reset_cmd(CommandContext& ctx, const Invocation& inv) {
  if (!inv.args.empty()) {
    return fail(codes::kUsage, "Usage: reset");
  }
  ctx.emit(diag::make(codes::kReset, "reset: the display is managed by the host window system; nothing to reset"));
  return CommandResult::success();
}

CommandResult updatedisplay_cmd(CommandContext& ctx, const Invocation& inv) {
  auto e = session(ctx);
  if (!e) {
    return failure(e.error());
  }
  WindowManager& wm = e.value()->windows;
  const int which = inv.args.size() == 1 ? lookup(inv.args[0], {"suspend", "resume"}) : (inv.args.empty() ? 2 : -2);
  if (which < 0) {
    return fail(codes::kUsage, "Usage: updatedisplay [suspend|resume]");
  }
  if (which != 2) {
    wm.set_display_suspended(which == 0);
  }
  if (!wm.display_suspended() && which != 0) {
    notify_all(*e.value(), Change::content);
  }
  return CommandResult::success();
}

}  // namespace

bool register_display_ops(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "expand", "expand [selection|surround|overlap|all] [toggle]",
                "Expands (or toggles) the instances under the box in this window.",
                [](CommandContext& c, const Invocation& i) { return expansion(c, i, true); });
  ok = add(registry, "unexpand", "unexpand [selection|surround|overlap|all]",
           "Unexpands the instances under the box in this window.",
           [](CommandContext& c, const Invocation& i) { return expansion(c, i, false); }) &&
       ok;
  ok = add(registry, "see", "see [no] layers|allSame", "Shows or hides layers, labels and subcells in the window.",
           see_cmd) &&
       ok;
  ok = add(registry, "grid", "grid [xSpacing [ySpacing [xOrig yOrig]]] | grid on|off|state|box|what|multiple|help",
           "Shows, hides or sets the window grid.", grid_cmd) &&
       ok;
  ok = add(registry, "redraw", "redraw", "Redraws every window.", redraw_cmd) && ok;
  ok = add(registry, "reset", "reset", "Re-initializes the display (no effect on a host-managed display).", reset_cmd) &&
       ok;
  ok = add(registry, "updatedisplay", "updatedisplay [suspend|resume]", "Updates, suspends or resumes redisplay.",
           updatedisplay_cmd) &&
       ok;
  return ok;
}

}  // namespace hefesto::view::detail
