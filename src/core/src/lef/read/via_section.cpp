// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF VIA and VIARULE sections (capability lef-read "LEF VIA and
// VIARULE definitions", "Via redefinition"): LefRead's LEF_SECTION_VIA /
// VIARULE case (lefRead.c:3055-3090), a new VIA inside a NONDEFAULTRULE
// (lefRead.c:1939-1959), and the via statements of LefReadLayerSection
// (LefAddViaGeometry, lefRead.c:2646-2690, 2895-2915).
//
// - The definition is started by LEFVIA's begin_via (Q-LEFDEF-8 fixed) and
//   each rectangle added by add_via_rect (cut growing, Q-LEFDEF-17).
// - Q-LEFDEF-7 (compat): magic-compat ON, POLYGON and the VIARULE
//   parameters are unknown keywords and RECT MASK / RECT ITERATE fail as a
//   bad RECT; OFF, RECT MASK n is a RECT, RECT ITERATE adds the rectangle
//   at each step, POLYGON adds its decomposition's rectangles, and a VIA
//   that names a VIARULE gets the generated geometry (ViaGenerator).
// - Q-LEFDEF-20 (compat): a RECT on an IGNORE layer reports "No layer
//   defined for RECT." with magic-compat ON, nothing with it OFF.
//
// Owner: LEFR (design D2).

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/lef/read/geometry.hpp"
#include "hefesto/lef/read/units.hpp"
#include "hefesto/lef/via/polygon.hpp"
#include "hefesto/lef/via/store.hpp"
#include "lef/read/via_body.hpp"

