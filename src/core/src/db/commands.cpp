// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MODEL commands, part 1: box, snap, paint, erase, splitpaint, getcell,
// flatten (minimal Magic forms used by tests/golden/MODEL/CASES.md; the full
// cmd-* capabilities follow in M2).
// (Magic ref: commands/CmdAB.c CmdBox, commands/CmdLQ.c CmdPaint,
// commands/CmdE.c CmdErase, commands/CmdRS.c CmdSplitPaint/CmdSnap,
// commands/CmdFI.c CmdGetcell/CmdFlatten)

#include "hefesto/db/commands.hpp"

#include <array>
#include <utility>

#include "db/command_util.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"

namespace hefesto::db {

namespace cu = cmdutil;
using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using cmd::Value;

namespace {

// --- box ---------------------------------------------------------------------

Value box_value(const cu::Session& s, const Rect& r) {
  return Value{Value::List{cu::coord_value(s, r.x0), cu::coord_value(s, r.y0), cu::coord_value(s, r.x1),
                           cu::coord_value(s, r.y1)}};
}

diag::Outcome<std::vector<Coord>> parse_coords(const cu::Session& s, const std::vector<std::string>& args,
                                               std::size_t first, std::size_t count) {
  if (args.size() != first + count) {
    return cu::bad_argument("expected " + std::to_string(count) + " coordinates");
  }
  std::vector<Coord> out;
  for (std::size_t i = first; i < args.size(); ++i) {
    auto c = cu::parse_coord(s, args[i]);
    if (!c.ok()) {
      return c.error();
    }
    out.push_back(c.value());
  }
  return out;
}

CommandResult box_set(const cu::Session& s, const std::vector<std::string>& args, std::size_t first) {
  auto c = parse_coords(s, args, first, 4);
  if (!c.ok()) {
    return cu::fail(c.error());
  }
  const auto& v = c.value();
  s.editor->box = canonical(Rect{v[0], v[1], v[2], v[3]});
  s.editor->box_root = s.editor->edit_cell;
  return CommandResult::success(box_value(s, *s.editor->box));
}

CommandResult box_resize(const cu::Session& s, const std::vector<std::string>& args, bool size) {
  auto b = cu::box(s);
  if (!b.ok()) {
    return cu::fail(b.error());
  }
  auto c = parse_coords(s, args, 1, 2);
  if (!c.ok()) {
    return cu::fail(c.error());
  }
  Rect r = b.value();
  const auto& v = c.value();
  r = size ? Rect{r.x0, r.y0, r.x0 + v[0], r.y0 + v[1]}
           : Rect{v[0], v[1], v[0] + (r.x1 - r.x0), v[1] + (r.y1 - r.y0)};
  s.editor->box = canonical(r);
  return CommandResult::success(box_value(s, *s.editor->box));
}

CommandResult box_query(const cu::Session& s, std::string_view what) {
  auto b = cu::box(s);
  if (!b.ok()) {
    return cu::fail(b.error());
  }
  const Rect& r = b.value();
  if (what == "width") {
    return CommandResult::success(cu::coord_value(s, r.x1 - r.x0));
  }
  if (what == "height") {
    return CommandResult::success(cu::coord_value(s, r.y1 - r.y0));
  }
  return CommandResult::success(box_value(s, r));
}

CommandResult cmd_box(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  const auto& args = inv.args;
  if (args.empty() || (args.size() == 1 && (args[0] == "values" || args[0] == "width" || args[0] == "height"))) {
    return box_query(s.value(), args.empty() ? "values" : args[0]);
  }
  if (args[0] == "values") {
    return box_set(s.value(), args, 1);
  }
  if (args[0] == "size" || args[0] == "position") {
    return box_resize(s.value(), args, args[0] == "size");
  }
  return box_set(s.value(), args, 0);
}

// --- snap --------------------------------------------------------------------

CommandResult cmd_snap(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  if (inv.args.size() > 1) {
    return cu::fail(cu::bad_argument("usage: snap ?internal|lambda|user?"));
  }
  if (inv.args.size() == 1) {
    const std::string& m = inv.args[0];
    if (m == "internal" || m == "int") {
      cu::set_snap_mode(s.value(), cu::Snap::internal);
    } else if (m == "lambda" || m == "on") {
      cu::set_snap_mode(s.value(), cu::Snap::lambda);
    } else if (m == "user" || m == "grid") {
      cu::set_snap_mode(s.value(), cu::Snap::user);
    } else {
      return cu::fail(cu::bad_argument("unknown snap mode \"" + m + "\""));
    }
  }
  return CommandResult::success(Value{std::string{cu::snap_name(cu::snap_mode(s.value()))}});
}

// --- paint / erase / splitpaint ------------------------------------------------

struct EditTarget {
  cu::Session s;
  CellDef* def = nullptr;
  Rect area;
};

diag::Outcome<EditTarget> edit_target(CommandContext& ctx) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return s.error();
  }
  auto def = cu::edit_cell(s.value());
  if (!def.ok()) {
    return def.error();
  }
  auto b = cu::box(s.value());
  if (!b.ok()) {
    return b.error();
  }
  return EditTarget{s.value(), def.value(), b.value()};
}

