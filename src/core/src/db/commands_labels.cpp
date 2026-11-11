// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MODEL commands, part 2: label, setlabel (commands_setlabel.cpp), port,
// property (cmd-labels-properties). Label changes are recorded for undo
// through the db label primitives (db/labels.hpp). Owner: HIER
// (openspec/changes/m2-interactive-editing/design.md D2).
// Ported in part from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdLQ.c
// (CmdLabel, CmdPort: PORT_MAKE index and direction rules, CmdProperty).
// Magic is Copyright (C) 1985, 1990 Regents of the University of California
// (permissive license, THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cstdint>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <utility>

#include "db/command_util.hpp"
#include "db/hier_commands.hpp"
#include "hefesto/cell/hier.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/display/font.hpp"

namespace hefesto::db {

namespace cu = cmdutil;
using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using cmd::Value;

namespace {

struct LabelTarget {
  cu::Session s;
  CellDef* def = nullptr;
  Rect area;
};

diag::Outcome<LabelTarget> label_target(CommandContext& ctx) {
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
  // ToolGetEditBox: the box in edit-cell coordinates (cmd-hierarchy "Edit
  // command": warned when the edit transform matches no instance).
  if (auto warning = cell::edit_transform_warning(*s.value().editor, s.value().magic_compat)) {
    cu::warn(s.value(), *warning);
  }
  return LabelTarget{s.value(), def.value(), editor::to_edit(*s.value().editor, b.value())};
}

// --- label -------------------------------------------------------------------
//
// CmdLabel (Magic ref: commands/CmdLQ.c:200-345): `label text [pos [layer]]`
// or `label text font [size [rotation [offx offy [pos [layer]]]]]`, starting
// from the `setlabel -default` values when they were set.

struct LabelArgs {
  Label lab;
  bool stop = false;  // an error was reported: no label is made
  std::optional<tech::TileType> type;
};

LabelArgs initial_label_args(const cu::Session& s, const std::string& text) {
  LabelArgs a;
  a.lab.text = text;
  a.lab.justification = -1;
  if (const auto& d = s.editor->label_defaults) {
    a.lab.justification = d->justification;
    a.lab.font = d->font;
    a.lab.size = d->size;
    a.lab.rotation = d->rotation;
    a.lab.offset = d->offset;
    a.lab.sticky = d->sticky;
    a.type = d->type;
  }
  return a;
}

// The second word: a position, else a font index or a font name.
void parse_label_second(const cu::Session& s, const std::string& word, LabelArgs& a) {
  const int pos = parse_position(word, false);
  if (pos >= 0) {
    a.lab.justification = pos;
    return;
  }
  const auto count = static_cast<std::int32_t>(s.editor->fonts.fonts.size());
  if (cu::str_is_int(word)) {
    a.lab.font = cu::c_atoi(word);
    if (a.lab.font < 0 || a.lab.font >= count) {  // reported; the label gets the default font
      cu::warn(s, count == 0 ? std::string{"No vector outline fonts are loaded!"}
                             : "Font value out of range (0 to " + std::to_string(count - 1) + ")");
    }
    return;
  }
  a.lab.font = s.editor->fonts.find(word);
  if (a.lab.font < -1) {
    cu::warn(s, "Unknown vector outline font \"" + word + "\"");
    a.stop = true;
  }
}

// The offset words of the font form: "x y" in one word (then the position
// is the next word) or two words. Returns the index of the position word.
std::size_t parse_label_offset(const cu::Session& s, const std::vector<std::string>& args, LabelArgs& a) {
  const std::string& first = args[4];
  const std::size_t space = first.find(' ');
  const std::string_view word{first};
  const bool one_word = space != std::string::npos;
  const std::string_view x = one_word ? word.substr(0, space) : word;
  const std::string_view y = one_word ? word.substr(space + 1) : std::string_view{args[5]};
  if (!cu::str_is_numeric(x) || !cu::str_is_numeric(y)) {
    cu::warn(s, "Uninterpretable offset value \"" + std::string{x} + " " + std::string{y} + "\"");
    a.stop = true;
    return args.size();
  }
  a.lab.offset = Point{cu::scaled_coord(s, x, 8, true), cu::scaled_coord(s, y, 8, false)};
  return one_word ? 5 : 6;
}

// Size, rotation, offset and position of the font form.
void parse_label_font_form(const cu::Session& s, const std::vector<std::string>& args, LabelArgs& a) {
  if (!s.editor->label_defaults) {
    a.lab.size = cu::default_font_size(s);  // Q-MISC-22
  }
  if (args.size() > 2 && cu::str_is_numeric(args[2])) {
    a.lab.size = cu::scaled_coord(s, args[2], 8, true);
  }
  if (args.size() > 3 && cu::str_is_int(args[3])) {
    a.lab.rotation = cu::c_atoi(args[3]);
  }
  std::size_t pos_index = 6;
  if (args.size() > 5) {
    pos_index = parse_label_offset(s, args, a);
  }
  if (a.stop || pos_index >= args.size()) {
    return;
  }
  const int pos = parse_position(args[pos_index], false);
  if (pos < 0) {
    cu::warn(s, position_error_text(args[pos_index], false));
    a.stop = true;
    return;
  }
  a.lab.justification = pos;
}

// The last word names the layer ('-' prefix: sticky).
void parse_label_type(const cu::Session& s, const std::string& word, LabelArgs& a) {
  std::string_view name{word};
  if (name.starts_with('-')) {
    a.lab.sticky = true;
    name.remove_prefix(1);
  }
  const auto found = s.tech->find_type(name);
  if (!found.ok()) {
    cu::warn(s, "Unknown layer: " + word);
    a.stop = true;
    return;
  }
  a.type = *found.id;
}

LabelArgs parse_label_args(const cu::Session& s, const std::vector<std::string>& args) {
  LabelArgs a = initial_label_args(s, args[0]);
  if (args.size() > 1) {
    parse_label_second(s, args[1], a);
  }
  if (!a.stop && a.lab.font >= 0) {
    parse_label_font_form(s, args, a);
  }
  const bool has_layer = a.lab.font < 0 ? args.size() > 2 : args.size() >= 7;
  if (!a.stop && has_layer) {
    parse_label_type(s, args.back(), a);
  }
  return a;
}

// CmdIllegalChars(text, " /", "Label name"); the message, or "" when legal.
std::string illegal_label_text(const std::string& text) {
  for (const char ch : text) {
    const auto c = static_cast<unsigned char>(ch);
    if (c > 127 || std::iscntrl(c) != 0) {
      constexpr std::string_view kHex = "0123456789abcdef";
      std::string hex;
      for (unsigned v = c; v != 0 || hex.empty(); v /= 16) {
        hex.insert(hex.begin(), kHex[v % 16]);
      }
      return "Label name contains illegal control character 0x" + hex;
    }
    if (ch == ' ' || ch == '/') {
      return std::string{"Label name contains illegal character \""} + ch + "\"";
    }
  }
  return {};
}

CommandResult cmd_label(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  if (inv.args.empty() || inv.args.size() > 8) {
    cu::warn(s.value(), "Usage: label text [direction [layer]]\n"
                        "or:    label text font [size [rotation [offsetx offsety [direction [layer]]]]]");
    return CommandResult::success();
  }
  LabelArgs a = parse_label_args(s.value(), inv.args);
  if (a.stop) {
    return CommandResult::success();
  }
  auto t = label_target(ctx);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const LabelTarget& e = t.value();
  if (a.lab.text.empty()) {
    cu::warn(e.s, "Can't have null label name.");
    return CommandResult::success();
  }
  if (const std::string bad = illegal_label_text(a.lab.text); !bad.empty()) {
    cu::warn(e.s, bad);
    return CommandResult::success();
  }
  // CmdLabelProc: duplicates at the same place are erased whatever their type.
  (void)erase_labels_with_text(*e.def, e.area, a.lab.text);
  a.lab.rect = e.area;
  if (a.lab.justification >= 0) {
    a.lab.justification = transform_position(invert(e.s.editor->edit_transform), a.lab.justification);
  }
  a.lab.type = a.type ? label_layer_for(*e.s.tech, *a.type) : tech::kSpace;
  const bool auto_justify = a.lab.justification < 0;
  auto put = put_label(*e.def, std::move(a.lab), auto_justify, e.s.editor->fonts.fonts.size());
  if (!put.ok()) {
    return cu::fail(put.error());
  }
  // CmdLabelProc: a label without a layer starts on space; DBAdjustLabels
  // then moves every label in the box to the paint under it, with notes.
  // (Magic ref: commands/CmdLQ.c:124-137)
  Label* made = put.value();
  cu::update_font_label(e.s, *made);
  std::vector<std::string> notes;
  (void)adjust_labels(*e.def, *e.s.tech, e.area, &notes);
  for (std::string& n : notes) {
    cu::print(e.s, std::move(n));
  }
  made->type = label_layer_for(*e.s.tech, made->type);
  recompute_bbox(*e.def, e.s.magic_compat);
  // DBWLabelChanged (ui-windows-and-views "Change-driven redisplay"): the
  // label's area (or rendered text box), grown by one unit so that a point
  // label has an area, with the window's label extent.
  cu::label_changed(e.s, *e.def, *made);
  return CommandResult::success();
}

// --- port --------------------------------------------------------------------
//
// CmdPort (Magic ref: commands/CmdLQ.c:1523-2320; cmd-labels-properties
// "Port command target", "Port attributes", "Port make, numbering and
// directions"). The target is the label named (text) or numbered (index)
// by the first word, else the single label of the right kind under the box
// (portFindLabel). Every change is recorded for undo (db::replace_label).

enum PortOption {
  kPortClass, kPortUse, kPortShape, kPortIndex, kPortEquivalent, kPortExists, kPortConnections, kPortFirst,
  kPortNext, kPortLast, kPortMake, kPortMakeall, kPortName, kPortRemove, kPortRenumber, kPortHelp
};

constexpr std::array<std::string_view, 16> kPortOptions = {
    "class", "use",  "shape", "index", "equivalent", "exists", "connections", "first",
    "next",  "last", "make",  "makeall", "name",     "remove", "renumber",    "help"};

constexpr std::array<std::string_view, 16> kPortOptionHelp = {
    "class\t[type]\t\tget [set] port class type",
    "use\t[type]\t\tget [set] port use type",
    "shape\t[type]\t\tget [set] port shape type",
    "index\t[number]\tget [set] port number",
    "equivalent [number]\tmake port equivalent to another port",
    "exists\t\t\treport if a label is a port or not",
    "connections [dir...]\tget [set] port connection directions",
    "first\t\t\treport the lowest port number used",
    "next [number]\t\treport the next port number used",
    "last\t\t\treport the highest port number used",
    "make [index] [dir...]\tturn a label into a port",
    "makeall [index] [dir]\tturn all labels into ports",
    "name\t\t\treport the port name",
    "remove\t\t\tturn a port back into a label",
    "renumber\t\trenumber all ports",
    "help\t\t\tprint this help information"};

CommandResult port_usage(const cu::Session& s) {
  std::string text = "Usage:  port [option], where [option] is one of:";
  for (const std::string_view line : kPortOptionHelp) {
    text += "\n    " + std::string{line};
  }
  cu::warn(s, text + "\n    <index> <directions>");
  return CommandResult::success();
}

std::uint8_t directions_of(Position pos) {
  constexpr std::array<std::uint8_t, 9> kDirs = {
      kPortNorth | kPortEast | kPortSouth | kPortWest,  // center
      kPortNorth, kPortNorth | kPortEast, kPortEast, kPortSouth | kPortEast,
      kPortSouth, kPortSouth | kPortWest, kPortWest, kPortNorth | kPortWest};
  return pos >= 0 && pos <= 8 ? kDirs[static_cast<std::size_t>(pos)] : 0;
}

/// One `port` invocation: the edit cell, the box in edit coordinates and
/// the words after the target (the option first).
struct PortCall {
  cu::Session s;
  CellDef* def = nullptr;
  Rect box;
  std::vector<std::string> args;
  bool quiet = false;

