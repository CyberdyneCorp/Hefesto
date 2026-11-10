// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Euclidean corner filter and derived contact borders of the basic check
// (drc-rule-semantics "Euclidean corner filter"; design D3/D6).
//
// (Magic ref: drc/DRCbasic.c drcCifPointToSegment, drcRadialClear,
// drcDerivedBorder, areaCheck (RADIAL_* tests); drc/DRCbasic.c drcTile
// (diagonal offset))
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/check/euclidean.hpp"

#include <cmath>

#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/tech/technology.hpp"
#include "kernel.hpp"

namespace hefesto::drc {

std::int64_t point_to_segment_squared(std::int32_t px, std::int32_t py, std::int32_t s1x, std::int32_t s1y,
                                      std::int32_t s2x, std::int32_t s2y) noexcept {
  std::int64_t x = std::int64_t{s2x} - s1x;
  std::int64_t y = std::int64_t{s2y} - s1y;
  const std::int64_t c = x * x + y * y;
  x = std::int64_t{px} - s1x;
  y = std::int64_t{py} - s1y;
  const std::int64_t a = x * x + y * y;
  x = std::int64_t{px} - s2x;
  y = std::int64_t{py} - s2y;
  const std::int64_t b = x * x + y * y;
  const std::int64_t frac = a - b;
  if (frac >= c) {
    return b;
  }
  if (-frac >= c) {
    return a;
  }
  // protod = (float)(c + a - b); a - (long)((protod * protod) / (float)(c << 2))
  const auto protod = static_cast<float>(c + a - b);
  const float square = protod * protod;
  const float quotient = square / static_cast<float>(c * 4);
  return a - static_cast<std::int64_t>(quotient);
}

bool radial_clear(std::int32_t sqx, std::int32_t sqy, std::int32_t s, std::int32_t border_num,
                  std::int32_t border_den) noexcept {
  const std::int64_t dx = std::int64_t{sqx} * border_den + border_num;
  const std::int64_t dy = std::int64_t{sqy} * border_den + border_num;
  const std::int64_t dr = std::int64_t{s} * border_den + border_num;
  return dx * dx + dy * dy >= dr * dr;
}

namespace {

/// Magic's 32-bit int product (wraps like the x86_64 build).
std::int32_t int_mul(std::int32_t a, std::int32_t b) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

}  // namespace

DiagonalOffset diagonal_offset(std::int32_t dist, std::int32_t width, std::int32_t height) noexcept {
  if (height == 0) {
    return {};
  }
  const double ww = static_cast<double>(int_mul(width, width));
  const double hh = static_cast<double>(int_mul(height, height));
  const double r = 1.0 / (1.0 + (ww / hh));
  const double scaled = static_cast<double>(dist) * std::sqrt(r);
  const auto dx = static_cast<std::int32_t>(scaled);
  return DiagonalOffset{dx, int_mul(dx, width) / height};
}

}  // namespace hefesto::drc

namespace hefesto::drc::check_detail {

namespace {

/// One corner test of areaCheck: the corner point P of the check region
/// and the offsets of the candidate from it, plus the split-tile fallback.
struct Corner {
  unsigned mark;
  bool west;    // P.x = E.xbot + s (else E.xtop - s); candidate lies west of P
  bool north;   // P.y = E.ytop - s (else E.ybot + s); candidate lies north of P
  db::Diagonal diagonal;  // split-tile fallback applies to this diagonal ...
  bool right_side;        // ... and side
  bool rising;            // segment (L,B)-(R,T) (else (L,T)-(R,B))
};

constexpr Corner kCorners[] = {
    {radial::kNW, true, true, db::Diagonal::ne_sw, false, true},
    {radial::kNE, false, true, db::Diagonal::nw_se, true, false},
    {radial::kSW, true, false, db::Diagonal::nw_se, false, false},
    {radial::kSE, false, false, db::Diagonal::ne_sw, true, true},
};

bool corner_exempt(const Corner& corner, const TileView& found, const Rect& r, const Search& s,
                   const ContactBorder& border) {
  const Rect& e = s.region;
  const std::int32_t px = corner.west ? e.x0 + s.sdist : e.x1 - s.sdist;
  const std::int32_t py = corner.north ? e.y1 - s.sdist : e.y0 + s.sdist;
  const std::int32_t sqx = corner.west ? px - r.x1 : r.x0 - px;
  const std::int32_t sqy = corner.north ? r.y0 - py : py - r.y1;
  if (sqx >= 0 && sqy >= 0 && radial_clear(sqx, sqy, s.sdist, border.num, border.den)) {
    return true;
  }
  if (!found.split || found.diagonal != corner.diagonal || found.right_side != corner.right_side) {
    return false;
  }
  const Rect& t = found.area;
  const std::int64_t d2 = corner.rising ? point_to_segment_squared(px, py, t.x0, t.y0, t.x1, t.y1)
                                        : point_to_segment_squared(px, py, t.x0, t.y1, t.x1, t.y0);
  return d2 > std::int64_t{s.sdist} * s.sdist;
}

}  // namespace

bool Kernel::radial_exempt(const TileView& found, const Rect& area, const Search& search) const {
  ContactBorder border;
  if ((search.rule->flags & (flags::kDerivedEdge | flags::kDerivedTarget)) != 0) {
    border = derived_border(found, search);
  }
  for (const Corner& corner : kCorners) {
    if ((search.marks & corner.mark) != 0 && corner_exempt(corner, found, area, search, border)) {
      return true;
    }
  }
  return false;
}

ContactBorder Kernel::derived_border(const TileView& found, const Search& search) const {
  ContactBorder border{0, 1};
  if (input_.cif == nullptr) {
    return border;
  }
  const Rule& rule = *search.rule;
  if (rule.has(flags::kDerivedEdge) && search.initial != nullptr && !search.initial->split) {
    if (const auto edge = input_.cif->contact_border(body_type(*search.initial))) {
      border = *edge;
    }
  }
  if (!rule.has(flags::kDerivedTarget) || found.split || input_.technology == nullptr) {
    return border;
  }
  const TileType type = body_type(found);
  if (!input_.technology->is_contact(type)) {
    return border;
  }
  const TileType residue = input_.technology->residue_on(type, rule.check_plane);
  if (residue == tech::kSpace || !rule.ok.test(residue)) {
    return border;
  }
  if (const auto target = input_.cif->contact_border(type)) {
    border.num += target->num;
    border.den = target->den;
  }
  return border;
}

}  // namespace hefesto::drc::check_detail
