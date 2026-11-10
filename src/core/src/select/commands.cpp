// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Selection commands (cmd-selection) and their registration: delete, copy,
// move (incl. move origin), stretch, clockwise/rotate, sideways,
// upsidedown, orient and array. `select` is in select_command.cpp, `what`
// and `identify` in what.cpp. Owner: SELECT.
// (Magic ref: commands/CmdCD.c CmdCopy/CmdClockwise/CmdDelete,
// commands/CmdLQ.c CmdMove/CmdOrient, commands/CmdRS.c CmdSideways/
// CmdStretch, commands/CmdTZ.c CmdUpsidedown, commands/CmdAB.c CmdArray)

#include "hefesto/select/commands.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "db/command_util.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/select/diag_codes.hpp"
#include "select/commands_internal.hpp"
#include "select/ops.hpp"

namespace hefesto::select {

namespace cu = db::cmdutil;
using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using cmd::Value;
using ops::Context;

namespace {

/// Runs `body(context, inv)` for a selection command.
template <typename Body>
CommandResult with_context(CommandContext& ctx, const Invocation& inv, Body&& body) {
  auto c = ops::context(ctx);
  if (!c.ok()) {
    return cu::fail(c.error());
  }
  return body(c.value(), inv);
}

/// ToolGetEditBox: the box must be in a window on the edit cell.
bool edit_box(const Context& c) {
  if (!ops::root_box(c)) {
    cu::warn(c.s, "Box tool must be present");
    return false;
  }
  return true;
}

/// Moves the box by `t` when it is in the selection's root.
void move_box(const Context& c, const db::Transform& t) {
  const auto box = ops::root_box(c);
  const db::CellDef* sel_root = c.ed->selection.root();
  if (box && (sel_root == nullptr || sel_root == c.root)) {
    ops::set_root_box(c, db::apply(t, *box));
  }
}

// ---- delete ---------------------------------------------------------------------

CommandResult cmd_delete_in(const Context& c, const Invocation& inv) {
  if (!inv.args.empty()) {
    return fail_text(codes::kUsage, "Usage: delete");
  }
  if (!edit_box(c)) {
    return CommandResult::success();
  }
  Snapshot before = c.ed->selection.snapshot();
  ops::delete_selection(c, "deleted", true);
  ops::commit(c, std::move(before));
  return CommandResult::success();
}

CommandResult cmd_delete(CommandContext& ctx, const Invocation& inv) {
  return with_context(ctx, inv, cmd_delete_in);
}

// ---- copy and move ----------------------------------------------------------------

/// A displacement in root coordinates, or the reason there is none.
struct Displacement {
  db::Point d;
  bool to_point = false;  // `to x y` or the pointer: the box is required
};

diag::Outcome<db::Coord> amount(const Context& c, const std::string& word) {
  return cu::parse_coord(c.s, word);
}

/// The displacement of direction `dir` with amounts (ax, ay); an invalid
/// name (-2) is the DX DY form, `center` is zero.
db::Point delta_of(int dir, db::Coord ax, db::Coord ay) {
  switch (dir) {
    case db::kNorth:
      return db::Point{0, ay};
    case db::kSouth:
      return db::Point{0, -ay};
    case db::kEast:
      return db::Point{ax, 0};
    case db::kWest:
      return db::Point{-ax, 0};
    case db::kNorthWest:
      return db::Point{-ax, ay};
    case db::kSouthWest:
      return db::Point{-ax, -ay};
    case db::kSouthEast:
      return db::Point{ax, -ay};
    case db::kCenter:
      return db::Point{0, 0};
    default:  // northeast and an invalid name (DX DY)
      return db::Point{ax, ay};
  }
}

/// The given amounts of a direction form: one amount along a Manhattan
/// direction, AX and AY (the last word) otherwise.
diag::Outcome<db::Point> given_amounts(const Context& c, int dir, const std::string& first, const std::string& last) {
  auto x = amount(c, first);
  auto y = amount(c, last);
  if (!x.ok() || !y.ok()) {
    return !x.ok() ? x.error() : y.error();
  }
  const bool vertical = dir == db::kNorth || dir == db::kSouth;
  const bool horizontal = dir == db::kEast || dir == db::kWest;
  return db::Point{vertical ? 0 : x.value(), horizontal ? 0 : (vertical ? x.value() : y.value())};
}

/// The DIR [AMOUNT] / DIR AX AY / DX DY forms (CmdCopy/CmdMove).
diag::Outcome<db::Point> direction_form(const Context& c, const std::vector<std::string>& a, std::size_t pos,
                                        bool copy) {
  const int dir = db::parse_position(a[pos], false);
  const std::size_t first = dir >= 0 ? pos + 1 : pos;
  if (a.size() >= pos + 2 && a.size() > first) {  // CmdCopy/CmdMove: tx_argc >= 3
    auto amounts = given_amounts(c, dir, a[first], a.back());
    if (!amounts.ok()) {
      return amounts.error();
    }
    return delta_of(dir, amounts.value().x, amounts.value().y);
  }
  if (dir < 0 && copy) {
    return diag::make(codes::kUsage, "Improperly defined copy. . . direction needed.");
  }
  auto one = amount(c, "1l");
  if (!one.ok()) {
    return one.error();
  }
  return delta_of(dir, one.value(), one.value());
}

/// Where `to X Y` (edit coordinates) or the pointer puts the selection.
diag::Outcome<db::Point> target_point(const Context& c, const std::vector<std::string>& a, std::size_t pos,
                                      std::string_view verb) {
  if (pos < a.size()) {
    auto x = amount(c, a[pos + 1]);
    auto y = amount(c, a[pos + 2]);
    if (!x.ok() || !y.ok()) {
      return !x.ok() ? x.error() : y.error();
    }
    return editor::to_root(*c.ed, db::Point{x.value(), y.value()});
  }
  const auto& p = c.ed->command_point;
  const db::CellDef* edit_root = c.ed->edit_root != nullptr ? c.ed->edit_root : c.ed->edit_cell;
  if (!p || edit_root != c.root) {
    return diag::make(codes::kNotInEditCell, "\"" + std::string{verb} +
                                                 "\" uses the point as the place to put down a\n    the "
                                                 "selection, but the point doesn't point to the\n    edit cell.");
  }
  return p->point;
}

/// The point forms: `to X Y` (edit coordinates) or the pointer, minus the
/// box lower-left.
diag::Outcome<db::Point> point_form(const Context& c, const std::vector<std::string>& a, std::size_t pos,
                                    std::string_view verb) {
  auto target = target_point(c, a, pos, verb);
  if (!target.ok()) {
    return target.error();
  }
  const db::Point root = target.value();
  const auto box = ops::root_box(c);
  const db::CellDef* sel_root = c.ed->selection.root();
  if (!box || (sel_root != nullptr && sel_root != c.root)) {
    return diag::make(codes::kNotInEditCell, "\"" + std::string{verb} +
                                                 "\" uses the box lower-left corner as a place\n    to pick up "
                                                 "the selection, but the box\n    isn't in a window containing "
                                                 "the selection.");
  }
  return db::Point{root.x - box->x0, root.y - box->y0};
}

diag::Outcome<Displacement> displacement(const Context& c, const std::vector<std::string>& a, std::size_t pos,
                                         bool copy, std::string_view verb) {
  if (pos < a.size() && a[pos] == "to") {
    if (a.size() != pos + 3) {
      return diag::make(codes::kUsage, "");  // the caller prints its usage
    }
    auto d = point_form(c, a, pos, verb);
    if (!d.ok()) {
      return d.error();
    }
    return Displacement{d.value(), true};
  }
  if (pos >= a.size()) {
    auto d = point_form(c, a, a.size(), verb);
    if (!d.ok()) {
      return d.error();
    }
    return Displacement{d.value(), true};
  }
  auto d = direction_form(c, a, pos, copy);
  if (!d.ok()) {
    return d.error();
  }
  return Displacement{d.value(), false};
}

/// CmdCopy/CmdMove recast the pointer form (no arguments) as `VERB to Xi Yi`
/// (the pointer in edit coordinates) so the command log no longer depends on
/// the pointer.
CommandResult logged_to_point(const Context& c, const Invocation& inv, std::string_view verb) {
  CommandResult result = CommandResult::success();
  if (inv.args.empty() && c.ed->command_point) {
    const db::Point p = editor::to_edit(*c.ed, c.ed->command_point->point);
    result.logged_as = Invocation{std::string{verb}, {"to", std::to_string(p.x) + "i", std::to_string(p.y) + "i"},
                                  inv.origin, std::nullopt};
  }
  return result;
}

std::string copy_move_usage(std::string_view verb, bool origin) {
  const std::string v{verb};
  const std::string o = origin ? "[origin] " : "";
  return "Usage: " + v + " " + o + "[direction [amount]]\n   or: " + v + " " + o + "to x y";
}

CommandResult cmd_copy_in(const Context& c, const Invocation& inv) {
  if (inv.args.size() > 3) {
    return fail_text(codes::kUsage, copy_move_usage("copy", false));
  }
  if (!inv.args.empty() && !edit_box(c)) {
    return CommandResult::success();
  }
  auto d = displacement(c, inv.args, 0, true, "Copy");
  if (!d.ok()) {
    return d.error().text.empty() ? fail_text(codes::kUsage, copy_move_usage("copy", false)) : cu::fail(d.error());
  }
  const db::Transform t = db::translation(d.value().d.x, d.value().d.y);
  if (c.ed->edit_cell == nullptr) {
    return fail_text(codes::kNotEditable, "The current cell is not editable.");
  }
  move_box(c, t);
  Snapshot before = c.ed->selection.snapshot();
  auto copied = ops::whole_selection(c, t);
  ops::copy_into_edit(c, *copied);
  ops::commit(c, std::move(before));
  return logged_to_point(c, inv, "copy");
}

CommandResult cmd_copy(CommandContext& ctx, const Invocation& inv) {
  return with_context(ctx, inv, cmd_copy_in);
}

/// DBMoveCell: the root cell's contents move by (-dx, -dy).
/// dbMoveProp over every property: dimension values (FIXED_BBOX, OBS_BBOX,
/// ...) and plane values (MASKHINTS_*) follow the geometry.
void move_properties(db::CellDef& root, db::Coord dx, db::Coord dy) {
  std::vector<std::pair<std::string, db::PropertyValue>> moved;
  root.properties().for_each([&](std::string_view key, const db::PropertyValue& value) {
    if (std::holds_alternative<db::DimensionList>(value) || std::holds_alternative<db::PlaneValue>(value)) {
      moved.emplace_back(std::string{key}, db::translate_property(value, dx, dy));
    }
  });
  for (auto& [key, value] : moved) {
    db::set_property(root, key, std::move(value));
  }
}

void move_origin(const Context& c, db::CellDef& root, db::Coord dx, db::Coord dy) {
  const db::Transform t = db::translation(-dx, -dy);
  db::CellDef moved("__MOVE__", root.plane_count());
  (void)ops::copy_paint(root, db::whole_plane(), t, moved, *c.tech, tech::TypeMask::all());
  for (std::size_t p = 0; p < root.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    root.plane(plane) = moved.plane(plane).clone();
  }
  for (db::Label& l : root.labels()) {
    l.rect = db::apply(t, l.rect);
    if (l.text_bbox) {
      l.text_bbox = db::apply(t, *l.text_bbox);
    }
  }
  for (db::Element& e : root.elements()) {
    e.area = db::apply(t, e.area);
  }
  for (const auto& u : root.uses()) {
    u->transform = db::compose(t, u->transform);
    db::compute_use_bbox(*u);
  }
  move_properties(root, -dx, -dy);  // DBMoveCell's DBPropEnum(dbMoveProp)
  db::mark_modified(root);
  db::recompute_bbox(root, c.s.magic_compat);
  c.ed->undo.clear();
  c.ed->windows.area_changed(root, root.extended_bbox(), std::nullopt);
}

CommandResult cmd_move_in(const Context& c, const Invocation& inv) {
  if (inv.args.size() > 3) {
    return fail_text(codes::kUsage, copy_move_usage("move", true));
  }
  if (!inv.args.empty() && !edit_box(c)) {
    return CommandResult::success();
  }
  const bool origin = !inv.args.empty() && inv.args[0] == "origin";
  auto d = displacement(c, inv.args, origin ? 1 : 0, false, "Move");
  if (!d.ok()) {
    return d.error().text.empty() ? fail_text(codes::kUsage, copy_move_usage("move", true)) : cu::fail(d.error());
  }
  const db::Point delta = d.value().d;
  if (origin) {
    move_origin(c, *c.root, delta.x, delta.y);
    const auto box = ops::root_box(c);
    if (box) {
      ops::set_root_box(c, db::apply(db::translation(-delta.x, -delta.y), *box));
    }
    c.ed->windows.translate(delta.x, delta.y);  // WindTranslate: the layout keeps its screen place
    return CommandResult::success();
  }
  const db::Transform t = db::translation(delta.x, delta.y);
  move_box(c, t);
  Snapshot before = c.ed->selection.snapshot();
  ops::transform_selection(c, t);
  ops::commit(c, std::move(before));
  return logged_to_point(c, inv, "move");
}

CommandResult cmd_move(CommandContext& ctx, const Invocation& inv) {
  return with_context(ctx, inv, cmd_move_in);
}

// ---- stretch ----------------------------------------------------------------------

/// The stretch displacement: DIR [AMOUNT] (Manhattan only), or the pointer
/// minus the box lower-left reduced to its larger axis.
diag::Outcome<db::Point> stretch_delta(const Context& c, const std::vector<std::string>& args) {
  if (args.empty()) {
    auto p = point_form(c, args, 0, "Stretch");
    if (!p.ok()) {
      return p.error();
    }
    db::Point d = p.value();
    (std::abs(d.x) >= std::abs(d.y) ? d.y : d.x) = 0;  // the larger axis only
    return d;
  }
  const int dir = db::parse_position(args[0], true);
  if (dir != db::kNorth && dir != db::kSouth && dir != db::kEast && dir != db::kWest) {
    return diag::make(codes::kUsage, "Usage: stretch [direction [amount]]");
  }
  auto a = amount(c, args.size() == 2 ? args[1] : std::string{"1l"});
  if (!a.ok()) {
    return a.error();
  }
  return delta_of(dir, a.value(), a.value());
}

/// CmdStretch recasts the pointer form as `stretch DIR Ni` for the log.
CommandResult logged_stretch(const Invocation& inv, const db::Point& d) {
  CommandResult result = CommandResult::success();
  std::string dir = "w";
  db::Coord n = -d.x;
  if (d.y != 0) {
    dir = d.y > 0 ? "n" : "s";
    n = d.y > 0 ? d.y : -d.y;
  } else if (d.x > 0) {
    dir = "e";
    n = d.x;
  }
  result.logged_as = Invocation{"stretch", {dir, std::to_string(n) + "i"}, inv.origin, std::nullopt};
  return result;
}

CommandResult cmd_stretch_in(const Context& c, const Invocation& inv) {
  if (inv.args.size() > 2) {
    return fail_text(codes::kUsage, "Usage: stretch [direction [amount]]");
  }
  auto delta = stretch_delta(c, inv.args);
  if (!delta.ok()) {
    return cu::fail(delta.error());
  }
  const db::Point d = delta.value();
  if (!edit_box(c)) {
    return CommandResult::success();
  }
  move_box(c, db::translation(d.x, d.y));
  Snapshot before = c.ed->selection.snapshot();
  ops::stretch_selection(c, d.x, d.y);
  ops::commit(c, std::move(before));
  return inv.args.empty() ? logged_stretch(inv, d) : CommandResult::success();
}

CommandResult cmd_stretch(CommandContext& ctx, const Invocation& inv) {
  return with_context(ctx, inv, cmd_stretch_in);
}

// ---- rotations, flips and orient ------------------------------------------------------

constexpr db::Transform kClockwise90{0, 1, 0, -1, 0, 0};
constexpr db::Transform kRotate180{-1, 0, 0, 0, -1, 0};
constexpr db::Transform kClockwise270{0, -1, 0, 1, 0, 0};
constexpr db::Transform kSideways{-1, 0, 0, 0, 1, 0};
constexpr db::Transform kUpsideDown{1, 0, 0, 0, -1, 0};
constexpr db::Transform kRef135{0, -1, 0, -1, 0, 0};  // x' = -y, y' = -x
constexpr db::Transform kRef45{0, 1, 0, 1, 0, 0};     // x' = y, y' = x

/// Applies `t2` to the selection, keeping its lower-left corner unless
/// `about_origin`; moves the box the same way.
CommandResult apply_transform(const Context& c, const db::Transform& t2, bool about_origin) {
  db::Transform t = t2;
  if (db::CellDef* def = c.ed->selection.def(); def != nullptr && !about_origin) {
    db::recompute_bbox(*def);
    const db::Rect b = def->bbox();
    const db::Rect moved = db::apply(t2, b);
    t = db::compose(db::translation(b.x0 - moved.x0, b.y0 - moved.y0), t2);
  }
  Snapshot before = c.ed->selection.snapshot();
  ops::transform_selection(c, t);
  ops::commit(c, std::move(before));
  const auto box = ops::root_box(c);
  if (box && c.ed->selection.root() == c.root) {
    ops::set_root_box(c, db::apply(t, *box));
  }
  return CommandResult::success();
}

bool takes_origin_flag(const std::vector<std::string>& args) {
  return !args.empty() && args.back().rfind("-orig", 0) == 0;
}

/// The clockwise angle argument: 90 by default, an integer otherwise.
std::optional<int> clockwise_degrees(const std::vector<std::string>& args, std::size_t n) {
  if (n == 0) {
    return 90;
  }
  if (n > 1 || !cu::str_is_int(args[0])) {
    return std::nullopt;
  }
  const int degrees = cu::c_atoi(args[0]);
  return degrees < 0 ? degrees + 360 : degrees;
}

CommandResult cmd_clockwise_in(const Context& c, const Invocation& inv) {
  const bool origin = takes_origin_flag(inv.args);
  const auto degrees = clockwise_degrees(inv.args, inv.args.size() - (origin ? 1 : 0));
  if (!degrees) {
    return fail_text(codes::kUsage, "Usage: " + inv.name + " [degrees]");
  }
  if (!edit_box(c)) {
    return CommandResult::success();
  }
  switch (*degrees) {
    case 90:
      return apply_transform(c, kClockwise90, origin);
    case 180:
      return apply_transform(c, kRotate180, origin);
    case 270:
      return apply_transform(c, kClockwise270, origin);
    default:
      return fail_text(codes::kBadAngle, "Rotation angle must be 90, 180, or 270 degrees");
  }
}

CommandResult cmd_clockwise(CommandContext& ctx, const Invocation& inv) {
  return with_context(ctx, inv, cmd_clockwise_in);
}

CommandResult flip(CommandContext& ctx, const Invocation& inv, const db::Transform& t) {
  return with_context(ctx, inv, [&](const Context& c, const Invocation&) {
    if (!inv.args.empty()) {
      return fail_text(codes::kUsage, "Usage: " + inv.name);
    }
    if (!edit_box(c)) {
      return CommandResult::success();
    }
    return apply_transform(c, t, false);
  });
}

CommandResult cmd_sideways(CommandContext& ctx, const Invocation& inv) {
  return flip(ctx, inv, kSideways);
}

CommandResult cmd_upsidedown(CommandContext& ctx, const Invocation& inv) {
  return flip(ctx, inv, kUpsideDown);
}

/// The orient keyword table (CmdOrient), matched case-insensitively with
/// Magic's unique-prefix rule; `180v` is sideways (Q-CMDEDIT-62).
std::optional<db::Transform> orientation(std::string_view word) {
  struct Name {
    std::string_view name;
    db::Transform t;
  };
  static const std::array<Name, 22> kNames = {{
      {"0", db::kIdentity}, {"90", kClockwise90}, {"180", kRotate180}, {"270", kClockwise270},
      {"v", kUpsideDown},   {"0v", kUpsideDown},  {"90v", kRef45},     {"180v", kSideways},
      {"270v", kRef135},    {"h", kSideways},     {"0h", kSideways},   {"90h", kRef135},
      {"180h", kUpsideDown}, {"270h", kRef45},    {"N", db::kIdentity}, {"E", kClockwise90},
      {"S", kRotate180},    {"W", kClockwise270}, {"FN", kSideways},   {"FE", kRef135},
      {"FS", kUpsideDown},  {"FW", kRef45},
  }};
  const auto lower = [](std::string_view s) {
    std::string out{s};
    for (char& ch : out) {
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
  };
  const std::string w = lower(word);
  std::optional<db::Transform> found;
  int matches = 0;
  for (const Name& n : kNames) {
    const std::string name = lower(n.name);
    if (name == w) {
      return n.t;
    }
    if (!w.empty() && name.rfind(w, 0) == 0) {
      ++matches;
      found = n.t;
    }
  }
  return matches == 1 ? found : std::nullopt;
}

CommandResult cmd_orient_in(const Context& c, const Invocation& inv) {
  const bool origin = takes_origin_flag(inv.args);
  const std::size_t n = inv.args.size() - (origin ? 1 : 0);
  if (!edit_box(c)) {
    return CommandResult::success();
  }
  const auto t = n == 1 ? orientation(inv.args[0]) : std::nullopt;
  if (!t) {
    return fail_text(codes::kUsage, "Usage: " + inv.name + " [orientation]");
  }
  return apply_transform(c, *t, origin);
}

CommandResult cmd_orient(CommandContext& ctx, const Invocation& inv) {
  return with_context(ctx, inv, cmd_orient_in);
}

// ---- array ---------------------------------------------------------------------------

constexpr std::array<std::string_view, 6> kArrayOptions = {"count", "width", "height", "pitch", "position", "help"};
constexpr std::array<std::string_view, 6> kArrayHelp = {
    "count \t\t[[xlo] xhi [ylo] yhi]\tarray subcells",
    "width \t\t[value]\t\t\tset or return array x-spacing",
    "height \t[value]\t\t\tset or return array y-spacing",
    "pitch \t\t[x y]\t\t\tset or return array spacing",
    "position \t[x y]\t\t\tset or return array origin",
    "help\t\t\t\t\tprint help information",
};

CommandResult array_help(const Context& c) {
  std::string out;
  for (const std::string_view line : kArrayHelp) {
    out += (out.empty() ? "    " : "\n    ") + std::string{line};
  }
  cu::print(c.s, std::move(out));
  return CommandResult::success();
}

/// selGetArrayFunc: a selected use's array in root orientation.
struct ArrayReport {
  const db::CellUse* use = nullptr;
  db::ArrayInfo info;
  db::Rect bbox;  // root coordinates
};

std::vector<ArrayReport> selected_arrays(const Context& c) {
  std::vector<ArrayReport> out;
  for (const auto& u : c.ed->selection.def()->uses()) {
    const auto found = ops::layout_use(c, *u);
    const db::CellUse& use = found ? *found->use : *u;
    const db::Transform& t = u->transform;
    db::ArrayInfo a = use.array;
    if (t.a == 0) {
      std::swap(a.xlo, a.ylo);
      std::swap(a.xhi, a.yhi);
    }
    const db::Coord ysep = (t.d * use.array.xsep - t.a * use.array.ysep) / (t.d * t.b - t.a * t.e);
    const db::Coord xsep = t.a == 0 ? (use.array.ysep - t.e * ysep) / t.d : (use.array.xsep - t.b * ysep) / t.a;
    a.xsep = xsep;
    a.ysep = ysep;
    out.push_back(ArrayReport{&use, a, u->bbox});
  }
  return out;
}

/// The values one query reports for one use.
Value::List array_values(const Context& c, int option, const ArrayReport& r) {
  switch (option) {
    case 0:
      return {r.info.xlo, r.info.xhi, r.info.ylo, r.info.yhi};
    case 1:
      return {cu::coord_value(c.s, r.info.xsep)};
    case 2:
      return {cu::coord_value(c.s, r.info.ysep)};
    case 3:
      return {cu::coord_value(c.s, r.info.xsep), cu::coord_value(c.s, r.info.ysep)};
    default:
      return {cu::coord_value(c.s, r.bbox.x0), cu::coord_value(c.s, r.bbox.y0)};
  }
}

std::string array_line(const Context& c, int option, const ArrayReport& r) {
  std::string out = "Cell use \"" + r.use->id + "\":";
  const Value::List v = array_values(c, option, r);
  switch (option) {
    case 0:
      return out + "x index " + v[0].to_string() + " to " + v[1].to_string() + ", y index " + v[2].to_string() +
             " to " + v[3].to_string();
    case 1:
      return out + "x separation " + v[0].to_string();
    case 2:
      return out + "y separation " + v[0].to_string();
    case 3:
      return out + "x separation " + v[0].to_string() + " y separation " + v[1].to_string();
    default:
      return out + "x=" + v[0].to_string() + " y=" + v[1].to_string();
  }
}

/// The queries: printed per use, or with -list the last use's values
/// (magic-compat) or one sublist per use (Q-CMDEDIT-63).
CommandResult array_query(const Context& c, int option, bool list) {
  const std::vector<ArrayReport> reports = selected_arrays(c);
  if (!list) {
    for (const ArrayReport& r : reports) {
      cu::print(c.s, array_line(c, option, r));
    }
    return CommandResult::success();
  }
  if (c.s.magic_compat) {
    return CommandResult::success(reports.empty() ? Value{} : Value{array_values(c, option, reports.back())});
  }
  Value::List all;
  for (const ArrayReport& r : reports) {
    all.emplace_back(array_values(c, option, r));
  }
  return CommandResult::success(Value{all});
}

/// `array [count] NX NY` / `XLO XHI YLO YHI`: the selection becomes arrays.
CommandResult array_make(const Context& c, const std::vector<std::string>& nums) {
  for (const std::string& n : nums) {
    if (!cu::str_is_int(n)) {
      return array_help(c);
    }
  }
  db::ArrayInfo a;
  if (nums.size() == 2) {
    a.xhi = cu::c_atoi(nums[0]) - 1;
    a.yhi = cu::c_atoi(nums[1]) - 1;
    if (a.xhi < 0 || a.yhi < 0) {
      return array_help(c);
    }
  } else {
    a.xlo = cu::c_atoi(nums[0]);
    a.xhi = cu::c_atoi(nums[1]);
    a.ylo = cu::c_atoi(nums[2]);
    a.yhi = cu::c_atoi(nums[3]);
  }
  const auto box = ops::root_box(c);
  if (!box) {
    return fail_text(codes::kNoBox, "Position the box to indicate the array spacing.");
  }
  a.xsep = box->x1 - box->x0;
  a.ysep = box->y1 - box->y0;
  Snapshot before = c.ed->selection.snapshot();
  ops::array_selection(c, a);
  ops::commit(c, std::move(before));
  return CommandResult::success();
}

/// An option with arguments: `count` makes arrays, the setters are not
/// implemented (Q-CMDEDIT-63).
CommandResult array_option(const Context& c, int option, const std::vector<std::string>& a, bool list) {
  if (option == 5) {
    return array_help(c);
  }
  if (a.size() == 1) {
    return array_query(c, option, list);
  }
  if (option != 0) {
    cu::print(c.s, "Unimplemented function.");
    return CommandResult::success();
  }
  if (a.size() == 3 || a.size() == 5) {
    return array_make(c, std::vector<std::string>(a.begin() + 1, a.end()));
  }
  return array_help(c);
}

CommandResult cmd_array_in(const Context& c, const Invocation& inv) {
  std::vector<std::string> a = inv.args;
  const bool list = !a.empty() && a[0].rfind("-list", 0) == 0;
  if (list) {
    a.erase(a.begin());
  }
  if (a.empty()) {
    return array_help(c);
  }
  const int option = cu::lookup_in(a[0], kArrayOptions);
  if (option < 0 && a.size() != 2 && a.size() != 4) {
    return array_help(c);
  }
  if (!edit_box(c)) {
    return CommandResult::success();
  }
  return option < 0 ? array_make(c, a) : array_option(c, option, a, list);
}

CommandResult cmd_array(CommandContext& ctx, const Invocation& inv) {
  return with_context(ctx, inv, cmd_array_in);
}

}  // namespace

bool register_select_commands(cmd::CommandRegistry& registry) {
  struct Entry {
    const char* name;
    const char* synopsis;
    const char* help;
    cmd::Handler handler;
  };
  const std::array<Entry, 14> kCommands = {{
      {"array", "array ?-list? ?count? xsize ysize | xlo xhi ylo yhi | width|height|pitch|position ?values?",
       "Arrays the selected subcells, paint and labels; the box size gives the spacing.", cmd_array},
      {"clockwise", "clockwise ?degrees? ?-origin?", "Rotates the selection clockwise.", cmd_clockwise},
      {"copy", "copy ?direction ?amount?? | copy dx dy | copy to x y", "Copies the selection.", cmd_copy},
      {"delete", "delete", "Deletes the selection from the edit cell.", cmd_delete},
      {"id", "id name", "Synonym of identify.", cmd_identify},
      {"identify", "identify name", "Sets the identifier of the selected instance.", cmd_identify},
      {"move", "move ?origin? ?direction ?amount?? | move ?origin? to x y", "Moves the selection.", cmd_move},
      {"orient", "orient orientation ?-origin?", "Changes the orientation of the selection.", cmd_orient},
      {"rotate", "rotate ?degrees? ?-origin?", "Synonym of clockwise.", cmd_clockwise},
      {"select", "select ?more|less|nocycle|same|do|no|simple|top? ?option? ?args? ?at x y?",
       "Selects layout items (select help lists the options).", cmd_select},
      {"sideways", "sideways", "Flips the selection left to right.", cmd_sideways},
      {"stretch", "stretch ?direction ?amount??", "Stretches the selection.", cmd_stretch},
      {"upsidedown", "upsidedown", "Flips the selection top to bottom.", cmd_upsidedown},
      {"what", "what ?-list|-listall?", "Describes the selection.", cmd_what},
  }};
  bool ok = true;
  for (const Entry& e : kCommands) {
    ok = cu::add(registry, e.name, e.synopsis, e.help, e.handler) && ok;
  }
  return ok;
}

}  // namespace hefesto::select
