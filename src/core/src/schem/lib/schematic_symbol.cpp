// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A schematic loaded as a symbol (capability schematic-symbols-and-libraries
// "Schematic used as symbol"). Instance graphics use the "Orientation
// transform" of capability schematic-file-format.
//
// Owner: SYMLIB.

#include "hefesto/schem/lib/schematic_symbol.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>
#include <vector>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/sym/instance.hpp"
#include "hefesto/schem/sym/types.hpp"

namespace hefesto::schem {

namespace {

constexpr int kWireLayer = 1;
constexpr int kNestedPinLayer = 7;

// "Boolean values" of schematic-file-format: digits are true when non-zero;
// true/on/yes in any ASCII case.
bool is_true(std::string_view value) {
  if (!value.empty() && std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; })) {
    return value.find_first_not_of('0') != std::string_view::npos;
  }
  std::string lower(value);
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; });
  return lower == "true" || lower == "on" || lower == "yes";
}

bool ignored(const std::string& props) {
  const auto value = get_token(props, "symbol_ignore");
  return value.has_value() && is_true(*value);
}

template <typename Record>
void copy_kept(const std::vector<Record>& from, std::vector<Record>& to) {
  std::copy_if(from.begin(), from.end(), std::back_inserter(to), [](const Record& r) { return !ignored(r.props); });
}

// The placement of one instance: "Orientation transform".
struct Placement {
  Point origin;
  int rotation = 0;
  int flip = 0;

  [[nodiscard]] Point apply(Point local) const {
    const double x = flip == 1 ? -local.x : local.x;
    switch (rotation & 3) {
      case 1: return {origin.x - local.y, origin.y + x};
      case 2: return {origin.x - x, origin.y - local.y};
      case 3: return {origin.x + local.y, origin.y - x};
      default: return {origin.x + x, origin.y + local.y};
    }
  }

  [[nodiscard]] LineRecord line(const LineRecord& line) const {
    return {line.layer, apply(line.p1), apply(line.p2), line.props};
  }

  [[nodiscard]] BoxRecord box(const BoxRecord& box) const {
    const Point a = apply(box.p1);
    const Point b = apply(box.p2);
    const int layer = box.layer == kPinLayer ? kNestedPinLayer : box.layer;
    return {layer, {std::min(a.x, b.x), std::min(a.y, b.y)}, {std::max(a.x, b.x), std::max(a.y, b.y)}, box.props};
  }

  [[nodiscard]] ArcRecord arc(const ArcRecord& arc) const {
    ArcRecord out = arc;
    out.centre = apply(arc.centre);
    const double r = rotation & 3;
    double start = flip == 0 ? std::fmod(arc.start_angle + 270.0 * r, 360.0)
                             : std::fmod(270.0 * r + 180.0 - arc.sweep_angle - arc.start_angle, 360.0);
    if (start < 0) start += 360.0;
    out.start_angle = start;
    return out;
  }

  [[nodiscard]] PolygonRecord polygon(const PolygonRecord& polygon) const {
    PolygonRecord out = polygon;
    for (Point& point : out.points) point = apply(point);
    return out;
  }

  [[nodiscard]] TextRecord text(const TextRecord& text) const {
    TextRecord out = text;
    out.origin = apply(text.origin);
    const int r = rotation & 3;
    const int step = (flip == 1 && (text.rotation & 1) == 1) ? r + 2 : r;
    out.rotation = (text.rotation + step) & 3;
    out.flip = text.flip ^ (flip & 1);
    return out;
  }
};

class Builder {
 public:
  Builder(const Document& schematic, const NestedSymbol& nested) : schematic_(schematic), nested_(nested) {}