  [[nodiscard]] std::size_t argc() const noexcept { return args.size() + 1; }  // Magic's argc after the target
};

// portFindLabel(port, unique = TRUE): the single label of the wanted kind
// overlapping (or, for zero-area labels, inside) the box; several are
// narrowed to those inside the box. nullopt when none or not unique.
std::optional<std::size_t> find_port_label(const CellDef& def, const Rect& box, bool want_port) {
  std::optional<std::size_t> found;
  for (std::size_t i = 0; i < def.labels().size(); ++i) {
    const Label& l = def.labels()[i];
    if (l.is_port() != want_port || !(overlap(box, l.rect) || surround(box, l.rect))) {
      continue;
    }
    if (!found) {
      found = i;
      continue;
    }
    found.reset();
    for (std::size_t k = 0; k < def.labels().size(); ++k) {
      if (surround(box, def.labels()[k].rect)) {
        if (found) {
          return std::nullopt;
        }
        found = k;
      }
    }
    return found;
  }
  return found;
}

// The label named by the first word: a port with that index, or the label
// with that text (a port among them preferred).
std::optional<std::size_t> named_port_label(const CellDef& def, const std::string& word) {
  std::optional<std::size_t> found;
  const bool by_index = cu::str_is_int(word);
  const std::int64_t index = by_index ? cu::c_atoi(word) : 0;
  for (std::size_t i = 0; i < def.labels().size(); ++i) {
    const Label& l = def.labels()[i];
    if (by_index && l.is_port() && std::int64_t{l.port_index} == index) {
      return i;
    }
    if (!by_index && l.text == word) {
      found = i;
      if (l.is_port()) {
        return i;
      }
    }
  }
  return found;
}

/// Sets `field` to `value` on every port of the cell with the target's text
/// and index (each change recorded).
template <typename F>
void update_equivalent_ports(CellDef& def, const Label& target, F update) {
  const std::string text = target.text;
  const std::uint32_t index = target.port_index;
  for (Label& other : def.labels()) {
    if (other.is_port() && other.text == text && other.port_index == index) {
      Label changed = other;
      update(changed);
      replace_label(def, other, std::move(changed));
    }
  }
}

template <typename E>
struct PortWord {
  std::string_view name;
  E value;
};

constexpr std::array<PortWord<PortClass>, 8> kClasses = {{
    {"default", PortClass::default_class}, {"input", PortClass::input}, {"output", PortClass::output},
    {"tristate", PortClass::tristate}, {"bidirectional", PortClass::bidirectional},
    {"inout", PortClass::bidirectional}, {"feedthrough", PortClass::feedthrough},
    {"feedthru", PortClass::feedthrough}}};
// reset, scan and tieoff are read from files (Q-CELL-6) but are not `port use` keywords.
constexpr std::array<PortWord<PortUse>, 7> kUses = {{
    {"default", PortUse::default_use}, {"analog", PortUse::analog}, {"signal", PortUse::signal},
    {"digital", PortUse::signal}, {"power", PortUse::power}, {"ground", PortUse::ground},
    {"clock", PortUse::clock}}};
constexpr std::array<PortWord<PortShape>, 5> kShapes = {{
    {"default", PortShape::default_shape}, {"abutment", PortShape::abutment}, {"ring", PortShape::ring},
    {"feedthrough", PortShape::feedthrough}, {"feedthru", PortShape::feedthrough}}};

/// The first keyword of `value`'s synonym group; "" when the value has none
/// (a use read from a file, such as reset: Magic returns nothing).
template <typename E, std::size_t N>
std::string_view name_of(const std::array<PortWord<E>, N>& table, E value) {
  for (const auto& w : table) {
    if (w.value == value) {
      return w.name;
    }
  }
  return {};
}

/// `port class|use|shape [value]`: a query returns the keyword; a value
/// (unique prefix) is set on the equivalent ports; an unknown value prints
/// the usage with the keywords and changes nothing.
template <typename E, std::size_t N>
CommandResult port_enum(const PortCall& c, Label& lab, E Label::*field, const std::array<PortWord<E>, N>& table,
                        std::string_view word) {
  if (c.args.size() == 1) {
    return CommandResult::success(Value{std::string{name_of(table, lab.*field)}});
  }
  std::vector<std::string_view> names;
  std::string usage = "Usage:  port " + std::string{word} + " <type>, where <type> is one of:";
  for (const auto& w : table) {
    names.push_back(w.name);
    usage += "\n    " + std::string{w.name};
  }
  const int idx = cu::lookup_in(c.args[1], names);
  if (idx < 0) {
    cu::warn(c.s, usage);
    return CommandResult::success();
  }
  const E value = table[static_cast<std::size_t>(idx)].value;
  update_equivalent_ports(*c.def, lab, [&](Label& l) { l.*field = value; });
  mark_modified(*c.def);
  return CommandResult::success();
}

CommandResult port_index(const PortCall& c, Label& lab) {
  if (c.args.size() == 1) {
    return CommandResult::success(Value{std::int64_t{lab.port_index}});
  }
  if (!cu::str_is_int(c.args[1])) {
    cu::warn(c.s, "Usage:  port index <integer>");
    return CommandResult::success();
  }
  const auto index = static_cast<std::uint32_t>(cu::c_atoi(c.args[1]));
  update_equivalent_ports(*c.def, lab, [&](Label& l) { l.port_index = index; });
  mark_modified(*c.def);
  return CommandResult::success();
}

// `port equivalent [N]`: the index of port N, or of another port with the
// same text.
CommandResult port_equivalent(const PortCall& c, Label& lab) {
  std::optional<std::uint32_t> index;
  for (const Label& other : c.def->labels()) {
    if (&other == &lab || !other.is_port()) {
      continue;
    }
    const bool wanted = c.args.size() > 1 ? (cu::str_is_int(c.args[1]) &&
                                             std::int64_t{other.port_index} == cu::c_atoi(c.args[1]))
                                          : other.text == lab.text;
    if (wanted) {
      index = other.port_index;
      break;
    }
  }
  if (!index) {
    cu::warn(c.s, "No equivalent port found.");
    return CommandResult::success();
  }
  Label changed = lab;
  changed.port_index = *index;
  replace_label(*c.def, lab, std::move(changed));
  return CommandResult::success();
}

std::string directions_text(std::uint8_t dirs) {
  std::string out;
  for (const auto& [bit, ch] : {std::pair{kPortNorth, 'n'}, std::pair{kPortEast, 'e'}, std::pair{kPortSouth, 's'},
                                std::pair{kPortWest, 'w'}}) {
    if ((dirs & bit) != 0) {
      out += ch;
    }
  }
  return out;
}

// The direction set of `words` (each a Manhattan direction in root
// orientation), or of the justification when there are none; nullopt
// after a bad direction (GeoNameToPos printed it).
std::optional<std::uint8_t> parse_directions(const PortCall& c, const Label& lab, std::size_t first) {
  if (first >= c.args.size()) {
    return directions_of(lab.justification);
  }
  std::uint8_t dirs = 0;
  for (std::size_t i = first; i < c.args.size(); ++i) {
    const int pos = parse_position(c.args[i], true);
    if (pos < 0) {
      cu::warn(c.s, position_error_text(c.args[i], true));
      return std::nullopt;
    }
    const Position edit_pos = transform_position(invert(c.s.editor->edit_transform), pos);
    if (edit_pos != kCenter) {
      dirs = static_cast<std::uint8_t>(dirs | directions_of(edit_pos));
    }
  }
  return dirs;
}

CommandResult port_connections(const PortCall& c, Label& lab) {
  if (c.args.size() == 1) {
    return CommandResult::success(Value{directions_text(lab.port_directions)});
  }
  const auto dirs = parse_directions(c, lab, 1);
  if (dirs) {
    Label changed = lab;
    changed.port_directions = *dirs;
    replace_label(*c.def, lab, std::move(changed));
  }
  return CommandResult::success();
}

// CmdPort PORT_MAKE: the index a new port takes when none is given (a
// same-text port wins, the first in label order; Q-CMDEDIT-92).
std::uint32_t next_port_index(const CellDef& def, const Label& lab) {
  std::int64_t idx = 0;
  for (const Label& sl : def.labels()) {
    if (&sl == &lab || !sl.is_port()) {
      continue;
    }
    if (sl.text == lab.text) {
      idx = std::int64_t{sl.port_index} - 1;
      break;
    }
    idx = std::max<std::int64_t>(idx, sl.port_index);
  }
  return static_cast<std::uint32_t>(idx + 1);
}

// The index of `port make INDEX`; nullopt after "Port index N is already
// used by port X" (a port with other text has it).
std::optional<std::uint32_t> given_port_index(const PortCall& c, const Label& lab, std::int64_t idx) {
  for (const Label& sl : c.def->labels()) {
    if (&sl != &lab && sl.is_port() && std::int64_t{sl.port_index} == idx && sl.text != lab.text) {
      const std::string n = std::to_string(idx);
      cu::warn(c.s, "Port index " + n + " is already used by port " + sl.text + ".\nUse command \"port index " + n +
                        "\" to force equivalence after defining the port.");
      return std::nullopt;
    }
  }
  return static_cast<std::uint32_t>(idx);
}

// One label of `port make|makeall`; false when the command must stop.
bool make_port(const PortCall& c, Label& lab) {
  std::size_t first = 1;
  std::optional<std::uint32_t> index;
  if (c.args.size() > 1 && cu::str_is_int(c.args[1])) {
    index = given_port_index(c, lab, cu::c_atoi(c.args[1]));
    if (!index) {
      return false;
    }
    first = 2;
  } else {
    index = next_port_index(*c.def, lab);
  }
  const auto dirs = parse_directions(c, lab, first);
  if (!dirs) {
    return false;
  }
  Label changed = lab;
  changed.port_index = *index;
  changed.port_directions = *dirs;
  replace_label(*c.def, lab, std::move(changed));
  return true;
}

CommandResult port_make(const PortCall& c, Label& lab) {
  if (lab.is_port()) {
    if (!c.quiet) {
      cu::warn(c.s, "The selected label is already a port.\nDo \"port help\" to get a list of options.");
    }
    return CommandResult::success();
  }
  (void)make_port(c, lab);
  mark_modified(*c.def);
  return CommandResult::success();
}

CommandResult port_makeall(const PortCall& c) {
  for (std::size_t i = 0; i < c.def->labels().size(); ++i) {
    Label& lab = c.def->labels()[i];
    if (lab.is_port() || !overlap(c.box, lab.rect)) {
      continue;
    }
    if (!make_port(c, lab)) {
      break;
    }
  }
  mark_modified(*c.def);
  return CommandResult::success();
}

CommandResult port_name(const PortCall& c, std::size_t index) {
  const Label lab = c.def->labels()[index];
  if (c.args.size() == 1) {
    return CommandResult::success(Value{lab.text});
  }
  // A duplicate of `setlabel text`: a new label, then the old one erased.
  Label renamed = lab;
  renamed.text = c.args[1];
  (void)put_label(*c.def, std::move(renamed), false, c.s.editor->fonts.fonts.size());
  erase_labels_with_text(*c.def, lab.rect, lab.text);
  return CommandResult::success();
}

CommandResult port_remove(const PortCall& c, Label& lab) {
  Label changed = lab;
  changed.port_directions = 0;
  changed.port_class = PortClass::default_class;
  changed.port_use = PortUse::default_use;
  changed.port_shape = PortShape::default_shape;
  changed.port_index = 0;
  changed.sticky = false;  // Magic clears every label flag
  replace_label(*c.def, lab, std::move(changed));
  return CommandResult::success();
}

// `port first|last` (lowest index of the ports / highest index) and
// `port next`: the smallest port index above the target's. A word after
// `next` is ignored, as in CmdPort (CMDEDIT-LBL-04); `port N next` names
// the target by index. `port last` (scripting-api "Result formats of
// commands used by scripts", Q-SCRIPT-6, resolution: compat): with
// magic-compat ON the highest index stored on any label, ports or not, as
// in Magic; with it OFF the highest index among the current ports, or -1.
CommandResult port_report_index(const PortCall& c, int option, const Label* lab) {
  std::int64_t result = -1;
  const std::int64_t ref = lab != nullptr ? std::int64_t{lab->port_index} : -1;
  for (const Label& l : c.def->labels()) {
    const std::int64_t idx = l.port_index;
    if (option == kPortLast) {
      result = c.s.magic_compat || l.is_port() ? std::max(result, idx) : result;
    } else if (l.is_port() && (option == kPortFirst || idx > ref) && (result == -1 || idx < result)) {
      result = idx;
    }
  }
  return CommandResult::success(Value{result});
}

CommandResult port_on_label(const PortCall& c, int option, std::size_t index) {
  Label& lab = c.def->labels()[index];
  switch (option) {
    case kPortClass: return port_enum(c, lab, &Label::port_class, kClasses, "class");
    case kPortUse: return port_enum(c, lab, &Label::port_use, kUses, "use");
    case kPortShape: return port_enum(c, lab, &Label::port_shape, kShapes, "shape");
    case kPortIndex: return port_index(c, lab);
    case kPortEquivalent: return port_equivalent(c, lab);
    case kPortConnections: return port_connections(c, lab);
    case kPortNext: return port_report_index(c, option, &lab);
    case kPortMake: return port_make(c, lab);
    case kPortName: return port_name(c, index);
    case kPortRemove: return port_remove(c, lab);
    default: return port_usage(c.s);
  }
}

bool needs_port(int option) {
  return option != kPortMake && option != kPortMakeall && option != kPortExists && option != kPortRenumber &&
         option != kPortLast && option != kPortFirst;
}

bool needs_label(int option) {
  return option != kPortLast && option != kPortFirst && option != kPortMakeall && option != kPortRenumber;
}

CommandResult run_port_option(PortCall& c, int option, std::optional<std::size_t> target, bool named) {
  if (!named && option != kPortLast && option != kPortFirst) {
    target = find_port_label(*c.def, c.box, option != kPortMake);
  }
  if (option == kPortExists) {
    return CommandResult::success(Value{std::int64_t{target && c.def->labels()[*target].is_port() ? 1 : 0}});
  }
  if (needs_label(option) && !target) {
    if (option != kPortRemove) {
      cu::warn(c.s, "Exactly one label may be present under the cursor box.\n"
                    "Use \"port <name> ...\" to specify a unique port.");
    }
    return CommandResult::success();
  }
  if (needs_port(option) && !c.def->labels()[*target].is_port()) {
    if (option != kPortRemove && !c.quiet) {
      cu::warn(c.s, "The selected label is not a port.");
    }
    return CommandResult::success();
  }
  switch (option) {
    case kPortFirst:
    case kPortLast:
      return port_report_index(c, option, nullptr);
    case kPortMakeall:
      return port_makeall(c);
    case kPortRenumber:
      renumber_ports(*c.def, c.s.magic_compat);
      mark_modified(*c.def);
      return CommandResult::success();
    default:
      return port_on_label(c, option, *target);
  }
}

// "port <name>|<index> option ...": the label named by the first word when
// the second is an option; nullopt (and `option` untouched) otherwise, an
// error result when the label does not exist.
std::optional<CommandResult> resolve_named_port(PortCall& c, int& option, std::optional<std::size_t>& target) {
  if (c.args.size() < 2) {
    return std::nullopt;
  }
  const int second = cu::lookup_in(c.args[1], kPortOptions);
  if (second < 0 || second == kPortHelp) {
    return std::nullopt;
  }
  target = named_port_label(*c.def, c.args[0]);
  if (!target) {
    if (!c.quiet) {
      cu::warn(c.s, cu::str_is_int(c.args[0]) ? "No label found with index " + c.args[0] + "."
                                              : "No port found with name " + c.args[0] + ".");
    }
    return CommandResult::success();
  }
  option = second;
  c.args.erase(c.args.begin());
  return std::nullopt;
}

CommandResult cmd_port(CommandContext& ctx, const Invocation& inv) {
  auto t = label_target(ctx);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  PortCall c{t.value().s, t.value().def, t.value().area, inv.args, false};
  if (!c.args.empty() && c.args.back() == "-quiet") {
    c.quiet = true;
    c.args.pop_back();
  }
  if (c.args.empty() || c.args.size() > 5) {
    return port_usage(c.s);
  }
  std::optional<std::size_t> target;
  int option = cu::lookup_in(c.args[0], kPortOptions);
  if (auto missing = resolve_named_port(c, option, target)) {
    return std::move(*missing);
  }
  if (option < 0 || option == kPortHelp) {
    return port_usage(c.s);
  }
  return run_port_option(c, option, target, target.has_value());
}

// --- property ------------------------------------------------------------------
//
// CmdDoProperty, Tcl build (Magic ref: commands/CmdLQ.c:2324-2800). Magic
// 8.3.694 defines PROPERTY_TYPE_COMPAT as 4, the index of "plane", not of
// "compat" (5): `property compat ...` takes "compat" as a type keyword and
// queries or sets the next word as a property name, and `property plane
// ...` is the compat switch. magic-compat ON replicates this argument quirk
// (decision X-2 tier 2); OFF uses the documented keywords
// (layout-properties "Property compat mode").

enum PropType { kString, kInteger, kDimension, kDouble, kPlane, kCompatKeyword };

std::optional<bool> parse_bool(std::string_view v) {
  const int idx = cu::lookup(v, {"disable", "no", "false", "off", "0", "enable", "yes", "true", "on", "1"});
  if (idx < 0) {
    return std::nullopt;
  }
  return idx > 4;
}

struct PropertyArgs {
  std::vector<std::string> words;  // name and values
  int type = kString;
  bool compat_switch = false;  // this invocation is the compat query/set
  bool dolist = false;
};

PropertyArgs parse_property_args(const std::vector<std::string>& args, bool magic_compat) {
  PropertyArgs p;
  p.words = args;
  if (!p.words.empty() && p.words.front() == "list") {
    p.dolist = true;
    p.words.erase(p.words.begin());
  }
  if (p.words.empty()) {
    return p;
  }
  const int found = cu::lookup(p.words.front(), {"string", "integer", "dimension", "double", "plane", "compat"});
  const int compat_index = magic_compat ? kPlane : kCompatKeyword;
  if (found == compat_index) {
    p.compat_switch = true;
  } else if (found >= 0) {
    p.type = found;
    p.words.erase(p.words.begin());
  }
  return p;
}

CommandResult property_compat(const cu::Session& s, const PropertyArgs& p) {
  if (p.words.size() == 1) {
    return CommandResult::success(Value{std::int64_t{s.editor->property_compat ? 1 : 0}});
  }
  const auto b = parse_bool(p.words[1]);
  if (!b) {
    return cu::fail(cu::bad_argument("Unknown property compat option \"" + p.words[1] + "\""));
  }
  s.editor->property_compat = *b;
  return CommandResult::success();
}

// The values of one property as Tcl list elements (printPropertiesFunc and
// the single-property result).
Value::List property_values(const cu::Session& s, const PropertyValue& v) {
  Value::List out;
  if (const auto* str = std::get_if<std::string>(&v)) {
    out.emplace_back(*str);
  } else if (const auto* ints = std::get_if<IntegerList>(&v)) {
    for (const std::int32_t i : ints->values) {
      out.emplace_back(std::int64_t{i});
    }
  } else if (const auto* dims = std::get_if<DimensionList>(&v)) {
    for (const std::int32_t i : dims->values) {
      out.push_back(cu::coord_value(s, i));
    }
  } else if (const auto* dbl = std::get_if<DoubleList>(&v)) {
    for (const std::int64_t i : dbl->values) {
      out.emplace_back(i);
    }
  } else if (const auto* plane = std::get_if<PlaneValue>(&v)) {
    // printPlanePropFunc: x values with the x grid, y values with the y grid.
    for (const Rect& r : plane_property_tiles(*plane)) {
      out.push_back(cu::coord_value(s, r.x0, cmd::Axis::x));
      out.push_back(cu::coord_value(s, r.y0, cmd::Axis::y));
      out.push_back(cu::coord_value(s, r.x1, cmd::Axis::x));
      out.push_back(cu::coord_value(s, r.y1, cmd::Axis::y));
    }
  }
  return out;
}

CommandResult property_list(const cu::Session& s, const CellDef& def) {
  Value::List items;
  def.properties().for_each([&](std::string_view name, const PropertyValue& v) {
    Value::List item{Value{std::string{name}}};
    for (Value& e : property_values(s, v)) {
      item.push_back(std::move(e));
    }
    items.emplace_back(std::move(item));
  });
  return CommandResult::success(Value{std::move(items)});
}

// GCC 14 reports a bogus -Wmaybe-uninitialized for the variant inside the
// Value moved out of `values` below (not GCC 15 or Clang).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
CommandResult property_get(const cu::Session& s, const CellDef& def, const PropertyArgs& p) {
  const PropertyValue* v = def.properties().find(p.words[0]);
  if (v == nullptr) {
    if (!p.dolist) {
      cu::warn(s, "Property name \"" + p.words[0] + "\" is not defined");
    }
    return CommandResult::success();
  }
  Value::List values = property_values(s, *v);
  if (std::holds_alternative<std::string>(*v) || (values.size() == 1 && !std::holds_alternative<PlaneValue>(*v))) {
    return CommandResult::success(std::move(values.front()));
  }
  return CommandResult::success(Value{std::move(values)});
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

int reserved_type(std::string_view name, int type) {
  if (name == "GDS_START" || name == "GDS_END") {
    return kDouble;
  }
  if (name == "GDS_FILE") {
    return kString;
  }
  if (name == "FIXED_BBOX" || name == "OBS_BBOX") {
    return kDimension;
  }
  return name.starts_with("MASKHINTS_") ? kPlane : type;
}

std::vector<std::string> split_words(std::string_view text) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])) != 0) {
      ++i;
    }
    const std::size_t start = i;
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])) == 0) {
      ++i;
    }
    if (i > start) {
      out.emplace_back(text.substr(start, i - start));
    }
  }
  return out;
}

