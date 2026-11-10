// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `wire` command (wiring-tool "wire command dispatch" and every
// subcommand). Ported from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdTZ.c
// CmdWire, Copyright (C) 1985, 1990 Regents of the University of California
// (see THIRD_PARTY_NOTICES.md). Magic's Tcl build is normative: `values` and
// `width` return their values instead of printing them. Owner: WIRE.

#include "hefesto/wiring/commands.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "hefesto/cmd/keywords.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/wiring/diag_codes.hpp"
#include "wiring/commands_internal.hpp"
#include "wiring/session.hpp"

namespace hefesto::wiring {

namespace {

enum Option : std::uint8_t {
  kDecrement,
  kHelp,
  kHorizontal,
  kIncrement,
  kLeg,
  kShow,
  kSwitch,
  kType,
  kValues,
  kVertical,
  kWidth,
  kSegment,
};

// cmdWireOption, verbatim (the help list and the "Wrong arguments" text).
constexpr std::array<std::string_view, 12> kOptionText = {
    "decrement layer|width \tdecrement the wire layer or width",
    "help                   print this help information",
    "horizontal             add a new horizontal wire leg",
    "increment layer|width \tincrement the wire layer or width",
    "leg                    add a new horizontal or vertical leg",
    "show\t\t\tshow next wire segment as a selection",
    "switch [layer width]   place contact and switch layers",
    "type [layer [width]]   select the type and size of wires",
    "values\t\t\tquery current wire type and width",
    "vertical               add a new vertical wire leg",
    "width [value]\t\tchange the width of the wire",
    "segment layer width x1 y1 x2 y2... [-noendcap]  (or)\n"
    "segment layer width filename [-noendcap]\n"
    "\t\t\tpaint one or more wire segments",
};

constexpr std::array<std::string_view, 12> kOptionNames = {"decrement", "help",   "horizontal", "increment",
                                                           "leg",       "show",   "switch",     "type",
                                                           "values",    "vertical", "width",    "segment"};

std::optional<Option> find_option(std::string_view word) {
  const cmd::LookupResult r = cmd::lookup(word, kOptionNames);
  if (r.match != cmd::Match::found) {
    return std::nullopt;
  }
  return static_cast<Option>(r.index);
}

std::string help_text() {
  std::string out = "Wiring commands have the form \":wire option\", where option is one of:";
  for (const std::string_view entry : kOptionText) {
    out += "\n    ";
    out += entry;
  }
  return out;
}

cmd::CommandResult bad_args(Option option) {
  return fail(codes::kUsage, "Wrong arguments.  The correct syntax is\n    \"wire " +
                                 std::string{kOptionText.at(option)} + "\"");
}

// DBTechNameType for `wire type|switch`: "doesn't exist" / "is ambiguous".
diag::Outcome<tech::TileType> layer(const WireSession& s, const std::string& name) {
  const auto found = s.tech->find_type(name.substr(0, name.find('/')));
  if (found.ok()) {
    return *found.id;
  }
  return diag::make(codes::kBadLayer, "Layer name \"" + name + "\" " +
                                          (found.error == tech::LookupError::ambiguous ? "is ambiguous." : "doesn't exist."));
}

// `x y` typed in edit-cell coordinates, as root coordinates.
db::Point root_point(const WireSession& s, const std::string& x, const std::string& y) {
  return editor::to_root(*s.ed, db::Point{s.parse_coord(x, false, true), s.parse_coord(y, false, false)});
}

// The routing layer one contact away (`wire increment|decrement type`).
cmd::CommandResult step_type(const WireSession& s, bool up) {
  const WireState& w = s.ed->wire;
  std::optional<tech::TileType> next;
  for (const WiringContact& c : s.section().contacts) {
    const tech::TileType from = up ? c.layer1 : c.layer2;
    if (!w.type || from != *w.type) {
      continue;
    }
    if (next) {
      s.emit(codes::kAmbiguousRouting, "Ambiguous directive---multiple routing types");
      break;  // Magic goes on with the first one
    }
    next = up ? c.layer2 : c.layer1;
  }
  if (!next) {
    return fail(codes::kNoContact, "No routing layer defined");
  }
  const db::Coord min_width = default_layer_width(*s.tech, *next, s.ed->grid);
  return add_contact(s, next, std::max(w.width, min_width));
}

cmd::CommandResult step(const WireSession& s, const std::vector<std::string>& args, Option option) {
  if (args.size() != 2 && args.size() != 3) {
    return bad_args(option);
  }
  const bool up = option == kIncrement;
  if (args[1] == "type" || args[1] == "layer") {
    return step_type(s, up);
  }
  if (args[1] != "width") {
    return bad_args(option);
  }
  const db::Coord value = args.size() == 3 ? s.parse_coord(args[2], true, true) : 1;
  pick_type(s, s.ed->wire.type, up ? s.ed->wire.width + value : s.ed->wire.width - value);
  return cmd::CommandResult::success();
}

cmd::CommandResult leg(const WireSession& s, const std::vector<std::string>& args, Option option) {
  const LegMode mode = option == kHorizontal ? LegMode::horizontal
                       : option == kVertical ? LegMode::vertical
                                             : LegMode::choose;
  if (args.size() > 1 && args[1] == "to") {
    if (args.size() != 4) {
      return bad_args(option);
    }
    return add_leg(s, root_point(s, args[2], args[3]), mode);
  }
  return add_leg(s, std::nullopt, mode);
}

cmd::CommandResult type(const WireSession& s, const std::vector<std::string>& args) {
  if (args.size() == 1) {
    return pick_type_at(s, std::nullopt);
  }
  if (args.size() == 4 && args[1] == "at") {
    return pick_type_at(s, root_point(s, args[2], args[3]));
  }
  if (args.size() != 2 && args.size() != 3) {
    return bad_args(kType);
  }
  const auto t = layer(s, args[1]);
  if (!t.ok()) {
    return cmd::CommandResult::failure(t.error());
  }
  const db::Coord width = args.size() == 2
                              ? std::max(s.ed->wire.width, default_layer_width(*s.tech, t.value(), s.ed->grid))
                              : s.parse_coord(args[2], true, true);
  pick_type(s, t.value(), width);
  return cmd::CommandResult::success();
}

cmd::CommandResult switch_layer(const WireSession& s, const std::vector<std::string>& args) {
  if (args.size() == 1) {
    return add_contact(s, std::nullopt, 0);
  }
  if (args.size() != 3) {
    return bad_args(kSwitch);
  }
  const auto t = layer(s, args[1]);
  if (!t.ok()) {
    return cmd::CommandResult::failure(t.error());
  }
  return add_contact(s, t.value(), s.parse_coord(args[2], true, true));
}

cmd::CommandResult values(const WireSession& s, const std::vector<std::string>& args) {
  if (args.size() != 1) {
    return cmd::CommandResult::success();  // Magic ignores extra words here
  }
  const WireState& w = s.ed->wire;
  return cmd::CommandResult::success(cmd::Value{cmd::Value::List{
      std::string{s.tech->type_name(w.type.value_or(tech::kSpace))}, s.format_length(w.width)}});
}

cmd::CommandResult width(const WireSession& s, const std::vector<std::string>& args) {
  if (args.size() == 1) {
    return cmd::CommandResult::success(cmd::Value{s.format_length(s.ed->wire.width)});
  }
  if (args.size() != 2) {
    return bad_args(kWidth);
  }
  pick_type(s, s.ed->wire.type, s.parse_coord(args[1], true, true));
  return cmd::CommandResult::success();
}

cmd::CommandResult run_option(const WireSession& s, const std::vector<std::string>& args, Option option) {
  switch (option) {
    case kDecrement:
    case kIncrement:
      return step(s, args, option);
    case kHelp:
      s.report(help_text());
      return cmd::CommandResult::success();
    case kHorizontal:
    case kLeg:
    case kVertical:
      return leg(s, args, option);
    case kShow:
      show_leg(s);
      return cmd::CommandResult::success();
    case kSwitch:
      return switch_layer(s, args);
    case kType:
      return type(s, args);
    case kValues:
      return values(s, args);
    case kWidth:
      return width(s, args);
    case kSegment:
      return segment_command(s, args);
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult cmd_wire(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  auto s = wire_session(ctx);
  if (!s.ok()) {
    return cmd::CommandResult::failure(s.error());
  }
  if (inv.args.empty()) {
    s.value().report(help_text());
    return cmd::CommandResult::success();
  }
  const auto option = find_option(inv.args[0]);
  if (!option) {
    ctx.emit(diag::make(codes::kUsage, "\"" + inv.args[0] + "\" isn't a valid wire option."));
    s.value().report(help_text());
    return cmd::CommandResult{cmd::Status::failed, {}, {}, false, std::nullopt};
  }
  std::optional<cmd::Invocation> logged = logged_form(inv, *s.value().ed);
  cmd::CommandResult result = run_option(s.value(), inv.args, *option);
  result.logged_as = std::move(logged);
  return result;
}

// Whether the invocation reads the cursor (CmdWire's needCoord).
bool needs_coordinates(const cmd::Invocation& inv, Option option) {
  if (option == kType) {
    return inv.args.size() == 1;
  }
  const bool leg_option = option == kLeg || option == kHorizontal || option == kVertical;
  return leg_option && !(inv.args.size() > 1 && inv.args[1] == "to");
}

}  // namespace

std::optional<cmd::Invocation> logged_form(const cmd::Invocation& invocation, const editor::EditorState& editor) {
  if (invocation.name != "wire" || invocation.args.empty()) {
    return std::nullopt;
  }
  const auto option = find_option(invocation.args[0]);
  const auto c = cursor(editor);
  if (!option || !needs_coordinates(invocation, *option) || !c) {
    return std::nullopt;
  }
  const db::Point p = editor::to_edit(editor, c->point);
  cmd::Invocation out = invocation;
  out.args = {invocation.args[0], *option == kType ? "at" : "to", std::to_string(p.x) + "i", std::to_string(p.y) + "i"};
  return out;
}

bool register_wiring_commands(cmd::CommandRegistry& registry) {
  bool ok = !registry
                 .add(cmd::CommandSpec{
                     "wire",
                     "wire type|leg|horizontal|vertical|switch|increment|decrement|show|values|width|segment|help"
                     " ?args?",
                     "Paints wires and places contacts with the wiring tool.", cmd_wire})
                 .has_value();
  ok = register_trackwire(registry) && ok;
  return ok;
}

}  // namespace hefesto::wiring
