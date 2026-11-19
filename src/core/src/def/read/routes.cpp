// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DefAddRoutes (capability def-read "Route layer resolution and width
// selection", "Route wire segment geometry", "Route via placement", "Other
// route elements", "Route painting and net labels"): one routing statement
// read into rectangles in doubled units, then painted (or, annotating,
// used to place the net label).
// - Q-LEFDEF-1 (tier 1, both modes): a non-positive default width is the
//   DRC default width.
// - Q-LEFDEF-63: after a generated via the route stays on its top layer
//   (magic-compat ON) or takes the via's other layer (OFF).
// - Q-LEFDEF-69: an unknown special-net `+` keyword skips to the next `;`
//   (ON) or to the next `+` or `;` (OFF).
// - Q-LEFDEF-70: POLYGON vertices become wire points (ON) or a painted
//   polygon (OFF).
// - Q-LEFDEF-71: VIRTUAL is ignored (ON) or makes the next point a
//   reference point only (OFF).
// - Q-LEFDEF-72: via orientations and MASK are via names (ON) or applied
//   and consumed (OFF).
// - Q-LEFDEF-73: RECT halves the reference point by truncation (ON) or
//   floor (OFF); a RECT without a reference point or with a failed parse
//   is reported and not painted in both modes.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defRead.c:62-762, Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: DEFR (design D2).

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <vector>

#include "def/read/reader.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/lef/via/polygon.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::def::detail {

namespace {

enum SpecialKey : std::size_t {
  kShape,
  kStyle,
  kUse,
  kVoltage,
  kFixedBump,
  kOriginal,
  kPattern,
  kEstCap,
  kWeight,
  kProperty,
  kRouted,
  kFixed,
  kCover,
  kShield,
};

constexpr std::array<std::string_view, 14> kSpecialKeys{
    "SHAPE",    "STYLE",  "USE",      "VOLTAGE", "FIXEDBUMP", "ORIGINAL", "PATTERN",
    "ESTCAP",   "WEIGHT", "PROPERTY", "ROUTED",  "FIXED",     "COVER",    "SHIELD",
};

constexpr std::array<std::string_view, 12> kShapes{
    "RING",     "PADRING",      "BLOCKRING",   "STRIPE",   "FOLLOWPIN",   "IOWIRE",
    "COREWIRE", "BLOCKWIRE",    "BLOCKAGEWIRE", "FILLWIRE", "FILLWIREOPC", "DRCFILL",
};

db::Coord clamp_coord(std::int64_t v) {
  return static_cast<db::Coord>(std::clamp<std::int64_t>(v, std::numeric_limits<db::Coord>::min(),
                                                         std::numeric_limits<db::Coord>::max()));
}

// (r + p) >> 1 corner by corner: a via rectangle (doubled units) at the
// doubled point `p`, in internal units.
db::Rect at_point(const db::Rect& r, db::Point p) {
  const auto half = [](std::int64_t v) { return clamp_coord(v >> 1); };
  return db::Rect{half(std::int64_t{r.x0} + p.x), half(std::int64_t{r.y0} + p.y), half(std::int64_t{r.x1} + p.x),
                  half(std::int64_t{r.y1} + p.y)};
}

}  // namespace

class RouteParser {
 public:
  RouteParser(Reader& reader, const Reader::RouteRequest& request) : r_(reader), q_(request) {}

  std::string run() {
    bool initial = true;
    for (;;) {
      if (!initial) {
        auto next = r_.next();
        if (!next) {
          token_.clear();
          break;
        }
        token_ = std::move(*next);
      }
      if (initial || token_ == "NEW" || token_ == "new") {
        initial = false;
        new_leg();
      } else if (!element(initial)) {
        break;
      }
    }
    r_.paint_routes(shapes_, q_.label);
    return token_;
  }

 private:
  // One route element; false when the token ends the statement.
  bool element(bool& initial) {
    if (q_.special && token_.starts_with('+')) {
      return special_plus(initial);
    }
    if (keyword_element()) {
      return true;
    }
    if (token_.starts_with('(')) {
      point();
      return true;
    }
    if (token_.starts_with(';') || token_.starts_with('+')) {
      return false;
    }
    via(token_);
    return true;
  }

