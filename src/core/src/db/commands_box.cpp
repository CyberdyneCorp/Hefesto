// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `box` command (capability cmd-box): queries, setters, move/grow/
// shrink, corner and cursor forms, `-edit`, normalization and bounds, remove,
// help and the default report. Ported from Magic VLSI 8.3.694 (fc5daa9e)
// commands/CmdAB.c (CmdBox) and dbwind/DBWtools.c (ToolMoveBox,
// ToolMoveCorner), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md). Hefesto's decisions Q-CMDEDIT-8..12
// are noted where they apply. Owner: EDIT.

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "db/command_util.hpp"
#include "db/edit_util.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::db {

namespace cu = cmdutil;
using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using cmd::Value;

namespace {

enum class Option : std::uint8_t {
  width, height, size, position, values, remove, select, move, grow, shrink, corner, exists, help, report
};

constexpr std::array<std::string_view, 13> kOptionNames = {"width", "height", "size",   "position", "values",
                                                           "remove", "select", "move",   "grow",     "shrink",
                                                           "corner", "exists", "help"};

constexpr std::array<std::string_view, 13> kOptionHelp = {
    "width\t\t[value]\t\t\tset or return box width",
    "height\t\t[value]\t\t\tset or return box height",
    "size\t\t[width height]\t\tset or return box size",
    "position\t\t[llx lly] [-edit]\tset or return box position",
    "values\t\t[llx lly urx ury] [-edit]\tset or return box coordinates",
    "remove\t\t\t\t\tremove cursor box from display",
    "select\t\t\t\t\tset box to selection bounding box",
    "move\t\t<direction> <distance> \tmove box position",
    "grow\t\t<direction> <distance>\texpand box size",
    "shrink\t\t<direction> <distance>  shrink box size",
    "corner\t\t<direction> <x> <y> | cursor\tset box corner",
    "exists\t\t\t\t\tis the cursor box present?",
    "help\t\t\t\t\tprint help information",
};

// The parsed command: the option, its arguments (the option word first,
// `-edit` removed) and the frame.
struct Request {
  cu::Session s;
  Option option = Option::report;
  std::vector<std::string> args;
  bool edit = false;
};

Option parse_option(const std::vector<std::string>& args) {
  if (args.empty()) {
    return Option::report;
  }
  const cmd::LookupResult r = cmd::lookup(args[0], kOptionNames);
  if (r.match == cmd::Match::ambiguous && !args[0].empty() && args[0][0] == 'h') {
    return Option::height;  // Magic's hack: `h` means height despite `help`
  }
  return r.match == cmd::Match::found ? static_cast<Option>(r.index) : Option::report;
}

CommandResult help(const cu::Session& s) {
  std::string text;
  for (const std::string_view line : kOptionHelp) {
    text += (text.empty() ? "    " : "\n    ") + std::string{line};
  }
  cu::print(s, std::move(text));
  return CommandResult::success();
}

// --- reports ----------------------------------------------------------------

using LineBuf = std::array<char, 256>;

// The microns or lambda line of CmdBox's report: values in float times
// `scale`, promoted to double for printf as in C.
std::string scaled_line(const Rect& r, float scale, bool lambda_line, std::int64_t area) {
  const auto f = [scale](std::int64_t v) { return static_cast<double>(static_cast<float>(v) * scale); };
  const double a = static_cast<double>(static_cast<float>(area) * scale * scale);
  LineBuf buf{};
  if (lambda_line) {
    std::snprintf(buf.data(), buf.size(), "   %6.2f x %-6.2f  (% 6.2f, % -6.2f), (% 6.2f, % -6.2f)", f(r.width()),
                  f(r.height()), f(r.x0), f(r.y0), f(r.x1), f(r.y1));
  } else {
    std::snprintf(buf.data(), buf.size(), "microns:  %6.3f x %-6.3f  (% 6.3f, % -6.3f), (% 6.3f, % -6.3f)",
                  f(r.width()), f(r.height()), f(r.x0), f(r.y0), f(r.x1), f(r.y1));
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
  std::snprintf(buf.data(), buf.size(), " %6" PRId64 " x %-6" PRId64 "  (% 6d, % -6d), (% 6d, % -6d)", r.width(),
                r.height(), r.x0, r.y0, r.x1, r.y1);
  std::string out = buf.data();
  if (area > 0) {
    LineBuf tail{};
    std::snprintf(tail.data(), tail.size(), "  %-10" PRId64, area);
    out += tail.data();
  }
  return out;
}

// CmdBox's BOX_DEFAULT report (Magic ref: commands/CmdAB.c:1184-1226).
std::string box_report(const cu::Session& s, const Rect& r, bool edit) {
  const std::int64_t area = r.width() * r.height();
  const tech::GridScale& g = s.editor->grid;
  // CIFGetOutputScale(1000); no output style: 1.0.
  float oscale = 1.0F;
  if (const auto& cif = cu::coordinate_units(s).cif_scale) {
    oscale = static_cast<float>(10 * (*cif)[0]) / static_cast<float>((*cif)[1] * 1000);
  }
  std::string out = std::string{edit ? "Edit" : "Root"} +
                    " cell box:\n           width x height  (   llx,  lly  ), (   urx,  ury  )";
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

Value pair(const cu::Session& s, std::int64_t x, std::int64_t y) {
  return Value{Value::List{cu::report_value(s, x, cmd::Axis::x), cu::report_value(s, y, cmd::Axis::y)}};
}

// The queries of "Box queries" (no value arguments).
CommandResult query(const cu::Session& s, Option option, const Rect& r) {
  switch (option) {
    case Option::width:
      return CommandResult::success(cu::report_value(s, r.width(), cmd::Axis::x));
    case Option::height:
      return CommandResult::success(cu::report_value(s, r.height(), cmd::Axis::y));
    case Option::size:
      return CommandResult::success(pair(s, r.width(), r.height()));
    case Option::position:
      return CommandResult::success(pair(s, r.x0, r.y0));
    default:
      break;
  }
  return CommandResult::success(Value{Value::List{cu::report_value(s, r.x0, cmd::Axis::x),
                                                  cu::report_value(s, r.y0, cmd::Axis::y),
                                                  cu::report_value(s, r.x1, cmd::Axis::x),
                                                  cu::report_value(s, r.y1, cmd::Axis::y)}});
}

// --- applying a new box -------------------------------------------------------

// A box in 64-bit coordinates, so that setters cannot overflow before the
// bounds check.
struct Wide {
  std::int64_t x0 = 0;
  std::int64_t y0 = 0;
  std::int64_t x1 = 0;
  std::int64_t y1 = 0;
};

Wide widen(const Rect& r) {
  return Wide{r.x0, r.y0, r.x1, r.y1};
}

bool in_bounds(std::int64_t v) noexcept {
  return v >= kPlaneMin && v <= kPlaneMax;
}

// Q-CMDEDIT-12: both corners are checked, whatever magic-compat.
bool in_bounds(const Wide& w) noexcept {
  return in_bounds(w.x0) && in_bounds(w.y0) && in_bounds(w.x1) && in_bounds(w.y1);
}

Rect narrow(const Wide& w) {
  return Rect{static_cast<Coord>(w.x0), static_cast<Coord>(w.y0), static_cast<Coord>(w.x1), static_cast<Coord>(w.y1)};
}

CommandResult out_of_bounds() {
  return cu::fail(diag::make(codes::kBoxOutOfBounds, "Box out of bounds."));
}

// ToolMoveBox(TOOL_BL, ll) then ToolMoveCorner(TOOL_TR, ur): two box events,
// the result normalized ("Box normalization and bounds").
CommandResult apply_box(const Request& q, CellDef* root, const Wide& next) {
  if (!in_bounds(next)) {
    return out_of_bounds();
  }
  Rect r = narrow(next);
  if (q.edit) {
    r = editor::to_root(*q.s.editor, r);
  }
  editor::EditorState& ed = *q.s.editor;
  if (const std::optional<Rect> old = ed.box; old && edit::box_root(ed) == root) {
    const Wide moved{r.x0, r.y0, std::int64_t{r.x0} + old->width(), std::int64_t{r.y0} + old->height()};
    if (in_bounds(moved)) {
      edit::set_box(ed, root, narrow(moved));
    }
  }
  edit::set_box(ed, root, canonical(r));
  return CommandResult::success();
}

// --- value parsing ----------------------------------------------------------

diag::Outcome<std::int64_t> coord(const Request& q, const std::string& text, cmd::Axis axis, bool relative) {
  auto v = cu::parse_distance(q.s, text, axis, relative);
  if (!v.ok()) {
    return v.error();
  }
  return std::int64_t{v.value()};
}

// Four absolute coordinates starting at args[first].
diag::Outcome<Wide> rect_args(const Request& q, std::size_t first) {
  std::array<std::int64_t, 4> v{};
  for (std::size_t i = 0; i < 4; ++i) {
    auto c = coord(q, q.args[first + i], i % 2 == 0 ? cmd::Axis::x : cmd::Axis::y, false);
    if (!c.ok()) {
      return c.error();
    }
    v.at(i) = c.value();
  }
  return Wide{v[0], v[1], v[2], v[3]};
}

// `box values X0 Y0 X1 Y1` and the four-number form: no box needed.
CommandResult set_from_args(const Request& q, std::size_t first, bool report) {
  auto w = rect_args(q, first);
  if (!w.ok()) {
    return cu::fail(w.error());
  }
  if (report && in_bounds(w.value())) {
    // Magic reports the new box in root coordinates (refEdit is cleared).
    const Rect r = canonical(narrow(w.value()));
    cu::print(q.s, box_report(q.s, q.edit ? editor::to_root(*q.s.editor, r) : r, false));
  }
  return apply_box(q, edit::default_root(*q.s.editor), w.value());
}

// --- setters ----------------------------------------------------------------

CommandResult set_size(const Request& q, CellDef* root, const Rect& b) {
  Wide w = widen(b);
  const Option o = q.option;
  const std::size_t want = o == Option::size || o == Option::position ? 3 : 2;
  if (q.args.size() != want) {
    return help(q.s);
  }
  const bool size = o != Option::position;
  const auto first = coord(q, q.args[1], o == Option::height ? cmd::Axis::y : cmd::Axis::x, size);
  const auto second = want == 3 ? coord(q, q.args[2], cmd::Axis::y, size) : first;
  if (!first.ok() || !second.ok()) {
    return cu::fail(first.ok() ? second.error() : first.error());
  }
  if (o == Option::width || o == Option::size) {
    w.x1 = w.x0 + first.value();
  }
  if (o == Option::height) {
    w.y1 = w.y0 + first.value();
  }
  if (o == Option::size) {
    w.y1 = w.y0 + second.value();
  }
  if (o == Option::position) {
    w = Wide{first.value(), second.value(), first.value() + b.width(), second.value() + b.height()};
  }
  return apply_box(q, root, w);
}

// The deltas of each side for a direction: {dx0, dy0, dx1, dy1} per unit.
struct SideDelta {
  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;
};

SideDelta move_delta(cmd::Direction d) {
  const int dx = d == cmd::Direction::east || d == cmd::Direction::northeast || d == cmd::Direction::southeast ? 1
                 : d == cmd::Direction::west || d == cmd::Direction::northwest || d == cmd::Direction::southwest
                     ? -1
                     : 0;
  const int dy = d == cmd::Direction::north || d == cmd::Direction::northeast || d == cmd::Direction::northwest ? 1
                 : d == cmd::Direction::south || d == cmd::Direction::southeast || d == cmd::Direction::southwest
                     ? -1
                     : 0;
  return SideDelta{dx, dy, dx, dy};
}

// grow moves the named sides outward; center moves all four.
SideDelta grow_delta(cmd::Direction d) {
  if (d == cmd::Direction::center) {
    return SideDelta{-1, -1, 1, 1};
  }
  const SideDelta m = move_delta(d);
  return SideDelta{m.x0 < 0 ? -1 : 0, m.y0 < 0 ? -1 : 0, m.x1 > 0 ? 1 : 0, m.y1 > 0 ? 1 : 0};
}

// The distance of `box move|grow|shrink DIR DIST` per axis (user units
// read DIST along the direction's axis, both for diagonals).
diag::Outcome<std::array<std::int64_t, 2>> distances(const Request& q, cmd::Direction d) {
  const std::string& text = q.args[2];
  if (q.s.editor->units.unit != cmd::InputUnit::user) {
    auto v = coord(q, text, cmd::Axis::y, true);  // Magic reads it as a y distance
    return v.ok() ? diag::Outcome<std::array<std::int64_t, 2>>{std::array{v.value(), v.value()}}
                  : diag::Outcome<std::array<std::int64_t, 2>>{v.error()};
  }
  const bool ew = d == cmd::Direction::east || d == cmd::Direction::west;
  const bool ns = d == cmd::Direction::north || d == cmd::Direction::south;
  auto x = coord(q, text, ns ? cmd::Axis::y : cmd::Axis::x, true);
  auto y = coord(q, text, ew ? cmd::Axis::x : cmd::Axis::y, true);
  if (!x.ok() || !y.ok()) {
    return x.ok() ? y.error() : x.error();
  }
  return std::array{x.value(), y.value()};
}

CommandResult move_grow_shrink(const Request& q, CellDef* root, const Rect& b) {
  if (q.args.size() != 3) {
    return help(q.s);
  }
  const auto d = cmd::parse_direction(q.args[1]);
  if (!d.ok()) {
    return cu::fail(d.error());
  }
  const auto dist = distances(q, d.value());
  if (!dist.ok()) {
    return cu::fail(dist.error());
  }
  const auto [dx, dy] = dist.value();
  if (dx == 0 && dy == 0) {
    return CommandResult::success();
  }
  SideDelta k = q.option == Option::move ? move_delta(d.value()) : grow_delta(d.value());
  if (q.option == Option::shrink) {
    k = SideDelta{-k.x0, -k.y0, -k.x1, -k.y1};
  }
  const Wide w{b.x0 + k.x0 * dx, b.y0 + k.y0 * dy, b.x1 + k.x1 * dx, b.y1 + k.y1 * dy};
  return apply_box(q, root, Wide{std::min(w.x0, w.x1), std::min(w.y0, w.y1), std::max(w.x0, w.x1),
                                 std::max(w.y0, w.y1)});
}

// --- corners ----------------------------------------------------------------

// The reference corner of a direction; Manhattan and center act as sw
// (Q-CMDEDIT-10).
Point corner_of(const Rect& r, cmd::Direction d) {
  switch (d) {
    case cmd::Direction::northeast:
      return Point{r.x1, r.y1};
    case cmd::Direction::northwest:
      return Point{r.x0, r.y1};
    case cmd::Direction::southeast:
      return Point{r.x1, r.y0};
    default:
      return Point{r.x0, r.y0};
  }
}

// ToolMoveCorner within one root: only that corner moves, then the box is
// turned right side out ("Box corner dragging").
Wide moved_corner(const Rect& r, cmd::Direction d, Point p) {
  Wide w = widen(r);
  const bool east = d == cmd::Direction::northeast || d == cmd::Direction::southeast;
  const bool north = d == cmd::Direction::northeast || d == cmd::Direction::northwest;
  (east ? w.x1 : w.x0) = p.x;
  (north ? w.y1 : w.y0) = p.y;
  return Wide{std::min(w.x0, w.x1), std::min(w.y0, w.y1), std::max(w.x0, w.x1), std::max(w.y0, w.y1)};
}

// ToolMoveBox: the box translated so its reference corner lands on `p`.
Wide moved_box(const Rect& r, cmd::Direction d, Point p) {
  const Point c = corner_of(r, d);
  const std::int64_t dx = std::int64_t{p.x} - c.x;
  const std::int64_t dy = std::int64_t{p.y} - c.y;
  return Wide{r.x0 + dx, r.y0 + dy, r.x1 + dx, r.y1 + dy};
}

// `box corner DIR X Y` (Q-CMDEDIT-9: Hefesto's form; four words print the usage).
CommandResult corner_abs(const Request& q, CellDef* root, const Rect& b) {
  if (q.args.size() == 3) {
    cu::print(q.s, "Usage: box corner <direction> <x> <y> | box corner <direction> cursor");
    return CommandResult::success();
  }
  if (q.args.size() != 4) {
    return help(q.s);
  }
  const auto d = cmd::parse_direction(q.args[1]);
  if (!d.ok()) {
    return cu::fail(d.error());
  }
  const auto x = coord(q, q.args[2], cmd::Axis::x, false);
  const auto y = coord(q, q.args[3], cmd::Axis::y, false);
  if (!x.ok() || !y.ok()) {
    return cu::fail(x.ok() ? y.error() : x.error());
  }
  if (!in_bounds(x.value()) || !in_bounds(y.value())) {
    return out_of_bounds();
  }
  const Point p{static_cast<Coord>(x.value()), static_cast<Coord>(y.value())};
  return apply_box(q, root, moved_corner(b, d.value(), p));
}

// The pointer, snapped ("Pointer snapping"), and the root of its window.
struct Pointer {
  Point point;
  CellDef* root = nullptr;
};

cmd::SnapGrid snap_grid(const editor::EditorState& ed, const view::LayoutWindow& w) {
  cmd::SnapGrid g;
  if (ed.snap == editor::SnapMode::lambda) {
    const auto pitch = static_cast<Coord>(std::max<std::int64_t>(1, ed.grid.internal / ed.grid.lambda));
    g.pitch_x = pitch;
    g.pitch_y = pitch;
  } else if (ed.snap == editor::SnapMode::user) {
    const Rect s = w.grid().spacing;
    g.origin = Point{s.x0, s.y0};
    g.pitch_x = s.x1 - s.x0;
    g.pitch_y = s.y1 - s.y0;
  }
  return g;
}

diag::Outcome<Pointer> pointer(const cu::Session& s) {
  const editor::EditorState& ed = *s.editor;
  if (!ed.command_point) {
    return cu::bad_argument("Cursor not in a window.");
  }
  const view::LayoutWindow* w = ed.windows.find(ed.command_point->window);
  if (w == nullptr || w->root() == nullptr) {
    return cu::bad_argument("Can't put box there.");
  }
  const Point p = ed.snap == editor::SnapMode::internal ? ed.command_point->point
                                                         : cmd::snap_point(ed.command_point->point, snap_grid(ed, *w));
  return Pointer{p, w->root()};
}

// The success of a cursor form, logged as `box values Xi Yi Xi Yi` with the
// resulting box (CmdBox's TxRebuildCommand).
CommandResult logged_as_values(const editor::EditorState& ed) {
  CommandResult result = CommandResult::success();
  if (ed.box) {
    const auto i = [](db::Coord v) { return std::to_string(v) + "i"; };
    result.logged_as = Invocation{"box", {"values", i(ed.box->x0), i(ed.box->y0), i(ed.box->x1), i(ed.box->y1)}, cmd::Origin::script, std::nullopt};
  }
  return result;
}

// The cursor forms of "Box cursor forms", logged as `box values ...`.
CommandResult cursor_form(const Request& q) {
  if (q.args.size() != 3) {
    return help(q.s);
  }
  const auto d = cmd::parse_direction(q.args[1]);
  if (!d.ok()) {
    return cu::fail(d.error());
  }
  const auto p = pointer(q.s);
  if (!p.ok()) {
    return cu::fail(p.error());
  }
  editor::EditorState& ed = *q.s.editor;
  CellDef* root = edit::box_root(ed);
  const Rect b = ed.box ? *ed.box : Rect{};
  if (!ed.box) {
    root = p.value().root;
  }
  if (q.option == Option::grow || q.option == Option::shrink) {
    return logged_as_values(ed);  // Q-CMDEDIT-11: Magic changes nothing
  }
  const bool whole = q.option == Option::move || root != p.value().root;
  const Wide next = whole ? moved_box(b, d.value(), p.value().point) : moved_corner(b, d.value(), p.value().point);
  if (!in_bounds(next)) {
    return out_of_bounds();
  }
  edit::set_box(ed, p.value().root, narrow(next));
  return logged_as_values(ed);
}

bool is_cursor_form(const Request& q) {
  const Option o = q.option;
  const bool takes_cursor = o == Option::move || o == Option::grow || o == Option::shrink || o == Option::corner;
  return takes_cursor && q.args.size() > 1 && q.args.back().starts_with("cursor");
}

// --- dispatch -----------------------------------------------------------------

// The box in the working frame (edit coordinates under -edit) and its root.
struct Current {
  CellDef* root = nullptr;
  Rect box;
};

diag::Outcome<Current> current(const Request& q) {
  const editor::EditorState& ed = *q.s.editor;
  if (q.edit) {
    auto b = edit::edit_box(q.s);
    if (!b.ok()) {
      return b.error();
    }
    return Current{edit::box_root(ed), b.value()};
  }
  if (!ed.box) {
    return diag::make(codes::kNoBox, "Box tool must be present");
  }
  return Current{edit::box_root(ed), *ed.box};
}

CommandResult select_box(const Request& q, const Current& c) {
  if (q.args.size() != 1) {
    return help(q.s);
  }
  const CellDef* sel = q.s.editor->selection.def();
  const Rect area = sel != nullptr ? sel->bbox() : Rect{};
  return apply_box(q, c.root, widen(q.edit ? editor::to_edit(*q.s.editor, area) : area));
}

CommandResult with_box(const Request& q, const Current& c) {
  switch (q.option) {
    case Option::width:
    case Option::height:
    case Option::size:
    case Option::position:
      return q.args.size() == 1 ? query(q.s, q.option, c.box) : set_size(q, c.root, c.box);
    case Option::values:
      return q.args.size() == 1 ? query(q.s, q.option, c.box) : help(q.s);
    case Option::select:
      return select_box(q, c);
    case Option::move:
    case Option::grow:
    case Option::shrink:
      return move_grow_shrink(q, c.root, c.box);
    case Option::corner:
      return corner_abs(q, c.root, c.box);
    default:
      break;
  }
  // The default form (Q-CMDEDIT-8: an unknown first word only reports).
  cu::print(q.s, box_report(q.s, c.box, q.edit));
  return CommandResult::success();
}

CommandResult without_box(const Request& q) {
  switch (q.option) {
    case Option::remove:
      edit::set_box(*q.s.editor, nullptr, std::nullopt);
      return CommandResult::success();
    case Option::exists:
      return CommandResult::success(Value{q.s.editor->box ? 1 : 0});
    case Option::help:
      return help(q.s);
    case Option::values:
      return set_from_args(q, 1, false);
    default:
      return set_from_args(q, 0, true);
  }
}

bool needs_no_box(const Request& q) {
  switch (q.option) {
    case Option::remove:
    case Option::exists:
    case Option::help:
      return true;
    case Option::values:
      return q.args.size() == 5;
    case Option::report:
      return q.args.size() == 4;
    default:
      return false;
  }
}

CommandResult cmd_box(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  Request q{s.value(), Option::report, inv.args, false};
  if (q.args.size() > 6) {
    return help(q.s);
  }
  if (!q.args.empty() && q.args.back().starts_with("-edit")) {
    q.edit = true;
    q.args.pop_back();
  }
  q.option = parse_option(q.args);
  if (needs_no_box(q)) {
    return without_box(q);
  }
  if (is_cursor_form(q)) {
    return cursor_form(q);
  }
  const auto c = current(q);
  if (!c.ok()) {
    return cu::fail(c.error());
  }
  return with_box(q, c.value());
}

}  // namespace

namespace cmdutil {

bool register_box_commands(cmd::CommandRegistry& registry) {
  return add(registry, "box", "box ?option? ?args? ?-edit?",
             "Shows or changes the box: width, height, size, position, values, remove, select, move, grow, shrink, "
             "corner, exists, help.",
             cmd_box);
}

}  // namespace cmdutil

}  // namespace hefesto::db
