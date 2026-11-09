// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `setlabel [-default] option ?value?` (cmd-labels-properties "Setlabel
// command", font-labels "Font-related label commands"). The labels changed
// or queried are the edit cell's labels in the box (Magic: the selected
// labels) until cmd-selection lands in M2; without an edit cell or a box no
// label is addressed, so `setlabel font <file> <scale>` still loads fonts
// from a startup script, as in Magic.
// Ported from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdRS.c (CmdSetLabel
// and its cmdLabel*Func callbacks). Magic is Copyright (C) 1985, 1990
// Regents of the University of California (permissive license,
// THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "db/command_util.hpp"
#include "hefesto/cell/fonts.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/display/font.hpp"

namespace hefesto::db::cmdutil {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;
using cmd::Value;

namespace {

enum Option { kBox, kFont, kFontList, kJustify, kLayer, kOffset, kRotate, kSize, kSticky, kText, kHelp };

constexpr std::array<std::string_view, 11> kOptionNames = {"box",    "font",   "fontlist", "justify",
                                                           "layer",  "offset", "rotate",   "size",
                                                           "sticky", "text",   "help"};

// cmdLabelSetOption, printed by `setlabel help`.
constexpr std::array<std::string_view, 11> kOptionHelp = {
    "box <lx> <ly> <ux> <uy>\tchange/get label box", "font <name>\t\tchange/get label font",
    "fontlist\t\tlist available fonts",              "justify <position>\tchange/get label justification",
    "layer <type>\t\tchange/get layer type",         "offset <x> <y>\t\tchange/get label offset",
    "rotate <degrees>\tchange/get label rotation",   "size <value>\t\tchange/get label size",
    "sticky [true|false]\tchange/get sticky property", "text <text>\t\tchange/get label text",
    "help\t\t\tprint this help info"};

/// One `setlabel` invocation after `-default` was taken off.
struct Call {
  Session s;
  bool defaults = false;           // -default: the default label, not the labels
  std::vector<std::string> args;   // the option word and its values
  CellDef* def = nullptr;          // the edit cell; nullptr: no label is addressed
  std::optional<Rect> area;        // the box