namespace hefesto::lef::detail {

namespace {

constexpr std::string_view kErrorCode = "LEF-READ-001";

/// LEF microns become integer generator units of 1/10000 micron.
constexpr float kUnitsPerMicron = 10000.0F;

enum class Param : std::uint8_t {
  viarule,
  cutsize,
  layers,
  cutspacing,
  enclosure,
  rowcol,
  origin,
  offset,
  pattern,
};

constexpr std::array<std::string_view, 9> kParams{"VIARULE", "CUTSIZE", "LAYERS",  "CUTSPACING", "ENCLOSURE",
                                                   "ROWCOL",  "ORIGIN",  "OFFSET", "PATTERN"};

const LayerTable& empty_table() {
  static const LayerTable table;
  return table;
}

}  // namespace

// VIA <name> / VIARULE <name>: a new name gets a new definition; a known
// one is redefined (VIA) or the section is skipped (VIARULE).
void Reader::via_section(const std::string& name, bool viarule) {
  const std::string end_name = truncate_name(name);
  const LayerTable& table = target_.layer_table != nullptr ? *target_.layer_table : empty_table();
  ViaDefinition* via =
      begin_via(target_.vias, table, name, viarule ? ViaSection::lef_viarule : ViaSection::lef_via, via_context());
  if (via == nullptr) {
    skip_section(end_name);
    return;
  }
  ViaBody body{*this, *via, name, !viarule};
  layer_body(name, end_name, LayerEntry{via->layer_class, 0, 0, 0, std::nullopt}, &body);
}

// VIA <name> inside a NONDEFAULTRULE, for a name with no entry.
void Reader::via_definition(const std::string& name) {
  ViaDefinition* via = begin_via(target_.vias, empty_table(), name, ViaSection::lef_via, via_context());
  if (via == nullptr) {
    skip_section(name);
    return;
  }
  ViaBody body{*this, *via, name, true};
  layer_body(name, name, LayerEntry{LayerClass::cut, 0, 0, 0, std::nullopt}, &body);
}

// --- ViaBody ----------------------------------------------------------------------------

void ViaBody::add(const LayerState& layer, const db::Rect& rect) {
  add_via_rect(via_, layer.type, rect, reader_.via_context());
}

void ViaBody::rect(const LayerState& layer, bool ignored) {
  const bool extended = !reader_.magic_compat();
  std::optional<Token> first = reader_.token();
  bool repeated = false;
  if (extended && first && keyword_is(first->text, "MASK")) {
    (void)reader_.token();  // the mask number is ignored
    first = reader_.token();
  }
  if (extended && first && keyword_is(first->text, "ITERATE")) {
    repeated = true;
    first = reader_.token();
  }
  const bool known = layer.type.has_value() || (ignored && extended);
  const auto read = read_rect(reader_.lexer(), reader_.messages(), known, reader_.options().scale / 2.0F,
                              std::move(first));
  if (read && layer.type) {
    if (repeated) {
      iterate(layer, *read);
    } else {
      add(layer, *read);
    }
  }
  reader_.skip_statement();
}

// RECT ITERATE ... DO nx BY ny STEP dx dy: the rectangle at each (i dx, j dy).
void ViaBody::iterate(const LayerState& layer, const db::Rect& rect) {
  const auto word = [this](std::string_view expected) {
    const auto token = reader_.token();
    return token && keyword_is(token->text, expected);
  };
  const auto count = [this]() -> std::optional<std::int64_t> {
    const auto token = reader_.token();
    const auto value = token ? parse_number(token->text) : std::nullopt;
    if (!value || !(*value >= 1.0F && *value <= static_cast<float>(kMaxViaRepeat))) {
      return std::nullopt;
    }
    return static_cast<std::int64_t>(*value);
  };
  std::optional<std::int64_t> nx;
  std::optional<std::int64_t> ny;
  std::optional<db::Coord> dx;
  std::optional<db::Coord> dy;
  if (word("DO") && (nx = count()) && word("BY") && (ny = count()) && word("STEP")) {
    dx = reader_.read_value(true);
    dy = reader_.read_value(true);
  }
  if (!nx || !ny || !dx || !dy || *nx * *ny > kMaxViaRepeat) {
    reader_.messages().report(Dialect::lef, diag::Severity::error, kErrorCode,
                              "Bad RECT ITERATE: requires DO <nx> BY <ny> STEP <dx> <dy>.");
    return;
  }
  for (std::int64_t i = 0; i < *nx; ++i) {
    for (std::int64_t j = 0; j < *ny; ++j) {
      const std::int64_t ox = i * *dx;
      const std::int64_t oy = j * *dy;
      const auto in_range = [](std::int64_t v) {
        return v >= std::numeric_limits<db::Coord>::min() && v <= std::numeric_limits<db::Coord>::max();
      };
      if (!in_range(rect.x0 + ox) || !in_range(rect.x1 + ox) || !in_range(rect.y0 + oy) || !in_range(rect.y1 + oy)) {
        continue;
      }
      add(layer, db::Rect{static_cast<db::Coord>(rect.x0 + ox), static_cast<db::Coord>(rect.y0 + oy),
                          static_cast<db::Coord>(rect.x1 + ox), static_cast<db::Coord>(rect.y1 + oy)});
    }
  }
}

// POLYGON: its decomposition's rectangles, each added as a RECT would be.
void ViaBody::polygon(const LayerState& layer) {
  const auto points = read_points(reader_.lexer(), reader_.messages(), reader_.options().scale / 2.0F, db::Point{});
  if (!layer.type || points.size() < 3) {
    return;
  }
  for (const db::Rect& rect : polygon_rects(points)) {
    add(layer, rect);
  }
}

bool ViaBody::extension(std::string_view keyword, const LayerState& layer) {
  if (reader_.magic_compat()) {
    return false;
  }
  if (keyword_is(keyword, "POLYGON")) {
    polygon(layer);
    return true;
  }
  return parameters_ && parameter(keyword);
}

namespace {

/// A LEF distance in generator units (1/10000 micron); nullopt when the
/// text is not a number or the value does not fit.
std::optional<std::int32_t> generator_units(const std::string& text) {
  const auto value = parse_number(text);
  if (!value) {
    return std::nullopt;
  }
  const float scaled = std::roundf(*value * kUnitsPerMicron);
  if (!(scaled > -2147483648.0F && scaled < 2147483648.0F)) {
    return std::nullopt;
  }
  return static_cast<std::int32_t>(scaled);
}

/// The point of values `args[first]`, `args[first + 1]`.
std::optional<db::Point> point_at(const std::vector<std::string>& args, std::size_t first) {
  if (args.size() < first + 2) {
    return std::nullopt;
  }
  const auto x = generator_units(args[first]);
  const auto y = generator_units(args[first + 1]);
  if (!x || !y) {
    return std::nullopt;
  }
  return db::Point{*x, *y};
}

/// ROWCOL r c: both counts at least 1 and their product within the cap.
std::optional<std::pair<std::int32_t, std::int32_t>> row_columns(const std::vector<std::string>& args) {
  const auto r = args.size() >= 2 ? parse_number(args[0]) : std::nullopt;
  const auto c = args.size() >= 2 ? parse_number(args[1]) : std::nullopt;
  if (!r || !c || !(*r >= 1.0F && *c >= 1.0F) || *r * *c > static_cast<float>(kMaxViaRepeat)) {
    return std::nullopt;
  }
  return std::pair{static_cast<std::int32_t>(*r), static_cast<std::int32_t>(*c)};
}

}  // namespace

// The tokens of the current statement up to its `;` (consumed).
std::vector<std::string> ViaBody::arguments() {
  std::vector<std::string> args;
  while (const auto token = reader_.token()) {
    if (token->text.starts_with(';')) {
      break;
    }
    args.push_back(token->text);
  }
  return args;
}

void ViaBody::layers_statement(const std::vector<std::string>& args) {
  ViaLayers layers;
  const std::array<std::optional<tech::TileType>*, 3> slots{&layers.bottom, &layers.cut, &layers.top};
  for (std::size_t i = 0; i < slots.size() && i < args.size(); ++i) {
    LayerState state;
    resolve_layer(args[i], LayerLookup::normal, state, reader_.layer_context());
    *slots[i] = state.type;
  }
  order_layers(layers, reader_.via_context());
  params_.layers = layers;
}

void ViaBody::rowcol_statement(const std::vector<std::string>& args) {
  if (const auto counts = row_columns(args)) {
    params_.rows = counts->first;
    params_.columns = counts->second;
    return;
  }
  reader_.messages().report(Dialect::lef, diag::Severity::error, kErrorCode,
                            "Bad ROWCOL: requires two counts whose product is at most 65536.");
}

// The VIARULE-parameter statements of a VIA (magic-compat OFF).
bool ViaBody::parameter(std::string_view keyword) {
  const auto index = lookup_full(keyword, kParams);
  if (!index) {
    return false;
  }
  const std::vector<std::string> args = arguments();
  switch (static_cast<Param>(*index)) {
    case Param::viarule:
      has_rule_ = true;  // a LEF VIARULE keeps no values: omitted ones are 0 (Q-LEFDEF-61)
      break;
    case Param::cutsize:
      params_.cut_size = point_at(args, 0);
      break;
    case Param::layers:
      layers_statement(args);
      break;
    case Param::cutspacing:
      params_.cut_spacing = point_at(args, 0);
      break;
    case Param::enclosure:
      params_.bottom_enclosure = point_at(args, 0);
      params_.top_enclosure = point_at(args, 2);
      break;
    case Param::rowcol:
      rowcol_statement(args);
      break;
    case Param::origin:
      params_.origin = point_at(args, 0).value_or(db::Point{});
      break;
    case Param::offset:
      params_.bottom_offset = point_at(args, 0).value_or(db::Point{});
      params_.top_offset = point_at(args, 2).value_or(db::Point{});
      break;
    case Param::pattern:
      params_.pattern = args.empty() ? std::string{} : args.front();
      break;
  }
  return true;
}

void ViaBody::finish() {
  if (!has_rule_ || reader_.magic_compat()) {
    return;
  }
  ViaContext context = reader_.via_context();
  params_.scale = reader_.options().scale * kUnitsPerMicron;
  const GeneratedViaParams params = complete_params(params_, nullptr, name_, context);
  apply_generated(via_, ViaGenerator::generated(params, context));
}

}  // namespace hefesto::lef::detail
