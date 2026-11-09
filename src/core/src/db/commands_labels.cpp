// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MODEL commands, part 2: label, setlabel, port, property (minimal Magic
// forms; labels are addressed through the box instead of the selection
// until cmd-selection lands in M2).
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
#include <variant>
#include <utility>

#include "db/command_util.hpp"
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
  return LabelTarget{s.value(), def.value(), b.value()};
}

std::vector<Label*> labels_in(CellDef& def, const Rect& area) {
  std::vector<Label*> out;
  for (Label& l : def.labels()) {
    if (label_in_area(l.rect, area)) {
      out.push_back(&l);
    }
  }
  return out;
}

// --- label -------------------------------------------------------------------

CommandResult cmd_label(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.empty() || inv.args.size() > 3) {
    return cu::fail(cu::bad_argument("usage: label text ?position? ?layer?"));
  }
  auto t = label_target(ctx);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const LabelTarget& e = t.value();
  Label lab;
  lab.text = inv.args[0];
  lab.rect = e.area;
  bool auto_justify = true;
  if (inv.args.size() >= 2) {
    const int pos = parse_position(inv.args[1], false);
    if (pos < 0) {
      return cu::fail(cu::bad_argument("bad position \"" + inv.args[1] + "\""));
    }
    lab.justification = pos;
    auto_justify = false;
  }
  if (inv.args.size() == 3) {
    auto layer = cu::parse_layer(*e.s.tech, inv.args[2]);
    if (!layer.ok()) {
      return cu::fail(layer.error());
    }
    lab.type = label_layer_for(*e.s.tech, layer.value());
  }
  auto put = put_label(*e.def, std::move(lab), auto_justify);
  if (!put.ok()) {
    return cu::fail(put.error());
  }
  // CmdLabelProc: a label without a layer starts on space; DBAdjustLabels
  // then moves every label in the box to the paint under it, with notes.
  // (Magic ref: commands/CmdLQ.c:124-137)
  Label* made = put.value();
  std::vector<std::string> notes;
  (void)adjust_labels(*e.def, *e.s.tech, e.area, &notes);
  for (std::string& n : notes) {
    cu::print(e.s, std::move(n));
  }
  made->type = label_layer_for(*e.s.tech, made->type);
  recompute_bbox(*e.def, e.s.magic_compat);
  // DBWLabelChanged (ui-windows-and-views "Change-driven redisplay"): the
  // label's area, grown by one unit so that a point label has an area, with
  // the window's label extent.
  e.s.editor->windows.area_changed(*e.def, expand(e.area, 1), std::nullopt);
  return CommandResult::success();
}

// --- setlabel ----------------------------------------------------------------

using LabelGetter = Value (*)(const cu::Session&, const Label&);
using LabelSetter = std::optional<diag::Diagnostic> (*)(const cu::Session&, Label&, const std::vector<std::string>&);

Value get_text(const cu::Session&, const Label& l) { return Value{l.text}; }
Value get_justify(const cu::Session&, const Label& l) { return Value{std::int64_t{l.justification}}; }
Value get_font(const cu::Session&, const Label& l) { return Value{std::int64_t{l.font}}; }
Value get_size(const cu::Session& s, const Label& l) { return cu::coord_value(s, l.size); }
Value get_rotate(const cu::Session&, const Label& l) { return Value{std::int64_t{l.rotation}}; }
Value get_offset(const cu::Session&, const Label& l) {
  return Value{Value::List{Value{std::int64_t{l.offset.x}}, Value{std::int64_t{l.offset.y}}}};
}
Value get_layer(const cu::Session& s, const Label& l) { return Value{std::string{s.tech->type_name(l.type)}}; }
Value get_sticky(const cu::Session&, const Label& l) { return Value{std::int64_t{l.sticky ? 1 : 0}}; }

std::optional<diag::Diagnostic> set_text(const cu::Session&, Label& l, const std::vector<std::string>& v) {
  if (v[0].empty()) {
    return diag::make(codes::kLabelEmpty, "a label needs non-empty text");
  }
  l.text = v[0];
  return std::nullopt;
}

std::optional<diag::Diagnostic> set_int_field(std::int32_t& field, const std::string& text) {
  auto n = cu::parse_int(text);
  if (!n.ok()) {
    return n.error();
  }
  field = static_cast<std::int32_t>(n.value());
  return std::nullopt;
}

