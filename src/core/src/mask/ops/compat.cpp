// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Compat branch points and pinned float paths of the operator kernels
// (capability mask-output-operators; design D6, D22).
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (cifGrowGridFunc,
// cifGrowEuclideanFunc, cifGrowMinFunc, GetExpandedAreaGrid), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include "hefesto/mask/ops/compat.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace hefesto::mask {

namespace {

db::Coord clamp_coord(std::int64_t v) noexcept {
  return static_cast<db::Coord>(std::clamp<std::int64_t>(v, db::kMinusInfinity, db::kInfinity));
}

/// A 32-bit int product with C's wrap-around (Magic's `int * int`).
std::int32_t mul32(std::int32_t a, std::int32_t b) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

/// (int) ceilf(x) for a float argument, as Magic writes it.
std::int32_t ceil_int(float x) noexcept { return static_cast<std::int32_t>(std::ceil(x)); }

/// The sqrtf argument of GetExpandedAreaGrid, clamped to 0 (Q-MASK-227).
float clamped_sqrtf(std::int32_t b) noexcept { return std::sqrt(static_cast<float>(std::max<std::int32_t>(b, 0))); }

/// GetExpandedAreaGrid's expansion across the gap of length `along` (the
/// gap's width when horizontal) and the other dimension `across`: returns
/// {expansion along, expansion across}.
struct Expansion {
  std::int32_t along = 0;
  std::int32_t across = 0;
};

Expansion expansion(std::int32_t w, std::int32_t along, std::int32_t across, bool overlap, bool space) noexcept {
  const std::int32_t a = w - along;
  Expansion e;
  e.along = ceil_int(static_cast<float>(static_cast<double>(static_cast<float>(a)) / 2.0));
  const std::int32_t grown = (overlap || space) ? along + e.along : e.along;
  const std::int32_t b = mul32(w, w) - mul32(grown, grown);
  e.across = (space && !overlap) ? ceil_int(clamped_sqrtf(b) - static_cast<float>(across)) : ceil_int(clamped_sqrtf(b));
  const std::int32_t b2 = mul32(w, w) - mul32(along, along);
  const std::int32_t across2 =
      ceil_int(static_cast<float>(static_cast<double>(clamped_sqrtf(b2) - static_cast<float>(across)) / 2.0));
  e.across = std::max(e.across, across2);
  return e;
}

/// GetExpandedAreaGrid's grid snap: lower edges down, upper edges up.
db::Coord snap_low(db::Coord v, std::int32_t limit) noexcept {
  const std::int32_t delta = v % limit;
  if (delta > 0) {
    return v - delta;
  }
  return delta < 0 ? v - (limit + delta) : v;
}

db::Coord snap_high(db::Coord v, std::int32_t limit) noexcept {
  const std::int32_t delta = v % limit;
  if (delta > 0) {
    return v + (limit - delta);
  }
  return delta < 0 ? v - delta : v;
}

/// One side of grow-grid with magic-compat ON (cifGrowGridFunc).
db::Coord grid_low_on(db::Coord v, std::int32_t g) noexcept { return v > db::kPlaneMin ? v - (std::abs(v) % g) : v; }
db::Coord grid_high_on(db::Coord v, std::int32_t g) noexcept { return v < db::kPlaneMax ? v + (std::abs(v) % g) : v; }
db::Coord grid_low_off(db::Coord v, std::int32_t g) noexcept {
  return v > db::kPlaneMin ? clamp_coord(div_floor(v, g) * g) : v;
}
db::Coord grid_high_off(db::Coord v, std::int32_t g) noexcept {
  return v < db::kPlaneMax ? clamp_coord(div_ceil(v, g) * g) : v;
}

}  // namespace

db::Rect divide_rect(const db::Rect& r, std::int32_t scale, bool outward) noexcept {
  if (outward) {
    return db::Rect{clamp_coord(div_floor(r.x0, scale)), clamp_coord(div_floor(r.y0, scale)),
                    clamp_coord(div_ceil(r.x1, scale)), clamp_coord(div_ceil(r.y1, scale))};
  }
  return db::Rect{r.x0 / scale, r.y0 / scale, r.x1 / scale, r.y1 / scale};
}

db::Rect q_mask_1_grow_grid(const db::Rect& r, std::int32_t g, const compat::CompatSettings& c) noexcept {
  if (c.magic_compat) {
    return db::Rect{grid_low_on(r.x0, g), grid_low_on(r.y0, g), grid_high_on(r.x1, g), grid_high_on(r.y1, g)};
  }
  return db::Rect{grid_low_off(r.x0, g), grid_low_off(r.y0, g), grid_high_off(r.x1, g), grid_high_off(r.y1, g)};
}

bool q_mask_2_below(std::int64_t total, const CloseThreshold& t, const compat::CompatSettings& c) noexcept {
  if (c.magic_compat) {
    return total < t.scaled;
  }
  // total·L² < declared·D², in 64 bits (saturating long double is avoided:
  // the factors are small unit ratios).
  const std::int64_t lhs = total * t.reduce * t.reduce;
  const std::int64_t rhs = t.declared * t.multiply * t.multiply;
  return lhs < rhs;
}

db::Rect q_mask_5_limit_search_area(const db::Rect& mask_area, std::int32_t scale,
                                    const compat::CompatSettings& c) noexcept {
  return c.magic_compat ? mask_area : divide_rect(mask_area, scale, true);
}

std::int64_t q_mask_5_tile_edge(std::int64_t edge, std::int32_t scale, const compat::CompatSettings& c) noexcept {
  return c.magic_compat ? edge : edge * scale;
}

db::Rect q_mask_6_label_area(const db::Rect& label, std::int32_t scale, const compat::CompatSettings& c) noexcept {
  if (c.magic_compat) {
    return label;
  }
  const auto mul = [scale](db::Coord v) { return clamp_coord(std::int64_t{v} * scale); };
  return db::Rect{mul(label.x0), mul(label.y0), mul(label.x1), mul(label.y1)};
}

db::Rect q_mask_21_to_internal(const db::Rect& r, std::int32_t scale, const compat::CompatSettings& c) noexcept {
  return divide_rect(r, scale, !c.magic_compat);
}

db::Rect q_mask_232_to_internal(const db::Rect& r, std::int32_t scale, const compat::CompatSettings& c) noexcept {
  return divide_rect(r, scale, !c.magic_compat);
}

EuclideanOffsets euclidean_offsets(std::int32_t d, std::int32_t width, std::int32_t height) noexcept {
  EuclideanOffsets out;
  double ratio = static_cast<double>(height) / static_cast<double>(width);
  double frac = ratio / (1 + std::sqrt(1 + ratio * ratio));
  out.dx = static_cast<std::int32_t>(std::ceil(static_cast<double>(d) * frac));
  ratio = static_cast<double>(width) / static_cast<double>(height);
  frac = ratio / (1 + std::sqrt(1 + ratio * ratio));
  out.dy = static_cast<std::int32_t>(std::ceil(static_cast<double>(d) * frac));
  return out;
}

std::int32_t grow_min_height(std::int32_t g, std::int32_t width) noexcept {
  const std::int32_t gw = g + width;
  const double arg = static_cast<double>(mul32(g, g)) - 0.25 * static_cast<double>(mul32(gw, gw)) + 0.5;
  return static_cast<std::int32_t>(std::sqrt(arg));
}

db::Rect bridge_expand(const db::Rect& r, std::int32_t w, bool space, std::int32_t grid_limit) noexcept {
  bool overlap = false;
  bool horiz = false;
  if (r.x0 > r.x1) {
    overlap = true;
    horiz = !space;
  }
  if (r.y0 > r.y1) {
    overlap = true;
    horiz = space;
  }
  db::Rect q = db::canonical(r);
  const auto width = static_cast<std::int32_t>(q.width());
  const auto height = static_cast<std::int32_t>(q.height());
  if (!overlap) {
    horiz = width > height;
  }
  std::int32_t dx = 0;
  std::int32_t dy = 0;
  if (horiz) {
    const Expansion e = expansion(w, width, height, overlap, space);
    dx = e.along;
    dy = e.across;
  } else {
    const Expansion e = expansion(w, height, width, overlap, space);
    dy = e.along;
    dx = e.across;
  }
  q = db::Rect{q.x0 - dx, q.y0 - dy, q.x1 + dx, q.y1 + dy};
  if (grid_limit > 1) {
    q = db::Rect{snap_low(q.x0, grid_limit), snap_low(q.y0, grid_limit), snap_high(q.x1, grid_limit),
                 snap_high(q.y1, grid_limit)};
  }
  return q;
}

}  // namespace hefesto::mask
