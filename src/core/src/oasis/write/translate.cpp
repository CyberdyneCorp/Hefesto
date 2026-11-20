// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDS element to OASIS element (oasis-write "Boundary translation", "Text
// translation", "Placements", "Array placements", "Reference properties",
// "Read-only GDS vendor cells"; Q-OASIS-32, 62, 63, 69). gds-write's own
// elements are BOUNDARY, TEXT, SREF and AREF; the PATH, BOX and NODE rules,
// MAG, non-quarter-turn angles and inexact AREF steps only occur in vendor
// structures, which the same functions translate.
// Owner: OASW.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <utility>

#include "internal.hpp"

namespace hefesto::oasis::detail {

namespace {

Point64 minus(Point64 a, Point64 b) {
  return {a.x - b.x, a.y - b.y};
}

void warn(TranslateContext& context, std::string text) {
  if (context.sink != nullptr) {
    context.sink->emit(diag::make_native(codes::kVendorElement, "Warning: " + std::move(text)));
  }
}

std::string layer_text(const GdsElement& e) {
  return std::to_string(e.layer) + "/" + std::to_string(e.datatype);
}

// The corners of an axis-parallel rectangle with positive width and
// height, in either rotational direction.
std::optional<std::pair<Point64, Point64>> rectangle_of(std::span<const Point64> v) {
  if (v.size() != 4) {
    return std::nullopt;
  }
  const auto [xlo, xhi] = std::ranges::minmax(v, {}, &Point64::x);
  const auto [ylo, yhi] = std::ranges::minmax(v, {}, &Point64::y);
  const Point64 lo{xlo.x, ylo.y};
  const Point64 hi{xhi.x, yhi.y};
  if (lo.x >= hi.x || lo.y >= hi.y) {
    return std::nullopt;
  }
  std::set<std::pair<std::int64_t, std::int64_t>> corners;
  for (std::size_t i = 0; i < 4; ++i) {
    const Point64 a = v[i];
    const Point64 b = v[(i + 1) % 4];
    if ((a.x != b.x) == (a.y != b.y)) {
      return std::nullopt;  // a diagonal or zero-length edge
    }
    corners.emplace(a.x, a.y);
  }
  return corners.size() == 4 ? std::optional{std::pair{lo, hi}} : std::nullopt;
}

std::size_t distinct_vertices(std::span<const Point64> v) {
  std::set<std::pair<std::int64_t, std::int64_t>> seen;
  for (const Point64& p : v) {
    seen.emplace(p.x, p.y);
  }
  return seen.size();
}

std::vector<Property> properties_of(const GdsElement& e) {
  std::vector<Property> out;
  out.reserve(e.properties.size());
  for (const GdsProperty& p : e.properties) {
    out.push_back(gds_property(p.attribute, unpadded(p.value)));
  }
  return out;
}

Translated single(Element element, const GdsElement& source) {
  Translated out;
  out.items.push_back(Translated::Item{std::move(element), properties_of(source)});
  return out;
}

Translated boundary(const GdsElement& e, TranslateContext& context) {
  const std::vector<Point64> v = boundary_vertices(e.points);
  if (v.empty() || (context.vendor && distinct_vertices(v) < 3)) {
    warn(context, "a BOUNDARY with fewer than 3 distinct vertices in cell " + context.cell + " is not written.");
    return {};
  }
  return single(shape_element(static_cast<std::uint64_t>(e.layer), static_cast<std::uint64_t>(e.datatype), v), e);
}

Translated box(const GdsElement& e, TranslateContext& context) {
  if (e.points.empty()) {
    warn(context, "a BOX without points in cell " + context.cell + " is not written.");
    return {};
  }
  const auto [xlo, xhi] = std::ranges::minmax(e.points, {}, &Point64::x);
  const auto [ylo, yhi] = std::ranges::minmax(e.points, {}, &Point64::y);
  Element r;
  r.kind = ElementKind::rectangle;
  r.layer = static_cast<std::uint64_t>(e.layer);
  r.datatype = static_cast<std::uint64_t>(e.datatype);
  r.position = {xlo.x, ylo.y};
  r.size = {xhi.x - xlo.x, yhi.y - ylo.y};
  return single(std::move(r), e);
}

Element circle(const GdsElement& e, Point64 centre, std::int64_t radius) {
  Element c;
  c.kind = ElementKind::circle;
  c.layer = static_cast<std::uint64_t>(e.layer);
  c.datatype = static_cast<std::uint64_t>(e.datatype);
  c.position = centre;
  c.radius = radius;
  return c;
}

// The path's half-width, or nullopt when the path makes the write fail
// (an odd width, Q-OASIS-63).
std::optional<std::int64_t> half_width(const GdsElement& e, TranslateContext& context) {
  std::int64_t width = e.width;
  if (width < 0) {
    warn(context, "a PATH with negative width " + std::to_string(width) + " in cell " + context.cell +
                      " is written with width " + std::to_string(-width) + ".");
    width = -width;
  }
  if (width % 2 != 0) {
    context.failed = true;
    if (context.sink != nullptr) {
      context.sink->emit(diag::make(codes::kOddPath, "Error: cell " + context.cell + " has a path of odd width " +
                                                         std::to_string(width) + " on " + layer_text(e) +
                                                         "; paths with odd width cannot be written to OASIS files."));
    }
    return std::nullopt;
  }
  return width / 2;
}

Translated path(const GdsElement& e, TranslateContext& context) {
  if (e.points.size() < 2) {
    warn(context, "a PATH with fewer than 2 points in cell " + context.cell + " is not written.");
    return {};
  }
  const std::optional<std::int64_t> hw = half_width(e, context);
  if (!hw) {
    return {};
  }
  Element p;
  p.kind = ElementKind::path;
  p.layer = static_cast<std::uint64_t>(e.layer);
  p.datatype = static_cast<std::uint64_t>(e.datatype);
  p.position = e.points.front();
  p.half_width = *hw;
  for (std::size_t i = 1; i < e.points.size(); ++i) {
    p.points.push_back(minus(e.points[i], e.points.front()));
  }
  switch (e.pathtype) {
    case 0:
    case 1:
      break;
    case 2:
      p.start_extension = *hw;
      p.end_extension = *hw;
      break;
    case 4:
      p.start_extension = e.begin_extension.value_or(0);
      p.end_extension = e.end_extension.value_or(0);
      break;
    default:
      warn(context, "a PATH of type " + std::to_string(e.pathtype) + " in cell " + context.cell +
                        " is written with flush ends.");
      break;
  }
  Translated out = single(std::move(p), e);
  if (e.pathtype == 1 && *hw > 0) {  // round ends: CIRCLE caps, as KLayout writes them
    out.items.push_back(Translated::Item{circle(e, e.points.front(), *hw), {}});
    out.items.push_back(Translated::Item{circle(e, e.points.back(), *hw), {}});
  }
  return out;
}

Translated text(const GdsElement& e, TranslateContext& context) {
  if (e.points.empty()) {
    warn(context, "a TEXT without a position in cell " + context.cell + " is not written.");
    return {};
  }
  Element t;
  t.kind = ElementKind::text;
  t.layer = static_cast<std::uint64_t>(e.layer);
  t.datatype = static_cast<std::uint64_t>(e.datatype);
  t.position = e.points.front();
  t.name = context.vendor && context.table != nullptr
               ? mapped_text(e.string, *context.table, context.strings, context.compat)
               : unpadded(e.string);
  return single(std::move(t), e);
}

// The transform fields of an SREF or AREF.
Element placement(const GdsElement& e, TranslateContext& context) {
  Element p;
  p.kind = ElementKind::placement;
  p.name = context.names->of(e.sname);
  p.flip = (e.strans & 0x8000U) != 0;
  p.position = e.points.empty() ? Point64{} : e.points.front();
  p.magnification = e.magnification.value_or(1.0);
  const double angle = e.angle.value_or(0.0);
  const double quarters = angle / 90.0;
  if (p.magnification == 1.0 && std::isfinite(quarters) && quarters == std::floor(quarters) &&
      std::fabs(quarters) < 1e15) {
    const auto turns = static_cast<std::int64_t>(quarters) % 4;
    p.angle = static_cast<double>((turns + 4) % 4) * 90.0;
  } else {
    p.angle = angle;
  }
  return p;
}

bool divides(std::int64_t value, std::int64_t count) {
  return count > 0 && value % count == 0;
}

// The type 10 repetition of an AREF whose steps are not whole.
std::optional<Repetition> explicit_offsets(const GdsElement& e, Point64 c, Point64 r, TranslateContext& context) {
  const auto columns = static_cast<std::uint64_t>(e.columns);
  const auto rows = static_cast<std::uint64_t>(e.rows);
  if (columns * rows > kMaxExplicitOffsets) {
    context.failed = true;
    if (context.sink != nullptr) {
      context.sink->emit(diag::make(codes::kCap, "Error: an AREF of " + std::to_string(columns) + " x " +
                                                     std::to_string(rows) + " in cell " + context.cell +
                                                     " has inexact steps and too many instances to list."));
    }
    return std::nullopt;
  }
  Repetition rep;
  rep.type = 10;
  for (std::uint64_t j = 0; j < rows; ++j) {
    for (std::uint64_t i = 0; i < columns; ++i) {
      if (i == 0 && j == 0) {
        continue;
      }
      const auto si = static_cast<std::int64_t>(i);
      const auto sj = static_cast<std::int64_t>(j);
      rep.offsets.push_back(Point64{si * c.x / e.columns + sj * r.x / e.rows, si * c.y / e.columns + sj * r.y / e.rows});
    }
  }
  return rep;
}

Translated aref(const GdsElement& e, TranslateContext& context) {
  if (e.points.size() < 3 || e.columns <= 0 || e.rows <= 0) {
    warn(context, "an AREF without 3 points or with a non-positive COLROW in cell " + context.cell +
                      " is not written.");
    return {};
  }
  Element p = placement(e, context);
  const Point64 c = minus(e.points[1], e.points[0]);
  const Point64 r = minus(e.points[2], e.points[0]);
  if (divides(c.x, e.columns) && divides(c.y, e.columns) && divides(r.x, e.rows) && divides(r.y, e.rows)) {
    p.repetition = array_repetition(static_cast<std::uint64_t>(e.columns), static_cast<std::uint64_t>(e.rows),
                                    Point64{c.x / e.columns, c.y / e.columns}, Point64{r.x / e.rows, r.y / e.rows});
    return single(std::move(p), e);
  }
  std::optional<Repetition> rep = explicit_offsets(e, c, r, context);
  if (!rep) {
    return {};
  }
  p.repetition = std::move(rep);
  return single(std::move(p), e);
}

Translated sref(const GdsElement& e, TranslateContext& context) {
  if (e.points.empty()) {
    warn(context, "an SREF without a position in cell " + context.cell + " is not written.");
    return {};
  }
  return single(placement(e, context), e);
}

Translated node(TranslateContext& context) {
  if (!context.node_warned) {
    context.node_warned = true;
    warn(context, "NODE elements in cell " + context.cell + " are not written.");
  }
  return {};
}

Repetition grid(std::uint8_t type, std::uint64_t columns, Point64 column_step, std::uint64_t rows, Point64 row_step) {
  Repetition r;
  r.type = type;
  r.columns = columns;
  r.rows = rows;
  r.column_step = column_step;
  r.row_step = row_step;
  return r;
}

bool along_x(Point64 v) {
  return v.y == 0 && v.x >= 0;
}

bool along_y(Point64 v) {
  return v.x == 0 && v.y >= 0;
}

}  // namespace

std::vector<Point64> boundary_vertices(std::span<const Point64> points) {
  std::vector<Point64> v(points.begin(), points.end());
  if (v.size() > 1 && v.back() == v.front()) {
    v.pop_back();
  }
  return v;
}

Element shape_element(std::uint64_t layer, std::uint64_t datatype, std::span<const Point64> v) {
  Element e;
  e.layer = layer;
  e.datatype = datatype;
  if (const auto box = rectangle_of(v)) {
    e.kind = ElementKind::rectangle;
    e.position = box->first;
    e.size = minus(box->second, box->first);
    return e;
  }
  e.kind = ElementKind::polygon;
  e.position = v.front();
  for (std::size_t i = 1; i < v.size(); ++i) {
    e.points.push_back(minus(v[i], v.front()));
  }
  return e;
}

std::optional<Repetition> array_repetition(std::uint64_t columns, std::uint64_t rows, Point64 a, Point64 b) {
  if (columns > 1 && rows > 1) {
    if (along_x(a) && along_y(b)) {
      return grid(1, columns, a, rows, b);
    }
    if (along_y(a) && along_x(b)) {
      return grid(1, rows, b, columns, a);
    }
    return grid(8, columns, a, rows, b);
  }
  if (columns == 1 && rows == 1) {
    return std::nullopt;
  }
  const std::uint64_t n = columns > 1 ? columns : rows;
  const Point64 w = columns > 1 ? a : b;
  if (along_x(w)) {
    return grid(2, n, w, 1, {});
  }
  if (along_y(w)) {
    return grid(3, 1, {}, n, w);
  }
  return grid(9, n, w, 1, {});
}

Property gds_property(std::int32_t attribute, std::string value) {
  Property p;
  p.name = std::string{standard_property_name(StandardProperty::gds_property)};
  p.standard = true;
  p.values = {PropertyValue::of_unsigned(static_cast<std::uint16_t>(attribute)),
              PropertyValue::of_string(PropertyValue::Kind::b_string, std::move(value))};
  return p;
}

Translated translate(const GdsElement& element, TranslateContext& context) {
  switch (element.kind) {
    case GdsKind::boundary:
      return boundary(element, context);
    case GdsKind::path:
      return path(element, context);
    case GdsKind::sref:
      return sref(element, context);
    case GdsKind::aref:
      return aref(element, context);
    case GdsKind::text:
      return text(element, context);
    case GdsKind::box:
      return box(element, context);
    default:
      return node(context);
  }
}

diag::Status write_items(const Translated& translated, Encoder& out) {
  for (const Translated::Item& item : translated.items) {
    if (diag::Status s = out.element(item.element); !s.ok()) {
      return s;
    }
    for (const Property& p : item.properties) {
      if (diag::Status s = out.property(p); !s.ok()) {
        return s;
      }
    }
  }
  return diag::Done{};
}

}  // namespace hefesto::oasis::detail