PaintOptions paint_options(const cu::Session& s) {
  PaintOptions o;
  o.locked = s.editor->locked_types;
  o.magic_compat = s.magic_compat;
  return o;
}

void finish_edit(const EditTarget& t) {
  (void)adjust_labels(*t.def, *t.s.tech, t.area);
  recompute_bbox(*t.def, t.s.magic_compat);
}

CommandResult cmd_paint(CommandContext& ctx, const Invocation& inv) {
  auto t = edit_target(ctx);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const EditTarget& e = t.value();
  auto mask = cu::parse_layers(*e.s.tech, cu::join_args(inv.args, 0));
  if (!mask.ok()) {
    return cu::fail(mask.error());
  }
  for (std::size_t ty = 0; ty < e.s.tech->type_count(); ++ty) {
    if (mask.value().test(static_cast<tech::TileType>(ty))) {
      if (auto st = paint(*e.def, *e.s.tech, e.area, static_cast<tech::TileType>(ty), paint_options(e.s)); !st.ok()) {
        return cu::fail(st.error());
      }
    }
  }
  finish_edit(e);
  return CommandResult::success();
}

CommandResult cmd_erase(CommandContext& ctx, const Invocation& inv) {
  auto t = edit_target(ctx);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const EditTarget& e = t.value();
  const std::string layers = inv.args.empty() ? std::string{"*,labels"} : cu::join_args(inv.args, 0);
  auto mask = cu::parse_layers(*e.s.tech, layers);
  if (!mask.ok()) {
    return cu::fail(mask.error());
  }
  if (auto st = erase(*e.def, *e.s.tech, e.area, mask.value(), paint_options(e.s)); !st.ok()) {
    return cu::fail(st.error());
  }
  (void)erase_labels(*e.def, *e.s.tech, e.area, mask.value());
  finish_edit(e);
  return CommandResult::success();
}

std::optional<TriangleCorner> parse_corner(std::string_view name) {
  constexpr std::array<std::pair<std::string_view, TriangleCorner>, 4> kCorners = {
      {{"nw", TriangleCorner::nw}, {"sw", TriangleCorner::sw}, {"se", TriangleCorner::se}, {"ne", TriangleCorner::ne}}};
  for (const auto& [n, c] : kCorners) {
    if (n == name) {
      return c;
    }
  }
  return std::nullopt;
}