  // RECT, POLYGON, VIRTUAL, TAPER, TAPERRULE and (magic-compat OFF) MASK;
  // matched case-sensitively, as Magic's strcmp.
  bool keyword_element() {
    if (token_ == "RECT") {
      rect();
    } else if (token_ == "POLYGON") {
      polygon();
    } else if (token_ == "VIRTUAL") {
      virtual_next_ = !r_.magic_compat();
    } else if (token_ == "TAPER") {
      width_ = r_.default_width(lefl_, route_layer_.value_or(tech::kSpace));
      extension_ = q_.special ? 0 : width_;
    } else if (token_ == "TAPERRULE") {
      taper_rule();
    } else if (token_ == "MASK" && !r_.magic_compat()) {
      (void)r_.take();  // Q-LEFDEF-72 OFF: MASK n, or the mask bits before a via
    } else {
      return false;
    }
    return true;
  }

  // The start of a leg: its layer, and for a special net its width.
  void new_leg() {
    valid_ = false;
    const std::string token = r_.take();
    token_ = token;
    const LefEntry entry = r_.lookup(token);
    if (!entry.empty()) {
      lefl_ = entry;
      route_layer_ = entry_type(entry);
    } else if (r_.named_without_record(token)) {
      lefl_ = LefEntry{};
    } else {
      route_layer_ = r_.lower_type(token);
      lefl_ = LefEntry{};
    }
    if (!route_layer_) {
      r_.error("Unknown layer type \"" + token + "\" for NEW route");
      return;
    }
    paint_layer_ = route_layer_;
    if (q_.special) {
      special_width();
    } else {
      rule_ = find_rule(lefl_);
      width_ = rule_ != nullptr ? rule_->width : r_.default_width(lefl_, *route_layer_);
      extension_ = width_;
    }
  }

  void special_width() {
    token_ = r_.take();
    const auto w = lef::parse_number(token_);
    if (!w) {
      r_.error("Bad width in special net");
      return;
    }
    const auto scaled = *w != 0.0F ? r_.scaled(*w) : std::nullopt;
    width_ = scaled ? *scaled : r_.default_width(lefl_, *route_layer_);
    extension_ = 0;
    save_width_ = width_;
  }

  static std::optional<tech::TileType> entry_type(const LefEntry& entry) {
    if (entry.via != nullptr) {
      return entry.via->type;
    }
    return entry.layer->type != tech::kSpace ? std::optional<tech::TileType>{entry.layer->type} : std::nullopt;
  }

  // The rule of the current non-default rule set for `entry`
  // (rule->lefInfo == lefl).
  const lef::NonDefaultRuleLayer* find_rule(const LefEntry& entry) const {
    if (q_.rules == nullptr) {
      return nullptr;
    }
    for (const lef::NonDefaultRuleLayer& rule : q_.rules->layers) {
      const LefEntry target = rule.target == lef::RuleTarget::none ? LefEntry{} : r_.lookup(rule.name);
      if (target == entry) {
        return &rule;
      }
    }
    return nullptr;
  }

  // `+ <keyword>` inside a special net's routing.
  bool special_plus(bool& initial) {
    const std::string keyword = r_.take();
    token_ = keyword;
    const auto key = lef::lookup_full(keyword, kSpecialKeys);
    if (!key) {
      r_.message("Unknown keyword \"" + keyword + "\" in SPECIALNET definition; ignoring.");
      return recover();
    }
    switch (*key) {
      case kStyle:
        return style();
      case kShape:
        return shape();
      case kProperty:
        (void)r_.take();
        (void)r_.take();
        return true;
      case kFixedBump:
        return true;
      case kShield:
        (void)r_.take();
        initial = true;
        return true;
      case kRouted:
      case kFixed:
      case kCover:
        initial = true;
        return true;
      default:
        (void)r_.take();  // USE, VOLTAGE, ORIGINAL, PATTERN, ESTCAP, WEIGHT
        return true;
    }
  }

  bool style() {
    token_ = r_.take();
    if (!parse_int(token_)) {
      r_.message("Net style \"" + token_ + "\" in SPECIALNET definition is not a number; ignoring.");
      return recover();
    }
    return true;
  }

  bool shape() {
    token_ = r_.take();
    if (!lef::lookup_full(token_, kShapes)) {
      r_.message("Unknown SHAPE \"" + token_ + "\" in SPECIALNET definition; ignoring.");
      return recover();
    }
    return true;
  }

  // Q-LEFDEF-69: ON skips through the next `;` and goes on reading routes;
  // OFF skips to the next `+` (read next) or `;` (which ends the net).
  bool recover() {
    if (r_.magic_compat()) {
      r_.skip_statement();
      return true;
    }
    while (auto token = r_.next()) {
      if (token->starts_with(';')) {
        token_ = std::move(*token);
        return false;
      }
      if (token->starts_with('+')) {
        r_.push_front(std::move(*token));
        return true;
      }
    }
    token_.clear();
    return false;
  }

