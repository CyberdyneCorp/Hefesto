// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DefReadPins (capability def-read "PINS section grammar", "Pin geometry
// placement", "Pin port labels"): pin rectangles painted with their
// placement and labelled as ports.
// - Q-LEFDEF-64: DIRECTION and USE carry over from pin to pin with
//   magic-compat ON, and reset at each pin with it OFF.
// - Q-LEFDEF-65: with magic-compat ON the rectangle list is never cleared
//   between pins (leftovers are painted with the next pin); with it OFF
//   rectangles are collected and painted per port.
// - Q-LEFDEF-66: POLYGON and VIA are unknown properties with magic-compat
//   ON, and painted and labelled with it OFF.
// - Fixed in both modes: an unknown pin layer skips its rectangle
//   (Q-LEFDEF-67), a malformed RECT skips the pin statement (Q-LEFDEF-68),
//   and the label size loop ends without a default width (Q-LEFDEF-89).
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defRead.c:1442-1763, Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: DEFR (design D2).

#include <algorithm>
#include <limits>
#include <array>
#include <string>
#include <vector>

#include "def/read/reader.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/lef/via/polygon.hpp"

namespace hefesto::def::detail {

namespace {

enum Property : std::size_t {
  kNet,
  kDirection,
  kLayer,
  kUse,
  kPlaced,
  kFixed,
  kCover,
  kPort,
  kSpecial,
  kPolygon,  // magic-compat OFF from here (Q-LEFDEF-66)
  kVia,
};

constexpr std::array<std::string_view, 11> kProperties{
    "NET", "DIRECTION", "LAYER", "USE", "PLACED", "FIXED", "COVER", "PORT", "SPECIAL", "POLYGON", "VIA",
};
constexpr std::size_t kMagicProperties = kPolygon;

constexpr std::array<std::string_view, 6> kClasses{"DEFAULT", "INPUT", "OUTPUT TRISTATE", "OUTPUT", "INOUT",
                                                   "FEEDTHRU"};
constexpr std::array<db::PortClass, 6> kClassBits{db::PortClass::default_class, db::PortClass::input,
                                                  db::PortClass::tristate,      db::PortClass::output,
                                                  db::PortClass::bidirectional, db::PortClass::feedthrough};

constexpr std::array<std::string_view, 9> kUses{"DEFAULT", "SIGNAL", "POWER", "GROUND", "CLOCK",
                                                "RESET",   "ANALOG", "SCAN",  "TIEOFF"};
constexpr std::array<db::PortUse, 9> kUseBits{db::PortUse::default_use, db::PortUse::signal, db::PortUse::power,
                                              db::PortUse::ground,      db::PortUse::clock,  db::PortUse::reset,
                                              db::PortUse::analog,      db::PortUse::scan,   db::PortUse::tieoff};

constexpr std::array<std::string_view, 2> kStatementKeys{"-", "END"};

constexpr std::uint8_t kAllDirections = db::kPortNorth | db::kPortEast | db::kPortSouth | db::kPortWest;

// A pin shape waiting for its placement: a rectangle, or a polygon
// (magic-compat OFF).
struct PinShape {
  tech::TileType type = tech::kSpace;
  db::Rect rect;
  std::vector<db::Point> polygon;
};

}  // namespace

class PinsReader {
 public:
  explicit PinsReader(Reader& reader) : r_(reader), cell_(reader.design()) {}

  void run(std::int32_t total);

 private:
  void pin();
  bool property(const std::string& keyword);
  void layer();
  void placement();
  void attribute(Property key);
  void class_or_use(bool use);
  void polygon();
  void via();
  void end_port();
  void paint_all(const db::Transform& t, bool erase);
  void paint_shape(const PinShape& shape, const db::Transform& t, bool erase);
  void label(tech::TileType type, const db::Rect& area, bool erase);