CommandResult cmd_splitpaint(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() < 2 || inv.args.size() > 3) {
    return cu::fail(cu::bad_argument("usage: splitpaint dir layer ?layer2?"));
  }
  auto t = edit_target(ctx);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const EditTarget& e = t.value();
  const auto corner = parse_corner(inv.args[0]);
  if (!corner) {
    return cu::fail(cu::bad_argument("direction must be nw, ne, sw or se"));
  }
  const TriangleSide side = triangle_side(*corner);
  for (std::size_t i = 1; i < inv.args.size(); ++i) {
    auto layer = cu::parse_layer(*e.s.tech, inv.args[i]);
    if (!layer.ok()) {
      return cu::fail(layer.error());
    }
    const bool right = i == 1 ? side.right_side : !side.right_side;
    if (auto st = paint_triangle(*e.def, *e.s.tech, e.area, side.diagonal, right, layer.value(), paint_options(e.s));
        !st.ok()) {
      return cu::fail(st.error());
    }
  }
  finish_edit(e);
  return CommandResult::success();
}

// --- getcell -------------------------------------------------------------------

std::optional<Transform> parse_orientation(std::string_view name) {
  constexpr std::array<std::pair<std::string_view, Transform>, 20> kNames = {{
      {"0", {1, 0, 0, 0, 1, 0}},     {"N", {1, 0, 0, 0, 1, 0}},       {"90", {0, 1, 0, -1, 0, 0}},
      {"E", {0, 1, 0, -1, 0, 0}},    {"180", {-1, 0, 0, 0, -1, 0}},   {"S", {-1, 0, 0, 0, -1, 0}},
      {"270", {0, -1, 0, 1, 0, 0}},  {"W", {0, -1, 0, 1, 0, 0}},      {"v", {1, 0, 0, 0, -1, 0}},
      {"FS", {1, 0, 0, 0, -1, 0}},   {"h", {-1, 0, 0, 0, 1, 0}},      {"FN", {-1, 0, 0, 0, 1, 0}},
      {"90v", {0, 1, 0, 1, 0, 0}},   {"FW", {0, 1, 0, 1, 0, 0}},      {"90h", {0, -1, 0, -1, 0, 0}},
      {"FE", {0, -1, 0, -1, 0, 0}},  {"180v", {-1, 0, 0, 0, 1, 0}},   {"180h", {1, 0, 0, 0, -1, 0}},
      {"270v", {0, -1, 0, -1, 0, 0}}, {"270h", {0, 1, 0, 1, 0, 0}},
  }};
  for (const auto& [n, t] : kNames) {
    if (n == name) {
      return t;
    }
  }
  return std::nullopt;
}

CommandResult cmd_getcell(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.empty() || inv.args.size() > 2) {
    return cu::fail(cu::bad_argument("usage: getcell name ?orientation?"));
  }
  auto t = edit_target(ctx);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const EditTarget& e = t.value();
  CellDef* child = e.s.editor->cells.find(inv.args[0]);
  if (child == nullptr) {
    return cu::fail(diag::make(codes::kCellNotFound, "no cell named \"" + inv.args[0] + "\""));
  }
  const auto orient = inv.args.size() == 2 ? parse_orientation(inv.args[1]) : std::optional<Transform>{kIdentity};
  if (!orient) {
    return cu::fail(cu::bad_argument("unknown orientation \"" + inv.args[1] + "\""));
  }
  const Rect moved = apply(*orient, child->bbox());
  PlaceOptions po;
  po.transform = compose(translation(e.area.x0 - moved.x0, e.area.y0 - moved.y0), *orient);
  auto use = place(*e.def, *child, po);
  if (!use.ok()) {
    return cu::fail(use.error());
  }
  recompute_bbox(*e.def, e.s.magic_compat);
  cu::set_current_use(e.s, use.value());
  return CommandResult::success(Value{use.value()->id});
}

// --- flatten -------------------------------------------------------------------

struct FlattenRequest {
  FlattenOptions options;
  bool in_place = false;
  std::string name;
};