std::optional<diag::Diagnostic> set_justify(const cu::Session&, Label& l, const std::vector<std::string>& v) {
  const int pos = parse_position(v[0], false);
  if (pos < 0) {
    return cu::bad_argument("bad position \"" + v[0] + "\"");
  }
  l.justification = pos;
  return std::nullopt;
}

std::optional<diag::Diagnostic> set_font(const cu::Session& s, Label& l, const std::vector<std::string>& v) {
  if (v[0] == "default" || v[0] == "-1") {
    l.font = -1;
    l.size = 0;
    l.rotation = 0;
    l.offset = Point{};
    return std::nullopt;
  }
  // A loaded font by name (DBNameToFont), else an index.
  if (const int index = s.editor->fonts.find(v[0]); index >= 0) {
    l.font = index;
    return std::nullopt;
  }
  return set_int_field(l.font, v[0]);
}

// Label::text_bbox of an outline-font label (font-labels "Label geometry"),
// so the extended bounding box (update_bounding_boxes) covers the text.
void update_text_bbox(const cu::Session& s, Label& l) {
  const display::OutlineFont* font = l.font >= 0 ? s.editor->fonts.font_of(l) : nullptr;
  if (font == nullptr) {
    l.text_bbox.reset();
    return;
  }
  l.text_bbox = display::font_label_geometry(l, *font, s.magic_compat).bbox;
}

std::optional<diag::Diagnostic> set_size(const cu::Session& s, Label& l, const std::vector<std::string>& v) {
  auto c = cu::parse_coord(s, v[0]);
  if (!c.ok()) {
    return c.error();
  }
  l.size = c.value();
  return std::nullopt;
}

std::optional<diag::Diagnostic> set_rotate(const cu::Session&, Label& l, const std::vector<std::string>& v) {
  return set_int_field(l.rotation, v[0]);
}

std::optional<diag::Diagnostic> set_offset(const cu::Session&, Label& l, const std::vector<std::string>& v) {
  if (v.size() != 2) {
    return cu::bad_argument("usage: setlabel offset x y");
  }
  if (auto e = set_int_field(l.offset.x, v[0])) {
    return e;
  }
  return set_int_field(l.offset.y, v[1]);
}

std::optional<diag::Diagnostic> set_layer(const cu::Session& s, Label& l, const std::vector<std::string>& v) {
  auto t = cu::parse_layer(*s.tech, v[0]);
  if (!t.ok()) {
    return t.error();
  }
  l.type = label_layer_for(*s.tech, t.value());
  return std::nullopt;
}

std::optional<diag::Diagnostic> set_sticky(const cu::Session&, Label& l, const std::vector<std::string>& v) {
  const std::string& b = v[0];
  l.sticky = b == "1" || b == "true" || b == "yes" || b == "on";
  return std::nullopt;
}

struct LabelOption {
  std::string_view name;
  LabelGetter get;
  LabelSetter set;
};

constexpr std::array<LabelOption, 8> kLabelOptions = {{
    {"text", get_text, set_text},
    {"justify", get_justify, set_justify},
    {"font", get_font, set_font},
    {"size", get_size, set_size},
    {"rotate", get_rotate, set_rotate},
    {"offset", get_offset, set_offset},
    {"layer", get_layer, set_layer},
    {"sticky", get_sticky, set_sticky},
}};

CommandResult cmd_setlabel(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.empty()) {
    return cu::fail(cu::bad_argument("usage: setlabel option ?value?"));
  }
  const auto* opt = std::find_if(kLabelOptions.begin(), kLabelOptions.end(),
                                 [&](const LabelOption& o) { return o.name == inv.args[0]; });
  if (opt == kLabelOptions.end()) {
    return cu::fail(cu::bad_argument("unknown setlabel option \"" + inv.args[0] + "\""));
  }
  auto t = label_target(ctx);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const LabelTarget& e = t.value();
  const std::vector<Label*> labels = labels_in(*e.def, e.area);
  if (inv.args.size() == 1) {
    Value::List values;
    for (const Label* l : labels) {
      values.push_back(opt->get(e.s, *l));
    }
    return CommandResult::success(Value{std::move(values)});
  }
  const std::vector<std::string> v(inv.args.begin() + 1, inv.args.end());
  for (Label* l : labels) {
    if (auto err = opt->set(e.s, *l, v)) {
      return cu::fail(*err);
    }
    update_text_bbox(e.s, *l);
  }
  if (!labels.empty()) {
    mark_modified(*e.def);
    recompute_bbox(*e.def, e.s.magic_compat);
  }
  return CommandResult::success();
}

