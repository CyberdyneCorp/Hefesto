// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fracturing of a polygon with more vertices than a GDS BOUNDARY holds
// (capability oasis-read "Input limits"; D1 of the user decisions of
// 2026-11-19, security review M6-S3). The outline is cut in two by an
// axis-parallel line through the median vertex coordinate, along the axis
// whose cut leaves the smaller larger half, and each half is cut again
// until it fits. A half is the polygon clipped to a closed half-plane
// (Sutherland-Hodgman), so the two halves share the cut points and their
// union is the polygon: where the cut line leaves and re-enters the
// polygon the half gets a zero-width edge pair along the line, which
// nonzero-winding fill (gds-read) paints as nothing. A cut point on a
// non-axis-parallel edge is rounded half away from zero to the unit grid,
// the same point in both halves. Collinear and repeated vertices left by
// the cuts are removed. An outline that a cut does not shrink (all its
// vertices on a few lines), one wider or taller than 2^32 (it cannot fit
// a GDS-equivalent element anyway), or a recursion deeper than kMaxDepth
// is not fractured.
//
// Owner: OASR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <algorithm>
#include <cstdint>
#include <limits>

#include "transcoder.hpp"

namespace hefesto::oasis::read_detail {

namespace {

using Ring = std::vector<Point64>;
using Wide = __int128;

constexpr int kMaxDepth = 64;
constexpr std::int64_t kMaxSpan = std::int64_t{1} << 32;

enum class Axis : std::uint8_t { x, y };

std::int64_t along(const Point64& p, Axis axis) {
  return axis == Axis::x ? p.x : p.y;
}

std::int64_t across(const Point64& p, Axis axis) {
  return axis == Axis::x ? p.y : p.x;
}

Point64 make(Axis axis, std::int64_t a, std::int64_t b) {
  return axis == Axis::x ? Point64{a, b} : Point64{b, a};
}

/// round(n / d), half away from zero; d != 0.
std::int64_t rounded_quotient(Wide n, Wide d) {
  if (d < 0) {
    n = -n;
    d = -d;
  }
  const Wide half = d / 2;
  const Wide q = n >= 0 ? (n + half) / d : -((-n + half) / d);
  return static_cast<std::int64_t>(q);
}

/// The point of edge p-q on the line `along == c` (p and q on either side).
Point64 cut_point(const Point64& p, const Point64& q, Axis axis, std::int64_t c) {
  const Wide pa = along(p, axis);
  const Wide pb = across(p, axis);
  const Wide da = static_cast<Wide>(along(q, axis)) - pa;
  const Wide db = static_cast<Wide>(across(q, axis)) - pb;
  const std::int64_t b = static_cast<std::int64_t>(pb) + rounded_quotient(db * (c - pa), da);
  return make(axis, c, b);
}

/// The polygon clipped to `along <= c` (low) or `along >= c` (high).
Ring clip(const Ring& ring, Axis axis, std::int64_t c, bool low) {
  const auto inside = [&](const Point64& p) { return low ? along(p, axis) <= c : along(p, axis) >= c; };
  Ring out;
  out.reserve(ring.size() / 2 + 8);
  for (std::size_t i = 0; i < ring.size(); ++i) {
    const Point64& p = ring[i];
    const Point64& q = ring[(i + 1) % ring.size()];
    const bool p_in = inside(p);
    const bool q_in = inside(q);
    if (p_in != q_in && along(p, axis) != c && along(q, axis) != c) {
      out.push_back(cut_point(p, q, axis, c));
    }
    if (q_in) {
      out.push_back(q);
    }
  }
  return out;
}

bool collinear(const Point64& a, const Point64& b, const Point64& c) {
  const Wide cross = static_cast<Wide>(b.x - a.x) * (c.y - a.y) - static_cast<Wide>(b.y - a.y) * (c.x - a.x);
  return cross == 0;
}

/// `ring` without repeated and collinear vertices.
Ring simplified(const Ring& ring) {
  Ring out;
  out.reserve(ring.size());
  for (const Point64& p : ring) {
    if (!out.empty() && out.back() == p) {
      continue;
    }
    while (out.size() >= 2 && collinear(out[out.size() - 2], out.back(), p)) {
      out.pop_back();
    }
    out.push_back(p);
  }
  std::size_t first = 0;
  while (out.size() - first >= 3) {
    if (out.back() == out[first] || collinear(out[out.size() - 2], out.back(), out[first])) {
      out.pop_back();
    } else if (collinear(out.back(), out[first], out[first + 1])) {
      ++first;
    } else {
      break;
    }
  }
  return Ring(out.begin() + static_cast<std::ptrdiff_t>(first), out.end());
}

/// The median vertex coordinate along `axis`.
std::int64_t median(const Ring& ring, Axis axis) {
  std::vector<std::int64_t> values;
  values.reserve(ring.size());
  for (const Point64& p : ring) {
    values.push_back(along(p, axis));
  }
  const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
  std::nth_element(values.begin(), middle, values.end());
  return *middle;
}

struct Split {
  Ring low;
  Ring high;
  [[nodiscard]] std::size_t larger() const noexcept { return std::max(low.size(), high.size()); }
};

Split split(const Ring& ring, Axis axis) {
  const std::int64_t c = median(ring, axis);
  return Split{simplified(clip(ring, axis, c, true)), simplified(clip(ring, axis, c, false))};
}

bool fits_span(std::span<const Point64> ring) {
  const auto [min_x, max_x] = std::minmax_element(ring.begin(), ring.end(), [](auto a, auto b) { return a.x < b.x; });
  const auto [min_y, max_y] = std::minmax_element(ring.begin(), ring.end(), [](auto a, auto b) { return a.y < b.y; });
  return static_cast<Wide>(max_x->x) - min_x->x <= kMaxSpan && static_cast<Wide>(max_y->y) - min_y->y <= kMaxSpan;
}

class Fracturer {
 public:
  explicit Fracturer(std::size_t max_vertices) : max_(max_vertices) {}

  bool run(Ring ring, int depth) {
    if (ring.size() < 3) {
      return true;  // nothing left to paint
    }
    if (ring.size() <= max_) {
      pieces_.push_back(std::move(ring));
      return true;
    }
    if (depth >= kMaxDepth) {
      return false;
    }
    Split best = split(ring, Axis::x);
    Split other = split(ring, Axis::y);
    if (other.larger() < best.larger()) {
      best = std::move(other);
    }
    if (best.larger() >= ring.size()) {
      return false;  // the cut does not shrink it
    }
    ring = {};
    return run(std::move(best.low), depth + 1) && run(std::move(best.high), depth + 1);
  }

  [[nodiscard]] std::vector<Ring> take() { return std::move(pieces_); }

 private:
  std::size_t max_;
  std::vector<Ring> pieces_;
};

}  // namespace

std::optional<std::vector<std::vector<Point64>>> fracture(std::span<const Point64> ring, std::size_t max_vertices) {
  if (max_vertices < 4 || ring.empty() || !fits_span(ring)) {
    return std::nullopt;
  }
  Fracturer f(max_vertices);
  if (!f.run(Ring(ring.begin(), ring.end()), 0)) {
    return std::nullopt;
  }
  return f.take();
}

}  // namespace hefesto::oasis::read_detail