// sscanf("%d") / sscanf("%lld"): a leading integer (after blanks and an
// optional sign), saturated instead of overflowing; 0 with Magic's error
// when there is none.
std::int64_t scan_integer(const cu::Session& s, const std::string& word, bool split) {
  std::string_view v = word;
  while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front())) != 0) {
    v.remove_prefix(1);
  }
  const bool negative = !v.empty() && v.front() == '-';
  if (!v.empty() && (v.front() == '+' || v.front() == '-')) {
    v.remove_prefix(1);
  }
  std::uint64_t magnitude = 0;
  const auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), magnitude);
  if (ptr == v.data()) {
    cu::warn(s, split ? "Unable to parse integer value from \"" + word + "\""
                      : "Unable to parse value \"" + word + "\" as an integer");
    return 0;
  }
  constexpr std::uint64_t kMax = static_cast<std::uint64_t>(INT64_MAX);
  if (ec == std::errc::result_out_of_range || magnitude > kMax) {
    return negative ? INT64_MIN : INT64_MAX;
  }
  const auto value = static_cast<std::int64_t>(magnitude);
  return negative ? -value : value;
}

// mask-hints "Setting mask hints with the property command": every value
// is a cmdParseCoord coordinate (even positions x, odd positions y; a word
// that is not a number reports and reads as 0), complete groups of four are
// painted with the CIF paint table (an inverted rectangle paints nothing)
// and trailing values are ignored.
diag::Outcome<PropertyValue> build_hint_property(const cu::Session& s, const std::vector<std::string>& values) {
  std::vector<Coord> coords;
  for (std::size_t i = 0; i < values.size(); ++i) {
    auto c = cu::parse_distance(s, values[i], i % 2 == 0 ? cmd::Axis::x : cmd::Axis::y, false);
    if (!c.ok()) {
      return c.error();
    }
    coords.push_back(c.value());
  }
  PlaneValue plane;
  for (std::size_t i = 0; i + 3 < coords.size(); i += 4) {
    plane.paint(Rect{coords[i], coords[i + 1], coords[i + 2], coords[i + 3]});
  }
  return PropertyValue{std::move(plane)};
}

