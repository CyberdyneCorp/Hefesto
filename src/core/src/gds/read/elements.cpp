// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The pure element arithmetic of the GDS reader (see
// hefesto/gds/read/elements.hpp). Ported from Magic 8.3.694
// calma/CalmaRdio.c (calmaReadTransform's angle and transform),
// calma/CalmaRdcl.c (calmaMagFraction, calmaMagnifiedCell's copy name),
// calma/CalmaRdpt.c (calmaElementPath's end extensions, calmaElementText's
// presentation) and cif/CIFrdpt.c (CIFPaintWirePath); Magic is (c) the
// Regents of the University of California, distributed under its
// permissive license (THIRD_PARTY_NOTICES.md).
//
// Floating point follows design D22: doubles in Magic's operation order,
// -ffp-contract=off for the area, round half away from zero (std::round,
// as C's round()).
//
// Deviations (X-2 tier 1, malformed input): a non-finite angle is 0, and
// an extension of a single-point path is ignored (Magic dereferences the
// missing second point).
//
// Owner: GDSR (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/gds/read/elements.hpp"

#include <array>
#include <climits>
#include <cmath>
#include <cstdio>
#include <deque>
#include <numbers>

#include "hefesto/db/geometry.hpp"

namespace hefesto::gds {

namespace {

constexpr double kMagicQuarterTurn = 1.5708;  // Magic's "90 degrees" (Q-MASK-118 ON)

std::int32_t quarter_round(std::int32_t angle) noexcept {
  if (angle < 45) {
    return 0;
  }
  if (angle < 135) {
    return 90;
  }
  if (angle < 225) {
    return 180;
  }
  return angle < 315 ? 270 : 0;
}

}  // namespace

ManhattanAngle manhattan_angle(double angle) noexcept {
  ManhattanAngle out;
  if (!std::isfinite(angle)) {
    return out;
  }
  const double clamped = std::fmax(std::fmin(angle, 2147483647.0), -2147483647.0);
  std::int64_t a = static_cast<std::int64_t>(clamped) % 360;  // (int)a, then the +-360 loops
  if (a < 0) {
    a += 360;
  }
  out.truncated = static_cast<std::int32_t>(a);
  if (a % 90 == 0) {
    out.degrees = out.truncated;
    return out;
  }
  out.rounded = true;
  out.degrees = quarter_round(out.truncated);
  return out;
}

db::Transform strans_transform(std::uint16_t flags, std::int32_t degrees) noexcept {
  db::Transform t;
  if ((flags & 0x8000U) != 0) {
    t = db::Transform{1, 0, 0, 0, -1, 0};  // GeoUpsideDownTransform
  }
  switch (degrees) {
    case 90:
      return db::compose(db::Transform{0, -1, 0, 1, 0, 0}, t);  // Geo270Transform (Magic angles are clockwise)
    case 180:
      return db::compose(db::Transform{-1, 0, 0, 0, -1, 0}, t);
    case 270:
      return db::compose(db::Transform{0, 1, 0, -1, 0, 0}, t);  // Geo90Transform
    default:
      return t;
  }
}

MagFraction mag_fraction(double mag) noexcept {
  std::int64_t h0 = 0;
  std::int64_t h1 = 1;
  std::int64_t k0 = 1;
  std::int64_t k1 = 0;
  double x = mag;
  for (;;) {
    if (!(x <= static_cast<double>(INT_MAX))) {
      break;  // also a NaN from 1 / 0
    }
    const auto a = static_cast<std::int64_t>(std::floor(x));
    if (a > 0 && h1 > (INT_MAX - h0) / a) {
      break;
    }
    const std::int64_t h2 = a * h1 + h0;
    const std::int64_t k2 = a * k1 + k0;
    if (k2 > kMaxMagDenominator) {
      break;
    }
    h0 = h1;
    h1 = h2;
    k0 = k1;
    k1 = k2;
    if (std::fabs(static_cast<double>(h1) / static_cast<double>(k1) - mag) <= 1.0e-12 * mag) {
      break;
    }
    x = 1.0 / (x - static_cast<double>(a));
  }
  if (h1 <= 0 || k1 <= 0) {
    h1 = 1;
    k1 = kMaxMagDenominator;
  }
  const bool exact = std::fabs(static_cast<double>(h1) / static_cast<double>(k1) - mag) <= 1.0e-12 * mag;
  return MagFraction{h1, k1, exact};
}

std::string magnified_name(std::string_view sname, std::int64_t num, std::int64_t den) {
  std::array<char, 32> text{};
  std::snprintf(text.data(), text.size(), "%.6g", static_cast<double>(num) / static_cast<double>(den));
  std::string mag{text.data()};
  for (char& c : mag) {
    if (c == '.') {
      c = 'p';
    }
  }
  return std::string{sname} + "_mag" + mag;
}

// --- end extensions -------------------------------------------------------------

namespace {

/// Magic's per-coordinate move of `p` away from `toward` by `e` (e > 0).
void move_away(PathPoint& p, const PathPoint& toward, std::int64_t e) noexcept {
  if (p.x > toward.x) {
    p.x += e;
  } else if (p.x < toward.x) {
    p.x -= e;
  }
  if (p.y > toward.y) {
    p.y += e;
  } else if (p.y < toward.y) {
    p.y -= e;
  }
}

/// Q-MASK-114 OFF: `p` moves by `e` along the direction from `from` to `p`.
void move_along(PathPoint& p, const PathPoint& from, std::int64_t e) noexcept {
  const double dx = static_cast<double>(p.x - from.x);
  const double dy = static_cast<double>(p.y - from.y);
  const double length = std::hypot(dx, dy);
  if (e == 0 || length == 0.0) {
    return;
  }
  p.x += static_cast<std::int64_t>(std::round(static_cast<double>(e) * dx / length));
  p.y += static_cast<std::int64_t>(std::round(static_cast<double>(e) * dy / length));
}

}  // namespace

void extend_path(std::vector<PathPoint>& points, std::int64_t begin, std::int64_t end, bool magic_compat) {
  if (points.size() < 2) {
    return;
  }
  PathPoint& first = points.front();
  PathPoint& last = points.back();
  const PathPoint second = points[1];
  const PathPoint before_last = points[points.size() - 2];
  if (magic_compat) {
    if (begin > 0) {
      move_away(first, second, begin);
    }
    if (end > 0) {
      move_away(last, before_last, end);
    }
    return;
  }
  move_along(first, second, begin);
  move_along(last, before_last, end);
}

// --- path outline ---------------------------------------------------------------

namespace {

std::string point_text(std::int64_t x, std::int64_t y) {
  return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
}

std::vector<PathPoint> without_repeats(const std::vector<PathPoint>& path, bool magic_compat) {
  // Q-MASK-116: Magic compares each point from the second on with the one
  // after it and drops the earlier of an equal pair (never the head); OFF
  // the head is compared too.
  std::vector<PathPoint> out;
  out.reserve(path.size());
  for (std::size_t i = 0; i < path.size(); ++i) {
    const bool comparable = i + 1 < path.size() && (i > 0 || !magic_compat);
    if (comparable && path[i + 1] == path[i]) {
      continue;
    }
    out.push_back(path[i]);
  }
  return out;
}

class OutlineBuilder {
 public:
  OutlineBuilder(std::int64_t width, bool endcap, bool magic_compat, PathOutline& out)
      : width_(static_cast<double>(width)),
        endcap_(endcap),
        magic_compat_(magic_compat),
        quarter_(magic_compat ? kMagicQuarterTurn : std::numbers::pi / 2),
        out_(out) {}

