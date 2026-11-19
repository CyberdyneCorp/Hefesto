// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PORT and OBS geometry (capability lef-read "PORT and OBS geometry",
// "Annotation of existing cells"): LefReadGeometry (lefRead.c:1166-1349),
// with LefPaintPolygon (lefRead.c:1037-1066) through LEFVIA's
// paint_polygon_rects and the annotation coverage check of lefConnectFunc
// and lefUnconnectFunc (lefRead.c:1086-1138).
//
// - Q-LEFDEF-15 (compat): magic-compat ON, WIDTH, PATH and VIA are skipped
//   to their `;`; OFF, WIDTH sets the path width, PATH paints its
//   Manhattan segments w wide (extended by w/2 at both ends) and VIA x y
//   name places a LEF via definition, each rectangle becoming port
//   geometry as a RECT's does.
// - Q-LEFDEF-18 (fix): a POLYGON with no current layer is skipped, as a
//   RECT is.
// - Q-LEFDEF-21 (compat): in annotation mode, magic-compat ON paints the
//   accepted RECTs and every POLYGON into the cell; OFF paints nothing and
//   checks each POLYGON rectangle as a RECT.
// - Q-LEFDEF-23 (replicate): a POLYGON's port rectangles come from its
//   single decomposition.
// - A rectangle whose corners, offset included, do not fit an internal
//   coordinate is dropped (Magic's int arithmetic is undefined there).
//
// Owner: LEFR (design D2).

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "hefesto/connect/tree.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/lef/read/geometry.hpp"
#include "hefesto/lef/read/units.hpp"
#include "hefesto/lef/via/polygon.hpp"
#include "hefesto/tech/technology.hpp"
#include "lef/read/macro_body.hpp"

namespace hefesto::lef::detail {

namespace {

enum class Key : std::uint8_t { layer, width, path, rect, polygon, via, port_class, end };

constexpr std::array<std::string_view, 8> kKeys{"LAYER", "WIDTH", "PATH", "RECT", "POLYGON", "VIA", "CLASS", "END"};

constexpr bool fits(std::int64_t v) noexcept {
  return v >= std::numeric_limits<db::Coord>::min() && v <= std::numeric_limits<db::Coord>::max();
}

std::optional<db::Rect> rect_of(std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1) {
  if (!fits(x0) || !fits(y0) || !fits(x1) || !fits(y1)) {
    return std::nullopt;
  }
  return db::Rect{static_cast<db::Coord>(x0), static_cast<db::Coord>(y0), static_cast<db::Coord>(x1),
                  static_cast<db::Coord>(y1)};
}

std::optional<db::Rect> shifted(const db::Rect& r, db::Point by) {
  return rect_of(std::int64_t{r.x0} + by.x, std::int64_t{r.y0} + by.y, std::int64_t{r.x1} + by.x,
                 std::int64_t{r.y1} + by.y);
}

// floor(v / 2) (Magic's arithmetic shift).
constexpr std::int64_t half_floor(std::int64_t v) noexcept {
  return v >= 0 ? v / 2 : -((-v + 1) / 2);
}

// The half-unit rectangle `area` about the centre (x2 / 2, y2 / 2), in
// internal units: the contact sizing of a cut RECT (x2, y2: the sums of its
// corners) and the placement of a via's rectangles.
std::optional<db::Rect> centred(std::int64_t x2, std::int64_t y2, const db::Rect& area) {
  return rect_of(half_floor(x2 + area.x0), half_floor(y2 + area.y0), half_floor(x2 + area.x1),
                 half_floor(y2 + area.y1));
}

db::Rect clipped(const db::Rect& r, const db::Rect& to) {
  return db::Rect{std::max(r.x0, to.x0), std::max(r.y0, to.y0), std::min(r.x1, to.x1), std::min(r.y1, to.y1)};
}

/// The body of one PORT or OBS.
class GeometryBody {
 public:
  GeometryBody(Reader& reader, MacroState& macro, bool port)
      : reader_(reader), macro_(macro), tech_(reader.options().tech), port_(port) {}

  std::vector<PortShape> run() {
    while (const auto keyword = reader_.token()) {
      if (!statement(*keyword)) {
        break;
      }
    }
    return std::move(shapes_);
  }

 private:
  [[nodiscard]] bool paints() const { return !macro_.annotating || reader_.magic_compat(); }
  [[nodiscard]] bool extended() const { return !reader_.magic_compat(); }

