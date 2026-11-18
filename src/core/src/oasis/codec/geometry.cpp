// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Element outlines (capability oasis-format "RECTANGLE record", "POLYGON
// record", "TRAPEZOID records", "CTRAPEZOID record"; design D11,
// Q-OASIS-4): the trapezoid vertex formulas and the 26-type CTRAPEZOID
// table, which match KLayout (dbOASISReader.cc ctraps_table) and gdstk
// (library.cpp CTRAPEZOID). Vertices are computed in 128 bits and an
// outline with a vertex outside 64 bits is empty.
//
// Owner: OASFMT (design D2).

#include <array>
#include <span>
#include <limits>

#include "hefesto/oasis/codec/elements.hpp"

namespace hefesto::oasis {

namespace {

__extension__ typedef __int128 Wide;  // NOLINT(modernize-use-using): __extension__ needs the typedef form

/// A coordinate cw * w + ch * h.
struct Linear {
  std::int8_t w = 0;
  std::int8_t h = 0;
};

struct Vertex {
  Linear x;
  Linear y;
};

constexpr Linear Z{0, 0};
constexpr Linear W{1, 0};
constexpr Linear H{0, 1};
constexpr Linear WmH{1, -1};
constexpr Linear HmW{-1, 1};
constexpr Linear W2{2, 0};
constexpr Linear H2{0, 2};

/// The CTRAPEZOID vertices of each type (oasis-format "CTRAPEZOID record");
/// the triangles repeat their last vertex, which the merge removes.
constexpr std::array<std::array<Vertex, 4>, 26> kCtrapezoids = {{
    {{{Z, Z}, {Z, H}, {WmH, H}, {W, Z}}},    // 0
    {{{Z, Z}, {Z, H}, {W, H}, {WmH, Z}}},    // 1
    {{{Z, Z}, {H, H}, {W, H}, {W, Z}}},      // 2
    {{{H, Z}, {Z, H}, {W, H}, {W, Z}}},      // 3
    {{{Z, Z}, {H, H}, {WmH, H}, {W, Z}}},    // 4
    {{{H, Z}, {Z, H}, {W, H}, {WmH, Z}}},    // 5
    {{{Z, Z}, {H, H}, {W, H}, {WmH, Z}}},    // 6
    {{{H, Z}, {Z, H}, {WmH, H}, {W, Z}}},    // 7
    {{{Z, Z}, {Z, H}, {W, HmW}, {W, Z}}},    // 8
    {{{Z, Z}, {Z, HmW}, {W, H}, {W, Z}}},    // 9
    {{{Z, Z}, {Z, H}, {W, H}, {W, W}}},      // 10
    {{{Z, W}, {Z, H}, {W, H}, {W, Z}}},      // 11
    {{{Z, Z}, {Z, H}, {W, HmW}, {W, W}}},    // 12
    {{{Z, W}, {Z, HmW}, {W, H}, {W, Z}}},    // 13
    {{{Z, Z}, {Z, HmW}, {W, H}, {W, W}}},    // 14
    {{{Z, W}, {Z, H}, {W, HmW}, {W, Z}}},    // 15
    {{{Z, Z}, {Z, W}, {W, Z}, {W, Z}}},      // 16
    {{{Z, Z}, {Z, W}, {W, W}, {W, W}}},      // 17
    {{{Z, Z}, {W, W}, {W, Z}, {W, Z}}},      // 18
    {{{Z, W}, {W, W}, {W, Z}, {W, Z}}},      // 19
    {{{Z, Z}, {H, H}, {H2, Z}, {H2, Z}}},    // 20
    {{{Z, H}, {H2, H}, {H, Z}, {H, Z}}},     // 21
    {{{Z, Z}, {Z, W2}, {W, W}, {W, W}}},     // 22
    {{{W, Z}, {Z, W}, {W, W2}, {W, W2}}},    // 23
    {{{Z, Z}, {Z, H}, {W, H}, {W, Z}}},      // 24
    {{{Z, Z}, {Z, W}, {W, W}, {W, Z}}},      // 25
}};

bool fits64(Wide v) noexcept {
  return v <= std::numeric_limits<std::int64_t>::max() && v >= std::numeric_limits<std::int64_t>::min();
}

struct WidePoint {
  Wide x = 0;
  Wide y = 0;
};

/// The outline of wide vertices, consecutive coincident vertices merged
/// (the last against the first too); empty when one does not fit.
std::vector<Point64> merged(std::span<const WidePoint> wide) {
  std::vector<Point64> out;
  for (const WidePoint& v : wide) {
    if (!fits64(v.x) || !fits64(v.y)) {
      return {};
    }
    const Point64 p{static_cast<std::int64_t>(v.x), static_cast<std::int64_t>(v.y)};
    if (out.empty() || out.back() != p) {
      out.push_back(p);
    }
  }
  while (out.size() > 1 && out.back() == out.front()) {
    out.pop_back();
  }
  return out;
}

std::vector<Point64> trapezoid(const Element& e) {
  const Wide w = e.size.x;
  const Wide h = e.size.y;
  const Wide a = e.delta_a;
  const Wide b = e.delta_b;
  const Wide amax = a > 0 ? a : 0;
  const Wide amin = a < 0 ? a : 0;
  const Wide bmax = b > 0 ? b : 0;
  const Wide bmin = b < 0 ? b : 0;
  if (e.vertical) {
    const std::array<WidePoint, 4> v{{{0, amax}, {0, h + bmin}, {w, h - bmax}, {w, -amin}}};
    return merged(v);
  }
  const std::array<WidePoint, 4> v{{{amax, h}, {w + bmin, h}, {w - bmax, 0}, {-amin, 0}}};
  return merged(v);
}

std::vector<Point64> ctrapezoid(const Element& e) {
  if (e.ctrapezoid_type >= kCtrapezoids.size()) {
    return {};
  }
  const Wide w = e.size.x;
  const Wide h = e.size.y;
  const auto value = [w, h](Linear c) { return c.w * w + c.h * h; };
  std::array<WidePoint, 4> v{};
  for (std::size_t i = 0; i < v.size(); ++i) {
    const Vertex& t = kCtrapezoids[e.ctrapezoid_type][i];
    v[i] = {value(t.x), value(t.y)};
  }
  return merged(v);
}

}  // namespace

bool ctrapezoid_uses_width(std::uint32_t type) noexcept {
  return type <= 25 && type != 20 && type != 21;
}

bool ctrapezoid_uses_height(std::uint32_t type) noexcept {
  return type <= 15 || type == 20 || type == 21 || type == 24;
}

std::vector<Point64> outline(const Element& element) {
  switch (element.kind) {
    case ElementKind::rectangle: {
      const Point64 s = element.size;
      return {{0, 0}, {s.x, 0}, {s.x, s.y}, {0, s.y}};
    }
    case ElementKind::polygon: {
      std::vector<Point64> vertices{Point64{}};
      vertices.insert(vertices.end(), element.points.begin(), element.points.end());
      return vertices;
    }
    case ElementKind::trapezoid:
      return trapezoid(element);
    case ElementKind::ctrapezoid:
      return ctrapezoid(element);
    default:
      return {};
  }
}

}  // namespace hefesto::oasis
