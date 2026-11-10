// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// tool, cursor, setpoint, pushbutton and the pick tool's tools.tcl button
// helpers keepselect, startselect, cancelselect (the wiring tool's
// `trackwire` is the wiring area's) (ui-tools-and-input
// "tool command", "Box tool buttons", "Other tools' buttons", "cursor
// command"; ui-command-dispatch "pushbutton and setpoint"; ui-gui-managers
// "Tool modes"). Owner: DISPATCH.
// (Magic ref: tcltk/tools.tcl:584-760, windows/windCmdAM.c:460-590,
// windows/windCmdSZ.c:214-294, windows/windCmdNR.c:203-260)

#include <array>
#include <cstdio>
#include <utility>

#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/ui/bindings.hpp"
#include "hefesto/ui/commands.hpp"
#include "hefesto/ui/diag_codes.hpp"
#include "hefesto/ui/input.hpp"
#include "hefesto/ui/tools.hpp"
#include "hefesto/view/color_window.hpp"
#include "ui/command_util.hpp"

namespace hefesto::ui::detail {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

// --- tool ----------------------------------------------------------------------

struct Switch {
  std::string_view tool;
  std::string_view message;
  int glyph;
};

constexpr std::array kSwitches{Switch{"box", "Switching to BOX tool.", 0},
                               Switch{"wiring", "Switching to WIRING tool.", 19},
                               Switch{"nettool", "Switching to NETLIST tool.", 18},
                               Switch{"pick", "Switching to PICK tool.", 22}};

void tool_info(CommandContext& ctx, editor::EditorState& editor) {
  std::string text = "Current tool is " + macro_table_for(editor, "layout") + ".\nButton command bindings:";
  static constexpr std::array<std::string_view, 11> kButtons = {
      "Button1",         "Button2",         "Button3",         "Shift_Button1", "Shift_Button2", "Shift_Button3",
      "Control_Button1", "Control_Button2", "Control_Button3", "Button4",       "Button5"};
  for (const std::string_view name : kButtons) {
    const std::array<std::string, 1> args{std::string{name}};
    const CommandResult shown = macro_command(editor, args, false, "layout");
    for (const auto& d : shown.diagnostics) {
      text += "\n" + d.text;
    }
  }
  print(ctx, std::move(text));
}

CommandResult tool(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  if (inv.args.size() > 1) {
    return raise(codes::kUsage, "wrong # args: should be \"tool ?type?\"");
  }
  editor::EditorState& ed = *editor.value();
  ensure_defaults(ed);
  if (!state(ed).motion.empty() || ed.wire.tracking) {
    return CommandResult::success();  // never switch during a drag
  }
  std::string type = inv.args.empty() ? "next" : inv.args[0];
  if (type == "next") {
    type = std::string{next_tool(macro_table_for(ed, "layout"))};
  }
  if (type == "type") {
    return CommandResult::success(macro_table_for(ed, "layout"));
  }
  if (type == "info") {
    tool_info(ctx, ed);
    return CommandResult::success();
  }
  for (const Switch& s : kSwitches) {
    if (s.tool == type) {
      print(ctx, std::string{s.message});
      (void)ed.tools.set_current(s.tool);
      state(ed).cursor_glyph = s.glyph;
    }
  }
  return CommandResult::success();
}

// --- cursor --------------------------------------------------------------------

enum class CursorUnits : std::uint8_t { internal, lambda, user, microns, window, screen };

std::optional<CursorUnits> cursor_option(std::string_view word) {
  static const std::vector<std::string_view> kOptions = {"internal", "lambda", "user",   "grid",
                                                         "microns",  "window", "screen", "units"};
  static constexpr std::array<CursorUnits, 8> kUnits = {CursorUnits::internal, CursorUnits::lambda,
                                                        CursorUnits::user,     CursorUnits::user,
                                                        CursorUnits::microns,  CursorUnits::window,
                                                        CursorUnits::screen,   CursorUnits::internal};
  const cmd::NameMatch m = cmd::match_name(word, kOptions);
  if (m.status != cmd::LookupStatus::found || m.index == 7) {
    return std::nullopt;  // `units` is listed by Magic but not handled (usage)
  }
  return kUnits.at(m.index);
}

CursorUnits default_cursor_units(const editor::EditorState& editor) {
  switch (editor.units.unit) {
    case cmd::InputUnit::lambda:
      return CursorUnits::lambda;
    case cmd::InputUnit::user:
      return CursorUnits::user;
    case cmd::InputUnit::microns:
      return CursorUnits::microns;
    default:
      return CursorUnits::internal;
  }
}

db::Point snapped(const editor::EditorState& editor, db::Point p) {
  return snap_to_tool_grid(editor, p);
}

cmd::InputUnit as_input_unit(CursorUnits u) {
  switch (u) {
    case CursorUnits::lambda:
      return cmd::InputUnit::lambda;
    case CursorUnits::user:
      return cmd::InputUnit::user;
    case CursorUnits::microns:
      return cmd::InputUnit::microns;
    default:
      return cmd::InputUnit::internal;
  }
}

CommandResult cursor_usage() {
  CommandResult result = usage("Usage: cursor glyphnum");
  result.diagnostics.push_back(
      diag::make(codes::kUsage, " (or): cursor [internal | lambda | microns | user | window | units]"));
  return result;
}

CommandResult cursor(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  editor::EditorState& ed = *editor.value();
  CursorUnits units = default_cursor_units(ed);
  if (inv.args.size() == 1) {
    if (const auto glyph = parse_int(inv.args[0])) {
      state(ed).cursor_glyph = static_cast<int>(*glyph);
      return CommandResult::success();
    }
    const auto option = cursor_option(inv.args[0]);
    if (!option) {
      return cursor_usage();
    }
    units = *option;
  }
  if (!ed.command_point) {
    return CommandResult::success();  // no pointer (Magic's null display)
  }
  if (units == CursorUnits::window || units == CursorUnits::screen) {
    const db::Point px = state(ed).pointer_pixel;
    return CommandResult::success(cmd::Value::List{cmd::Value{std::int64_t{px.x}}, cmd::Value{std::int64_t{px.y}}});
  }
  const db::Point p = snapped(ed, ed.command_point->point);
  const cmd::UnitsState shown{as_input_unit(units), false};
  const cmd::CoordinateUnits scale = coordinate_units(ed);
  return CommandResult::success(
      cmd::Value::List{cmd::format_value(p.x, shown, scale, cmd::ValueKind::length, cmd::Axis::x),
                       cmd::format_value(p.y, shown, scale, cmd::ValueKind::length, cmd::Axis::y)});
}

// --- setpoint ------------------------------------------------------------------

const view::LayoutWindow* window_named(const editor::EditorState& editor, const std::string& word) {
  if (const auto id = parse_int(word)) {
    return editor.windows.find(static_cast<view::WindowId>(*id));
  }
  return editor.windows.find_named(word);
}

/// The color window `word` names (its id), or nullptr.
const view::ColorWindow* color_window_named(editor::EditorState& editor, const std::string& word) {
  const auto id = parse_int(word);
  return id ? editor.color_map.find(static_cast<view::WindowId>(*id)) : nullptr;
}

CommandResult report_point(CommandContext& ctx, editor::EditorState& editor) {
  const db::Point px = state(editor).pointer_pixel;
  const view::LayoutWindow* w =
      editor.command_point ? editor.windows.find(editor.command_point->window) : editor.windows.current();
  const view::ColorWindow* cw =
      editor.command_point && w == nullptr ? editor.color_map.find(editor.command_point->window) : nullptr;
  if (cw != nullptr) {
    const db::Point p = view::color_window_mapping(editor, *cw).to_layout(px.x, px.y);
    return CommandResult::success(cmd::Value::List{std::int64_t{px.x}, std::int64_t{px.y}, std::int64_t{p.x},
                                                   std::int64_t{p.y}});
  }
  if (w == nullptr) {
    print(ctx, "Point is at screen coordinates (" + std::to_string(px.x) + ", " + std::to_string(px.y) + ").");
    return CommandResult::success();
  }
  const db::Point p = w->mapping().to_layout(px.x, px.y);
  return CommandResult::success(cmd::Value::List{std::int64_t{px.x}, std::int64_t{px.y}, std::int64_t{p.x},
                                                 std::int64_t{p.y}});
}

CommandResult setpoint(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  editor::EditorState& ed = *editor.value();
  const auto& a = inv.args;
  if (a.empty()) {
    return report_point(ctx, ed);
  }
  const auto x = a.size() >= 2 ? parse_int(a[0]) : std::nullopt;
  const auto y = a.size() >= 2 ? parse_int(a[1]) : std::nullopt;
  if (a.size() > 3 || !x || !y) {
    return usage("Usage: " + inv.name + " [x y [window ID|name]]");
  }
  const view::LayoutWindow* w = a.size() == 3 ? window_named(ed, a[2]) : ed.windows.current();
  const view::ColorWindow* cw = a.size() == 3 && w == nullptr ? color_window_named(ed, a[2]) : nullptr;
  const auto px = static_cast<std::int32_t>(*x);
  const auto py = static_cast<std::int32_t>(*y);
  cmd::PointerState pointer;
  pointer.window = w == nullptr ? -1 : w->id();
  pointer.point = w == nullptr ? db::Point{px, py} : w->mapping().to_layout(px, py);
  pointer.pixel_area = w == nullptr ? db::Rect{px, py, px, py} : w->mapping().screen_to_surface({px, py, px, py});
  if (cw != nullptr) {  // a color window: the following commands go to its client
    const view::ViewMapping m = view::color_window_mapping(ed, *cw);
    pointer.window = cw->id;
    pointer.point = m.to_layout(px, py);
    pointer.pixel_area = m.screen_to_surface({px, py, px, py});
  }
  ed.command_point = pointer;
  state(ed).pointer_pixel = db::Point{px, py};
  state(ed).point_forced = true;
  return CommandResult::success();
}

// --- pushbutton ------------------------------------------------------------------

CommandResult pushbutton(CommandContext& ctx, const Invocation& inv) {
  static const std::vector<std::string_view> kButtons = {"left", "middle", "right"};
  static const std::vector<std::string_view> kActions = {"down", "up"};
  const auto button = inv.args.size() == 2 ? cmd::match_name(inv.args[0], kButtons) : cmd::NameMatch{};
  const auto action = inv.args.size() == 2 ? cmd::match_name(inv.args[1], kActions) : cmd::NameMatch{};
  if (button.status != cmd::LookupStatus::found || action.status != cmd::LookupStatus::found) {
    return usage("Usage: " + inv.name + " button action");
  }
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  editor::EditorState& ed = *editor.value();
  static constexpr std::array kButtonIds = {Button::left, Button::middle, Button::right};
  const db::Point point = ed.command_point ? ed.command_point->point : db::Point{};
  const auto command = box_tool_button(ed, kButtonIds.at(button.index), action.index == 0, ed.box, point);
  return command ? run_forwarded(ctx, *command) : CommandResult::success();
}

// --- tools.tcl button helpers ------------------------------------------------------

CommandResult keepselect(CommandContext& ctx, const Invocation& /*inv*/) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  return state(*editor.value()).motion.empty() ? run_forwarded(ctx, {"box", "move", "bl", "cursor"})
                                               : run_forwarded(ctx, {"select", "keep"});
}