  bool statement(const Token& keyword);
  bool keyed_statement(Key key);
  void layer_statement();
  void rect_statement();
  void polygon_statement();
  void width_statement();
  void path_statement();
  void via_statement();
  void place(tech::TileType type, const db::Rect& area, bool contact_sizing);
  void paint(tech::TileType type, const db::Rect& area);
  void paint_polygon(const std::vector<db::Point>& points);
  void check_polygon(const std::vector<db::Point>& points);
  [[nodiscard]] bool covered(tech::TileType type, const db::Rect& area) const;
  [[nodiscard]] std::optional<db::Rect> contact_sized(tech::TileType type, const db::Rect& area) const;

  Reader& reader_;
  MacroState& macro_;
  const tech::Technology& tech_;
  bool port_;
  LayerState layer_;              // curlayer, otherlayer and the contact area
  db::Coord path_width_ = 0;      // WIDTH (magic-compat OFF)
  std::vector<PortShape> shapes_;  // the PORT's geometry list
};

bool GeometryBody::statement(const Token& keyword) {
  const auto index = lookup_full(keyword.text, kKeys);
  if (!index) {
    reader_.info("Unknown keyword \"" + keyword.text + "\" in LEF file; ignoring.");
    reader_.skip_statement();
    return true;
  }
  const auto key = static_cast<Key>(*index);
  if (key == Key::end) {
    if (reader_.parse_end(std::nullopt) != EndMatch::not_matched) {
      return false;
    }
    reader_.error("Geometry (PORT or OBS) END statement missing.");
    return true;
  }
  return keyed_statement(key);
}

bool GeometryBody::keyed_statement(Key key) {
  switch (key) {
    case Key::layer:
      layer_statement();
      break;
    case Key::rect:
      rect_statement();
      break;
    case Key::polygon:
      polygon_statement();
      break;
    case Key::width:
      width_statement();
      break;
    case Key::path:
      path_statement();
      break;
    case Key::via:
      via_statement();
      break;
    default:
      reader_.skip_statement();  // CLASS
      break;
  }
  return true;
}

// LAYER n: normal lookup in a PORT, obstruction lookup in an OBS.
void GeometryBody::layer_statement() {
  if (const auto name = reader_.token()) {
    resolve_layer(name->text, port_ ? LayerLookup::normal : LayerLookup::obstruction, layer_,
                  reader_.layer_context());
  }
  reader_.skip_statement();
}

void GeometryBody::rect_statement() {
  if (layer_.type) {
    const auto read = read_rect(reader_.lexer(), reader_.messages(), true, reader_.options().scale);
    const auto area = read ? shifted(*read, macro_.offset) : std::nullopt;
    if (area) {
      place(*layer_.type, *area, true);
    }
  }
  reader_.skip_statement();
}

// The cut of a contact layer takes the contact area's size about its centre.
std::optional<db::Rect> GeometryBody::contact_sized(tech::TileType type, const db::Rect& area) const {
  if (!tech_.is_contact(type) || !layer_.contact_area || layer_.contact_area->is_null()) {
    return area;
  }
  return centred(std::int64_t{area.x0} + area.x1, std::int64_t{area.y0} + area.y1, *layer_.contact_area);
}

// One accepted rectangle: the annotation check, contact sizing, paint (the
// secondary obstruction type too in an OBS) and the PORT's list.
void GeometryBody::place(tech::TileType type, const db::Rect& area, bool contact_sizing) {
  if (macro_.annotating && !covered(type, area)) {
    return;
  }
  const auto sized = contact_sizing ? contact_sized(type, area) : std::optional<db::Rect>{area};
  if (!sized) {
    return;
  }
  if (paints()) {
    paint(type, *sized);
    if (!port_ && layer_.secondary) {
      paint(*layer_.secondary, *sized);
    }
  }
  if (port_) {
    shapes_.insert(shapes_.begin(), PortShape{type, *sized});
  }
}

void GeometryBody::paint(tech::TileType type, const db::Rect& area) {
  (void)db::paint_type(macro_.cell, tech_, area, type);
}

// lefConnectFunc / lefUnconnectFunc: `area` is covered, in the cell and
// its subcells, by types that connect to `type`.
bool GeometryBody::covered(tech::TileType type, const db::Rect& area) const {
  if (area.is_null()) {
    return true;
  }
  db::Plane check;
  db::PaintRow fill{};
  fill.fill(1);
  const tech::PlaneMask planes = tech_.paint_planes(type);
  (void)connect::search_tree_tiles(
      macro_.cell, area, tech_.connects_to(type), {}, reader_.magic_compat(), [&](const connect::TreeTile& t) {
        if ((planes & tech::plane_bit(t.plane)) != 0) {
          const db::Rect part = clipped(db::apply(t.to_root, t.tile.area), area);
          if (!part.is_null()) {
            (void)check.paint(part, fill);
          }
        }
        return true;
      });
  bool full = true;
  (void)check.enumerate(area, tech::TypeMask::of(tech::kSpace), [&](const db::TileView&) {
    full = false;
    return false;
  });
  return full;
}

// POLYGON: the vertices in reverse file order, decomposed and painted on
// every plane of the layer; its rectangles are appended to the PORT's list.
void GeometryBody::polygon_statement() {
  if (!layer_.type) {
    reader_.skip_statement();  // Q-LEFDEF-18 fix
    return;
  }
  const auto points = read_points(reader_.lexer(), reader_.messages(), reader_.options().scale, macro_.offset);
  if (points.size() < 3) {
    return;
  }
  if (macro_.annotating && !reader_.magic_compat()) {
    check_polygon(points);  // Q-LEFDEF-21 OFF
  } else {
    paint_polygon(points);
  }
}

void GeometryBody::paint_polygon(const std::vector<db::Point>& points) {
  auto rects = paint_polygon_rects(macro_.cell, tech_, *layer_.type, points);
  if (!port_ && layer_.secondary) {
    (void)paint_polygon_rects(macro_.cell, tech_, *layer_.secondary, points);
  }
  if (port_ && rects.ok()) {
    for (const db::Rect& rect : rects.value()) {
      shapes_.push_back(PortShape{*layer_.type, rect});
    }
  }
}

void GeometryBody::check_polygon(const std::vector<db::Point>& points) {
  for (const db::Rect& rect : polygon_rects(points)) {
    if (port_ && covered(*layer_.type, rect)) {
      shapes_.push_back(PortShape{*layer_.type, rect});
    }
  }
}

// WIDTH w (magic-compat OFF): the width of the PATHs that follow.
void GeometryBody::width_statement() {
  if (extended()) {
    if (const auto width = reader_.read_value(); width && *width >= 0) {
      path_width_ = *width;
    }
  }
  reader_.skip_statement();
}

// PATH x y ... (magic-compat OFF): each Manhattan segment as a rectangle
// path_width_ wide, extended by half the width at both ends.
void GeometryBody::path_statement() {
  if (!extended() || !layer_.type) {
    reader_.skip_statement();
    return;
  }
  const auto points = read_points(reader_.lexer(), reader_.messages(), reader_.options().scale, macro_.offset, "path");
  const std::int64_t lo = path_width_ / 2;
  const std::int64_t hi = path_width_ - lo;
  for (std::size_t i = 1; i < points.size(); ++i) {
    const db::Point a = points[i - 1];
    const db::Point b = points[i];
    if (a.x != b.x && a.y != b.y) {
      continue;  // a diagonal segment has no rectangle
    }
    const auto area = rect_of(std::int64_t{std::min(a.x, b.x)} - lo, std::int64_t{std::min(a.y, b.y)} - lo,
                              std::int64_t{std::max(a.x, b.x)} + hi, std::int64_t{std::max(a.y, b.y)} + hi);
    if (area) {
      place(*layer_.type, *area, false);
    }
  }
}

// VIA x y name (magic-compat OFF): the named via's contact area and extra
// rectangles, each with its own type, centred at the point.
void GeometryBody::via_statement() {
  if (!extended()) {
    reader_.skip_statement();
    return;
  }
  float x = 0.0F;
  float y = 0.0F;
  const PointRead read = read_point(reader_.lexer(), x, y);
  const auto name = read == PointRead::point ? reader_.token() : std::nullopt;
  if (read != PointRead::none) {
    reader_.skip_statement();
  }
  if (!name) {
    return;
  }
  const auto* entry = reader_.target().vias.find(name->text);
  const auto centre = shifted(db::Rect{to_internal(x, reader_.options().scale).value_or(0),
                                       to_internal(y, reader_.options().scale).value_or(0), 0, 0},
                              macro_.offset);
  if (entry == nullptr || !entry->value || !centre) {
    reader_.info("Unknown via \"" + name->text + "\" in LEF geometry; ignoring.");
    return;
  }
  const ViaDefinition& via = *entry->value;
  const std::int64_t x2 = std::int64_t{centre->x0} * 2;
  const std::int64_t y2 = std::int64_t{centre->y0} * 2;
  if (via.type) {
    if (const auto area = centred(x2, y2, via.area)) {
      place(*via.type, *area, false);
    }
  }
  for (const ViaShape& shape : via.shapes) {
    if (const auto area = centred(x2, y2, shape.area)) {
      place(shape.type, *area, false);
    }
  }
}

}  // namespace

std::vector<PortShape> read_geometry(Reader& reader, MacroState& macro, bool port) {
  GeometryBody body{reader, macro, port};
  return body.run();
}

}  // namespace hefesto::lef::detail
