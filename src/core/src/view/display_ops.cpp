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
#include <unordered_set>
#include <vector>

#include "hefesto/cell/hier.hpp"
#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/db/array_overlap.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/view/diag_codes.hpp"
#include "view/command_util.hpp"

namespace hefesto::view::detail {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

// --- expand / unexpand -------------------------------------------------------------

enum class Mode : std::uint8_t { selection, surround, overlap, all };

/// The types an expansion change redisplays (cmdExpandFunc: DBAllButSpaceBits).
tech::TypeMask all_but_space() {
  tech::TypeMask m = tech::TypeMask::all();
  m.set(tech::kSpace, false);
  return m;
}

/// One expansion change: the bit, the cell read when it is expanded and
/// unavailable, and cmdExpandFunc's redisplay of the instance in the window.
/// Returns false when the cell is unavailable (not expanded).
bool set_expansion(LayoutWindow& window, WindowManager& wm, const db::CellUse& use, bool next, CommandContext& ctx) {
  if (next && !use.def->flags().available && wm.cell_reader() != nullptr) {
    (void)wm.cell_reader()(ctx, *use.def);  // DBExpand reads the cell (database/DBexpand.c)
  }
  if (next && use.def->flags().not_found) {
    return false;
  }
  const bool changed = window.is_expanded(use) != next;
  window.set_expanded(use, next);
  if (changed && use.parent != nullptr) {
    wm.area_changed(*use.parent, use.bbox, all_but_space(), window.id());
  }
  return true;
}

struct ExpandWalk {
  LayoutWindow& window;
  WindowManager& wm;
  db::Rect box;
  Mode mode;
  bool expand;  // false: unexpand
  bool toggle;
  CommandContext& ctx;
  std::vector<std::string> unavailable;
  bool too_deep = false;  // a search stopped at db::kMaxSearchDepth
  std::unordered_set<const db::CellUse*> acted{};  // the OFF walk's instances already acted on

  [[nodiscard]] bool matches(const db::Rect& bbox) const {
    switch (mode) {
      case Mode::all: return true;
      case Mode::overlap: return db::overlap(bbox, box);
      case Mode::surround: return db::surround(box, bbox);
      case Mode::selection: return false;  // the selected instances are acted on directly
    }
    return false;
  }

  void act(const db::CellUse& use) {
    const bool next = toggle ? !window.is_expanded(use) : expand;
    if (!set_expansion(window, wm, use, next, ctx)) {
      unavailable.push_back(use.def->name());
    }
  }

  /// The uses of `def` that DBExpandAll's DBCellSrArea finds over `area`
  /// (`def`'s coordinates): an area search of the use index, which bins it
  /// the way Magic's does (the writers' structure order depends on that).
  [[nodiscard]] std::vector<db::CellUse*> search(const db::CellDef& def, const db::Rect& area) const {
    std::vector<db::CellUse*> found;
    (void)db::uses_overlapping(def, area, ctx.project().compat, [&](db::CellUse& u) {
      if (u.def != nullptr) {
        found.push_back(&u);
      }
      return true;
    });
    return found;
  }

  /// DBExpandAll from the window's root.
  void run(const db::CellDef& root) {
    if (ctx.project().compat.magic_compat) {
      walk_magic(root, mode == Mode::all ? db::whole_plane() : box, 0);
    } else {
      walk(root, db::kIdentity, 0);
    }
  }

  /// Both walks recurse once per level; they stop below db::kMaxSearchDepth
  /// in both modes (a crash has no Magic output to match).
  [[nodiscard]] bool deeper_allowed(std::size_t depth) {
    if (depth + 1 < db::kMaxSearchDepth) {
      return true;
    }
    too_deep = true;
    return false;
  }

  /// magic-compat OFF: acts on every matching instance at any depth reached
  /// through instances that are expanded after the action, descending into
  /// an expanded array under every element the box overlaps (Q-MASK-419).
  /// Each instance is acted on at most once per command, so a toggle is not
  /// undone when an instance matches under several elements or parents.
  void walk(const db::CellDef& def, const db::Transform& to_root, std::size_t depth) {
    const db::Rect area = mode == Mode::all ? db::whole_plane() : db::apply(db::invert(to_root), box);
    for (db::CellUse* use : search(def, area)) {
      if (matches(db::apply(to_root, use->bbox)) && acted.insert(use).second) {
        act(*use);
      }
      if (window.is_expanded(*use) && deeper_allowed(depth)) {
        for (const db::Transform& et : element_frames(*use, area)) {
          walk(*use->def, db::compose(to_root, et), depth + 1);
        }
      }
    }
  }

