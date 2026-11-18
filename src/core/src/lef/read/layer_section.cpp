// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF LAYER section (capability lef-read "LEF LAYER section"):
// LefRead's LEF_SECTION_LAYER case (lefRead.c:3093-3148) and
// LefReadLayerSection (lefRead.c:2722-2944) for a LAYER.
//
// - Q-LEFDEF-4 (fix): a name without an entry is treated as absent.
// - Q-LEFDEF-5 (fix): a new ROUTE entry starts like a tech `routing` line
//   (LayerAccess::add_route).
// - Q-LEFDEF-6 (compat): magic-compat ON, every SPACING overwrites (the
//   last wins); OFF, the first unqualified SPACING, else the smallest.
// - A value that is not a number leaves the field unchanged (Magic stores
//   an uninitialised float; X-2 tier 1).
// - A RECT is read for its messages and dropped: Magic adds it to the
//   entry's via geometry through the union shared with the route values
//   (LefAddViaGeometry). The VIA-class case is LEFVIA's via geometry
//   (tasks.md 4.1); for a ROUTE entry it overwrites the route values.
//
// Owner: LEFR (design D2).

#include <array>

#include "hefesto/lef/read/geometry.hpp"
#include "hefesto/tech/technology.hpp"
#include "lef/read/reader.hpp"

namespace hefesto::lef::detail {

namespace {

// LefReadLayerSection's layer_keys table, in its order.
enum class Key : std::uint8_t {
  type,
  width,
  maxwidth,
  area,
  spacing,
  spacingtable,
  pitch,
  direction,
  offset,
  wireextension,
  resistance,
  capacitance,
  edgecapacitance,
  thickness,
  height,
  property,
  accurrentdensity,
  dccurrentdensity,
  minimumdensity,
  antennaarearatio,
  antennadiffarearatio,
  antennasidearearatio,
  default_,
  layer,
  rect,
  foreign,
  enclosure,
  preferenclosure,
  overhang,
  metaloverhang,
  via,
  generate,
  end,
};

constexpr std::array<std::string_view, 33> kKeys{
    "TYPE",           "WIDTH",          "MAXWIDTH",         "AREA",
    "SPACING",        "SPACINGTABLE",   "PITCH",            "DIRECTION",
    "OFFSET",         "WIREEXTENSION",  "RESISTANCE",       "CAPACITANCE",
    "EDGECAPACITANCE", "THICKNESS",     "HEIGHT",           "PROPERTY",
    "ACCURRENTDENSITY", "DCCURRENTDENSITY", "MINIMUMDENSITY", "ANTENNAAREARATIO",
    "ANTENNADIFFAREARATIO", "ANTENNASIDEAREARATIO", "DEFAULT", "LAYER",
    "RECT",           "FOREIGN",        "ENCLOSURE",        "PREFERENCLOSURE",
    "OVERHANG",       "METALOVERHANG",  "VIA",              "GENERATE",
    "END",
};

// layer_type_keys, in the order of Magic's CLASS_ROUTE, CLASS_VIA, ...
constexpr std::array<std::string_view, 4> kTypeKeys{"ROUTING", "CUT", "MASTERSLICE", "OVERLAP"};

// The name Magic prints for a class (layer_type_keys[lefClass]). Magic
// indexes past the table for BOUND and IGNORE (printing "(null)"); Hefesto
// names them.
std::string_view class_name(LayerClass layer_class) noexcept {
  switch (layer_class) {
    case LayerClass::route:
      return "ROUTING";
    case LayerClass::cut:
      return "CUT";
    case LayerClass::masterslice:
      return "MASTERSLICE";
    case LayerClass::overlap:
      return "OVERLAP";
    case LayerClass::bound:
      return "BOUND";
    case LayerClass::obstruction:
      return "OBSTRUCTION";
    default:
      return "IGNORE";
  }
}

// Statements read to their `;` and otherwise ignored.
bool ignored_key(Key key) noexcept {
  switch (key) {
    case Key::maxwidth:
    case Key::area:
    case Key::spacingtable:
    case Key::property:
    case Key::offset:
    case Key::wireextension:
    case Key::resistance:
    case Key::capacitance:
    case Key::edgecapacitance:
    case Key::thickness:
    case Key::height:
    case Key::minimumdensity:
    case Key::antennaarearatio:
    case Key::antennadiffarearatio:
    case Key::antennasidearearatio:
    case Key::foreign:
    case Key::via:
    case Key::enclosure:
    case Key::preferenclosure:
    case Key::overhang:
    case Key::metaloverhang:
      return true;
    default:
      return false;
  }
}

/// The body of one LAYER section.
class LayerBody {
 public:
  LayerBody(Reader& reader, std::string_view key, std::string end_name, LayerEntry entry)
      : reader_(reader), key_(key), end_name_(std::move(end_name)), entry_(std::move(entry)) {}

