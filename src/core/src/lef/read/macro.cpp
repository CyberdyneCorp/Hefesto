// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MACRO and SITE sections (capability lef-read "MACRO and SITE cell
// creation", "Macro coordinate offset", "Macro statements and cell
// properties", "Macro completion"): LefReadMacro (lefRead.c:2066-2448).
//
// - Q-LEFDEF-11 (compat): magic-compat ON, `-annotate` still creates cells
//   for macros that are not duplicates; OFF, a macro whose cell does not
//   exist is skipped.
// - Q-LEFDEF-12 (compat): magic-compat ON, ORIGIN adds to the coordinate
//   offset and the box is the bounding box minus the offset (geometry at
//   g - ORIGIN - F, box (F, F + SIZE)); OFF, the offset is -F only and the
//   box is (-ORIGIN - F, SIZE - ORIGIN - F), LEF's frame.
// - Q-LEFDEF-19 (fix): an empty CLASS or SYMMETRY sets no property.
// - Hefesto-only, magic-compat OFF (no Q-row; reported to the user): a
//   SITE, SOURCE or PROPERTY statement missing its value sets nothing and
//   does not swallow the next statement, and a duplicate macro skipped by
//   `-annotate` is skipped to its own END (Magic skips to `END MACRO`).
// - A statement at the end of the input ends the macro (Magic dereferences
//   a missing token), and a size or box that does not fit an internal
//   coordinate saturates.
//
// Owner: LEFR (design D2).

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/lef/read/geometry.hpp"
#include "hefesto/lef/read/units.hpp"
#include "lef/read/macro_body.hpp"

namespace hefesto::lef::detail {

namespace {

enum class Key : std::uint8_t { class_, size, origin, symmetry, source, site, pin, obs, timing, foreign, property, end };

constexpr std::array<std::string_view, 12> kKeys{"CLASS", "SIZE", "ORIGIN", "SYMMETRY", "SOURCE",   "SITE",
                                                 "PIN",   "OBS",  "TIMING", "FOREIGN",  "PROPERTY", "END"};

db::Coord saturated(std::int64_t v) noexcept {
  return static_cast<db::Coord>(
      std::clamp<std::int64_t>(v, std::numeric_limits<db::Coord>::min(), std::numeric_limits<db::Coord>::max()));
}

void put_string(db::CellDef& cell, std::string_view name, std::string value) {
  db::set_property(cell, name, db::PropertyValue{std::move(value)});
}

/// The body of one MACRO or SITE section, read into `macro.cell`.
class MacroBody {
 public:
  MacroBody(Reader& reader, MacroState& macro, std::string name)
      : reader_(reader), macro_(macro), name_(std::move(name)) {}

  void run() {
    while (const auto keyword = reader_.token()) {
      if (!statement(*keyword)) {
        break;
      }
    }
    finish();
  }

 private:
  [[nodiscard]] float scale() const { return reader_.options().scale; }

  bool statement(const Token& keyword);
  void keyed_statement(Key key);
  void joined_property(std::string_view name);
  void size_statement();
  void origin_statement();
  void foreign_statement();
  void site_statement();
  void source_statement();
  void property_statement();
  void pin_statement();
  /// The next token, nullopt at the end of the input or (magic-compat OFF,
  /// `stop_at_end`) at a `;`, which is then consumed.
  std::optional<Token> value_token();
  [[nodiscard]] db::Rect macro_box() const;
  void finish();