diag::Outcome<PropertyValue> build_property(const cu::Session& s, int type, const std::vector<std::string>& values,
                                            bool split) {
  if (type == kString) {
    return PropertyValue{values.front()};
  }
  if (type == kPlane) {
    return build_hint_property(s, values);
  }
  IntegerList ints;
  DimensionList dims;
  DoubleList dbl;
  std::vector<Coord> coords;
  for (const std::string& w : values) {
    if (type == kInteger) {
      ints.values.push_back(static_cast<std::int32_t>(std::clamp<std::int64_t>(scan_integer(s, w, split), INT32_MIN,
                                                                               INT32_MAX)));
    } else if (type == kDouble) {
      dbl.values.push_back(scan_integer(s, w, split));
    } else {
      auto c = cu::parse_coord(s, w);
      if (!c.ok()) {
        return c.error();
      }
      coords.push_back(c.value());
    }
  }
  if (type == kInteger) {
    return PropertyValue{ints};
  }
  if (type == kDouble) {
    return PropertyValue{dbl};
  }
  dims.values.assign(coords.begin(), coords.end());
  return PropertyValue{dims};
}

CommandResult property_put(const cu::Session& s, CellDef& def, const PropertyArgs& p) {
  const std::string& name = p.words[0];
  if (p.words[1].empty()) {
    (void)change_property(def, name, std::nullopt);
  } else {
    const int type = reserved_type(name, p.type);
    const bool split = p.words.size() == 2;
    const std::vector<std::string> values =
        split && type != kString ? split_words(p.words[1])
                                 : std::vector<std::string>{p.words.begin() + 1, p.words.end()};
    if (values.empty()) {
      return CommandResult::success();
    }
    auto v = build_property(s, type, values, split);
    if (!v.ok()) {
      return cu::fail(v.error());
    }
    (void)change_property(def, name, std::move(v).value());
  }
  mark_modified(def);
  return CommandResult::success();
}