// --- port --------------------------------------------------------------------

std::uint8_t directions_of(Position pos) {
  constexpr std::array<std::uint8_t, 9> kDirs = {
      kPortNorth | kPortEast | kPortSouth | kPortWest,  // center
      kPortNorth, kPortNorth | kPortEast, kPortEast, kPortSouth | kPortEast,
      kPortSouth, kPortSouth | kPortWest, kPortWest, kPortNorth | kPortWest};
  return pos >= 0 && pos <= 8 ? kDirs[static_cast<std::size_t>(pos)] : 0;
}

// CmdPort PORT_MAKE: the index a new port takes when none is given.
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

std::optional<diag::Diagnostic> port_make(const CellDef& def, Label& lab, const std::vector<std::string>& args) {
  if (lab.is_port()) {
    return cu::bad_argument("label \"" + lab.text + "\" is already a port");
  }
  std::size_t next = 1;
  if (args.size() > 1 && cu::parse_int(args[1]).ok()) {
    lab.port_index = static_cast<std::uint32_t>(cu::parse_int(args[1]).value());
    next = 2;
  } else {
    lab.port_index = next_port_index(def, lab);
  }
  std::uint8_t dirs = 0;
  if (next == args.size()) {
    dirs = directions_of(lab.justification);
  }
  for (std::size_t i = next; i < args.size(); ++i) {
    const int pos = parse_position(args[i], true);
    if (pos < 0) {
      return cu::bad_argument("bad direction \"" + args[i] + "\"");
    }
    if (pos != kCenter) {
      dirs = static_cast<std::uint8_t>(dirs | directions_of(pos));
    }
  }
  lab.port_directions = dirs;
  return std::nullopt;
}

/// One port attribute keyword of Magic's `port` command tables, synonyms
/// included (Magic ref: commands/CmdLQ.c:1558-1625).
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

/// `port class|use|shape [value]` (Magic ref: commands/CmdLQ.c:1870-1990): a
/// query returns the keyword; a value (Lookup: unique prefix) is set on every
/// port of the cell with the same text and index; an unknown value prints
/// the usage with the keywords and changes nothing. `changed` tells whether
/// anything was set.
template <typename E, std::size_t N>
CommandResult port_enum(const cu::Session& s, CellDef& def, const Label& lab, E Label::*field,
                        const std::array<PortWord<E>, N>& table, const std::vector<std::string>& args, bool& changed) {
  if (args.size() == 1) {
    return CommandResult::success(Value{std::string{name_of(table, lab.*field)}});
  }
  int idx = -1;
  std::string usage = "Usage:  port " + args[0] + " <type>, where <type> is one of:";
  {
    std::vector<std::string_view> names;
    for (const auto& w : table) {
      names.push_back(w.name);
      usage += "\n    " + std::string{w.name};
    }
    idx = cu::lookup_in(args[1], names);
  }
  if (idx < 0) {
    cu::warn(s, usage);
    return CommandResult::success();
  }
  const E value = table[static_cast<std::size_t>(idx)].value;
  const std::string text = lab.text;
  const std::uint32_t index = lab.port_index;
  for (Label& other : def.labels()) {
    if (other.is_port() && other.text == text && other.port_index == index) {
      other.*field = value;
    }
  }
  changed = true;
  return CommandResult::success();
}

CommandResult port_index(Label& lab, const std::vector<std::string>& args) {
  if (args.size() == 1) {
    return CommandResult::success(Value{std::int64_t{lab.port_index}});
  }
  auto n = cu::parse_int(args[1]);
  if (!n.ok()) {
    return cu::fail(n.error());
  }
  lab.port_index = static_cast<std::uint32_t>(n.value());
  return CommandResult::success();
}

void port_remove(Label& lab) {
  lab.port_directions = 0;
  lab.port_class = PortClass::default_class;
  lab.port_use = PortUse::default_use;
  lab.port_shape = PortShape::default_shape;
  lab.sticky = false;
}

