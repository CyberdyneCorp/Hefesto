// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// BOUNDARY, BOX and PATH elements (capability gds-read "BOUNDARY elements",
// "Non-Manhattan boundary decomposition", "BOX elements", "PATH elements",
// "Path-to-polygon conversion", "Path centerline properties", "Layer and
// datatype mapping", "Polygon subcells", "Path subcells", "Coordinate
// conversion and automatic rescaling"). Ported from Magic 8.3.694
// calma/CalmaRdpt.c (calmaElementBoundary, calmaElementBox,
// calmaElementPath, calmaReadPath, calmaLayerError), cif/CIFrdpt.c
// (CIFPropRecordPath) and cif/CIFrdpoly.c (CIFPolyToRects, through
// db::decompose_polygon); Magic is (c) the Regents of the University of
// California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).
//
// Compat decisions: Q-MASK-113 (path type 1: flush ON, half-width
// extension OFF), Q-MASK-114 (end extensions, gds::extend_path), Q-MASK-115
// (a negative width is dropped ON, its absolute value used OFF), Q-MASK-116
// and 118 (gds::path_outline), Q-MASK-119 (savepaths values in read units
// ON, internal units OFF).
//
// Deviations (X-2 tier 1): read-unit values are 64-bit and clamped to the
// usable plane when painted; the OFF savepaths conversion divides by S with
// floor and never subdivides the grid.
//
// Owner: GDSR (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "gds/read/reader.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/polygon.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/values.hpp"