CommandResult startselect(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  editor::EditorState& ed = *editor.value();
  UiState& ui = state(ed);
  if (!ui.motion.empty()) {
    ui.motion.clear();
    ui.cursor_glyph = 22;
    return run_forwarded(ctx, {"copy", "center", "0"});
  }
  CommandResult result = CommandResult::success();
  if (inv.args.size() > 1 && inv.args[1] == "pick") {
    result = run_forwarded(ctx, {"select", "pick"});
  } else {
    const CommandResult what = run_nested(ctx, {"what", "-list"});
    if (what.ok() && what.value.to_string() == "{} {} {}") {
      result = run_forwarded(ctx, {"select", "nocycle"});
    }
  }
  ui.drag_origin = ed.command_point ? snapped(ed, ed.command_point->point) : db::Point{};
  ui.motion = "*bypass select move %Xi %Yi";
  ui.cursor_glyph = 21;
  return result;
}

CommandResult cancelselect(CommandContext& ctx, const Invocation& /*inv*/) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  UiState& ui = state(*editor.value());
  if (ui.motion.empty()) {
    return run_forwarded(ctx, {"box", "corner", "ur", "cursor"});
  }
  ui.motion.clear();
  ui.cursor_glyph = 22;
  return run_forwarded(ctx, {"select", "clear"});
}

}  // namespace

