// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Geometry and TEXT elements of the GDS-equivalent stream (capability
// oasis-read "Coordinate conversion" rule 1, "Layer and datatype mapping",
// "Shape repetitions", "RECTANGLE elements", "POLYGON, TRAPEZOID and
// CTRAPEZOID elements", "CIRCLE elements", "PATH elements", "TEXT
// elements"). Each element instance, at each displacement of its
// repetition in order, becomes one GDS BOUNDARY, PATH or TEXT; gds-read
// then maps its layer, converts and rescales its coordinates and paints it.
// An instance with a coordinate, width or extension outside 32 bits is
// dropped with "coordinate overflow" (once per element).
//
// Owner: OASR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <array>
#include <limits>

#include "hefesto/mask/input/style.hpp"
#include "hefesto/oasis/read/circle.hpp"
#include "transcoder.hpp"

namespace hefesto::oasis::read_detail {

namespace {

using gds::RecordType;

bool is_boundary(ElementKind kind) {
  switch (kind) {
    case ElementKind::rectangle:
    case ElementKind::polygon:
    case ElementKind::trapezoid:
    case ElementKind::ctrapezoid:
    case ElementKind::circle:
      return true;
    default:
      return false;
  }
}

std::optional<std::int32_t> int32_value(std::int64_t v) {
  return coordinate(v, 0);
}

/// The PATHTYPE of oasis-read "PATH elements" for half-width `hw` and
/// extensions `es`, `ee`.
std::int16_t path_type(std::int64_t hw, std::int64_t es, std::int64_t ee) {
  if (es == 0 && ee == 0) {
    return 0;
  }
  if (hw > 0 && es == hw && ee == hw) {
    return 2;
  }
  return 4;
}

}  // namespace

std::vector<Point64> Transcoder::shape_outline(const Element& e) {
  switch (e.kind) {
    case ElementKind::rectangle:
      return {{0, 0}, {e.size.x, 0}, {e.size.x, e.size.y}, {0, e.size.y}};
    case ElementKind::circle:
      return circle_vertices(e.radius, options_.circle_points);
    case ElementKind::path:
    case ElementKind::text:
      return {};
    default:
      return outline(e);
  }
}

void Transcoder::shape(const Element& e) {
  std::vector<std::vector<Point64>> outlines;
  if (is_boundary(e.kind)) {
    outlines = boundaries(shape_outline(e));
    if (outlines.empty()) {
      return;  // a radius of 0, an outline the decoder rejected, or reported
    }
  } else if (e.kind == ElementKind::path && e.points.size() + 1 > kMaxPoints) {
    error("OASIS-READ-006", "path with " + std::to_string(e.points.size() + 1) +
                                " points (a GDS-equivalent element holds " + std::to_string(kMaxPoints) +
                                "); dropped");
    return;
  }
  check_numbers(e.kind == ElementKind::text ? "text" : "element", e.layer, e.datatype);
  const std::uint64_t count = e.repetition ? instance_count(*e.repetition) : 1;
  overflow_reported_ = false;
  if (e.kind == ElementKind::text) {
    text_ = text_string(e);
  }
  for (std::uint64_t i = 0; i < count; ++i) {
    if (!room()) {
      return;
    }
    const std::optional<Point64> base = instance_base(e, i);
    if (!base) {
      continue;
    }
    if (e.kind == ElementKind::text) {
      text(e, *base);
    } else if (e.kind == ElementKind::path) {
      path(e, *base);
    }
    for (const std::vector<Point64>& outline : outlines) {
      boundary(e, outline, *base);
    }
  }
}

std::vector<std::vector<Point64>> Transcoder::boundaries(std::vector<Point64> rel) {
  if (rel.empty()) {
    return {};
  }
  const std::size_t vertices = rel.size() + 1;  // with the closing one
  if (vertices <= kMaxPoints) {
    return {std::move(rel)};
  }
  auto pieces = fracture(rel, kMaxPoints - 1);
  if (!pieces || pieces->empty()) {
    error("OASIS-READ-006", "polygon with " + std::to_string(vertices) +
                                " points cannot be fractured into GDS-equivalent elements of " +
                                std::to_string(kMaxPoints) + " points; dropped");
    return {};
  }
  error("OASIS-READ-008", "polygon with " + std::to_string(vertices) + " points fractured into " +
                              std::to_string(pieces->size()) + " polygons (a GDS-equivalent element holds " +
                              std::to_string(kMaxPoints) + ")");
  return std::move(*pieces);
}

void Transcoder::check_numbers(std::string_view what, std::uint64_t layer, std::uint64_t datatype) {
  const auto limit = static_cast<std::uint64_t>(kMaxGdsInt2);
  if (layer > limit || datatype > limit) {
    error("OASIS-READ-009", std::string{what} + " on layer " + std::to_string(layer) + ", datatype " +
                                std::to_string(datatype) + ": a number above " + std::to_string(limit) +
                                " is read as " + std::to_string(limit));
  }
}

std::optional<Point64> Transcoder::instance_base(const Element& e, std::uint64_t i) {
  const Point64 d = e.repetition ? offset_at(*e.repetition, i) : Point64{};
  Point64 base;
  if (__builtin_add_overflow(e.position.x, d.x, &base.x) || __builtin_add_overflow(e.position.y, d.y, &base.y)) {
    overflow();
    return std::nullopt;
  }
  return base;
}

void Transcoder::overflow() {
  if (!overflow_reported_) {
    overflow_reported_ = true;
    error("OASIS-READ-004", "coordinate overflow; element instance dropped");
  }
}

std::optional<std::vector<std::int32_t>> Transcoder::points(std::span<const Point64> outline, Point64 base,
                                                            bool close) {
  std::vector<std::int32_t> xy;
  xy.reserve(2 * (outline.size() + 1));
  for (const Point64& p : outline) {
    const auto x = coordinate(base.x, p.x);
    const auto y = coordinate(base.y, p.y);
    if (!x || !y) {
      overflow();
      return std::nullopt;
    }
    xy.push_back(*x);
    xy.push_back(*y);
  }
  if (close && !outline.empty()) {
    xy.push_back(xy[0]);
    xy.push_back(xy[1]);
  }
  return xy;
}

void Transcoder::boundary(const Element& e, std::span<const Point64> outline, Point64 base) {
  const auto xy = points(outline, base, true);
  if (!xy) {
    return;
  }
  const std::int16_t layer = int2_number(e.layer);
  const std::int16_t datatype = int2_number(e.datatype);
  const bool box = e.kind == ElementKind::rectangle && options_.style != nullptr &&
                   !options_.style->calma_layer(layer, datatype);
  gds_.empty(box ? RecordType::box : RecordType::boundary);
  gds_.int2(RecordType::layer, std::array<std::int16_t, 1>{layer});
  gds_.int2(box ? RecordType::boxtype : RecordType::datatype, std::array<std::int16_t, 1>{datatype});
  gds_.int4(RecordType::xy, *xy);
  gds_.empty(RecordType::endel);
}

void Transcoder::path(const Element& e, Point64 base) {
  std::vector<Point64> centerline;
  centerline.reserve(e.points.size() + 1);
  centerline.push_back(Point64{});
  centerline.insert(centerline.end(), e.points.begin(), e.points.end());
  const auto xy = points(centerline, base, false);
  const auto width = e.half_width <= std::numeric_limits<std::int32_t>::max() / 2
                         ? std::optional<std::int32_t>{static_cast<std::int32_t>(2 * e.half_width)}
                         : std::nullopt;
  const auto begin = int32_value(e.start_extension);
  const auto end = int32_value(e.end_extension);
  if (!xy || !width || !begin || !end) {
    overflow();
    return;
  }
  const std::int16_t type = path_type(e.half_width, e.start_extension, e.end_extension);
  gds_.empty(RecordType::path);
  gds_.int2(RecordType::layer, std::array<std::int16_t, 1>{int2_number(e.layer)});
  gds_.int2(RecordType::datatype, std::array<std::int16_t, 1>{int2_number(e.datatype)});
  gds_.int2(RecordType::pathtype, std::array<std::int16_t, 1>{type});
  gds_.int4(RecordType::width, std::array<std::int32_t, 1>{*width});
  if (type == 4) {
    gds_.int4(RecordType::bgnextn, std::array<std::int32_t, 1>{*begin});
    gds_.int4(RecordType::endextn, std::array<std::int32_t, 1>{*end});
  }
  gds_.int4(RecordType::xy, *xy);
  gds_.empty(RecordType::endel);
}

std::string Transcoder::text_string(const Element& e) {
  std::string string = e.name;
  if (e.reference) {
    const auto found = index_.names.textstrings.find(*e.reference);
    string = found != index_.names.textstrings.end() ? found->second : std::string{};  // reported by the decoder
  }
  if (string.size() > kMaxString) {
    error("OASIS-READ-006", "text string of " + std::to_string(string.size()) + " bytes cut to " +
                                std::to_string(kMaxString));
    string.resize(kMaxString);
  }
  return string;
}

void Transcoder::text(const Element& e, Point64 base) {
  const std::array<Point64, 1> at{Point64{}};
  const auto xy = points(at, base, false);
  if (!xy) {
    return;
  }
  gds_.empty(RecordType::text);
  gds_.int2(RecordType::layer, std::array<std::int16_t, 1>{int2_number(e.layer)});
  gds_.int2(RecordType::texttype, std::array<std::int16_t, 1>{int2_number(e.datatype)});
  gds_.int4(RecordType::xy, *xy);
  gds_.string(RecordType::string, text_);
  gds_.empty(RecordType::endel);
}

}  // namespace hefesto::oasis::read_detail