CommandResult property_command(const cu::Session& s, CellDef* def, const std::vector<std::string>& args) {
  const PropertyArgs p = parse_property_args(args, s.magic_compat);
  if (p.compat_switch) {
    return property_compat(s, p);
  }
  if (def == nullptr) {
    return cu::fail(diag::make(codes::kNoEditCell, "No cell to set a property on."));
  }
  if (p.words.empty()) {
    return property_list(s, *def);
  }
  if (p.words.size() == 1) {
    return property_get(s, *def, p);
  }
  return property_put(s, *def, p);
}

CommandResult cmd_property(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  return property_command(s.value(), cu::property_target(s.value()), inv.args);
}

}  // namespace

namespace cmdutil {

CommandResult property_on(const Session& s, CellDef& def, const std::vector<std::string>& args) {
  return property_command(s, &def, args);
}

CellDef* property_target(const Session& s) {
  const view::LayoutWindow* w = s.editor->windows.current();
  if (w != nullptr && w->root() != nullptr) {
    return w->root();  // CmdProperty: the window's root cell (Q-CMDEDIT-93)
  }
  return s.editor->edit_cell;
}

bool register_label_commands(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "label",
                "label text ?position? ?layer? | label text font ?size? ?rotation? ?offx offy? ?position? ?layer?",
                "Creates a label on the box (layer chosen from the paint when omitted).", cmd_label);
  ok = add(registry, "setlabel",
           "setlabel ?-default? box|font|fontlist|justify|layer|offset|rotate|size|sticky|text|help ?value?",
           "Shows or changes attributes of the selected edit-cell labels, or the defaults of later labels.",
           setlabel_command) && ok;
  ok = add(registry, "port",
           "port [name|index] class|use|shape|index|equivalent|exists|connections|first|next|last|make|makeall|"
           "name|remove|renumber|help ?args?",
           "Shows or changes port attributes of the named label or the label in the box.", cmd_port) && ok;
  ok = add(registry, "property", "property ?compat ?bool?? | ?type? ?name ?value??",
           "Lists, shows or sets properties of the window's root cell (the edit cell without a window).", cmd_property) && ok;
  return ok;
}

}  // namespace cmdutil

}  // namespace hefesto::db
