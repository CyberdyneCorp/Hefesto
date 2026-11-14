// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CIF parser's geometry commands: L (CIFParseLayer,
// cif/CIFrdpt.c:766-827), B (CIFParseBox, :68-177), R (CIFParseFlash,
// :179-237), P (CIFParsePoly, :829-872) and W (CIFParseWire, :705-764).
// Coordinates stay in read units; the engine converts them. Owner: CIF.

#include "hefesto/db/geometry.hpp"
#include "parser_impl.hpp"

namespace hefesto::cif::detail {

namespace {

bool in_plane(std::int64_t v) noexcept {
  return v >= db::kPlaneMin && v <= db::kPlaneMax;
}

bool in_plane(const ReadPoint& p) noexcept {
  return in_plane(p.x) && in_plane(p.y);
}

}  // namespace

bool Parser::geometry_allowed() const noexcept {
  // Magic's cifReadPlane != NULL; a definition dropped by the symbol cap
  // takes no geometry either.
  return layer_.has_value() && (!in_definition_ || current_.has_value());
}

void Parser::layer() {
  lex_.take();  // the 'L'
  const std::string name = lex_.layer_name();
  if (const auto index = style_.find(name)) {
    label_type_ = style_.layers[*index].label_type;
    layer_ = index;
  } else {
    layer_.reset();
    label_type_ = 0;  // TT_SPACE
    error("layer " + name + " isn't known in the current style.");
  }
  lex_.skip_to_semicolon();
}

void Parser::paint_rect(const Rect64& r, std::string_view element) {
  if (range_error_ || !in_plane(r.x0) || !in_plane(r.y0) || !in_plane(r.x1) || !in_plane(r.y1)) {
    outside_plane(element);
    return;
  }
  if (r.x0 >= r.x1 || r.y0 >= r.y1) {
    return;  // an empty rectangle paints nothing (DBPaintPlane)
  }
  check(target_.paint(*layer_, mask::ReadShape{db::Rect{static_cast<db::Coord>(r.x0), static_cast<db::Coord>(r.y0),
                                                        static_cast<db::Coord>(r.x1), static_cast<db::Coord>(r.y1)}}));
}

void Parser::paint_polygon(const std::vector<ReadPoint>& points, std::string_view element) {
  mask::Polygon polygon;
  polygon.reserve(points.size());
  for (const ReadPoint& p : points) {
    if (range_error_ || !in_plane(p)) {
      outside_plane(element);
      return;
    }
    polygon.push_back(db::Point{static_cast<db::Coord>(p.x), static_cast<db::Coord>(p.y)});
  }
  check(target_.paint(*layer_, mask::ReadShape{std::move(polygon)}));
}

// --- B ----------------------------------------------------------------------

std::optional<Rect64> Parser::box_extent() {
  // Length and width are parsed as a point so that they share its scaling.
  auto size = point(1);
  if (!size) {
    error("box, but no length and/or width; ignored.");
    return std::nullopt;
  }
  if (size->x <= 0) {
    error("box length not strictly positive; ignored.");
    return std::nullopt;
  }
  if (size->y <= 0) {
    error("box width not strictly positive; ignored.");
    return std::nullopt;
  }
  const std::int64_t saved = scale_n_;
  const auto center = point(2);  // doubled, so a half-unit center is exact
  if (!center) {
    error("box, but no center; ignored.");
    return std::nullopt;
  }
  if (saved != scale_n_) {
    *size = ReadPoint{mul(size->x, scale_n_ / saved), mul(size->y, scale_n_ / saved)};
  }
  Rect64 r{-size->x, -size->y, size->x, size->y};
  if (const auto dx = lex_.signed_integer()) {
    const auto dy = lex_.signed_integer();
    if (!dy) {
      error("box, direction botched; box ignored.");
      return std::nullopt;
    }
    r = rotate(direction(*dx, *dy), r);
  }
  // Offset by the doubled center only now that the rotation is done.
  return Rect64{r.x0 + center->x, r.y0 + center->y, r.x1 + center->x, r.y1 + center->y};
}

Rect64 Parser::rotate(const Transform64& t, const Rect64& r) {
  const std::int64_t xa = t.a * r.x0 + t.b * r.y0;
  const std::int64_t ya = t.d * r.x0 + t.e * r.y0;
  const std::int64_t xb = t.a * r.x1 + t.b * r.y1;
  const std::int64_t yb = t.d * r.x1 + t.e * r.y1;
  return Rect64{std::min(xa, xb), std::min(ya, yb), std::max(xa, xb), std::max(ya, yb)};
}

void Parser::box() {
  lex_.take();  // the 'B'
  if (!geometry_allowed()) {
    lex_.skip_to_semicolon();
    return;
  }
  const auto doubled = box_extent();
  if (!doubled) {
    lex_.skip_to_semicolon();
    return;
  }
  const Rect64& d = *doubled;
  const bool odd = ((d.x0 | d.y0 | d.x1 | d.y1) & 1) != 0;
  if (odd && !options_.magic_compat && !range_error_) {
    // Q-MASK-315 OFF: subdivide the read units by 2 so the box is exact; the
    // doubled coordinates are then the coordinates in the new units.
    if (options_.rescale_allowed && mul(scale_n_, 2) <= rescale_limit()) {
      static_cast<void>(rescale(2));
      paint_rect(d, "box");
      return;
    }
    // No subdivision: round outward, so the box contains the exact one.
    // The doubled values stay within +-2^62, so the negation cannot overflow.
    warning("CIF units at maximum scale; value is rounded");
    paint_rect(Rect64{d.x0 >> 1, d.y0 >> 1, -((-d.x1) >> 1), -((-d.y1) >> 1)}, "box");
    return;
  }
  // Q-MASK-315 ON: C division truncates toward zero, as Magic does.
  paint_rect(Rect64{d.x0 / 2, d.y0 / 2, d.x1 / 2, d.y1 / 2}, "box");
}

// --- R ----------------------------------------------------------------------

void Parser::flash() {
  lex_.take();  // the 'R'
  if (!geometry_allowed()) {
    lex_.skip_to_semicolon();
    return;
  }
  const auto diameter_in = unsigned_integer();
  if (!diameter_in) {
    error("roundflash, but no diameter; ignored.");
    lex_.skip_to_semicolon();
    return;
  }
  std::int64_t diameter = mul(*diameter_in, scale_n_);
  if (diameter % scale_d_ != 0) {
    warning("Roundflash diameter snapped to nearest integer boundary.");
  }
  diameter /= scale_d_;
  const std::int64_t saved = scale_n_;
  const auto center = point(2);
  if (!center) {
    error("roundflash, but no center; ignored.");
    lex_.skip_to_semicolon();
    return;
  }
  if (saved != scale_n_) {
    diameter = mul(diameter, scale_n_ / saved);
  }
  // Q-MASK-316 (replicate): the flash is its bounding square.
  paint_rect(Rect64{(center->x - diameter) / 2, (center->y - diameter) / 2, (center->x + diameter) / 2,
                    (center->y + diameter) / 2},
             "round flash");
}

// --- P ----------------------------------------------------------------------

void Parser::polygon() {
  lex_.take();  // the 'P'
  if (!geometry_allowed()) {
    lex_.skip_to_semicolon();
    return;
  }
  const auto points = path(1);
  if (!points) {
    error("polygon, but improper path; ignored.");
    lex_.skip_to_semicolon();
    return;
  }
  paint_polygon(*points, "polygon");
}

// --- W ----------------------------------------------------------------------

void Parser::wire() {
  lex_.take();  // the 'W'
  if (!geometry_allowed()) {
    lex_.skip_to_semicolon();
    return;
  }
  const auto width_in = unsigned_integer();
  if (!width_in) {
    error("wire, but no width; ignored.");
    lex_.skip_to_semicolon();
    return;
  }
  std::int64_t width = mul(*width_in, scale_n_);
  if (width % scale_d_ != 0) {
    warning("Wire width snapped to nearest integer boundary.");
  }
  width /= scale_d_;
  const std::int64_t saved = scale_n_;
  const auto points = path(2);
  if (!points) {
    error("wire, but improper path; ignored.");
    lex_.skip_to_semicolon();
    return;
  }
  if (saved != scale_n_) {
    width = mul(width, scale_n_ / saved);
  }
  paint_wire(*points, width);
}

void Parser::paint_wire(const std::vector<ReadPoint>& points, std::int64_t width) {
  if (range_error_) {
    outside_plane("wire");
    return;
  }
  const WireOutline outline = wire_outline(points, width);
  for (const std::string& text : outline.warnings) {
    messages_.console(read_codes::kWireWarning, text);
  }
  if (outline.truncated) {
    messages_.console(read_codes::kWireMitre, outline.mitre_message);
    messages_.console(read_codes::kWireMitre, "Route has been truncated.");
  }
  if (outline.out_of_range) {
    outside_plane("wire");
    return;
  }
  for (const mask::Polygon& polygon : outline.polygons) {
    check(target_.paint(*layer_, mask::ReadShape{polygon}));
  }
}

}  // namespace hefesto::cif::detail