  void taper_rule() {
    const std::string name = r_.take();
    if (const auto* set = r_.context().rules_table.find(name); set != nullptr && set->value) {
      const lef::NonDefaultRule* saved = q_.rules;
      q_.rules = &*set->value;
      if (const auto* rule = find_rule(lefl_)) {
        width_ = rule->width;
        extension_ = rule->width;
      }
      q_.rules = saved;
    } else if (name == "DEFAULT") {
      width_ = r_.default_width(lefl_, route_layer_.value_or(tech::kSpace));
      extension_ = q_.special ? 0 : width_;
    } else {
      r_.error("Unknown nondefault rule \"" + name + "\"");
    }
  }

  // RECT ( dx1 dy1 dx2 dy2 ): relative to the reference point.
  void rect() {
    std::string token = r_.take();
    if (token.starts_with('(')) {
      token = r_.take();
    }
    std::array<db::Coord, 4> v{};
    static constexpr std::array<std::string_view, 4> kAxes{"X", "Y", "X", "Y"};
    for (std::size_t i = 0; i < v.size(); ++i) {
      const auto value = lef::parse_number(token);
      const auto scaled = value ? r_.scaled(*value) : std::nullopt;
      if (!scaled) {
        r_.error("Cannot parse " + std::string{kAxes[i]} + " coordinate in RECT.");
        return end_coordinate(token);
      }
      v[i] = *scaled;
      token = r_.take();
    }
    if (!token.starts_with(')')) {
      r_.error("Bad coordinates in RECT.");
      return end_coordinate(token);
    }
    if (!valid_) {
      r_.fix_error("No reference point for RECT");  // Q-LEFDEF-73: Magic reads an undefined point
      return;
    }
    const std::int64_t hx = half(refp_.x);
    const std::int64_t hy = half(refp_.y);
    const db::Rect area = db::canonical(
        db::Rect{clamp_coord(hx + v[0]), clamp_coord(hy + v[1]), clamp_coord(hx + v[2]), clamp_coord(hy + v[3])});
    emit(paint_layer_, area);
  }

  // Q-LEFDEF-73: trunc(P/2) (ON) or floor(P/2) (OFF).
  [[nodiscard]] db::Coord half(db::Coord p) const { return r_.magic_compat() ? p / 2 : (p >> 1); }

  void polygon() {
    r_.error("Route has POLYGON entries, this is not handled!");
    if (r_.magic_compat()) {
      return end_coordinate(r_.take());  // the first vertex; the others become points
    }
    const auto points = r_.read_points(r_.oscale());
    if (points && points->size() >= 3 && route_layer_) {
      shapes_.push_back(RouteShape{*route_layer_, {}, *points});
    }
  }

  // Magic's endCoord: tokens through the next `)`.
  void end_coordinate(std::string token) {
    while (!token.starts_with(')') && !(token.empty() && r_.ended())) {
      token = r_.take();
    }
    token_ = token;
  }

  void via(const std::string& name) {
    if (!valid_) {
      r_.error("Route has via name \"" + name + "\" but no points!");
      return;
    }
    const LefEntry entry = r_.lookup(name);
    if (entry.empty() && !r_.named_without_record(name)) {
      r_.error("Via name \"" + name + "\" unknown in route.");
      return;
    }
    lefl_ = entry;
    bool contact = false;
    if (!entry.empty()) {
      contact = place_via(entry);
    } else if (const auto type = r_.lower_type(name)) {
      r_.error("Error: Via \"" + name + "\" named but undefined.");
      paint_layer_ = *type;
      emit(*type, db::Rect{clamp_coord(std::int64_t{refp_.x} - width_) >> 1,
                           clamp_coord(std::int64_t{refp_.y} - width_) >> 1,
                           clamp_coord(std::int64_t{refp_.x} + width_) >> 1,
                           clamp_coord(std::int64_t{refp_.y} + width_) >> 1});
    } else {
      r_.error("Via name \"" + name + "\" unknown in route.");
      return;
    }
    switch_layer(contact, entry);
  }

  // The via's rectangles at the reference point, the extra list first;
  // with magic-compat OFF a following orientation turns them (Q-LEFDEF-72).
  bool place_via(const LefEntry& entry) {
    const db::Transform turn = orientation();
    const auto place = [&](tech::TileType type, const db::Rect& r) { emit(type, at_point(db::apply(turn, r), refp_)); };
    std::optional<tech::TileType> type;
    db::Rect area;
    if (entry.via != nullptr) {
      for (const lef::ViaShape& shape : entry.via->shapes) {
        place(shape.type, shape.area);
      }
      type = entry.via->type;
      area = entry.via->area;
    } else {
      type = entry.layer->type != tech::kSpace ? std::optional<tech::TileType>{entry.layer->type} : std::nullopt;
      area = entry.layer->contact_area.value_or(db::Rect{0, 0, 0, 0});
    }
    if (!type) {
      return false;  // a via with no type (Magic paints type -1)
    }
    paint_layer_ = *type;
    place(*type, area);
    return true;
  }