bool register_tool_commands(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "tool", "tool ?next|type|info|box|wiring|nettool|pick?",
                "Switches the active tool (box -> wiring -> nettool -> pick), returns it (type) or prints its button "
                "bindings (info). Refused silently during a drag.",
                tool);
  ok = add(registry, "cursor", "cursor ?glyphnum|internal|lambda|user|grid|microns|window|screen?",
           "Returns the pointer position (in the given units) or sets the cursor glyph.", cursor) &&
       ok;
  ok = add(registry, "setpoint", "setpoint ?x y ?window ID|name??",
           "Forces the point and window of the following commands (window pixels, y from the bottom), or returns "
           "the current point as x y X Y.",
           setpoint) &&
       ok;
  ok = add(registry, "pushbutton", "pushbutton button action",
           "Synthesizes a left/middle/right button down/up event at the command point.", pushbutton) &&
       ok;
  ok = add(registry, "keepselect", "keepselect window", "Pick tool button 1 (tools.tcl magic::keepselect).",
           keepselect) &&
       ok;
  ok = add(registry, "startselect", "startselect window ?pick|copy?",
           "Pick tool button 2: starts or ends a pick or copy drag (tools.tcl magic::startselect).", startselect) &&
       ok;
  ok = add(registry, "cancelselect", "cancelselect window", "Pick tool button 3 (tools.tcl magic::cancelselect).",
           cancelselect) &&
       ok;
  return ok;
}

}  // namespace hefesto::ui::detail

namespace hefesto::ui {

// ToolSnapToGrid with the `snap` grid.
db::Point snap_to_tool_grid(const editor::EditorState& editor, db::Point p) {
  cmd::SnapGrid grid;
  if (editor.snap == editor::SnapMode::internal) {
    return p;
  }
  if (editor.snap == editor::SnapMode::lambda) {
    const std::int64_t pitch = editor.grid.lambda > 0 ? editor.grid.internal / editor.grid.lambda : 1;
    grid.pitch_x = grid.pitch_y = static_cast<db::Coord>(pitch > 0 ? pitch : 1);
  } else if (const view::LayoutWindow* w = editor.windows.current()) {
    const db::Rect s = w->grid().spacing;
    grid.origin = db::Point{s.x0, s.y0};
    grid.pitch_x = s.x1 - s.x0 > 0 ? s.x1 - s.x0 : 1;
    grid.pitch_y = s.y1 - s.y0 > 0 ? s.y1 - s.y0 : 1;
  }
  return cmd::snap_point(p, grid);
}

}  // namespace hefesto::ui
