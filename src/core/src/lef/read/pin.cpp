// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PIN section of a macro (capability lef-read "PIN attributes", "Port
// labels in creation mode", "Annotation of existing cells"): LefReadPin
// (lefRead.c:1508-1820) and LefReadPort (lefRead.c:1381-1500).
//
// - Q-LEFDEF-13 (compat): magic-compat ON, `DIRECTION OUTPUT TRISTATE` is
//   output (only the first token is looked up); OFF, it is tristate.
// - Q-LEFDEF-14 (compat): magic-compat ON, the USE values SCAN, RESET and
//   TIEOFF are improper; OFF, they are the uses scan, reset and tie-off.
// - Q-LEFDEF-21 (compat): in annotation mode with magic-compat OFF, a
//   non-port label is never converted when a port label of the same name
//   exists.
// - A DIRECTION, USE or SHAPE at the end of the input reports the improper
//   statement (Magic dereferences a missing token).
//
// Owner: LEFR (design D2).

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/labels.hpp"
#include "lef/read/macro_body.hpp"

namespace hefesto::lef::detail {

namespace {

enum class Key : std::uint8_t {
  direction,
  use,
  port,
  capacitance,
  antennadiffarea,
  antennagatearea,
  antennamodel,
  antennapartialmetalarea,
  antennapartialmetalsidearea,
  antennapartialcutarea,
  antennamaxareacar,
  antennamaxsideareacar,
  shape,
  netexpr,
  end,
};

constexpr std::array<std::string_view, 15> kKeys{
    "DIRECTION",           "USE",          "PORT",           "CAPACITANCE",
    "ANTENNADIFFAREA",     "ANTENNAGATEAREA", "ANTENNAMODEL", "ANTENNAPARTIALMETALAREA",
    "ANTENNAPARTIALMETALSIDEAREA", "ANTENNAPARTIALCUTAREA", "ANTENNAMAXAREACAR", "ANTENNAMAXSIDEAREACAR",
    "SHAPE",               "NETEXPR",      "END",
};

constexpr std::array<std::string_view, 6> kClasses{"DEFAULT", "INPUT", "OUTPUT", "OUTPUT TRISTATE", "INOUT",
                                                   "FEEDTHRU"};
constexpr std::array<db::PortClass, 6> kClassValues{
    db::PortClass::default_class, db::PortClass::input,         db::PortClass::output,
    db::PortClass::tristate,      db::PortClass::bidirectional, db::PortClass::feedthrough};

// The LEF 5.8 uses, then the DEF values Q-LEFDEF-14 OFF accepts.
constexpr std::array<std::string_view, 9> kUses{"DEFAULT", "SIGNAL", "ANALOG", "POWER", "GROUND",
                                                "CLOCK",   "SCAN",   "RESET",  "TIEOFF"};
constexpr std::array<db::PortUse, 9> kUseValues{db::PortUse::default_use, db::PortUse::signal, db::PortUse::analog,
                                                db::PortUse::power,       db::PortUse::ground, db::PortUse::clock,
                                                db::PortUse::scan,        db::PortUse::reset,  db::PortUse::tieoff};
constexpr std::size_t kLefUses = 6;

constexpr std::array<std::string_view, 4> kShapes{"DEFAULT", "ABUTMENT", "RING", "FEEDTHRU"};
constexpr std::array<db::PortShape, 4> kShapeValues{db::PortShape::default_shape, db::PortShape::abutment,
                                                    db::PortShape::ring, db::PortShape::feedthrough};

constexpr std::uint8_t kAllDirections = db::kPortNorth | db::kPortEast | db::kPortSouth | db::kPortWest;

bool ignored_key(Key key) noexcept {
  return key != Key::direction && key != Key::use && key != Key::port && key != Key::shape && key != Key::end;
}

char lower(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool equal_ignoring_case(std::string_view a, std::string_view b) noexcept {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return lower(x) == lower(y);
         });
}

/// The bus delimiter translation of annotation: the first `<` and `>`
/// become `[` and `]`, or else the first `[` and `]` become `<` and `>`.
std::string translated(std::string name) {
  if (const auto open = name.find('<'); open != std::string::npos) {
    name[open] = '[';
    if (const auto close = name.find('>'); close != std::string::npos) {
      name[close] = ']';
    }
  } else if (const auto open2 = name.find('['); open2 != std::string::npos) {
    name[open2] = '<';
    if (const auto close = name.find(']'); close != std::string::npos) {
      name[close] = '>';
    }
  }
  return name;
}

/// The largest port index of the cell's port labels plus one (0 when it
/// has none).
std::uint32_t next_port(const db::CellDef& cell) {
  std::int64_t top = -1;
  for (const db::Label& label : cell.labels()) {
    if (label.is_port()) {
      top = std::max<std::int64_t>(top, label.port_index);
    }
  }
  return static_cast<std::uint32_t>(top + 1);
}

/// The body of one PIN section.
class PinBody {
 public:
  PinBody(Reader& reader, MacroState& macro, std::string name, std::int64_t number)
      : reader_(reader), macro_(macro), name_(std::move(name)), text_(name_), number_(number) {}