  /// Magic's locargc - 1: the option word and its values.
  [[nodiscard]] std::size_t n() const noexcept { return args.size(); }
  [[nodiscard]] bool query() const noexcept { return args.size() == 1; }
  [[nodiscard]] LabelDefaults& dl() const { return *s.editor->label_defaults; }
};

[[nodiscard]] bool addressed(const Call& c, const Label& l) {
  return c.def != nullptr && c.area && label_in_area(l.rect, *c.area);
}

/// SelEnumLabels with a query callback: one result element per label.
template <typename Get>
CommandResult query_labels(const Call& c, Get get) {
  Value::List values;
  if (c.def != nullptr) {
    for (const Label& l : c.def->labels()) {
      if (addressed(c, l)) {
        values.push_back(get(l));
      }
    }
  }
  return CommandResult::success(Value{std::move(values)});
}

/// SelEnumLabelsMirror with a change callback returning whether the label
/// changed: the rendered text box is recomputed (DBFontLabelSetBBox), the
/// old and new areas are redisplayed and the cell is marked modified.
template <typename Change>
CommandResult change_labels(const Call& c, Change change) {
  if (c.def == nullptr) {
    return CommandResult::success();
  }
  bool any = false;
  for (Label& l : c.def->labels()) {
    if (!addressed(c, l)) {
      continue;
    }
    const Label before = l;
    if (!change(l)) {
      continue;
    }
    update_font_label(c.s, l);
    label_changed(c.s, *c.def, before);
    label_changed(c.s, *c.def, l);
    any = true;
  }
  if (any) {
    mark_modified(*c.def);
    recompute_bbox(*c.def, c.s.magic_compat);
  }
  return CommandResult::success();
}

template <typename T>
bool assign(T& field, const T& value) {
  if (field == value) {
    return false;
  }
  field = value;
  return true;
}

// --- values ------------------------------------------------------------------

/// DBWPrintValue of a size or offset stored in eighths (÷8, truncated).
Value eighths_value(const Session& s, std::int32_t v) { return coord_value(s, v / 8); }

std::string font_name(const Session& s, std::int32_t font) {
  if (font < 0 || static_cast<std::size_t>(font) >= s.editor->fonts.fonts.size()) {
    return "default";
  }
  return s.editor->fonts.fonts[static_cast<std::size_t>(font)]->name;
}

/// The system path entries, expanded (PaOpen's SysLibPath).
std::vector<platform::Path> system_dirs(const Session& s) {
  std::vector<platform::Path> dirs;
  for (const std::string& entry : s.editor->search_paths.system) {
    if (const auto dir = cell::expand_entry(entry, s.editor->path_variables); dir && !dir->empty()) {
      dirs.emplace_back(*dir);
    }
  }
  return dirs;
}

// --- options -----------------------------------------------------------------

CommandResult opt_help(const Call& c) {
  std::string text = "Usage:  setlabel [option], where [option] is one of:";
  for (const std::string_view line : kOptionHelp) {
    text += "\n    " + std::string{line};
  }
  warn(c.s, std::move(text));
  return CommandResult::success();
}

CommandResult opt_fontlist(const Call& c) {
  Value::List names;
  for (const auto& f : c.s.editor->fonts.fonts) {
    names.emplace_back(f->name);
  }
  return CommandResult::success(Value{std::move(names)});
}

CommandResult opt_text(const Call& c) {
  if (c.defaults) {
    warn(c.s, "Cannot set a default label text.");
    return CommandResult::success();
  }
  if (c.query()) {
    return query_labels(c, [](const Label& l) { return Value{l.text}; });
  }
  if (c.def == nullptr) {
    return CommandResult::success();
  }
  // cmdLabelTextFunc: a new label with the new text and the old attributes
  // is appended, then every label with the old rectangle and text is erased.
  const std::string& text = c.args[1];
  std::vector<Label> renamed;
  for (const Label& l : c.def->labels()) {
    if (addressed(c, l) && l.text != text) {
      renamed.push_back(l);
    }
  }
  for (Label& fresh : renamed) {
    const std::string old = fresh.text;
    fresh.text = text;
    update_font_label(c.s, fresh);
    c.def->labels().push_back(fresh);
    erase_labels_by_content(*c.def, fresh.rect, old);
    label_changed(c.s, *c.def, fresh);
  }
  if (!renamed.empty()) {
    mark_modified(*c.def);
    recompute_bbox(*c.def, c.s.magic_compat);
  }
  return CommandResult::success();
}

// `setlabel font <index>` prints the font's name. Magic declares the index
// in an inner block, so the font applied afterwards stays -1 (Q-MISC-28).
void print_font_index(const Call& c, std::int32_t index) {
  const auto count = static_cast<std::int32_t>(c.s.editor->fonts.fonts.size());
  if (index < -1 || index >= count) {
    warn(c.s, count == 0 ? std::string{"No vector outline fonts are loaded."}
                         : "Font index out of range (0 to " + std::to_string(count - 1) + ")");
  } else {
    print(c.s, font_name(c.s, index));
  }
}

// DBNameToFont, loading the font file when the name is unknown; nullopt
// when no font of that name exists afterwards.
std::optional<std::int32_t> resolve_font(const Call& c) {
  const std::string& name = c.args[1];
  int font = c.s.editor->fonts.find(name);
  if (font >= -1) {
    return font;
  }
  double scale = 1.0;
  if (c.n() == 3 && str_is_numeric(c.args[2])) {
    scale = std::atof(c.args[2].c_str());
  }
  auto loaded = cell::load_font_file(c.s.ctx->services().files, system_dirs(c.s), name, scale, c.s.editor->fonts);
  if (!loaded.ok()) {
    warn(c.s, "Error loading font \"" + name + "\"");
  }
  font = c.s.editor->fonts.find(name);
  return font < -1 ? std::nullopt : std::optional<std::int32_t>{font};
}

CommandResult opt_font(const Call& c) {
  std::int32_t font = -1;
  if (c.n() == 2 && str_is_int(c.args[1])) {
    print_font_index(c, c_atoi(c.args[1]));
  } else if (c.n() >= 2 && !str_is_int(c.args[1])) {
    const auto found = resolve_font(c);
    if (!found) {
      return CommandResult::success();
    }
    font = *found;
  }
  if (c.defaults) {
    if (c.query()) {
      return CommandResult::success(Value{font_name(c.s, c.dl().font)});
    }
    c.dl().font = font;
    return CommandResult::success();
  }
  if (c.query()) {
    return query_labels(c, [&](const Label& l) { return Value{font_name(c.s, l.font)}; });
  }
  // cmdLabelFontFunc: a label of size 0 given a font gets the default size.
  const std::int32_t size = default_font_size(c.s);
  return change_labels(c, [&](Label& l) {
    if (!assign(l.font, font)) {
      return false;
    }
    if (font > -1 && l.size == 0) {
      l.size = size;
    }
    return true;
  });
}

CommandResult opt_justify(const Call& c) {
  Position pos = -1;
  if (c.n() == 2) {
    pos = parse_position(c.args[1], false);
    if (pos < 0) {
      warn(c.s, position_error_text(c.args[1], false));
      return CommandResult::success();
    }
  }
  if (c.defaults) {
    if (c.query()) {
      return CommandResult::success(Value{std::string{position_long_name(c.dl().justification)}});
    }
    c.dl().justification = pos;
    return CommandResult::success();
  }
  if (c.query()) {
    return query_labels(c, [](const Label& l) { return Value{std::string{position_long_name(l.justification)}}; });
  }
  return change_labels(c, [&](Label& l) { return assign(l.justification, pos); });
}

CommandResult opt_size(const Call& c) {
  std::int32_t size = 0;
  if (c.n() == 2) {
    if (str_is_numeric(c.args[1])) {
      size = scaled_coord(c.s, c.args[1], 8, true);
    }
    if (size <= 0) {
      return CommandResult::success();
    }
  }
  if (c.defaults) {
    if (c.query()) {
      return CommandResult::success(eighths_value(c.s, c.dl().size));  // Q-MISC-23: ÷8 like the labels
    }
    c.dl().size = size;
    return CommandResult::success();
  }
  if (c.query()) {
    return query_labels(c, [&](const Label& l) { return eighths_value(c.s, l.size); });
  }
  return change_labels(c, [&](Label& l) { return assign(l.size, size); });
}

// The offset of `setlabel offset x y` or `setlabel offset "x y"`; nullopt
// after the usage message (a single word without a space).
std::optional<Point> parse_offset(const Call& c) {
  if (c.n() == 3) {
    return Point{scaled_coord(c.s, c.args[1], 8, true), scaled_coord(c.s, c.args[2], 8, false)};
  }
  if (c.n() != 2) {
    return Point{};
  }
  const std::string& word = c.args[1];
  const std::size_t space = word.find(' ');
  if (space == std::string::npos) {
    warn(c.s, "Usage:  setlabel offset <x> <y>");
    return std::nullopt;
  }
  const std::string_view v{word};
  return Point{scaled_coord(c.s, v.substr(0, space), 8, true), scaled_coord(c.s, v.substr(space + 1), 8, false)};
}

Value offset_value(const Session& s, const Point& p) {
  // Q-MISC-23: x and y (Magic prints x twice), both ÷8.
  return Value{Value::List{eighths_value(s, p.x), eighths_value(s, p.y)}};
}

CommandResult opt_offset(const Call& c) {
  const auto offset = parse_offset(c);
  if (!offset) {
    return CommandResult::success();
  }
  if (c.defaults) {
    if (c.query()) {
      return CommandResult::success(offset_value(c.s, c.dl().offset));
    }
    c.dl().offset = *offset;
    return CommandResult::success();
  }
  if (c.query()) {
    return query_labels(c, [&](const Label& l) { return offset_value(c.s, l.offset); });
  }
  return change_labels(c, [&](Label& l) { return assign(l.offset, *offset); });
}

CommandResult opt_rotate(const Call& c) {
  std::int32_t rotate = 0;
  if (c.n() == 2 && str_is_int(c.args[1])) {
    rotate = c_atoi(c.args[1]);
  }
  if (c.defaults) {
    if (c.query()) {
      return CommandResult::success(Value{std::int64_t{c.dl().rotation}});
    }
    c.dl().rotation = rotate;
    return CommandResult::success();
  }
  if (c.query()) {
    return query_labels(c, [](const Label& l) { return Value{std::int64_t{l.rotation}}; });
  }
  return change_labels(c, [&](Label& l) { return assign(l.rotation, rotate); });
}

CommandResult opt_sticky(const Call& c) {
  bool sticky = false;
  if (c.n() == 2) {
    const int idx = lookup(c.args[1], {"no", "false", "off", "0", "yes", "true", "on", "1"});
    if (idx < 0) {
      warn(c.s, "Unknown sticky option \"" + c.args[1] + "\"");
      return CommandResult::success();
    }
    sticky = idx > 3;
  }
  if (c.defaults) {
    if (c.query()) {
      return CommandResult::success(Value{std::int64_t{c.dl().sticky ? 1 : 0}});
    }
    c.dl().sticky = sticky;
    return CommandResult::success();
  }
  if (c.query()) {
    return query_labels(c, [](const Label& l) { return Value{std::int64_t{l.sticky ? 1 : 0}}; });
  }
  return change_labels(c, [&](Label& l) { return assign(l.sticky, sticky); });
}

// The layer of `setlabel layer <type>`: nullopt for "default" (-1), the
// type otherwise; `ok` false after Magic's DBTechNoisyNameType message.
std::optional<tech::TileType> parse_label_layer(const Call& c, bool& ok) {
  ok = true;
  const std::string& name = c.args[1];
  if (lookup(name, {"default"}) == 0 && name.size() == 7) {  // strcasecmp
    return std::nullopt;
  }
  const auto found = c.s.tech->find_type(name);
  if (!found.ok()) {
    const char* why = found.error == tech::LookupError::ambiguous ? "Ambiguous" : "Unrecognized";
    warn(c.s, std::string{why} + " layer (type) name \"" + name + "\"");
    ok = false;
    return std::nullopt;
  }
  return *found.id;
}

CommandResult opt_layer(const Call& c) {
  if (c.n() == 3) {
    return CommandResult::success();  // Magic applies an uninitialized type here
  }
  std::optional<tech::TileType> type;
  if (c.n() == 2) {
    bool ok = true;
    type = parse_label_layer(c, ok);
    if (!ok) {
      return CommandResult::success();
    }
  }
  if (c.defaults) {
    if (c.query()) {
      const auto& t = c.dl().type;
      return CommandResult::success(Value{t ? std::string{c.s.tech->type_name(*t)} : std::string{"default"}});
    }
    c.dl().type = type;
    return CommandResult::success();
  }
  if (c.query()) {
    return query_labels(c, [&](const Label& l) { return Value{std::string{c.s.tech->type_name(l.type)}}; });
  }
  // Magic stores type -1 for "default"; a label here becomes unattached (space).
  const tech::TileType target = type ? label_layer_for(*c.s.tech, *type) : tech::kSpace;
  return change_labels(c, [&](Label& l) { return assign(l.type, target); });
}

// The rectangle of `setlabel box`: four words, or one word holding two
// (a point) or four values; nullopt after "Label box requires four
// coordinates." Coordinates are relative distances (Q-CMDEDIT-91).
std::optional<Rect> parse_box(const Call& c) {
  std::vector<std::string_view> words;
  if (c.n() == 5) {
    words.assign(c.args.begin() + 1, c.args.end());
  } else {
    const std::string_view text{c.args[1]};
    std::size_t i = 0;
    while (i < text.size() && words.size() < 4) {
      const std::size_t start = text.find_first_not_of(" \t", i);
      if (start == std::string_view::npos) {
        break;
      }
      const std::size_t end = std::min(text.find(' ', start), text.size());
      words.push_back(text.substr(start, end - start));
      i = end;
    }
  }
  if (words.size() == 2) {
    words.push_back(words[0]);
    words.push_back(words[1]);
  }
  if (words.size() != 4) {
    warn(c.s, "Label box requires four coordinates.");
    return std::nullopt;
  }
  return Rect{scaled_coord(c.s, words[0], 1, true), scaled_coord(c.s, words[1], 1, false),
              scaled_coord(c.s, words[2], 1, true), scaled_coord(c.s, words[3], 1, false)};
}

CommandResult opt_box(const Call& c) {
  if (c.defaults) {
    warn(c.s, "Cannot set a default label box.");
    return CommandResult::success();
  }
  if (c.n() != 2 && c.n() != 5) {
    return query_labels(c, [&](const Label& l) {
      return Value{Value::List{coord_value(c.s, l.rect.x0), coord_value(c.s, l.rect.y0), coord_value(c.s, l.rect.x1),
                               coord_value(c.s, l.rect.y1)}};
    });
  }
  const auto rect = parse_box(c);
  if (!rect) {
    return CommandResult::success();
  }
  return change_labels(c, [&](Label& l) { return assign(l.rect, *rect); });
}

using OptionHandler = CommandResult (*)(const Call&);

constexpr std::array<OptionHandler, 11> kHandlers = {opt_box,    opt_font,   opt_fontlist, opt_justify,
                                                     opt_layer,  opt_offset, opt_rotate,   opt_size,
                                                     opt_sticky, opt_text,   opt_help};

// The option CmdSetLabel runs: help for too many values, -1 for an unknown
// or ambiguous option word.
int choose_option(const Call& c) {
  if (c.args.empty()) {
    return kHelp;
  }
  const int option = lookup_in(c.args[0], kOptionNames);
  const std::size_t limit = option == kBox ? 5 : 3;
  return c.n() > limit ? kHelp : option;
}

}  // namespace

CommandResult setlabel_command(CommandContext& ctx, const Invocation& inv) {
  auto s = session(ctx);
  if (!s.ok()) {
    return fail(s.error());
  }
  Call c{s.value(), false, inv.args, s.value().editor->edit_cell, s.value().editor->box};
  if (c.args.size() >= 2 && c.args[0].starts_with("-def")) {
    if (!s.value().editor->label_defaults) {
      s.value().editor->label_defaults = LabelDefaults{};
    }
    c.defaults = true;
    c.args.erase(c.args.begin());
  }
  const int option = choose_option(c);
  if (option < 0) {
    warn(c.s, "Unknown setlabel option \"" + c.args[0] + "\"");
    return CommandResult::success();
  }
  return kHandlers[static_cast<std::size_t>(option)](c);
}

}  // namespace hefesto::db::cmdutil