  // Q-LEFDEF-72 OFF: a DEF orientation after the via name.
  db::Transform orientation() {
    if (r_.magic_compat()) {
      return db::Transform{};
    }
    auto token = r_.next();
    if (!token) {
      return db::Transform{};
    }
    if (const auto found = lef::lookup_full(*token, kOrientations); found && *token == kOrientations[*found]) {
      return orientation_transform(*found);
    }
    r_.push_front(std::move(*token));
    return db::Transform{};
  }

  // After a via the route continues on the residue of its type that is not
  // the route layer (Q-LEFDEF-63 OFF: a generated via's other layer).
  void switch_layer(bool contact, const LefEntry& entry) {
    const tech::Technology& tech = r_.context().tech;
    if (!contact && !tech.is_contact(paint_layer_.value_or(tech::kSpace))) {
      return;
    }
    if (!paint_layer_) {
      return;
    }
    const auto next = next_layer(*paint_layer_, entry);
    if (!next) {
      return;
    }
    route_layer_ = *next;
    lefl_ = LefEntry{r_.context().layers.magic_to_lef(*next), nullptr};
    rule_ = find_rule(lefl_);
    width_ = q_.special ? save_width_ : (rule_ != nullptr ? rule_->width : r_.default_width(lefl_, *next));
    extension_ = q_.special ? 0 : width_;
  }

  std::optional<tech::TileType> next_layer(tech::TileType via_type, const LefEntry& entry) const {
    if (!r_.magic_compat() && entry.via != nullptr && entry.via->generated) {
      if (const auto other = generated_other(*entry.via)) {
        return other;
      }
    }
    const tech::Technology& tech = r_.context().tech;
    const tech::ContactInfo* info = tech.contact(via_type);
    const tech::TypeMask residues = info != nullptr ? info->residues : tech::TypeMask::of(via_type);
    for (std::size_t t = tech::kFirstUserType; t < tech.user_type_end(); ++t) {
      const auto type = static_cast<tech::TileType>(t);
      if (residues.test(type) && type != route_layer_) {
        return type;
      }
    }
    return std::nullopt;
  }

  // Q-LEFDEF-63 OFF: B when the route is on the top layer T, else T.
  std::optional<tech::TileType> generated_other(const lef::ViaDefinition& via) const {
    const tech::Technology& tech = r_.context().tech;
    std::optional<tech::TileType> bottom;
    for (const lef::ViaShape& shape : via.shapes) {
      if (!tech.is_contact(shape.type) && shape.type != via.type) {
        bottom = shape.type;
        break;
      }
    }
    if (!bottom || !via.type) {
      return std::nullopt;
    }
    return route_layer_ == via.type ? *bottom : *via.type;
  }

  // ( x y [ext] ).
  void point() {
    paint_layer_ = route_layer_;
    const db::Point previous = refp_;
    const db::Coord low_extension = extend_;
    std::string token = r_.take();
    if (!coordinate(token, refp_.x, "X")) {
      return end_coordinate(token);
    }
    token = r_.take();
    if (!coordinate(token, refp_.y, "Y")) {
      return end_coordinate(token);
    }
    extend_ = extension_;
    token = r_.take();
    if (!token.starts_with(')')) {
      const auto z = lef::parse_number(token);
      const auto e = z ? r_.doubled(*z) : std::nullopt;
      if (e) {
        extend_ = *e;
      } else {
        r_.error("Can't parse route extension value.");
      }
    }
    segment(previous, low_extension);
    end_coordinate(token);
  }

  // One coordinate: `*` keeps the reference's; false after an error.
  bool coordinate(const std::string& token, db::Coord& out, std::string_view axis) {
    if (token.starts_with('*')) {
      if (!valid_) {
        r_.error("No reference point for \"*\" wildcard");
        return false;
      }
      return true;
    }
    const auto value = lef::parse_number(token);
    const auto doubled = value ? r_.doubled(*value) : std::nullopt;
    if (!doubled) {
      r_.error("Cannot parse " + std::string{axis} + " coordinate.");
      return false;
    }
    out = *doubled;
    return true;
  }