  /// The segment prev -> cur, with `next` the following point or null.
  /// Returns false when the path is truncated.
  bool segment(const PathPoint& prev, const PathPoint& cur, const PathPoint* next);

 private:
  [[nodiscard]] std::int64_t rounded(double v);
  void start_polygon(double xmin, double ymin);
  bool joint(const PathPoint& prev, const PathPoint& cur, double& phi, double& xmax, double& ymax);
  void adjust_joint(double phi);
  void move_onto_line(PathPoint& moved, const PathPoint& to, double phi) const;
  void close_polygon();

  double width_;
  bool endcap_;
  bool magic_compat_;
  double quarter_;
  PathOutline& out_;
  std::deque<PathPoint> poly_;  // left chain prepended, right chain appended
  bool first_ = true;
  bool started_ = false;
  double theta_ = 0;
  double save_theta_ = 0;
};

std::int64_t OutlineBuilder::rounded(double v) {
  const double r = std::round(v);
  if (!(r >= db::kPlaneMin && r <= db::kPlaneMax)) {
    out_.out_of_range = true;
    return 0;
  }
  return static_cast<std::int64_t>(r);
}

void OutlineBuilder::start_polygon(double xmin, double ymin) {
  if (endcap_) {
    xmin -= width_ * std::cos(theta_);
    ymin -= width_ * std::sin(theta_);
  }
  const double xoff = width_ * std::cos(theta_ - quarter_);
  const double yoff = width_ * std::sin(theta_ - quarter_);
  first_ = false;
  poly_.push_front(PathPoint{rounded((xmin + xoff) / 2), rounded((ymin + yoff) / 2)});
  poly_.push_front(PathPoint{rounded((xmin - xoff) / 2), rounded((ymin - yoff) / 2)});
}

bool OutlineBuilder::joint(const PathPoint& prev, const PathPoint& cur, double& phi, double& xmax, double& ymax) {
  const double test_mitre = std::fabs(std::cos(0.5 * (phi - theta_)));
  if (test_mitre >= 0.25) {
    return true;
  }
  if (test_mitre >= 1.0e-10) {
    out_.truncated = true;
    out_.mitre_message = "Error: mitre limit exceeded at wire junction at " + point_text(cur.x, cur.y) + ".";
    return false;
  }
  const std::string where = point_text(cur.x / 2, cur.y / 2);
  out_.warnings.push_back(prev == cur ? "Warning: duplicate point in path at " + where + "."
                                      : "Warning: direction reversal in path at " + where + ".");
  phi = theta_;
  if (endcap_) {
    xmax += width_ * std::cos(theta_);
    ymax += width_ * std::sin(theta_);
  }
  first_ = true;
  return true;
}

void OutlineBuilder::move_onto_line(PathPoint& moved, const PathPoint& to, double phi) const {
  const double d1 = std::cos(save_theta_) * std::sin(phi) - std::sin(save_theta_) * std::cos(phi);
  if (std::fabs(d1) > 1.0e-4) {
    const double r2 = (std::sin(phi) * static_cast<double>(to.x - moved.x) -
                       std::cos(phi) * static_cast<double>(to.y - moved.y)) /
                      d1;
    moved.x += static_cast<std::int64_t>(std::round(r2 * std::cos(save_theta_)));
    moved.y += static_cast<std::int64_t>(std::round(r2 * std::sin(save_theta_)));
  }
}

void OutlineBuilder::adjust_joint(double phi) {
  PathPoint& save = poly_[poly_.size() - 2];  // the previous right-chain point
  const PathPoint& tail = poly_.back();
  const PathPoint& head = poly_.front();
  PathPoint& second = poly_[1];               // the previous left-chain point
  const double a1 =
      std::fabs(std::atan2(static_cast<double>(tail.y - save.y), static_cast<double>(tail.x - save.x)) - theta_);
  const double a2 = std::fabs(
      std::atan2(static_cast<double>(head.y - second.y), static_cast<double>(head.x - second.x)) - theta_);
  if (a1 > 0.1 && a1 < 6.1) {
    move_onto_line(save, tail, phi);
  } else if (a2 > 0.1 && a2 < 6.1) {
    move_onto_line(second, head, phi);
  }
}

void OutlineBuilder::close_polygon() {
  mask::Polygon polygon;
  polygon.reserve(poly_.size());
  for (const PathPoint& p : poly_) {
    out_.out_of_range = out_.out_of_range || p.x < db::kPlaneMin || p.x > db::kPlaneMax || p.y < db::kPlaneMin ||
                        p.y > db::kPlaneMax;
    polygon.push_back(db::Point{static_cast<db::Coord>(p.x), static_cast<db::Coord>(p.y)});
  }
  out_.polygons.push_back(std::move(polygon));
  poly_.clear();
}

bool OutlineBuilder::segment(const PathPoint& prev, const PathPoint& cur, const PathPoint* next) {
  double xmax = static_cast<double>(cur.x);
  double ymax = static_cast<double>(cur.y);
  save_theta_ = theta_;
  theta_ = std::atan2(ymax - static_cast<double>(prev.y), xmax - static_cast<double>(prev.x));
  if (!started_ && !magic_compat_) {
    save_theta_ = theta_;  // Q-MASK-118 OFF: the first joint's saved angle is the first segment's
  }
  started_ = true;
  if (first_) {
    start_polygon(static_cast<double>(prev.x), static_cast<double>(prev.y));
  }
  double phi = theta_;
  if (next != nullptr) {
    phi = std::atan2(static_cast<double>(next->y) - ymax, static_cast<double>(next->x) - xmax);
  } else if (endcap_) {
    xmax += width_ * std::cos(theta_);
    ymax += width_ * std::sin(theta_);
  }
  if (!joint(prev, cur, phi, xmax, ymax)) {
    return false;
  }
  const double alpha = 0.5 * (phi - theta_);
  const double delta = (0.5 * (phi + theta_)) - quarter_;
  const double cwidth = width_ / std::cos(alpha);
  const double xoff = cwidth * std::cos(delta);
  const double yoff = cwidth * std::sin(delta);
  poly_.push_front(PathPoint{rounded((xmax - xoff) / 2), rounded((ymax - yoff) / 2)});
  poly_.push_back(PathPoint{rounded((xmax + xoff) / 2), rounded((ymax + yoff) / 2)});
  if (first_ || next == nullptr) {
    close_polygon();
  } else {
    adjust_joint(phi);
  }
  return true;
}

}  // namespace

PathOutline path_outline(const std::vector<PathPoint>& points, std::int64_t width, bool endcap, bool magic_compat) {
  PathOutline out;
  if (points.empty()) {
    return out;
  }
  const std::vector<PathPoint> path = without_repeats(points, magic_compat);
  OutlineBuilder builder(width, endcap, magic_compat, out);
  std::size_t cur = path.size() == 1 ? 0 : 1;  // a single point is both ends
  std::size_t prev = 0;
  for (; cur < path.size(); prev = cur, ++cur) {
    const PathPoint* next = cur + 1 < path.size() ? &path[cur + 1] : nullptr;
    if (!builder.segment(path[prev], path[cur], next)) {
      break;
    }
  }
  if (out.out_of_range) {
    out.polygons.clear();
  }
  return out;
}

db::Position presentation_position(std::int32_t presentation) noexcept {
  switch (presentation & 0x000f) {
    case 0x000a:
      return db::kNorthWest;
    case 0x0006:
      return db::kWest;
    case 0x0002:
      return db::kSouthWest;
    case 0x0009:
      return db::kNorth;
    case 0x0005:
      return db::kCenter;
    case 0x0001:
      return db::kSouth;
    case 0x0008:
      return db::kNorthEast;
    case 0x0004:
      return db::kEast;
    default:
      return db::kSouthEast;
  }
}

}  // namespace hefesto::gds