// CmdFlatten's option switch on the fourth character of "-no..."/"-do...".
std::optional<diag::Diagnostic> apply_flatten_option(FlattenRequest& r, std::string_view arg) {
  const bool invert = arg.substr(1, 2) == "no";
  if (!invert && arg.substr(1, 2) != "do") {
    return cu::bad_argument("bad flatten option \"" + std::string{arg} + "\"");
  }
  switch (arg[3]) {
    case 'i':
      r.in_place = !invert;
      break;
    case 'l':
      r.options.labels = invert ? FlattenLabels::none : FlattenLabels::all;
      break;
    case 't':
      r.options.labels = invert ? FlattenLabels::all : FlattenLabels::top_only;
      break;
    case 'p':
      r.options.descend = (arg.substr(3, 4) == "prop" && !invert) ? Descend::property_flatten : Descend::all;
      break;
    case 's':
      r.options.descend = invert ? Descend::no_subcircuit : Descend::all;
      break;
    case 'v':
      r.options.descend = invert ? Descend::no_vendor : Descend::all;
      break;
    default:
      break;  // -dobox / -doports: accepted, no effect in M1
  }
  return std::nullopt;
}

diag::Outcome<FlattenRequest> parse_flatten(const std::vector<std::string>& args, bool magic_compat) {
  FlattenRequest r;
  r.options.magic_compat = magic_compat;
  for (const std::string& a : args) {
    if (a.size() > 3 && a[0] == '-') {
      if (auto err = apply_flatten_option(r, a)) {
        return *err;
      }
    } else {
      r.name = a;
    }
  }
  if (r.name.empty()) {
    return cu::bad_argument("usage: flatten ?options? name");
  }
  return r;
}

CommandResult cmd_flatten(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  auto def = cu::edit_cell(s.value());
  if (!def.ok()) {
    return cu::fail(def.error());
  }
  auto req = parse_flatten(inv.args, s.value().magic_compat);
  if (!req.ok()) {
    return cu::fail(req.error());
  }
  const FlattenRequest& r = req.value();
  if (r.in_place) {
    CellUse* use = find_use(*def.value(), r.name);
    if (use == nullptr || use->id != r.name) {
      return cu::fail(diag::make(codes::kUseNotFound, "no instance \"" + r.name + "\" in the edit cell"));
    }
    auto st = flatten_in_place(*use, *s.value().tech, r.options);
    return st.ok() ? CommandResult::success() : cu::fail(st.error());
  }
  auto made = flatten_to_new(s.value().editor->cells, *def.value(), r.name, *s.value().tech, r.options);
  return made.ok() ? CommandResult::success(Value{made.value()->name()}) : cu::fail(made.error());
}

}  // namespace

namespace cmdutil {

bool register_paint_commands(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "box", "box ?values|size|position|width|height? ?args?",
                "Shows or sets the box tool, in the current snap units.", cmd_box);
  ok = add(registry, "snap", "snap ?internal|lambda|user?", "Shows or sets the snap (coordinate unit) mode.",
           cmd_snap) && ok;
  ok = add(registry, "paint", "paint layers", "Paints the layers into the box area of the edit cell.", cmd_paint) &&
       ok;
  ok = add(registry, "erase", "erase ?layers?", "Erases the layers (default everything) from the box area.",
           cmd_erase) && ok;
  ok = add(registry, "splitpaint", "splitpaint dir layer ?layer2?",
           "Paints a triangle of the box (right angle at dir) and optionally the other half.", cmd_splitpaint) &&
       ok;
  ok = add(registry, "getcell", "getcell name ?orientation?",
           "Places an instance of a cell with its lower-left corner at the box.", cmd_getcell) && ok;
  ok = add(registry, "flatten", "flatten ?options? name",
           "Flattens the edit cell into a new cell (-doinplace: flattens an instance).", cmd_flatten) && ok;
  return ok;
}

}  // namespace cmdutil

bool register_db_commands(cmd::CommandRegistry& registry) {
  bool ok = cmdutil::register_paint_commands(registry);
  ok = cmdutil::register_label_commands(registry) && ok;
  ok = cmdutil::register_cell_commands(registry) && ok;
  return ok;
}

}  // namespace hefesto::db