  /// The element transforms the OFF walk descends through: one frame for
  /// `all`; otherwise every element overlapping `area` (parent coordinates)
  /// whose cell's bounding box `area` does not surround, and the first one
  /// it surrounds (inside such elements every instance lies under the box,
  /// so they all match the same instances).
  [[nodiscard]] std::vector<db::Transform> element_frames(const db::CellUse& use, const db::Rect& area) const {
    if (mode == Mode::all) {
      return {use.transform};
    }
    std::vector<db::Transform> frames;
    const db::ElementRange r = db::array_overlap(use, area);
    bool covered_seen = false;
    for (std::int32_t y = r.ylo; y <= r.yhi; ++y) {
      for (std::int32_t x = r.xlo; x <= r.xhi; ++x) {
        const db::Transform et = db::element_transform(use, x, y);
        const bool covered = db::surround(area, db::apply(et, use.def->bbox()));
        if (!covered || !covered_seen) {
          frames.push_back(et);
        }
        covered_seen = covered_seen || covered;
      }
    }
    return frames;
  }

  /// magic-compat ON, dbExpandFunc (database/DBexpand.c:182-252) under
  /// DBCellSrArea's dbCellSrFunc (database/DBcellsrch.c:1131-1225): for each
  /// use found, only the first array element overlapping `area` is visited
  /// (dbExpandFunc returns 2; none when the area falls between elements);
  /// the use is acted on by dbExpandFunc's test in that element's frame, and
  /// the search goes on into its child, expanded or not, over `area` in that
  /// frame (DBCellSrArea reads an unavailable child first; a child that
  /// cannot be read is not searched).
  void walk_magic(const db::CellDef& def, const db::Rect& area, std::size_t depth) {
    for (db::CellUse* use : search(def, area)) {
      const db::ElementRange r = db::array_overlap(*use, area);
      if (r.xlo > r.xhi || r.ylo > r.yhi) {
        continue;
      }
      const db::Transform et = db::element_transform(*use, r.xlo, r.ylo);
      const db::Rect child_area = mode == Mode::all ? area : db::apply(db::invert(et), area);
      if (act_magic(*use, child_area) && deeper_allowed(depth) && read_for_search(*use->def)) {
        walk_magic(*use->def, child_area, depth + 1);
      }
    }
  }

  /// dbExpandFunc's test: `overlap` (and `all`) acts on every use found;
  /// `surround` on a use unless its child's bounding box surrounds the area
  /// without the area surrounding it (database/DBexpand.c:207-208), which
  /// is not "the box surrounds the instance" (Q-MASK-419).
  [[nodiscard]] bool magic_matches(const db::CellUse& use, const db::Rect& child_area) const {
    if (mode != Mode::surround) {
      return true;
    }
    const db::Rect& b = use.def->bbox();
    return !db::surround(b, child_area) || db::surround(child_area, b);
  }

  /// dbExpandFunc's expansion change; false when the child had to be read
  /// to expand it and could not be (Magic then returns 2 without searching it).
  bool act_magic(const db::CellUse& use, const db::Rect& child_area) {
    const bool expanded = window.is_expanded(use);
    const bool next = toggle ? !expanded : expand;
    if (next == expanded || !magic_matches(use, child_area)) {
      return true;
    }
    if (!set_expansion(window, wm, use, next, ctx)) {
      unavailable.push_back(use.def->name());
      return false;
    }
    return true;
  }