  Document build() {
    out_.kind = FileKind::symbol;
    out_.file_version = schematic_.file_version;
    out_.header_text = schematic_.header_text;
    out_.globals.k = schematic_.globals.k;
    copy_kept(schematic_.lines, out_.lines);
    copy_kept(schematic_.boxes, out_.boxes);
    copy_kept(schematic_.arcs, out_.arcs);
    copy_kept(schematic_.polygons, out_.polygons);
    copy_texts();
    copy_wires();
    for (const InstanceRecord& instance : schematic_.instances) place(instance);
    return std::move(out_);
  }

  std::vector<Pin> take_pins() { return std::move(pins_); }

 private:
  void copy_texts() {
    for (const TextRecord& text : schematic_.texts) {
      if (!ignored(text.props) && !floater_info(text).floater) out_.texts.push_back(text);
    }
  }

  void copy_wires() {
    for (const WireRecord& wire : schematic_.wires) {
      if (!ignored(wire.props)) out_.lines.push_back({kWireLayer, wire.p1, wire.p2, wire.props});
    }
  }

  void place(const InstanceRecord& instance) {
    if (ignored(instance.props)) return;
    const Symbol& symbol = nested_(instance);
    const std::string type = symbol_type(symbol);
    if (ignored_in_schematic_symbol(type)) return;
    const Placement at{instance.origin, instance.rotation, instance.flip};
    const Document& graphics = symbol.document;
    for (const LineRecord& line : graphics.lines) out_.lines.push_back(at.line(line));
    for (const BoxRecord& box : graphics.boxes) out_.boxes.push_back(at.box(box));
    for (const ArcRecord& arc : graphics.arcs) out_.arcs.push_back(at.arc(arc));
    for (const PolygonRecord& polygon : graphics.polygons) out_.polygons.push_back(at.polygon(polygon));
    for (const TextRecord& text : graphics.texts) out_.texts.push_back(at.text(text));
    if (is_port_type(type)) add_pin(instance, symbol, type, at);
  }

  void add_pin(const InstanceRecord& instance, const Symbol& symbol, const std::string& type, const Placement& at) {
    Pin pin;
    pin.name = instance_attribute(instance.props, symbol, "lab").value_or("");
    pin.direction = std::string{port_direction(type)};
    if (!symbol.pins.empty()) {
      pin.box = at.box(symbol.pins.front().box);
    } else {
      pin.box.p1 = pin.box.p2 = instance.origin;
    }
    pin.box.layer = kPinLayer;
    pin.box.props = set_token(set_token("", "name", pin.name), "dir", pin.direction);
    pins_.push_back(std::move(pin));
  }

  const Document& schematic_;
  const NestedSymbol& nested_;
  Document out_;
  std::vector<Pin> pins_;
};

std::vector<Pin> ordered_like(std::vector<Pin> pins, const Symbol& order_from) {
  std::vector<Pin> ordered;
  ordered.reserve(pins.size());
  std::vector<bool> taken(pins.size(), false);
  for (const PinIndex index : pin_order(order_from)) {
    const std::string& name = order_from.pins[index].name;
    for (std::size_t i = 0; i < pins.size(); ++i) {
      if (taken[i] || pins[i].name != name) continue;
      taken[i] = true;
      ordered.push_back(std::move(pins[i]));
      break;
    }
  }
  for (std::size_t i = 0; i < pins.size(); ++i) {
    if (!taken[i]) ordered.push_back(std::move(pins[i]));
  }
  return ordered;
}

}  // namespace

Symbol schematic_symbol(std::string reference, platform::Path file, const Document& schematic,
                        const NestedSymbol& nested, const Symbol* order_from) {
  Builder builder(schematic, nested);
  Symbol symbol;
  symbol.reference = std::move(reference);
  symbol.file = std::move(file);
  symbol.document = builder.build();
  symbol.document.path = symbol.file;
  symbol.pins = builder.take_pins();
  if (order_from != nullptr) symbol.pins = ordered_like(std::move(symbol.pins), *order_from);
  for (const Pin& pin : symbol.pins) symbol.document.boxes.push_back(pin.box);
  return symbol;
}

}  // namespace hefesto::schem
