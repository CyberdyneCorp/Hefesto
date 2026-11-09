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

#include <algorithm>
#include <array>
#include <cstdio>
#include <utility>

#include "db/command_util.hpp"
#include "hefesto/editor/editor_state.hpp"
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

// Fixed-size printf buffer for one report line (the values are bounded).
using LineBuf = std::array<char, 256>;

// The microns or lambda line of CmdBox's report: values in float times
// `scale`, promoted to double for printf as in C.
std::string scaled_line(const Rect& r, float scale, bool lambda_line, std::int64_t area) {
  const auto f = [scale](Coord v) { return static_cast<double>(static_cast<float>(v) * scale); };
  const double a = static_cast<double>(static_cast<float>(area) * scale * scale);
  LineBuf buf{};
  if (lambda_line) {
    std::snprintf(buf.data(), buf.size(), "   %6.2f x %-6.2f  (% 6.2f, % -6.2f), (% 6.2f, % -6.2f)", f(r.x1 - r.x0),
                  f(r.y1 - r.y0), f(r.x0), f(r.y0), f(r.x1), f(r.y1));
  } else {
    std::snprintf(buf.data(), buf.size(), "microns:  %6.3f x %-6.3f  (% 6.3f, % -6.3f), (% 6.3f, % -6.3f)",
                  f(r.x1 - r.x0), f(r.y1 - r.y0), f(r.x0), f(r.y0), f(r.x1), f(r.y1));
  }
  std::string out = buf.data();
  if (area > 0) {
    LineBuf tail{};
    std::snprintf(tail.data(), tail.size(), lambda_line ? "  %-10.2f" : "  %-10.3f", a);
    out += tail.data();
  }
  return out;
}

std::string internal_line(const Rect& r, std::int64_t area) {
  LineBuf buf{};
  std::snprintf(buf.data(), buf.size(), " %6d x %-6d  (% 6d, % -6d), (% 6d, % -6d)", r.x1 - r.x0, r.y1 - r.y0, r.x0,
                r.y0, r.x1, r.y1);
  std::string out = buf.data();
  if (area > 0) {
    LineBuf tail{};
    std::snprintf(tail.data(), tail.size(), "  %-10lld", static_cast<long long>(area));
    out += tail.data();
  }
  return out;
}

// CmdBox's BOX_DEFAULT report (Magic ref: commands/CmdAB.c:1184-1226).
std::string box_report(const cu::Session& s, const Rect& r) {
  const std::int64_t area = std::int64_t{r.x1 - r.x0} * std::int64_t{r.y1 - r.y0};
  const tech::GridScale& g = s.editor->grid;
  // CIFGetOutputScale(1000). The style is kept as read (in lambda); Magic
  // rescales it with every grid change (CIFTechOutputScale), which is the
  // factor lambda/internal of the session grid. No output style: 1.0.
  float oscale = 1.0F;
  if (const auto& cif = s.tech->cif_output_grid()) {
    oscale = static_cast<float>(10 * cif->scale_factor * g.lambda) /
             static_cast<float>(cif->expander * 1000 * g.internal);
  }
  std::string out = "Root cell box:\n           width x height  (   llx,  lly  ), (   urx,  ury  )";
  if (area > 0) {
    out += "  area (units^2)";
  }
  out += "\n\n" + scaled_line(r, oscale, false, area) + "\nlambda:";
  if (g.internal != g.lambda) {
    const float iscale = static_cast<float>(g.lambda) / static_cast<float>(g.internal);  // DBLambda[0] / DBLambda[1]
    out += scaled_line(r, iscale, true, area) + "\ninternal:";
  } else {
    out += "  ";
  }
  return out + internal_line(r, area);
}

CommandResult box_set(const cu::Session& s, const std::vector<std::string>& args, std::size_t first) {
  auto c = parse_coords(s, args, first, 4);
  if (!c.ok()) {
    return cu::fail(c.error());
  }
  const auto& v = c.value();
  s.editor->box = canonical(Rect{v[0], v[1], v[2], v[3]});
  s.editor->box_root = s.editor->edit_cell;
  return CommandResult::success();
}