  /// DBCellSrArea's DBCellRead of the cell it searches; false when it is
  /// still unavailable after the read (database/DBcellsrch.c:1151-1153).
  bool read_for_search(db::CellDef& def) {
    if (!def.flags().available && wm.cell_reader() != nullptr) {
      (void)wm.cell_reader()(ctx, def);
    }
    return !def.flags().not_found;
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

void report_unavailable(CommandContext& ctx, const std::vector<std::string>& names) {
  for (const auto& name : names) {
    ctx.emit(diag::make(codes::kExpand, "Cell " + name + " is unavailable.  It could not be expanded."));
  }
}

/// The instance an `expand instance` path names under `root`: use ids
/// separated by '/', each optionally followed by an array subscript (the
/// whole array is one instance), as hierarchical names are written.
const db::CellUse* find_instance(const db::CellDef& root, std::string_view path) {
  const db::CellDef* def = &root;
  const db::CellUse* use = nullptr;
  while (def != nullptr && !path.empty()) {
    const std::size_t slash = path.find('/');
    std::string_view id = path.substr(0, slash);
    path = slash == std::string_view::npos ? std::string_view{} : path.substr(slash + 1);
    if (const std::size_t bracket = id.find('['); bracket != std::string_view::npos && id.ends_with(']')) {
      id = id.substr(0, bracket);
    }
    use = db::find_use(*def, id);
    if (use == nullptr) {
      return nullptr;
    }
    def = use->def;
  }
  return path.empty() ? use : nullptr;
}

/// Hefesto extension: `expand instance PATH [toggle]` / `unexpand instance
/// PATH` act on the one instance PATH names, in the pointed window, without
/// the box and without descending (the hierarchy browser's toggle).
CommandResult instance_expansion(CommandContext& ctx, const Invocation& inv, bool expand) {
  const bool toggle = expand && inv.args.size() == 3 && std::string_view("toggle").starts_with(inv.args[2]);
  if (inv.args.size() != 2 && !toggle) {
    return fail(codes::kUsage, expand ? "Usage: expand instance path [toggle]" : "Usage: unexpand instance path");
  }
  auto t = pointed(ctx);
  if (!t) {
    return failure(t.error());
  }
  LayoutWindow& w = t.value().window;
  const db::CellUse* use = w.root() != nullptr ? find_instance(*w.root(), inv.args[1]) : nullptr;
  if (use == nullptr || use->def == nullptr) {
    return fail(codes::kInstance, "Instance " + inv.args[1] + " not found in this window.");
  }
  const bool next = toggle ? !w.is_expanded(*use) : expand;
  if (!set_expansion(w, t.value().editor.windows, *use, next, ctx)) {
    report_unavailable(ctx, {use->def->name()});
  }
  t.value().editor.windows.notify(w.id(), Change::expansion);
  return CommandResult::success();
}

/// `expand|unexpand selection`: the selected instances of the window's
/// hierarchy (SelectExpand), without the box and without descending.
CommandResult selection_expansion(CommandContext& ctx, const ExpandArgs& args, bool expand) {
  auto t = pointed(ctx);
  if (!t) {
    return failure(t.error());
  }
  LayoutWindow& w = t.value().window;
  ExpandWalk walk{w, t.value().editor.windows, db::Rect{}, Mode::selection, expand, args.toggle, ctx, {}};
  WindowManager& wm = t.value().editor.windows;
  const auto read = [&](db::CellDef& def) {
    if (wm.cell_reader() != nullptr) {
      (void)wm.cell_reader()(ctx, def);
    }
  };
  for (const cell::InstanceRef& ref :
       cell::selected_instances(t.value().editor, ctx.project().compat.magic_compat, read)) {
    if (ref.use != nullptr && ref.use->def != nullptr) {
      walk.act(*ref.use);
    }
  }
  report_unavailable(ctx, walk.unavailable);
  t.value().editor.windows.notify(w.id(), Change::expansion);
  return CommandResult::success();
}

CommandResult expansion(CommandContext& ctx, const Invocation& inv, bool expand) {
  if (inv.args.size() >= 2 && inv.args[0] == "instance") {
    return instance_expansion(ctx, inv, expand);
  }
  const auto args = parse_expand_args(inv.args, expand ? Mode::overlap : Mode::surround, expand);
  if (!args) {
    return fail(codes::kUsage, expand ? "Usage: expand [selection|surround|overlap|all] [toggle]"
                                      : "Usage: unexpand [selection|surround|overlap|all]");
  }
  if (args->mode == Mode::selection) {
    return selection_expansion(ctx, *args, expand);
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
  // `expand all toggle` expands with magic-compat ON: CmdExpand passes
  // DB_EXPAND for `all` with or without toggle (commands/CmdE.c:950-966).
  const bool toggle = args->toggle && !(args->mode == Mode::all && ctx.project().compat.magic_compat);
  ExpandWalk walk{w, t.value().editor.windows, box.value_or(db::Rect{}), args->mode, expand, toggle, ctx, {}};
  walk.run(*w.root());
  report_unavailable(ctx, walk.unavailable);
  if (walk.too_deep) {
    ctx.emit(diag::make(codes::kExpandDepth, std::string(expand ? "expand" : "unexpand") +
                                                 ": the hierarchy is deeper than " +
                                                 std::to_string(db::kMaxSearchDepth) +
                                                 " levels; instances below that depth were not changed."));
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
  w.mark_redisplay_all();  // WindAreaChanged(w, NULL)
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

// cmdParseCoord of one grid word in the current units (spacings relative,
// origins absolute); an unparsable word is reported and counts as 0.
diag::Outcome<db::Coord> grid_coordinate(CommandContext& ctx, const Target& t, const std::string& word, cmd::Axis axis,
                                         bool relative) {
  const auto value = cmd::parse_distance(word, coordinate_units(t.editor), cmd::DistanceRequest{axis, relative, 1},
                                         &ctx.sink());
  if (!value.ok()) {
    return value.error();
  }
  return db::Coord{value.value()};
}

CommandResult grid_set(CommandContext& ctx, Target& t, const std::vector<std::string>& values) {
  if (values.size() == 3 || values.size() > 4) {
    return fail(codes::kUsage, std::string(kGridUsage));
  }
  // CmdGrid: the x spacing, then the y spacing, are checked as they are read.
  std::array<db::Coord, 4> v{0, 0, 0, 0};
  for (std::size_t i = 0; i < values.size(); ++i) {
    const bool spacing = i < 2;
    const auto c = grid_coordinate(ctx, t, values[i], i % 2 == 0 ? cmd::Axis::x : cmd::Axis::y, spacing);
    if (!c.ok()) {
      return CommandResult::failure(c.error());
    }
    if (spacing && c.value() <= 0) {
      return fail(codes::kGrid, "Grid spacing must be greater than zero.");
    }
    v[i] = c.value();
  }
  const db::Coord sx = v[0];
  const db::Coord sy = values.size() >= 2 ? v[1] : sx;
  GridState& g = t.window.grid();
  g.spacing = db::Rect{v[2], v[3], v[2] + sx, v[3] + sy};
  g.visible = true;
  return CommandResult::success();
}

CommandResult grid_option(CommandContext& ctx, Target& t, int option, const Invocation& inv) {
  GridState& g = t.window.grid();
  const db::Rect& b = g.spacing;
  switch (option) {
    case 0:  // box
    case 7:  // what
      if (option == 0 && inv.args.size() > 1) {
        return grid_set(ctx, t, std::vector<std::string>(inv.args.begin() + 1, inv.args.end()));
      }
      // Tcl build: "%d %d %d %d" (commands/CmdFI.c:1350-1362)
      return CommandResult::success(std::to_string(b.x0) + " " + std::to_string(b.y0) + " " + std::to_string(b.x1) +
                                    " " + std::to_string(b.y1));
    case 1:  // help
      print(ctx, std::string(kGridUsage) +
                     "\nor     grid <option>\nwhere <option> is one of: on, off, state, box, what, help, or multiple.");
      return CommandResult::success();
    case 2:
      return grid_multiple(g, inv);
    case 3:
    case 4:
      g.visible = option == 4;
      return CommandResult::success();
    case 5:
      return CommandResult::success(cmd::Value{std::int64_t{g.visible ? 1 : 0}});  // Tcl_NewBooleanObj
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
  const CommandResult result = option < 0 ? grid_set(ctx, t.value(), inv.args) : grid_option(ctx, t.value(), option, inv);
  t.value().editor.windows.notify(t.value().window.id(), Change::grid);
  return result;
}

// --- redraw / reset / updatedisplay ----------------------------------------------------

CommandResult redraw_cmd(CommandContext& ctx, const Invocation& inv) {
  auto e = session(ctx);
  if (!e) {
    return failure(e.error());
  }
  if (!inv.args.empty()) {
    return fail(codes::kUsage, "Usage: redraw");
  }
  e.value()->windows.redisplay_all();
  e.value()->windows.update_display();
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
  if (which != 0) {
    wm.update_display();  // performs the pending redisplay unless suspended
  }
  return CommandResult::success();
}

}  // namespace

bool register_display_ops(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "expand", "expand [selection|surround|overlap|all] [toggle] | expand instance path [toggle]",
                "Expands (or toggles) the instances under the box, or one instance by path, in this window.",
                [](CommandContext& c, const Invocation& i) { return expansion(c, i, true); });
  ok = add(registry, "unexpand", "unexpand [selection|surround|overlap|all] | unexpand instance path",
           "Unexpands the instances under the box, or one instance by path, in this window.",
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
