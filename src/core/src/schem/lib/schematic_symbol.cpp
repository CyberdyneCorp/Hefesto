// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A schematic loaded as a symbol (capability schematic-symbols-and-libraries
// "Schematic used as symbol"). Instance graphics use the "Orientation
// transform" of capability schematic-file-format.
//
// The levels are flattened straight into the one output document through the
// chain of placements that leads to them, so the work is bounded by the
// records added (Q-SCHEM-32): a nested schematic is never built as a symbol
// of its own, and a subtree known to add nothing at a level is skipped the
// next time it is placed there (a chain of empty schematics that each place
// the next twice would otherwise take 2^40 visits). The recursion is at most
// kMaxSymbolDepth deep.
//
// Owner: SYMLIB.

#include "hefesto/schem/lib/schematic_symbol.hpp"

#include <algorithm>
#include <deque>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/transform.hpp"
#include "hefesto/schem/sym/instance.hpp"
#include "hefesto/schem/sym/types.hpp"

namespace hefesto::schem {

namespace {

constexpr int kWireLayer = 1;
constexpr int kNestedPinLayer = 7;
constexpr double kPinHalfSize = 2.5;  // generated pins are 5x5 units

bool ignored(const std::string& props) {
  const auto value = get_token(props, "symbol_ignore");
  return value.has_value() && is_true(*value);
}

bool kept_text(const TextRecord& text) {
  return !ignored(text.props) && !floater_info(text).floater;
}

// The chain of placements from the level-0 schematic down to the records
// being added, applied innermost first.
class Chain {
 public:
  void push(const InstanceRecord& instance) { placements_.push_back({instance.origin, instance.rotation, instance.flip}); }
  void pop() { placements_.pop_back(); }
  [[nodiscard]] bool empty() const noexcept { return placements_.empty(); }

  [[nodiscard]] Point point(Point p) const {
    for (auto it = placements_.rbegin(); it != placements_.rend(); ++it) p = transform_point(*it, p);
    return p;
  }

  [[nodiscard]] LineRecord line(const LineRecord& line) const { return {line.layer, point(line.p1), point(line.p2), line.props}; }

  [[nodiscard]] BoxRecord box(const BoxRecord& box) const {
    const Point a = point(box.p1);
    const Point b = point(box.p2);
    return {box.layer, {std::min(a.x, b.x), std::min(a.y, b.y)}, {std::max(a.x, b.x), std::max(a.y, b.y)}, box.props};
  }

  [[nodiscard]] ArcRecord arc(const ArcRecord& arc) const {
    ArcRecord out = arc;
    out.centre = point(arc.centre);
    for (auto it = placements_.rbegin(); it != placements_.rend(); ++it) {
      out.start_angle = transform_arc_start(*it, out.start_angle, arc.sweep_angle);
    }
    return out;
  }

  [[nodiscard]] PolygonRecord polygon(const PolygonRecord& polygon) const {
    PolygonRecord out = polygon;
    for (Point& p : out.points) p = point(p);
    return out;
  }

  [[nodiscard]] TextRecord text(const TextRecord& text) const {
    TextRecord out = text;
    out.origin = point(text.origin);
    for (auto it = placements_.rbegin(); it != placements_.rend(); ++it) {
      const TextOrientation orientation = transform_text(*it, out.rotation, out.flip);
      out.rotation = orientation.rotation;
      out.flip = orientation.flip;
    }
    return out;
  }

 private:
  std::vector<Placement> placements_;
};

// The generated pin of a port instance at level 0: a 5x5 layer-5 box
// centred on its origin, `name=<lab> dir=<d> ` and its other attributes.
Pin generated_pin(const InstanceRecord& instance, const Symbol& port, std::string_view type) {
  Pin pin;
  pin.name = instance_attribute(instance.props, port, "lab").value_or("");
  pin.direction = std::string{port_direction(type)};
  const Point at = instance.origin;
  const std::string others = set_token(set_token(instance.props, "name", std::nullopt), "lab", std::nullopt);
  pin.box = {kPinLayer,
             {at.x - kPinHalfSize, at.y - kPinHalfSize},
             {at.x + kPinHalfSize, at.y + kPinHalfSize},
             "name=" + pin.name + " dir=" + pin.direction + " " + others};
  return pin;
}

class Flattener {
 public:
  Flattener(const NestedResolver& nested, FlattenLimits limits) : nested_(nested), limits_(limits) {}