  Reader& r_;
  db::CellDef& cell_;
  std::int32_t processed_ = 0;
  std::int32_t total_ = 0;  // the declared count (LefEstimate)
  bool has_ports_ = false;
  // Magic's state: the rectangle list (head first), the pin attributes and
  // the pending placement.
  std::vector<PinShape> shapes_;
  db::PortClass direction_ = db::PortClass::default_class;
  db::PortUse use_ = db::PortUse::default_use;
  std::string name_;
  bool pending_ = false;
  std::optional<tech::TileType> layer_;
  db::Transform transform_;
  // magic-compat OFF: the current port's placement (Q-LEFDEF-65).
  std::optional<db::Transform> port_placement_;
};

void PinsReader::run(std::int32_t total) {
  total_ = total;
  while (const auto token = r_.next()) {
    const auto key = lef::lookup_full(*token, kStatementKeys);
    if (!key) {
      r_.message("Unknown keyword \"" + *token + "\" in PINS definition; ignoring.");
      r_.skip_statement();
      continue;
    }
    if (*key == 1) {
      if (r_.end_matches("PINS")) {
        break;
      }
      r_.error("Pins END statement missing.");
      continue;
    }
    pin();
  }
  r_.count_check(processed_, total, "  Processed " + std::to_string(processed_) + " pins total.", "pins");
}

void PinsReader::pin() {
  has_ports_ = false;
  if (pending_) {
    r_.error("Pin specified without layer, was not placed.");
  }
  r_.estimate(total_);
  ++processed_;
  const auto name = r_.next();
  if (!name) {
    r_.error("Bad pin statement:  Need pin name");
    return;
  }
  name_ = name->substr(0, 2047);  // %2047s
  pending_ = false;
  layer_.reset();
  if (!r_.magic_compat()) {
    direction_ = db::PortClass::default_class;  // Q-LEFDEF-64 OFF
    use_ = db::PortUse::default_use;
    shapes_.clear();
    port_placement_.reset();
  }
  while (const auto token = r_.next()) {
    if (token->starts_with(';')) {
      break;
    }
    if (token->starts_with('+') && !property(r_.take())) {
      return;  // the statement was skipped
    }
  }
  if (!r_.magic_compat()) {
    end_port();
  }
}

// One `+` property; false when it skipped the rest of the statement.
bool PinsReader::property(const std::string& keyword) {
  const auto key = lef::lookup_full(keyword, kProperties);
  if (!key || (*key >= kMagicProperties && r_.magic_compat())) {
    r_.message("Unknown pin property \"" + keyword + "\" in PINS definition; ignoring.");
    return true;
  }
  switch (*key) {
    case kLayer: {
      bool semicolon = false;
      layer_ = r_.read_layer(false);
      const auto rect = r_.read_rect(layer_.has_value(), r_.oscale(), semicolon);
      if (!rect) {
        if (!semicolon) {
          r_.skip_statement();  // Q-LEFDEF-68: Magic dereferences NULL
        }
        return false;
      }
      if (layer_) {  // Q-LEFDEF-67: an unknown layer adds nothing
        shapes_.insert(shapes_.begin(), PinShape{*layer_, *rect, {}});
        layer();
      }
      return true;
    }
    case kPlaced:
    case kFixed:
    case kCover:
      placement();
      return true;
    default:
      attribute(static_cast<Property>(*key));
      return true;
  }
}

// After a LAYER rectangle: paint the list when a placement was waiting.
void PinsReader::layer() {
  if (pending_ && r_.magic_compat()) {
    // Magic erases matching labels in annotation mode only on the
    // placement path (defRead.c:1639-1680 against 1720-1727).
    paint_all(transform_, false);
    pending_ = false;
  }
}

void PinsReader::placement() {
  const auto t = r_.read_location(db::Rect{0, 0, 0, 0}, false, {});
  if (!t) {
    return;  // Magic goes on with the previous (or an uninitialised) transform
  }
  if (!r_.magic_compat()) {
    port_placement_ = *t;
    return;
  }
  transform_ = *t;
  if (!layer_) {
    pending_ = true;
  } else {
    paint_all(transform_, true);
    pending_ = false;
  }
}

void PinsReader::attribute(Property key) {
  if (key == kPort) {
    if (has_ports_) {
      ++processed_;
    }
    has_ports_ = true;
    if (!r_.magic_compat()) {
      end_port();
    }
  } else if (key == kUse || key == kDirection) {
    class_or_use(key == kUse);
  } else if (key == kNet) {
    (void)r_.take();
  } else if (key == kPolygon) {
    polygon();
  } else if (key == kVia) {
    via();
  }
  // SPECIAL: ignored
}

// DIRECTION <class> or USE <use>.
void PinsReader::class_or_use(bool use) {
  const std::string token = r_.take();
  const auto found = use ? lef::lookup_full(token, kUses) : lef::lookup_full(token, kClasses);
  if (!found) {
    r_.error(std::string{use ? "Unknown pin use \"" : "Unknown pin class \""} + token + "\"");
  } else if (use) {
    use_ = kUseBits.at(*found);
  } else {
    direction_ = kClassBits.at(*found);
  }
}

// + POLYGON <layer> <pt> <pt> <pt> ... (magic-compat OFF).
void PinsReader::polygon() {
  const auto type = r_.read_layer(false);
  const auto points = r_.read_points(r_.oscale());
  if (!points || points->size() < 3) {
    r_.error("Bad polygon in PINS definition; ignoring.");
    return;
  }
  if (type) {
    shapes_.insert(shapes_.begin(), PinShape{*type, {}, *points});
  }
}

// + VIA <name> ( x y ) (magic-compat OFF): the via's rectangles at the point.
void PinsReader::via() {
  const std::string name = r_.take();
  const auto points = r_.read_points(r_.oscale() / 2.0F);
  if (!points || points->size() != 1) {
    r_.error("Bad via location in PINS definition; ignoring.");
    return;
  }
  const LefEntry entry = r_.lookup(name);
  if (entry.via == nullptr) {
    r_.error("Via name \"" + name + "\" unknown in PINS definition.");
    return;
  }
  const db::Point at = points->front();
  const auto half = [&at](const db::Rect& r) {
    const auto shift = [](db::Coord v, db::Coord d) { return static_cast<db::Coord>((std::int64_t{v} + d) >> 1); };
    return db::Rect{shift(r.x0, at.x), shift(r.y0, at.y), shift(r.x1, at.x), shift(r.y1, at.y)};
  };
  for (const lef::ViaShape& shape : entry.via->shapes) {
    shapes_.insert(shapes_.begin(), PinShape{shape.type, half(shape.area), {}});
  }
  if (entry.via->type) {
    shapes_.insert(shapes_.begin(), PinShape{*entry.via->type, half(entry.via->area), {}});
  }
}

// magic-compat OFF: the end of a port (a pin's `;` or `+ PORT`).
void PinsReader::end_port() {
  if (!shapes_.empty() && port_placement_) {
    paint_all(*port_placement_, true);
  } else if (!shapes_.empty()) {
    r_.fix_error("Pin specified without placement, was not placed.");
  } else if (port_placement_) {
    r_.error("Pin specified without layer, was not placed.");
  }
  shapes_.clear();
  port_placement_.reset();
}

void PinsReader::paint_all(const db::Transform& t, bool erase) {
  for (const PinShape& shape : shapes_) {
    paint_shape(shape, t, erase);
  }
  shapes_.clear();
  ++r_.pin_number();
}

void PinsReader::paint_shape(const PinShape& shape, const db::Transform& t, bool erase) {
  if (shape.polygon.empty()) {
    const db::Rect area = db::apply(t, shape.rect);
    r_.paint(cell_, shape.type, area);
    label(shape.type, area, erase);
    return;
  }
  std::vector<db::Point> vertices;
  vertices.reserve(shape.polygon.size());
  for (const db::Point& p : shape.polygon) {
    vertices.push_back(db::apply(t, p));
  }
  const auto rects = lef::paint_polygon_rects(cell_, r_.context().tech, shape.type, vertices);
  if (rects.ok()) {
    for (const db::Rect& rect : rects.value()) {
      label(shape.type, rect, erase);
    }
  }
}

// DBPutFontLabel with Magic's size rule: the DRC default width doubled
// while twice it is below the (unrotated) height, times 8. Without a
// default width the height is used (Q-LEFDEF-89: Magic loops forever).
void PinsReader::label(tech::TileType type, const db::Rect& area, bool erase) {
  std::int64_t height = area.height();
  std::int32_t rotation = 0;
  if (height > area.width() * 2) {
    height = area.width();
    rotation = 90;
  }
  std::int64_t size = r_.drc_width(type);
  if (size <= 0) {
    size = height;
  }
  while (size > 0 && size * 2 < height) {
    size *= 2;
  }
  size = std::min<std::int64_t>(size * 8, std::numeric_limits<std::int32_t>::max());
  if (r_.context().options.annotate && erase) {
    (void)db::erase_labels_with_text(cell_, area, name_);
  }
  db::Label l;
  l.text = name_;
  l.type = type;
  l.rect = area;
  l.justification = db::kCenter;
  l.font = 0;
  l.size = static_cast<std::int32_t>(size);
  l.rotation = rotation;
  l.sticky = true;
  l.port_directions = kAllDirections;
  l.port_class = direction_;
  l.port_use = use_;
  l.port_index = r_.pin_number();
  (void)db::put_label(cell_, std::move(l), false, r_.context().font_count);
}

void Reader::pins(std::int32_t total) {
  PinsReader{*this}.run(total);
}

}  // namespace hefesto::def::detail