  void run() {
    while (const auto keyword = reader_.token()) {
      if (!statement(*keyword)) {
        break;
      }
    }
    finish();
  }

 private:
  [[nodiscard]] bool route() const noexcept { return entry_.layer_class == LayerClass::route; }

  bool statement(const Token& keyword);
  bool keyed_statement(Key key);
  void type_statement();
  void route_value(std::optional<db::Coord> RouteUpdate::*field);
  void spacing_statement();
  void direction_statement();
  void current_density(bool alternating);
  void layer_statement();
  void rect_statement();
  void finish();

  Reader& reader_;
  std::string_view key_;  // the table name (the full token)
  std::string end_name_;  // the name END must match (127 characters)
  LayerEntry entry_;
  RouteUpdate update_;
  LayerState layer_;  // the current layer of LAYER statements (curlayer)
  // Q-LEFDEF-6 OFF: the first unqualified SPACING and the smallest one.
  std::optional<db::Coord> first_plain_spacing_;
  std::optional<db::Coord> smallest_spacing_;
};

// One statement; false at the section's END.
bool LayerBody::statement(const Token& keyword) {
  const auto index = lookup_full(keyword.text, kKeys);
  if (!index) {
    reader_.info("Unknown keyword \"" + keyword.text + "\" in LEF file; ignoring.");
    reader_.skip_statement();
    return true;
  }
  const auto key = static_cast<Key>(*index);
  if (ignored_key(key)) {
    reader_.skip_statement();
    return true;
  }
  if (key == Key::end) {
    if (reader_.parse_end(end_name_) != EndMatch::not_matched) {
      return false;
    }
    reader_.error("Layer END statement missing.");
    return true;
  }
  return keyed_statement(key);
}

bool LayerBody::keyed_statement(Key key) {
  switch (key) {
    case Key::type:
      type_statement();
      break;
    case Key::width:
      route_value(&RouteUpdate::width);
      break;
    case Key::pitch:
      route_value(&RouteUpdate::pitch);
      break;
    case Key::spacing:
      spacing_statement();
      break;
    case Key::direction:
      direction_statement();
      break;
    case Key::accurrentdensity:
    case Key::dccurrentdensity:
      current_density(key == Key::accurrentdensity);
      break;
    case Key::layer:
      layer_statement();
      break;
    case Key::rect:
      rect_statement();
      break;
    default:
      break;  // DEFAULT, GENERATE: nothing, not even the `;`
  }
  return true;
}

// TYPE t: ROUTING and CUT must agree with the entry's class; the class
// never changes.
void LayerBody::type_statement() {
  const auto type = reader_.token();
  if (!type) {
    return;
  }
  const auto key = lookup_full(type->text, kTypeKeys);
  if (!key) {
    reader_.warning("Unknown layer type \"" + type->text + "\" in LEF file; ignoring.");
  } else if ((*key == 0U && entry_.layer_class != LayerClass::route) ||
             (*key == 1U && entry_.layer_class != LayerClass::cut)) {
    reader_.error("Attempt to reclassify layer " + end_name_ + " from " + std::string{class_name(entry_.layer_class)} +
                  " to " + std::string{kTypeKeys[*key]});
  }
  reader_.skip_statement();
}

// WIDTH v / PITCH v: ROUTE entries only, from the first value.
void LayerBody::route_value(std::optional<db::Coord> RouteUpdate::*field) {
  const auto value = reader_.read_value();
  if (route() && value) {
    update_.*field = value;
  }
  reader_.skip_statement();
}

void LayerBody::spacing_statement() {
  const auto value = reader_.read_value();
  const bool plain = reader_.skip_statement() == 0;
  if (!route() || !value) {
    return;
  }
  if (reader_.magic_compat()) {
    update_.spacing = value;  // Q-LEFDEF-6 ON: the last one wins
    return;
  }
  if (plain && !first_plain_spacing_) {
    first_plain_spacing_ = value;
  }
  if (!smallest_spacing_ || *value < *smallest_spacing_) {
    smallest_spacing_ = value;
  }
}

void LayerBody::direction_statement() {
  const auto direction = reader_.token();
  if (direction && route()) {
    const char first = direction->text.empty() ? '\0' : direction->text.front();
    update_.direction = (first == 'h' || first == 'H') ? RouteDirection::horizontal : RouteDirection::vertical;
  }
  reader_.skip_statement();
}

// ACCURRENTDENSITY / DCCURRENTDENSITY: two tokens, then the extra
// statements of a FREQUENCY or WIDTH table (exact-case compares, as Magic's
// strcmp), then the statement end.
void LayerBody::current_density(bool alternating) {
  (void)reader_.token();  // the value type
  const auto second = reader_.token();
  if (alternating && second && second->text == "FREQUENCY") {
    reader_.skip_statement();
    const auto third = reader_.token();
    if (third && third->text == "WIDTH") {
      reader_.skip_statement();
    }
  } else if (!alternating && second && second->text == "WIDTH") {
    reader_.skip_statement();
  }
  reader_.skip_statement();
}

// LAYER n: the layer of the RECTs that follow (LefReadLayer, normal lookup).
void LayerBody::layer_statement() {
  if (const auto name = reader_.token()) {
    resolve_layer(name->text, LayerLookup::normal, layer_, reader_.layer_context());
  }
  reader_.skip_statement();
}

// RECT: read in half units for its messages, then dropped (see the file
// comment).
void LayerBody::rect_statement() {
  (void)read_rect(reader_.lexer(), reader_.messages(), layer_.type.has_value(), reader_.options().scale / 2.0F);
  reader_.skip_statement();
}

void LayerBody::finish() {
  if (!reader_.magic_compat()) {
    const auto spacing = first_plain_spacing_ ? first_plain_spacing_ : smallest_spacing_;
    if (spacing) {
      update_.spacing = spacing;
    }
  }
  if (route() && !update_.empty()) {
    reader_.target().layers.update_route(key_, update_);
  }
}

}  // namespace

// LAYER <name>: an entry of the table, or a new ROUTE entry for a magic
// type that is not a contact; anything else skips the section.
void Reader::layer_section() {
  const auto name = token();
  if (!name) {
    return;
  }
  const std::string end_name = truncate_name(name->text);
  auto entry = target_.layers.find(name->text);
  if (!entry) {
    const auto type = magic_type(options_.tech, name->text);
    if (!type) {
      skip_section(end_name);  // probably a masterslice or overlap layer
      return;
    }
    if (options_.tech.is_contact(*type)) {
      error("Layer " + name->text + " maps to a magic contact layer; must be defined in lef section of techfile");
      skip_section(end_name);
      return;
    }
    target_.layers.add_route(name->text, *type);
    entry = target_.layers.find(name->text);
  } else if (entry->type == 0) {
    error("Layer " + name->text + " is only defined for obstructions!");
    skip_section(end_name);
    return;
  }
  LayerBody body{*this, name->text, end_name, entry.value_or(LayerEntry{LayerClass::route, 0, 0, 0, std::nullopt})};
  body.run();
}

}  // namespace hefesto::lef::detail