  void run() {
    if (macro_.annotating) {
      find_first_label();
    }
    while (const auto keyword = reader_.token()) {
      if (!statement(*keyword)) {
        break;
      }
    }
  }

 private:
  [[nodiscard]] std::vector<db::Label>& labels() { return macro_.cell.labels(); }
  [[nodiscard]] std::optional<std::size_t> next(std::size_t index) {
    return index + 1 < labels().size() ? std::optional{index + 1} : std::nullopt;
  }

  bool statement(const Token& keyword);
  void direction_statement();
  void use_statement();
  void shape_statement();
  void find_first_label();
  [[nodiscard]] std::optional<std::size_t> find_from(std::size_t start, bool ports_only, bool any_case) const;
  void port();
  void annotated_port();
  bool annotate_labels(std::optional<std::size_t>& at, bool has_port);
  void erase_named();
  void put_labels(const std::vector<PortShape>& shapes, std::optional<std::size_t> replace);
  void set_port(db::Label& label, std::uint32_t index);
  void set_attributes(db::Label& label, std::uint32_t index) const;
  void replace_label(db::Label& label, const PortShape& shape);

  Reader& reader_;
  MacroState& macro_;
  std::string name_;  // the pin name END matches
  std::string text_;  // the label text (testpin), translated in annotation
  std::int64_t number_;
  PinAttributes attributes_;
  std::optional<std::size_t> first_;  // annotation: firstlab
  bool first_port_ = true;
};

bool PinBody::statement(const Token& keyword) {
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
  switch (key) {
    case Key::direction:
      direction_statement();
      break;
    case Key::use:
      use_statement();
      break;
    case Key::shape:
      shape_statement();
      break;
    case Key::port:
      port();
      break;
    default:
      if (reader_.parse_end(name_) != EndMatch::not_matched) {
        return false;
      }
      reader_.error("Pin END statement missing.");
      break;
  }
  return true;
}

void PinBody::direction_statement() {
  const auto token = reader_.token();
  auto subkey = token ? lookup_full(token->text, kClasses) : std::nullopt;
  if (!subkey) {
    reader_.error("Improper DIRECTION statement");
  } else {
    if (*subkey == 2U && !reader_.magic_compat()) {
      // Q-LEFDEF-13 OFF: OUTPUT TRISTATE is read as two tokens.
      const auto second = reader_.token();
      if (second && keyword_is(second->text, "TRISTATE")) {
        subkey = 3U;
      } else if (second && second->text.starts_with(';')) {
        attributes_.port_class = kClassValues[*subkey];
        return;
      }
    }
    attributes_.port_class = kClassValues[*subkey];
  }
  reader_.skip_statement();
}

void PinBody::use_statement() {
  const auto token = reader_.token();
  const std::size_t known = reader_.magic_compat() ? kLefUses : kUses.size();
  const auto subkey = token ? lookup_full(token->text, std::span<const std::string_view>{kUses}.first(known)) : std::nullopt;
  if (!subkey) {
    reader_.error("Improper USE statement");
  } else {
    attributes_.use = kUseValues[*subkey];
  }
  reader_.skip_statement();
}

void PinBody::shape_statement() {
  const auto token = reader_.token();
  const auto subkey = token ? lookup_full(token->text, kShapes) : std::nullopt;
  if (!subkey) {
    reader_.error("Improper SHAPE statement");
  } else {
    attributes_.shape = kShapeValues[*subkey];
  }
  reader_.skip_statement();
}

std::optional<std::size_t> PinBody::find_from(std::size_t start, bool ports_only, bool any_case) const {
  const auto& all = macro_.cell.labels();
  for (std::size_t i = start; i < all.size(); ++i) {
    const bool named = any_case ? equal_ignoring_case(all[i].text, text_) : all[i].text == text_;
    if (named && (!ports_only || all[i].is_port())) {
      return i;
    }
  }
  return std::nullopt;
}

// The first label of the annotated cell for this pin: its exact name, the
// name with its bus delimiters translated, the name ignoring case, or else
// the cell's first label.
void PinBody::find_first_label() {
  first_ = find_from(0, false, false);
  if (!first_) {
    const std::string original = text_;
    text_ = translated(text_);
    first_ = find_from(0, false, false);
    if (!first_) {
      text_ = original;
    }
  }
  if (!first_) {
    first_ = find_from(0, false, true);
    if (first_) {
      text_ = labels()[*first_].text;
    }
  }
  if (!first_ && !labels().empty()) {
    first_ = 0;
  }
}

void PinBody::port() {
  if (macro_.annotating) {
    annotated_port();
    return;
  }
  put_labels(read_geometry(reader_, macro_, true), std::nullopt);
}

void PinBody::set_port(db::Label& label, std::uint32_t index) {
  db::Label updated = label;
  set_attributes(updated, index);
  db::replace_label(macro_.cell, label, std::move(updated));
}

void PinBody::set_attributes(db::Label& label, std::uint32_t index) const {
  label.port_class = attributes_.port_class;
  label.port_use = attributes_.use;
  label.port_shape = attributes_.shape;
  label.port_directions = kAllDirections;
  label.port_index = index;
}

// The annotated label `label` takes the first shape: its rectangle and
// type, the pin's attributes, sticky, and its own port index (a new one
// when it was not a port).
void PinBody::replace_label(db::Label& label, const PortShape& shape) {
  number_ = label.is_port() ? label.port_index : next_port(macro_.cell);
  db::Label updated = label;
  updated.rect = shape.rect;
  updated.type = shape.type;
  updated.sticky = true;
  set_attributes(updated, static_cast<std::uint32_t>(number_));
  db::replace_label(macro_.cell, label, std::move(updated));
}

// The labels named text_ from `at` on become ports with the pin's
// attributes, until one that cannot be (a non-port label while a port
// label exists, or a label with no area); `at` is left on that label, or
// nullopt at the end. True when a label was annotated.
bool PinBody::annotate_labels(std::optional<std::size_t>& at, bool has_port) {
  bool annotated = false;
  for (; at; at = next(*at)) {
    db::Label& label = labels()[*at];
    if (label.text != text_) {
      continue;
    }
    if ((has_port && !label.is_port()) || label.rect.is_null()) {
      return annotated;
    }
    const std::uint32_t index = label.is_port() ? label.port_index : next_port(macro_.cell);
    number_ = index;
    set_port(label, index);
    annotated = true;
  }
  return annotated;
}

// DBEraseLabelsByContent(cell, NULL, -1, text_).
void PinBody::erase_named() {
  for (std::size_t i = labels().size(); i > 0; --i) {
    if (labels()[i - 1].text == text_) {
      db::remove_label(macro_.cell, i - 1);
    }
  }
}

// One PORT of a pin of an existing cell (lef-read "Annotation of existing
// cells").
void PinBody::annotated_port() {
  const std::optional<std::size_t> port_label = first_ ? find_from(*first_, true, false) : std::nullopt;
  const bool has_port = port_label.has_value();
  std::optional<std::size_t> at = has_port ? port_label : first_;
  const bool annotated = annotate_labels(at, has_port);
  first_ = at ? next(*at) : std::nullopt;
  if (annotated) {
    reader_.skip_section(std::nullopt);
  } else {
    if (!at && first_port_) {
      erase_named();
    }
    std::optional<std::size_t> replace = at;
    if (replace && has_port && !labels()[*replace].is_port() && !reader_.magic_compat()) {
      replace.reset();  // Q-LEFDEF-21 OFF: the non-port label is left alone
    }
    put_labels(read_geometry(reader_, macro_, true), replace);
  }
  first_port_ = false;
}

// LefReadPort's labels: the first shape updates `replace` when given, every
// other shape makes a new sticky port label.
void PinBody::put_labels(const std::vector<PortShape>& shapes, std::optional<std::size_t> replace) {
  for (const PortShape& shape : shapes) {
    if (replace) {
      replace_label(labels()[*replace], shape);
      replace.reset();
      continue;
    }
    for (const db::Label& existing : labels()) {
      if (existing.is_port() && existing.text == text_) {
        number_ = existing.port_index;
        break;
      }
    }
    db::Label label;
    label.text = text_;
    label.type = shape.type;
    label.rect = shape.rect;
    label.sticky = true;
    set_attributes(label, static_cast<std::uint32_t>(std::max<std::int64_t>(number_, 0)));
    (void)db::put_label(macro_.cell, std::move(label), /*auto_justify=*/true);
  }
}

}  // namespace

void read_pin(Reader& reader, MacroState& macro, const std::string& name, std::int64_t number) {
  PinBody body{reader, macro, name, number};
  body.run();
}

}  // namespace hefesto::lef::detail
