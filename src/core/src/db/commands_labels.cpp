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
#include <utility>

#include "db/command_util.hpp"
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
  if (inv.args.size() < 3) {
    put.value()->type = label_layer_for(*e.s.tech, choose_label_layer(*e.def, *e.s.tech, *put.value()));
  }
  recompute_bbox(*e.def, e.s.magic_compat);
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

template <typename E, std::size_t N>
std::optional<E> lookup(const std::array<std::pair<std::string_view, E>, N>& table, std::string_view name) {
  for (const auto& [n, v] : table) {
    if (n == name) {
      return v;
    }
  }
  return std::nullopt;
}

template <typename E, std::size_t N>
std::string_view name_of(const std::array<std::pair<std::string_view, E>, N>& table, E value) {
  for (const auto& [n, v] : table) {
    if (v == value) {
      return n;
    }
  }
  return "default";
}

constexpr std::array<std::pair<std::string_view, PortClass>, 6> kClasses = {{
    {"default", PortClass::default_class}, {"input", PortClass::input}, {"output", PortClass::output},
    {"tristate", PortClass::tristate}, {"bidirectional", PortClass::bidirectional},
    {"feedthrough", PortClass::feedthrough}}};
constexpr std::array<std::pair<std::string_view, PortUse>, 9> kUses = {{
    {"default", PortUse::default_use}, {"signal", PortUse::signal}, {"analog", PortUse::analog},
    {"power", PortUse::power}, {"ground", PortUse::ground}, {"clock", PortUse::clock}, {"reset", PortUse::reset},
    {"scan", PortUse::scan}, {"tieoff", PortUse::tieoff}}};
constexpr std::array<std::pair<std::string_view, PortShape>, 4> kShapes = {{
    {"default", PortShape::default_shape}, {"abutment", PortShape::abutment}, {"ring", PortShape::ring},
    {"feedthrough", PortShape::feedthrough}}};

template <typename E, std::size_t N>
CommandResult port_enum(Label& lab, E& field, const std::array<std::pair<std::string_view, E>, N>& table,
                        const std::vector<std::string>& args) {
  if (args.size() == 1) {
    return CommandResult::success(Value{std::string{name_of(table, field)}});
  }
  const auto v = lookup(table, args[1]);
  if (!v) {
    return cu::fail(cu::bad_argument("bad value \"" + args[1] + "\""));
  }
  field = *v;
  (void)lab;
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

CommandResult port_on_label(CellDef& def, Label& lab, const std::vector<std::string>& args) {
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
    return port_enum(lab, lab.port_class, kClasses, args);
  }
  if (op == "use") {
    return port_enum(lab, lab.port_use, kUses, args);
  }
  if (op == "shape") {
    return port_enum(lab, lab.port_shape, kShapes, args);
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
  CommandResult r = port_on_label(*e.def, *labels.front(), inv.args);
  const std::string& op = inv.args[0];
  const bool changes = op == "make" || op == "remove" || (inv.args.size() > 1 && op != "exists");
  if (r.ok() && changes) {
    mark_modified(*e.def);
  }
  return r;
}

// --- property ------------------------------------------------------------------


std::optional<bool> parse_bool(std::string_view v) {
  for (const std::string_view t : {"enable", "yes", "true", "on", "1"}) {
    if (v == t) {
      return true;
    }
  }
  for (const std::string_view f : {"disable", "no", "false", "off", "0"}) {
    if (v == f) {
      return false;
    }
  }
  return std::nullopt;
}

CommandResult property_compat(const cu::Session& s, const std::vector<std::string>& args) {
  if (args.size() == 1) {
    return CommandResult::success(Value{std::int64_t{s.editor->property_compat ? 1 : 0}});
  }
  const auto b = parse_bool(args[1]);
  if (!b) {
    return cu::fail(cu::bad_argument("bad boolean \"" + args[1] + "\""));
  }
  s.editor->property_compat = *b;
  return CommandResult::success();
}

CommandResult property_list(const CellDef& def) {
  Value::List items;
  def.properties().for_each([&](std::string_view name, const PropertyValue& v) {
    items.push_back(Value{Value::List{Value{std::string{name}}, Value{property_text(v)}}});
  });
  return CommandResult::success(Value{std::move(items)});
}

bool is_type_keyword(std::string_view s) {
  return s == "string" || s == "integer" || s == "dimension" || s == "double";
}

// Dimensions are typed in the current snap units: each space-separated value
// converted to internal units.
diag::Outcome<std::string> dimension_text(const cu::Session& s, const std::string& text) {
  std::string out;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::size_t end = text.find(' ', pos);
    const std::string tok = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    if (!tok.empty()) {
      auto c = cu::parse_coord(s, tok);
      if (!c.ok()) {
        return c.error();
      }
      out += (out.empty() ? "" : " ") + std::to_string(c.value());
    }
    pos = end == std::string::npos ? text.size() : end + 1;
  }
  return out;
}

CommandResult property_put(const cu::Session& s, CellDef& def, std::string_view type, const std::string& name,
                           const std::string& text) {
  if (text.empty()) {
    set_property(def, name, std::nullopt);
    mark_modified(def);
    return CommandResult::success();
  }
  std::string value_text = text;
  if (type == "dimension") {
    auto internal = dimension_text(s, text);
    if (!internal.ok()) {
      return cu::fail(internal.error());
    }
    value_text = std::move(internal).value();
  }
  auto v = parse_property_value(type, value_text);
  if (!v.ok()) {
    return cu::fail(v.error());
  }
  set_property(def, name, coerce_reserved_property(name, v.value()));
  mark_modified(def);
  return CommandResult::success();
}

CommandResult cmd_property(CommandContext& ctx, const Invocation& inv) {
  auto s = cu::session(ctx);
  if (!s.ok()) {
    return cu::fail(s.error());
  }
  const auto& args = inv.args;
  if (!args.empty() && args[0] == "compat") {
    return property_compat(s.value(), args);
  }
  auto def = cu::edit_cell(s.value());
  if (!def.ok()) {
    return cu::fail(def.error());
  }
  std::size_t first = 0;
  std::string_view type = "string";
  if (!args.empty() && is_type_keyword(args[0])) {
    type = args[0];
    first = 1;
  }
  if (args.size() == first) {
    return property_list(*def.value());
  }
  const std::string& name = args[first];
  if (args.size() == first + 1) {
    const PropertyValue* v = def.value()->properties().find(name);
    return CommandResult::success(v != nullptr ? Value{property_text(*v)} : Value{});
  }
  return property_put(s.value(), *def.value(), type, name, cu::join_args(args, first + 1));
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