CommandResult port_on_label(const cu::Session& s, CellDef& def, Label& lab, const std::vector<std::string>& args,
                            bool& changed) {
  const std::string& op = args[0];
  if (op == "exists") {
    return CommandResult::success(Value{std::int64_t{lab.is_port() ? 1 : 0}});
  }
  if (op == "make") {
    auto err = port_make(def, lab, args);
    return err ? cu::fail(*err) : CommandResult::success();
  }
  if (op == "remove") {
    port_remove(lab);
    return CommandResult::success();
  }
  if (op == "index") {
    return port_index(lab, args);
  }
  if (op == "class") {
    return port_enum(s, def, lab, &Label::port_class, kClasses, args, changed);
  }
  if (op == "use") {
    return port_enum(s, def, lab, &Label::port_use, kUses, args, changed);
  }
  if (op == "shape") {
    return port_enum(s, def, lab, &Label::port_shape, kShapes, args, changed);
  }
  return cu::fail(cu::bad_argument("unknown port option \"" + op + "\""));
}

CommandResult cmd_port(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.empty()) {
    return cu::fail(cu::bad_argument("usage: port option ?args?"));
  }
  auto t = label_target(ctx);
  if (!t.ok()) {
    return cu::fail(t.error());
  }
  const LabelTarget& e = t.value();
  if (inv.args[0] == "renumber") {
    renumber_ports(*e.def, e.s.magic_compat);
    mark_modified(*e.def);
    return CommandResult::success();
  }
  const std::vector<Label*> labels = labels_in(*e.def, e.area);
  if (labels.size() != 1) {
    return cu::fail(diag::make(codes::kNoLabel, labels.empty() ? "no label in the box" : "more than one label in the box"));
  }
  const std::string& op = inv.args[0];
  const bool attribute = op == "class" || op == "use" || op == "shape";
  bool changed = !attribute && (op == "make" || op == "remove" || (inv.args.size() > 1 && op != "exists"));
  CommandResult r = port_on_label(e.s, *e.def, *labels.front(), inv.args, changed);
  if (r.ok() && changed) {
    mark_modified(*e.def);
  }
  return r;
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
    for (const Rect& r : plane_property_tiles(*plane)) {
      for (const Coord c : {r.x0, r.y0, r.x1, r.y1}) {
        out.push_back(cu::coord_value(s, c));
      }
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

diag::Outcome<PropertyValue> build_property(const cu::Session& s, int type, const std::vector<std::string>& values,
                                            bool split) {
  if (type == kString) {
    return PropertyValue{values.front()};
  }
  IntegerList ints;
  DimensionList dims;
  DoubleList dbl;
  PlaneValue plane;
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
  if (type == kDimension) {
    dims.values.assign(coords.begin(), coords.end());
    return PropertyValue{dims};
  }
  for (std::size_t i = 0; i + 3 < coords.size(); i += 4) {
    plane.rects.push_back(canonical(Rect{coords[i], coords[i + 1], coords[i + 2], coords[i + 3]}));
  }
  return PropertyValue{plane};
}

CommandResult property_put(const cu::Session& s, CellDef& def, const PropertyArgs& p) {
  const std::string& name = p.words[0];
  if (p.words[1].empty()) {
    set_property(def, name, std::nullopt);
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
    set_property(def, name, std::move(v).value());
  }
  mark_modified(def);
  return CommandResult::success();
}

CommandResult cmd_property(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  const PropertyArgs p = parse_property_args(inv.args, s.value().magic_compat);
  if (p.compat_switch) {
    return property_compat(s.value(), p);
  }
  auto def = cu::edit_cell(s.value());
  if (!def.ok()) {
    return cu::fail(def.error());
  }
  if (p.words.empty()) {
    return property_list(s.value(), *def.value());
  }
  if (p.words.size() == 1) {
    return property_get(s.value(), *def.value(), p);
  }
  return property_put(s.value(), *def.value(), p);
}

}  // namespace

namespace cmdutil {

bool register_label_commands(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "label", "label text ?position? ?layer?",
                "Creates a label on the box (layer chosen from the paint when omitted).", cmd_label);
  ok = add(registry, "setlabel", "setlabel text|justify|font|size|rotate|offset|layer|sticky ?value?",
           "Shows or changes attributes of the labels in the box.", cmd_setlabel) && ok;
  ok = add(registry, "port", "port make|remove|renumber|index|class|use|shape|exists ?args?",
           "Shows or changes port attributes of the label in the box.", cmd_port) && ok;
  ok = add(registry, "property", "property ?compat ?bool?? | ?type? ?name ?value??",
           "Lists, shows or sets properties of the edit cell.", cmd_property) && ok;
  return ok;
}

}  // namespace cmdutil

}  // namespace hefesto::db