namespace hefesto::gds::read_detail {

namespace {

constexpr std::string_view kCompatNote = "GDS-READ-012";
constexpr std::string_view kReadError = "GDS-READ-006";
constexpr std::int64_t kLargePoint = 0x0fffffff;

db::Coord clamp_coord(std::int64_t v) noexcept {
  return static_cast<db::Coord>(std::clamp<std::int64_t>(v, db::kPlaneMin, db::kPlaneMax));
}

db::Rect clamp_rect(std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1) noexcept {
  return db::Rect{clamp_coord(x0), clamp_coord(y0), clamp_coord(x1), clamp_coord(y1)};
}

/// The edges of the closed path, plus one corner per diagonal edge
/// (CIFMakeManhattanPath), for the "fewer than 4 points" test.
std::size_t manhattan_edge_count(const mask::Polygon& points) {
  if (points.empty()) {
    return 0;
  }
  std::size_t edges = points.size() - 1;
  std::size_t diagonals = 0;
  for (std::size_t i = 0; i + 1 < points.size(); ++i) {
    if (points[i].x != points[i + 1].x && points[i].y != points[i + 1].y) {
      ++diagonals;
    }
  }
  if (points.back() != points.front()) {
    ++edges;
    if (points.back().x != points.front().x && points.back().y != points.front().y) {
      ++diagonals;
    }
  }
  return edges + diagonals;
}

void multiply(std::vector<PathPoint>& points, std::int64_t r) {
  for (PathPoint& p : points) {
    p.x *= r;
    p.y *= r;
  }
}

/// a * b / c in 64 bits, saturating (the OFF negative-width conversion).
std::int64_t mul_div(std::int64_t a, std::int64_t b, std::int64_t c) noexcept {
  std::int64_t product = 0;
  if (__builtin_mul_overflow(a, b, &product)) {
    return (a < 0) != (b < 0) ? db::kPlaneMin : db::kPlaneMax;
  }
  return c == 0 ? product : product / c;
}

}  // namespace

// --- shared ---------------------------------------------------------------------

void Reader::layer_error(std::string_view message, std::int32_t layer, std::int32_t datatype) {
  if (current_ != nullptr && current_->flags().vendor_gds) {
    return;  // read-only cells expect unhandled layers
  }
  if (layer_errors_.insert(layer_key(layer, datatype)).second) {
    error(std::string{message} + ", layer=" + std::to_string(layer) + " type=" + std::to_string(datatype));
  }
}

std::optional<std::vector<PathPoint>> Reader::read_path(std::int64_t k, std::int64_t& rescaled) {
  rescaled = 1;
  std::optional<Record> record = next();
  if (!record) {
    error("EOF when reading path.");
    return std::nullopt;
  }
  if (record->type != static_cast<std::uint8_t>(RecordType::xy)) {
    unexpected(RecordType::xy, record->type);
    return std::nullopt;
  }
  non_manhattan_path_ = false;
  std::vector<PathPoint> points;
  const std::size_t count = record->data.size() / 8;
  points.reserve(k != 0 ? count : 0);
  for (std::size_t i = 0; i < count; ++i) {
    std::int64_t r = 1;
    PathPoint p;
    p.x = convert(*i4_at(record->data, 2 * i), k, r);
    multiply(points, r);
    rescaled *= r;
    p.y = convert(*i4_at(record->data, 2 * i + 1), k, r);
    multiply(points, r);
    p.x *= r;
    rescaled *= r;
    if (std::llabs(p.x) > kLargePoint || std::llabs(p.y) > kLargePoint) {
      error("Warning:  Very large point in path:  (" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")");
    }
    if (k == 0) {
      continue;
    }
    if (!points.empty() && points.back().x != p.x && points.back().y != p.y) {
      non_manhattan_path_ = true;
    }
    points.push_back(p);
  }
  if (non_manhattan_path_) {
    non_manhattan_ = true;
  }
  if (points.empty()) {
    return std::nullopt;
  }
  return points;
}

void Reader::paint_polygon(std::size_t cif_layer, const mask::Polygon& polygon, db::Rect* last_strip) {
  std::vector<mask::MaskPlane*>& current = planes();
  mask::MaskPlane* plane = cif_layer < current.size() ? current[cif_layer] : nullptr;
  const auto pieces = db::decompose_polygon(polygon);
  if (!pieces.ok()) {
    return;
  }
  bool strip_seen = false;
  for (const db::PolygonPiece& piece : pieces.value()) {
    if (!piece.triangle && !strip_seen) {
      strip_seen = true;
      if (last_strip != nullptr) {
        *last_strip = piece.area;  // Magic's rp: the strip found last, painted first
      }
    }
    if (plane == nullptr) {
      continue;
    }
    if (piece.triangle) {
      (void)plane->paint_triangle(piece.area, piece.triangle->diagonal, piece.triangle->right_side,
                                  compat_.magic_compat);
    } else {
      (void)plane->paint(piece.area);
    }
  }
}

db::CellDef* Reader::begin_subcell(const std::string& name) {
  const auto taken = def_init_.find(name);
  if (taken != def_init_.end() && taken->second != nullptr) {
    return nullptr;  // a structure of the file has that name: read into the current cell
  }
  db::CellDef* def = find_cell(name, nullptr, nullptr);
  if (def == nullptr) {
    return nullptr;
  }
  clear_definition(*def);
  def->flags().available = true;
  return def;
}

void Reader::end_subcell(db::CellDef& sub, db::CellDef* parent, const PlaneSet& saved) {
  engine_->paint_current(planes(), sub);
  db::recompute_bbox(sub, compat_.magic_compat);
  drc::check_this(editor_, sub, sub.bbox());
  db::mark_modified(sub);
  generate_ids(sub);
  restore_planes(saved);
  current_ = parent;
  if (parent != nullptr) {
    (void)place_use(*parent, sub, db::kIdentity, db::ArrayInfo{}, std::string{}, false);
  }
}

// --- BOUNDARY ---------------------------------------------------------------------

std::optional<Reader::LayerPair> Reader::read_layer_pair(RecordType datatype, std::string_view missing) {
  skip_set({RecordType::elflags, RecordType::plex});
  const std::optional<std::int32_t> layer = read_i2(RecordType::layer);
  const std::optional<std::int32_t> dt = layer ? read_i2(datatype) : std::nullopt;
  if (!layer || !dt) {
    if (!missing.empty()) {
      error(std::string{missing});
    }
    return std::nullopt;
  }
  return LayerPair{*layer, *dt};
}

void Reader::element_boundary() {
  const auto pair = read_layer_pair(RecordType::datatype, "Missing layer or datatype in boundary/box.");
  if (!pair) {
    return;
  }
  const std::optional<std::size_t> cif = engine_->style().calma_layer(pair->layer, pair->datatype);
  if (!cif) {
    layer_error("Unknown layer/datatype in boundary", pair->layer, pair->datatype);
  }
  std::int64_t rescaled = 1;
  const std::optional<std::vector<PathPoint>> points = read_path(cif ? 1 : 0, rescaled);
  if (!points) {
    if (cif) {
      error("Error while reading path for boundary/box; ignored.");
    }
    return;
  }
  mask::Polygon polygon;
  polygon.reserve(points->size() + 1);
  for (const PathPoint& p : *points) {
    polygon.push_back(db::Point{clamp_coord(p.x), clamp_coord(p.y)});
  }
  boundary_polygon(*cif, std::move(polygon));
}

void Reader::boundary_polygon(std::size_t cif_layer, mask::Polygon polygon) {
  db::CellDef* parent = current_;
  db::CellDef* sub = nullptr;
  PlaneSet saved;
  if (settings().polygon_subcells != PolygonSubcells::none && non_manhattan_path_) {
    char name[24];
    std::snprintf(name, sizeof name, "polygon%05lld", static_cast<long long>(++polygon_count_));
    sub = begin_subcell(name);
    if (sub != nullptr) {
      current_ = sub;
      saved = save_planes(true);
    }
  }
  if (polygon.back() != polygon.front()) {
    error("Boundary is not closed.");
    polygon.push_back(polygon.front());
  }
  const std::size_t edges = manhattan_edge_count(polygon);
  db::Rect last_strip;
  bool strip = false;
  if (edges >= 4) {
    last_strip = db::Rect{db::kPlaneMax, db::kPlaneMax, db::kPlaneMin, db::kPlaneMin};
    paint_polygon(cif_layer, polygon, &last_strip);
    strip = last_strip.x0 <= last_strip.x1;
  } else if (edges > 0) {
    messages_.cif_error(diag::make(kReadError, "polygon with fewer than 4 points."));
  }
  if (strip) {
    pin_shape(cif_layer, last_strip);
  }
  if (sub != nullptr) {
    end_subcell(*sub, parent, saved);
  }
}

// --- BOX --------------------------------------------------------------------------

void Reader::element_box() {
  const auto pair = read_layer_pair(RecordType::boxtype, "Missing layer or datatype in boundary/box.");
  if (!pair) {
    return;
  }
  const std::optional<std::size_t> cif = engine_->style().calma_layer(pair->layer, pair->datatype);
  if (!cif) {
    layer_error("Unknown layer/datatype in box", pair->layer, pair->datatype);
    return;
  }
  std::optional<Record> xy = next();
  if (!xy) {
    error("EOF when reading box.");
    return;
  }
  if (xy->type != static_cast<std::uint8_t>(RecordType::xy)) {
    unexpected(RecordType::xy, xy->type);
    return;
  }
  if (xy->data.size() / 8 != 5) {
    error("Box doesn't have 5 points.");
    return;
  }
  std::optional<std::array<std::int64_t, 4>> box;  // xbot, ybot, xtop, ytop
  for (std::size_t i = 0; i < 5; ++i) {
    std::int64_t r = 1;
    const std::int64_t x = convert(*i4_at(xy->data, 2 * i), 1, r);
    std::int64_t r2 = 1;
    std::int64_t y = convert(*i4_at(xy->data, 2 * i + 1), 1, r2);
    const std::int64_t factor = r * r2;
    if (box && factor > 1) {
      for (std::int64_t& v : *box) {
        v *= factor;
      }
    }
    const std::int64_t px = x * r2;
    if (!box) {
      box = std::array<std::int64_t, 4>{px, y, px, y};
      continue;
    }
    (*box)[0] = std::min((*box)[0], px);
    (*box)[1] = std::min((*box)[1], y);
    (*box)[2] = std::max((*box)[2], px);
    (*box)[3] = std::max((*box)[3], y);
  }
  std::vector<mask::MaskPlane*>& current = planes();
  if (*cif < current.size() && current[*cif] != nullptr) {
    (void)current[*cif]->paint(clamp_rect((*box)[0], (*box)[1], (*box)[2], (*box)[3]));
  }
}

// --- PATH ---------------------------------------------------------------------------

std::int32_t Reader::path_type() {
  std::int32_t type = 0;
  const std::optional<Record> next_record = peek();
  if (next_record && next_record->type == static_cast<std::uint8_t>(RecordType::pathtype)) {
    type = read_i2(RecordType::pathtype).value_or(0);
  }
  if (type == 0 || type == 2 || type == 4) {
    return type;
  }
  if (type == 1 && !compat_.magic_compat) {
    counted(diag::make_native(kCompatNote, "Warning: pathtype 1 (round) approximated by a half-width square "
                                           "extension (Q-MASK-113)."));
    return 2;
  }
  error("Warning: pathtype " + std::to_string(type) + " unsupported (ignored).");
  return 0;
}

std::optional<std::int64_t> Reader::path_width() {
  const std::optional<Record> next_record = peek();
  std::int64_t width = 0;
  if (next_record && next_record->type == static_cast<std::uint8_t>(RecordType::width)) {
    const std::optional<std::int32_t> value = read_i4(RecordType::width);
    if (!value) {
      error("Error in reading WIDTH in calmaElementPath()");
      return std::nullopt;
    }
    width = *value;
  }
  const ReadScale s = scale_->scale();
  if (width < 0 && !compat_.magic_compat) {
    // Q-MASK-115 OFF: |W| without the magnification of the enclosing reference.
    counted(diag::make_native(kCompatNote, "Warning: negative path width " + std::to_string(width) +
                                               " read as its absolute value (Q-MASK-115)."));
    const std::int64_t num = mul_div(s.num, mag_den_, 1);
    return mul_div(-width, num, mul_div(s.den, mag_num_, 1));
  }
  width *= s.num;
  if (width % s.den != 0) {
    error("Wire width snapped to nearest integer boundary.");
  }
  return width / s.den;
}

std::int64_t Reader::path_extension(RecordType type) {
  const std::optional<Record> next_record = peek();
  if (!next_record || next_record->type != static_cast<std::uint8_t>(type)) {
    return 0;
  }
  const std::optional<std::int32_t> value = read_i4(type);
  if (!value) {
    error(std::string{"Error in reading "} + (type == RecordType::bgnextn ? "BGNEXTN" : "ENDEXTN") +
          " in path (ignored)");
    return 0;
  }
  const ReadScale s = scale_->scale();
  std::int64_t e = std::int64_t{*value} * s.num;
  if (e % s.den != 0) {
    error("Wire extension snapped to nearest integer boundary.");
  }
  return e * 2 / s.den;
}

void Reader::element_path() {
  const auto pair = read_layer_pair(RecordType::datatype, {});
  if (!pair) {
    return;
  }
  const std::int32_t type = path_type();
  std::optional<std::int64_t> width = path_width();
  if (!width) {
    return;
  }
  std::int64_t begin = path_extension(RecordType::bgnextn);
  std::int64_t end = path_extension(RecordType::endextn);
  std::int64_t rescaled = 1;
  std::optional<std::vector<PathPoint>> points = read_path(2, rescaled);
  if (!points) {
    error("Improper path; ignored.");
    return;
  }
  *width *= rescaled;
  begin *= rescaled;
  end *= rescaled;
  extend_path(*points, begin, end, compat_.magic_compat);
  if (*width <= 0) {
    return;  // zero (and, with magic-compat ON, negative) widths are dropped
  }
  const std::optional<std::size_t> cif = engine_->style().calma_layer(pair->layer, pair->datatype);
  if (!cif) {
    layer_error("Unknown layer/datatype in path", pair->layer, pair->datatype);
    return;
  }
  path_geometry(*cif, *points, *width, type == 2);
}

void Reader::path_geometry(std::size_t cif_layer, const std::vector<PathPoint>& points, std::int64_t width,
                           bool endcap) {
  db::CellDef* parent = current_;
  db::CellDef* sub = nullptr;
  PlaneSet saved;
  if (settings().path_subcells) {
    char name[24];
    std::snprintf(name, sizeof name, "path%05lld", static_cast<long long>(++path_count_));
    sub = begin_subcell(name);
    if (sub != nullptr) {
      current_ = sub;
      saved = save_planes(true);
    }
  }
  if (settings().savepaths && current_ != nullptr) {
    record_path(*current_, points);
  }
  const PathOutline outline = path_outline(points, width, endcap, compat_.magic_compat);
  for (const std::string& warning : outline.warnings) {
    messages_.print(diag::make(kReadError, warning));
  }
  for (const mask::Polygon& polygon : outline.polygons) {
    paint_polygon(cif_layer, polygon, nullptr);
  }
  if (outline.truncated) {
    messages_.print(diag::make(kReadError, outline.mitre_message));
    messages_.print(diag::make(kReadError, "Route has been truncated."));
  }
  if (outline.out_of_range) {
    error("path outline outside the usable plane; ignored.");
  }
  if (sub != nullptr) {
    end_subcell(*sub, parent, saved);
  }
}

void Reader::record_path(db::CellDef& def, const std::vector<PathPoint>& points) {
  std::string name = "path";
  for (std::int64_t i = 0; def.properties().find(name) != nullptr; ++i) {
    name = "path_" + std::to_string(i);
  }
  const std::int64_t s = scale_factor();
  db::DimensionList values;
  values.values.reserve(points.size() * 2);
  const auto value = [&](std::int64_t v) {
    std::int64_t half = v >> 1;  // arithmetic shift: floor(v / 2)
    if (!compat_.magic_compat) {
      half = half >= 0 ? half / s : -((-half + s - 1) / s);  // Q-MASK-119 OFF: internal units
    }
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(half, INT32_MIN, INT32_MAX));
  };
  for (const PathPoint& p : points) {
    values.values.push_back(value(p.x));
    values.values.push_back(value(p.y));
  }
  db::set_property(def, name, db::PropertyValue{std::move(values)});
}

}  // namespace hefesto::gds::read_detail