CommandResult box_resize(const cu::Session& s, const std::vector<std::string>& args, bool size) {
  auto b = cu::box(s);
  if (!b.ok()) {
    return cu::fail(b.error());
  }
  if (args.size() == 1) {
    const Rect& r = b.value();
    return CommandResult::success(size ? Value{Value::List{cu::coord_value(s, r.x1 - r.x0),
                                                           cu::coord_value(s, r.y1 - r.y0)}}
                                       : Value{Value::List{cu::coord_value(s, r.x0), cu::coord_value(s, r.y0)}});
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
  return CommandResult::success();
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

// The default form: `box` alone, `box x1 y1 x2 y2` (sets the box first) or an
// unrecognized option: prints the report, returns nothing.
CommandResult box_default(const cu::Session& s, const std::vector<std::string>& args) {
  if (args.size() == 4) {
    if (auto set = box_set(s, args, 0); !set.ok()) {
      return set;
    }
  }
  auto b = cu::box(s);
  if (!b.ok()) {
    return cu::fail(b.error());
  }
  cu::print(s, box_report(s, b.value()));
  return CommandResult::success();
}

bool is_query(const std::vector<std::string>& args) {
  return args.size() == 1 && (args[0] == "values" || args[0] == "width" || args[0] == "height");
}

CommandResult cmd_box(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  const auto& args = inv.args;
  if (is_query(args)) {
    return box_query(s.value(), args[0]);
  }
  if (!args.empty() && args[0] == "values") {
    return box_set(s.value(), args, 1);
  }
  if (!args.empty() && (args[0] == "size" || args[0] == "position")) {
    return box_resize(s.value(), args, args[0] == "size");
  }
  return box_default(s.value(), args);
}

// --- snap --------------------------------------------------------------------

// CmdSnap (Magic ref: commands/CmdRS.c:2944-3021): setting returns nothing;
// `snap` / `snap list` return the grid name.
CommandResult cmd_snap(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  enum Choice { off, internal, lambda, grid, user, on, list };
  int n = list;
  if (!inv.args.empty()) {
    n = cu::lookup(inv.args[0], {"off", "internal", "lambda", "grid", "user", "on", "list"});
    if (n < 0) {
      cu::print(s.value(), "Usage: snap [internal | lambda | user]");
      return CommandResult::success();
    }
  }
  if (n == internal && !cu::units_internal(s.value())) {
    cu::set_units_internal(s.value());
    cu::warn(s.value(),
             "Warning:  snap setting is also changing units.  This usage is deprecated\n"
             "and may be removed in the future.  Use \"units\" to change units, and\n"
             "change units before setting snap to keep this message from appearing.");
  }
  switch (n) {
    case off:
    case internal:
      cu::set_snap_mode(s.value(), cu::Snap::internal);
      return CommandResult::success();
    case lambda:
      cu::set_snap_mode(s.value(), cu::Snap::lambda);
      return CommandResult::success();
    case list:
      return CommandResult::success(Value{std::string{cu::snap_name(cu::snap_mode(s.value()))}});
    default:
      cu::set_snap_mode(s.value(), cu::Snap::user);
      return CommandResult::success();
  }
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

// DBAdjustLabels after paint/erase, with its "Moving label" / "Adjusting
// size of label" notes (Magic ref: database/DBlabel.c:690-725).
// Then DBWAreaChanged over the edited area (ui-windows-and-views
// "Change-driven redisplay"): the changed layers, or every layer and the
// label extent when labels were erased or adjusted.
void finish_edit(const EditTarget& t, const tech::TypeMask& layers, bool labels_changed = false) {
  std::vector<std::string> notes;
  const bool adjusted = adjust_labels(*t.def, *t.s.tech, t.area, &notes);
  for (std::string& n : notes) {
    cu::print(t.s, std::move(n));
  }
  recompute_bbox(*t.def, t.s.magic_compat);
  const bool labels = labels_changed || adjusted;
  t.s.editor->windows.area_changed(*t.def, t.area, labels ? std::nullopt : std::optional<tech::TypeMask>(layers));
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
  finish_edit(e, mask.value());
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
  const bool labels = erase_labels(*e.def, *e.s.tech, e.area, mask.value()) > 0;
  finish_edit(e, mask.value(), labels);
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
  tech::TypeMask painted;
  for (std::size_t i = 1; i < inv.args.size(); ++i) {
    auto layer = cu::parse_layer(*e.s.tech, inv.args[i]);
    if (!layer.ok()) {
      return cu::fail(layer.error());
    }
    painted.set(layer.value());
    const bool right = i == 1 ? side.right_side : !side.right_side;
    if (auto st = paint_triangle(*e.def, *e.s.tech, e.area, side.diagonal, right, layer.value(), paint_options(e.s));
        !st.ok()) {
      return cu::fail(st.error());
    }
  }
  finish_edit(e, painted);
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

}  // namespace

// CmdGetcell for a definition already in memory (the CELL area reads it
// first): placed with the box's lower-left corner as the reference point,
// the box then covers the new instance and the result is its identifier.
// (Magic ref: commands/CmdFI.c:1067-1120, commands/CmdCD.c:4893-5420)
CommandResult getcell_command(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.empty() || inv.args.size() > 2) {
    return cu::fail(cu::bad_argument("Usage: getcell cellName [child refPointChild] [parent refPointParent]"));
  }
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  if (s.value().editor->edit_cell == nullptr) {
    return cu::fail(diag::make(codes::kNoEditCell, "No cell def being edited; cannot place cell use!"));
  }
  const std::string& name = inv.args[0];
  if (!s.value().editor->box) {
    return cu::fail(diag::make(codes::kNoBox, "The box's lower-left corner must point to the place\n"
                                              "    in the edit cell where you'd like to put \"" + name + "\"."));
  }
  auto t = edit_target(ctx);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const EditTarget& e = t.value();
  CellDef* child = e.s.editor->cells.find(name);
  if (child == nullptr) {
    return cu::fail(diag::make(codes::kCellNotFound, "Cell " + name + " couldn't be read"));
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
  e.s.editor->box = use.value()->bbox;  // DBWSetBox(EditRootDef, newUse bbox)
  return CommandResult::success(Value{use.value()->id});
}

namespace {

// --- flatten -------------------------------------------------------------------

struct FlattenRequest {
  FlattenOptions options;
  bool in_place = false;
  bool dolabels = true;    // -nolabels / -dolabels
  bool toplabels = false;  // -dotoplabels / -notoplabels
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
      r.dolabels = !invert;
      break;
    case 't':
      r.toplabels = !invert;
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
  // CmdFlatten prefers the top labels over all labels; DBFlattenInPlace the
  // reverse (Magic ref: commands/CmdFI.c:2205-2217, database/DBcellcopy.c:624-631).
  const FlattenLabels top = r.toplabels ? FlattenLabels::top_only : FlattenLabels::none;
  const FlattenLabels all = r.dolabels ? FlattenLabels::all : FlattenLabels::none;
  if (r.in_place) {
    r.options.labels = r.dolabels ? FlattenLabels::all : top;
  } else {
    r.options.labels = r.toplabels ? FlattenLabels::top_only : all;
  }
  return r;
}

}  // namespace

CommandResult flatten_command(CommandContext& ctx, const Invocation& inv) {
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
  FlattenRequest& r = req.value();
  r.options.font_update = [sess = s.value()](Label& l) { cu::update_font_label(sess, l); };
  if (r.in_place) {
    CellUse* use = find_use(*def.value(), r.name);
    if (use == nullptr || use->id != r.name) {
      return cu::fail(diag::make(codes::kUseNotFound, "no instance \"" + r.name + "\" in the edit cell"));
    }
    auto st = flatten_in_place(*use, *s.value().tech, r.options);
    return st.ok() ? CommandResult::success() : cu::fail(st.error());
  }
  if (s.value().editor->cells.find(r.name) != nullptr) {
    return cu::fail(diag::make(codes::kCellExists, r.name + " already exists"));
  }
  auto made = flatten_to_new(s.value().editor->cells, *def.value(), r.name, *s.value().tech, r.options);
  return made.ok() ? CommandResult::success() : cu::fail(made.error());  // CmdFlatten sets no result
}

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