  Document run(const Document& schematic) {
    out_.kind = FileKind::symbol;
    out_.file_version = schematic.file_version;
    out_.header_text = schematic.header_text;
    out_.globals.k = schematic.globals.k;
    add_level(schematic, 0);
    return std::move(out_);
  }

  // The pins: the level-0 layer-5 boxes, then the generated ones.
  std::vector<Pin> pins() {
    std::vector<Pin> all = std::move(own_pins_);
    all.insert(all.end(), std::make_move_iterator(generated_pins_.begin()),
               std::make_move_iterator(generated_pins_.end()));
    return all;
  }

  [[nodiscard]] const FlattenReport& report() const noexcept { return report_; }

 private:
  [[nodiscard]] bool stopped() const noexcept { return report_.depth_reached || report_.size_reached; }

  // Takes a slot for one record; false (and stop) when the limit is reached.
  bool take_slot() {
    if (stopped()) return false;
    if (count_ >= limits_.max_records) {
      report_.size_reached = true;
      return false;
    }
    ++count_;
    return true;
  }

  template <typename Record>
  void add(std::vector<Record>& to, Record record) {
    if (take_slot()) to.push_back(std::move(record));
  }

  // The records of one schematic level (level 0 is the symbol's own).
  void add_level(const Document& schematic, int level) {
    add_own_graphics(schematic, level);
    for (const InstanceRecord& instance : schematic.instances) {
      if (stopped()) return;
      if (!ignored(instance.props)) place(instance, schematic, level);
    }
  }

  // Adds `place(record)` to `to` for each record not `symbol_ignore`d.
  template <typename Record, typename Out, typename Place>
  void add_kept(std::vector<Out>& to, const std::vector<Record>& records, Place place) {
    for (const Record& record : records) {
      if (!ignored(record.props)) add(to, place(record));
    }
  }

  void add_own_graphics(const Document& schematic, int level) {
    add_kept(out_.lines, schematic.lines, [this](const LineRecord& line) { return chain_.line(line); });
    for (const BoxRecord& box : schematic.boxes) {
      if (!ignored(box.props) && !is_graph_box(box)) add_own_box(box, level);
    }
    add_kept(out_.arcs, schematic.arcs, [this](const ArcRecord& arc) { return chain_.arc(arc); });
    add_kept(out_.polygons, schematic.polygons, [this](const PolygonRecord& p) { return chain_.polygon(p); });
    for (const TextRecord& text : schematic.texts) {
      if (kept_text(text)) add(out_.texts, chain_.text(text));
    }
    add_kept(out_.lines, schematic.wires,
             [this](const WireRecord& wire) { return chain_.line({kWireLayer, wire.p1, wire.p2, wire.props}); });
  }

  // A layer-5 box of the level-0 schematic is a pin; deeper ones are drawn
  // on layer 7.
  void add_own_box(const BoxRecord& box, int level) {
    if (box.layer != kPinLayer) return add(out_.boxes, chain_.box(box));
    if (level > 0) {
      BoxRecord moved = chain_.box(box);
      moved.layer = kNestedPinLayer;
      return add(out_.boxes, std::move(moved));
    }
    if (take_slot()) own_pins_.push_back(make_pin(box));
  }

  void place(const InstanceRecord& instance, const Document& from, int level) {
    const NestedTarget target = nested_(instance, from);
    if (target.schematic != nullptr) return open(instance, *target.schematic, level + 1);
    if (target.symbol == nullptr) return;
    const std::string type = symbol_type(*target.symbol);
    if (ignored_in_schematic_symbol(type)) return;
    chain_.push(instance);
    add_symbol_graphics(target.symbol->document);
    chain_.pop();
    if (level == 0 && is_port_type(type)) add_generated_pin(instance, *target.symbol, type);
  }