  void segment(db::Point previous, db::Coord low_extension) {
    const bool reference_only = virtual_next_;
    virtual_next_ = false;
    if (!valid_ || reference_only) {
      valid_ = true;  // Q-LEFDEF-71 OFF: a VIRTUAL point is a reference point only
      return;
    }
    if (previous.x != refp_.x && previous.y != refp_.y) {
      r_.error("Can't deal with nonmanhattan geometry in route.");
      return;
    }
    std::int64_t lo_ext = low_extension;
    std::int64_t hi_ext = extend_;
    if (refp_.x < previous.x || refp_.y < previous.y) {
      hi_ext = low_extension;
      lo_ext = extend_;
    }
    const db::Rect line = db::canonical(db::Rect{previous.x, previous.y, refp_.x, refp_.y});
    const bool vertical = line.x0 == line.x1;
    const bool horizontal = line.y0 == line.y1;
    const std::int64_t w = width_;
    const auto half = [](std::int64_t v) { return clamp_coord(v >> 1); };
    emit(paint_layer_, db::Rect{half(line.x0 - (vertical ? w : lo_ext)), half(line.y0 - (horizontal ? w : lo_ext)),
                                half(line.x1 + (vertical ? w : hi_ext)), half(line.y1 + (horizontal ? w : hi_ext))});
  }

  void emit(std::optional<tech::TileType> type, const db::Rect& rect) {
    if (type) {
      shapes_.push_back(RouteShape{*type, rect, {}});
    }
  }

  Reader& r_;
  Reader::RouteRequest q_;
  std::string token_;
  std::vector<RouteShape> shapes_;
  db::Point refp_;
  bool valid_ = false;
  bool virtual_next_ = false;
  std::optional<tech::TileType> route_layer_;
  std::optional<tech::TileType> paint_layer_;
  LefEntry lefl_;
  const lef::NonDefaultRuleLayer* rule_ = nullptr;
  db::Coord width_ = 0;
  db::Coord save_width_ = 0;
  db::Coord extension_ = 0;
  db::Coord extend_ = 0;
};

// A route polygon (Q-LEFDEF-70 OFF): painted unless annotating; its
// first rectangle is where a label goes.
db::Rect Reader::polygon_area(db::CellDef& cell, const RouteShape& shape, bool annotate) const {
  std::vector<db::Rect> rects;
  if (annotate) {
    rects = lef::polygon_rects(shape.polygon);
  } else if (auto painted = lef::paint_polygon_rects(cell, context_.tech, shape.type, shape.polygon); painted.ok()) {
    rects = std::move(painted).value();
  }
  return rects.empty() ? db::Rect{} : rects.front();
}

// GOTOPOINT at the lower-left corner of `area` lands on a tile of `type`.
bool Reader::on_paint_of(const db::CellDef& cell, tech::TileType type, const db::Rect& area) const {
  const auto plane = context_.tech.home_plane(type);
  if (!plane || *plane >= cell.plane_count()) {
    return false;
  }
  return cell.plane(*plane).type_at(db::Point{area.x0, area.y0}) == type;
}

std::string Reader::add_routes(const RouteRequest& request) {
  return RouteParser{*this, request}.run();
}

// The rectangles of a routing statement painted in order; the first one
// carries the net label (annotation: the first one on paint of its type).
void Reader::paint_routes(const std::vector<RouteShape>& shapes, const std::optional<std::string>& label) {
  if (shapes.empty()) {
    return;
  }
  db::CellDef& cell = design();
  bool labeled = !label.has_value();
  const auto put = [&](const RouteShape& s, const db::Rect& r) {
    db::Label l;
    l.text = *label;
    l.type = s.type;
    const db::Coord cx = static_cast<db::Coord>((std::int64_t{r.x0} + r.x1) / 2);
    const db::Coord cy = static_cast<db::Coord>((std::int64_t{r.y0} + r.y1) / 2);
    l.rect = db::Rect{cx, cy, cx, cy};
    l.justification = db::kCenter;
    (void)db::put_label(cell, std::move(l));
    labeled = true;
  };
  const bool annotate = context_.options.annotate;
  for (std::size_t i = 0; i < shapes.size(); ++i) {
    const RouteShape& s = shapes[i];
    db::Rect area = s.rect;
    if (!s.polygon.empty()) {
      area = polygon_area(cell, s, annotate);
    } else if (!annotate) {
      paint(cell, s.type, area);
    }
    if (labeled) {
      continue;
    }
    if (!annotate || on_paint_of(cell, s.type, area)) {
      put(s, area);
    } else if (i + 1 == shapes.size()) {
      out_.emit(diag::make(kLabelMissed,
                           "Warning:  Label \"" + *label + "\" did not land on any existing net geometry."));
      put(s, area);
    }
  }
}

}  // namespace hefesto::def::detail