  Reader& reader_;
  MacroState& macro_;
  std::string name_;  // the name END matches
  bool has_size_ = false;
  db::Point lower_;  // lefBBox lower-left
  db::Point upper_;  // lefBBox upper-right (set by SIZE)
  db::Point foreign_;  // the FOREIGN part of the offset (Q-LEFDEF-12 OFF)
  std::int64_t pin_number_ = 1;
};

bool MacroBody::statement(const Token& keyword) {
  const auto index = lookup_full(keyword.text, kKeys);
  if (!index) {
    reader_.info("Unknown keyword \"" + keyword.text + "\" in LEF file; ignoring.");
    reader_.skip_statement();
    return true;
  }
  const auto key = static_cast<Key>(*index);
  if (key != Key::end) {
    keyed_statement(key);
    return true;
  }
  if (reader_.parse_end(name_) != EndMatch::not_matched) {
    return false;
  }
  reader_.error("Macro END statement missing.");
  return true;
}

void MacroBody::keyed_statement(Key key) {
  switch (key) {
    case Key::class_:
      joined_property("LEFclass");
      break;
    case Key::symmetry:
      joined_property("LEFsymmetry");
      break;
    case Key::size:
      size_statement();
      break;
    case Key::origin:
      origin_statement();
      break;
    case Key::foreign:
      foreign_statement();
      break;
    case Key::site:
      site_statement();
      break;
    case Key::source:
      source_statement();
      break;
    case Key::property:
      property_statement();
      break;
    case Key::pin:
      pin_statement();
      break;
    case Key::obs:
      if (macro_.annotating) {
        reader_.skip_section(std::nullopt);
      } else {
        (void)read_geometry(reader_, macro_, false);
      }
      break;
    default:
      reader_.skip_section("TIMING");
      break;
  }
}

// CLASS / SYMMETRY: the tokens up to `;` joined by single spaces.
void MacroBody::joined_property(std::string_view name) {
  std::string joined;
  while (const auto token = reader_.token()) {
    if (token->text.starts_with(';')) {
      break;
    }
    joined += joined.empty() ? token->text : " " + token->text;
  }
  if (!joined.empty()) {  // Q-LEFDEF-19 fix
    put_string(macro_.cell, name, std::move(joined));
  }
}

void MacroBody::size_statement() {
  const auto x_token = reader_.token();
  const auto x = x_token ? parse_number(x_token->text) : std::nullopt;
  const auto by = x ? reader_.token() : std::nullopt;  // `BY` is not checked
  const auto y_token = by ? reader_.token() : std::nullopt;
  const auto y = y_token ? parse_number(y_token->text) : std::nullopt;
  const auto w = x ? to_internal(*x, scale()) : std::nullopt;
  const auto h = y ? to_internal(*y, scale()) : std::nullopt;
  if (!w || !h) {
    reader_.error("Bad macro SIZE; requires values X BY Y.");
  } else {
    upper_ = db::Point{saturated(std::int64_t{*w} + lower_.x), saturated(std::int64_t{*h} + lower_.y)};
    has_size_ = true;
  }
  reader_.skip_statement();
}

void MacroBody::origin_statement() {
  float x = 0.0F;
  float y = 0.0F;
  const auto ox = read_point(reader_.lexer(), x, y) == PointRead::point ? to_internal(x, scale()) : std::nullopt;
  const auto oy = ox ? to_internal(y, scale()) : std::nullopt;
  if (!ox || !oy) {
    reader_.error("Bad macro ORIGIN; requires 2 values.");
    reader_.skip_statement();
    return;
  }
  lower_ = db::Point{saturated(-std::int64_t{*ox}), saturated(-std::int64_t{*oy})};
  if (has_size_) {
    upper_ = db::Point{saturated(std::int64_t{upper_.x} + lower_.x), saturated(std::int64_t{upper_.y} + lower_.y)};
  }
  if (reader_.magic_compat()) {
    // Q-LEFDEF-12 ON: ORIGIN shifts the geometry that follows.
    macro_.offset = db::Point{saturated(std::int64_t{macro_.offset.x} + lower_.x),
                              saturated(std::int64_t{macro_.offset.y} + lower_.y)};
  }
  reader_.skip_statement();
}

void MacroBody::foreign_statement() {
  if (!reader_.token()) {
    return;  // the cell name is not used
  }
  float x = 0.0F;
  float y = 0.0F;
  const PointRead read = read_point(reader_.lexer(), x, y);
  const auto fx = read == PointRead::point ? to_internal(x, scale()) : std::nullopt;
  const auto fy = fx ? to_internal(y, scale()) : std::nullopt;
  if (read == PointRead::none) {
    return;
  }
  if (!fx || !fy) {
    reader_.error("Bad origin in macro FOREIGN; requires 0 or 2 values.");
  } else {
    macro_.offset = db::Point{saturated(std::int64_t{macro_.offset.x} - *fx),
                              saturated(std::int64_t{macro_.offset.y} - *fy)};
    foreign_ = db::Point{saturated(std::int64_t{foreign_.x} - *fx), saturated(std::int64_t{foreign_.y} - *fy)};
  }
  reader_.skip_statement();
}

std::optional<Token> MacroBody::value_token() {
  auto token = reader_.token();
  if (token && token->text.starts_with(';') && !reader_.magic_compat()) {
    return std::nullopt;  // the statement ended: nothing to skip
  }
  return token;
}

// SITE s: LEFsite is the first token.
void MacroBody::site_statement() {
  const auto site = value_token();
  if (!site) {
    return;
  }
  put_string(macro_.cell, "LEFsite", site->text);
  reader_.skip_statement();
}

// SOURCE s: ignored (deprecated since LEF 5.6).
void MacroBody::source_statement() {
  if (value_token()) {
    reader_.skip_statement();
  }
}

// PROPERTY key value: "key value" appended to LEFproperties.
void MacroBody::property_statement() {
  const auto key = value_token();
  const auto value = key ? value_token() : std::nullopt;
  if (!value) {
    return;
  }
  std::string text = truncate_name(key->text) + " " + value->text;
  if (const auto* old = macro_.cell.properties().find("LEFproperties")) {
    if (const auto* previous = std::get_if<std::string>(old)) {
      text = *previous + " " + text;
    }
  }
  put_string(macro_.cell, "LEFproperties", std::move(text));
  reader_.skip_statement();
}

void MacroBody::pin_statement() {
  const auto name = reader_.token();
  if (!name) {
    return;
  }
  read_pin(reader_, macro_, truncate_name(name->text), pin_number_++);
}

// The macro box: the bounding box less the offset (Q-LEFDEF-12 ON), or
// shifted by the FOREIGN point (OFF).
db::Rect MacroBody::macro_box() const {
  const db::Point shift = reader_.magic_compat() ? db::Point{saturated(-std::int64_t{macro_.offset.x}),
                                                              saturated(-std::int64_t{macro_.offset.y})}
                                                 : foreign_;
  return db::Rect{saturated(std::int64_t{lower_.x} + shift.x), saturated(std::int64_t{lower_.y} + shift.y),
                  saturated(std::int64_t{upper_.x} + shift.x), saturated(std::int64_t{upper_.y} + shift.y)};
}

void MacroBody::finish() {
  db::CellDef& cell = macro_.cell;
  db::recompute_bbox(cell, reader_.magic_compat());
  std::optional<db::Rect> drc_area;
  std::optional<db::Rect> fixed;
  if (has_size_) {
    fixed = macro_box();
  } else if (!macro_.annotating) {
    reader_.warning("   Macro does not define size:  computing from geometry");
    fixed = cell.bbox();
    drc_area = cell.bbox();
  }
  if (fixed) {
    db::set_property(cell, "FIXED_BBOX", db::PropertyValue{db::DimensionList{{fixed->x0, fixed->y0, fixed->x1,
                                                                                 fixed->y1}}});
  }
  if (!macro_.annotating) {
    // DBAdjustLabelsNew changes nothing here: every label the reader made
    // is sticky. LEFview marks the cell as an abstract view.
    put_string(cell, "LEFview", "TRUE");
  }
  reader_.target().macros->finished(cell, drc_area);
}

/// A MACRO or SITE name already used for a cell of this read: the first
/// free `<name>_<k>`.
std::string renamed(const Reader& reader, const std::string& name) {
  for (std::size_t k = 1;; ++k) {
    std::string candidate = name + "_" + std::to_string(k);
    if (!reader.made_here(candidate)) {
      return candidate;
    }
  }
}

}  // namespace

// MACRO <name> / SITE <name>: a new cell, or an existing one annotated.
void Reader::macro_section(const std::string& name) {
  MacroHost* host = target_.macros;
  if (host == nullptr) {
    skip_section(name);
    return;
  }
  std::string cell_name = name;
  const bool duplicate = made_here(name);
  if (duplicate) {
    cell_name = renamed(*this, name);
    warning("Cell \"" + name + "\" was already defined in this file.  Renaming this cell \"" + cell_name + "\"");
  }
  db::CellDef* cell = host->find(cell_name);
  const bool annotating = cell != nullptr;
  if (cell == nullptr && options_.macros.annotate && (duplicate || !magic_compat())) {
    // Q-LEFDEF-11: -annotate skips a macro with no cell (always for a
    // duplicate; with magic-compat OFF for every macro). Magic skips a
    // duplicate to `END MACRO`.
    skip_section(duplicate && magic_compat() ? std::string{"MACRO"} : name);
    return;
  }
  if (cell == nullptr) {
    cell = host->create(cell_name);
    if (cell == nullptr) {
      skip_section(name);
      return;
    }
    note_made(cell_name);
  } else {
    host->annotating(*cell);
  }
  if (options_.macros.datestamp) {
    cell->set_timestamp(*options_.macros.datestamp);
    cell->flags().fixed_timestamp = true;
  }
  MacroState state{*cell, annotating, db::Point{}};
  MacroBody{*this, state, name}.run();
}

}  // namespace hefesto::lef::detail