  void open(const InstanceRecord& instance, const Document& schematic, int level) {
    if (level >= limits_.max_depth) {
      report_.depth_reached = true;
      return;
    }
    if (ignored_in_schematic_symbol(get_token(schematic.globals.k, "type").value_or(""))) return;
    const auto key = std::make_pair(&schematic, level);
    if (empty_.count(key) != 0) return;
    const std::size_t before = count_;
    chain_.push(instance);
    add_level(schematic, level);
    chain_.pop();
    if (count_ == before && !stopped()) empty_.insert(key);
  }

  void add_symbol_graphics(const Document& graphics) {
    for (const LineRecord& line : graphics.lines) add(out_.lines, chain_.line(line));
    for (const BoxRecord& box : graphics.boxes) {
      if (is_graph_box(box)) continue;
      BoxRecord placed = chain_.box(box);
      if (placed.layer == kPinLayer) placed.layer = kNestedPinLayer;
      add(out_.boxes, std::move(placed));
    }
    for (const ArcRecord& arc : graphics.arcs) add(out_.arcs, chain_.arc(arc));
    for (const PolygonRecord& polygon : graphics.polygons) add(out_.polygons, chain_.polygon(polygon));
    for (const TextRecord& text : graphics.texts) add(out_.texts, chain_.text(text));
  }

  void add_generated_pin(const InstanceRecord& instance, const Symbol& port, std::string_view type) {
    if (take_slot()) generated_pins_.push_back(generated_pin(instance, port, type));
  }

  const NestedResolver& nested_;
  FlattenLimits limits_;
  Document out_;
  Chain chain_;
  std::vector<Pin> own_pins_;
  std::vector<Pin> generated_pins_;
  std::size_t count_ = 0;
  std::set<std::pair<const Document*, int>> empty_;  // subtrees that add nothing at a level
  FlattenReport report_;
};

// For each name of order_from in its pin order, the first pin of that name
// not yet taken; then the rest in their order. The pins are bucketed by
// name, so the cost is O(n log n), not O(n^2).
std::vector<Pin> ordered_like(std::vector<Pin> pins, const Symbol& order_from) {
  std::map<std::string, std::deque<std::size_t>> by_name;
  for (std::size_t i = 0; i < pins.size(); ++i) by_name[pins[i].name].push_back(i);
  std::vector<Pin> ordered;
  ordered.reserve(pins.size());
  std::vector<bool> taken(pins.size(), false);
  for (const PinIndex index : pin_order(order_from)) {
    const auto bucket = by_name.find(order_from.pins[index].name);
    if (bucket == by_name.end() || bucket->second.empty()) continue;
    const std::size_t i = bucket->second.front();
    bucket->second.pop_front();
    taken[i] = true;
    ordered.push_back(std::move(pins[i]));
  }
  for (std::size_t i = 0; i < pins.size(); ++i) {
    if (!taken[i]) ordered.push_back(std::move(pins[i]));
  }
  return ordered;
}

}  // namespace

bool is_graph_box(const BoxRecord& box) {
  const auto flags = get_token(box.props, "flags");
  return flags.has_value() && flags->find("graph") != std::string::npos;
}

Symbol schematic_symbol(std::string reference, platform::Path file, const Document& schematic,
                        const NestedResolver& nested, const Symbol* order_from, FlattenReport* report,
                        FlattenLimits limits) {
  Flattener flattener(nested, limits);
  Symbol symbol;
  symbol.reference = std::move(reference);
  symbol.file = std::move(file);
  symbol.document = flattener.run(schematic);
  symbol.document.path = symbol.file;
  symbol.pins = flattener.pins();
  if (order_from != nullptr) symbol.pins = ordered_like(std::move(symbol.pins), *order_from);
  for (const Pin& pin : symbol.pins) symbol.document.boxes.push_back(pin.box);
  if (report != nullptr) *report = flattener.report();
  return symbol;
}

}  // namespace hefesto::schem
