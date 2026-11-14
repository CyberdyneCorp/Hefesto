// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CIF wire outlines: a port of CIFPaintWirePath with end caps
// (cif/CIFrdpt.c:339-560). Owner: CIF.

#include "hefesto/cif/read/wire.hpp"

#include <cmath>
#include <cstddef>
#include <deque>

#include "hefesto/db/geometry.hpp"

namespace hefesto::cif {

namespace {

constexpr double kQuarterTurn = 1.5708;  // Magic's "90 degrees"

std::vector<ReadPoint> without_repeats(const std::vector<ReadPoint>& path) {
  // Magic compares each point from the second on with the one after it and
  // drops the earlier of an equal pair; the first two are never compared.
  std::vector<ReadPoint> out;
  out.reserve(path.size());
  out.push_back(path.front());
  for (std::size_t i = 1; i < path.size(); ++i) {
    if (i + 1 < path.size() && path[i + 1] == path[i]) {
      continue;
    }
    out.push_back(path[i]);
  }
  return out;
}

std::string point_text(std::int64_t x, std::int64_t y) {
  return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
}

class WireBuilder {
 public:
  WireBuilder(std::int64_t width, WireOutline& out) : width_(static_cast<double>(width)), out_(out) {}

  /// Processes the segment prev -> cur; `next` is the following point or
  /// null at the end. Returns false when the route is truncated.
  bool segment(const ReadPoint& prev, const ReadPoint& cur, const ReadPoint* next);

 private:
  [[nodiscard]] std::int64_t rounded(double v);
  void start_polygon(double xmin, double ymin);
  bool joint_angle(const ReadPoint& prev, const ReadPoint& cur, double& phi, double& xmax, double& ymax);
  void adjust_joint(double phi);
  void move_onto_line(ReadPoint& moved, const ReadPoint& from, const ReadPoint& to, double phi) const;
  void close_polygon();

  double width_;
  WireOutline& out_;
  std::deque<ReadPoint> poly_;
  bool first_ = true;
  double theta_ = 0;
  double save_theta_ = 0;
};

std::int64_t WireBuilder::rounded(double v) {
  const double r = std::round(v);
  if (!(r >= db::kPlaneMin && r <= db::kPlaneMax)) {
    out_.out_of_range = true;
    return 0;
  }
  return static_cast<std::int64_t>(r);
}

void WireBuilder::start_polygon(double xmin, double ymin) {
  // Back the first point up by the end cap (the half width), then emit its
  // two corners: the first becomes the tail, the second the head.
  xmin -= width_ * std::cos(theta_);
  ymin -= width_ * std::sin(theta_);
  const double xoff = width_ * std::cos(theta_ - kQuarterTurn);
  const double yoff = width_ * std::sin(theta_ - kQuarterTurn);
  first_ = false;
  poly_.push_front(ReadPoint{rounded((xmin + xoff) / 2), rounded((ymin + yoff) / 2)});
  poly_.push_front(ReadPoint{rounded((xmin - xoff) / 2), rounded((ymin - yoff) / 2)});
}

bool WireBuilder::joint_angle(const ReadPoint& prev, const ReadPoint& cur, double& phi, double& xmax,
                              double& ymax) {
  const double alpha = 0.5 * (phi - theta_);
  const double test_mitre = std::fabs(std::cos(alpha));
  if (test_mitre >= 0.25) {
    return true;
  }
  if (test_mitre >= 1.0e-10) {
    out_.truncated = true;
    out_.mitre_message = "Error: mitre limit exceeded at wire junction at " + point_text(cur.x, cur.y) +
                         ".";
    return false;
  }
  // The wire reverses: close this outline here (with an end cap) and start
  // a new one at this point.
  const std::string where = point_text(cur.x / 2, cur.y / 2);
  out_.warnings.push_back(prev == cur ? "Warning: duplicate point in path at " + where + "."
                                      : "Warning: direction reversal in path at " + where + ".");
  phi = theta_;
  xmax += width_ * std::cos(theta_);
  ymax += width_ * std::sin(theta_);
  first_ = true;
  return true;
}

void WireBuilder::move_onto_line(ReadPoint& moved, const ReadPoint& from, const ReadPoint& to, double phi) const {
  const double d1 = std::cos(save_theta_) * std::sin(phi) - std::sin(save_theta_) * std::cos(phi);
  if (std::fabs(d1) > 1.0e-4) {
    const double r2 = (std::sin(phi) * static_cast<double>(to.x - from.x) -
                       std::cos(phi) * static_cast<double>(to.y - from.y)) /
                      d1;
    moved.x += static_cast<std::int64_t>(std::round(r2 * std::cos(save_theta_)));
    moved.y += static_cast<std::int64_t>(std::round(r2 * std::sin(save_theta_)));
  }
}

void WireBuilder::adjust_joint(double phi) {
  // When a new offset segment runs backward relative to theta, move the
  // previous offset point to the intersection of the two offset lines.
  ReadPoint& save = poly_[poly_.size() - 2];
  const ReadPoint& tail = poly_.back();
  const ReadPoint& head = poly_.front();
  ReadPoint& second = poly_[1];
  const double a1 =
      std::fabs(std::atan2(static_cast<double>(tail.y - save.y), static_cast<double>(tail.x - save.x)) - theta_);
  const double a2 = std::fabs(
      std::atan2(static_cast<double>(head.y - second.y), static_cast<double>(head.x - second.x)) - theta_);
  if (a1 > 0.1 && a1 < 6.1) {
    move_onto_line(save, save, tail, phi);
  } else if (a2 > 0.1 && a2 < 6.1) {
    move_onto_line(second, second, head, phi);
  }
}

void WireBuilder::close_polygon() {
  mask::Polygon polygon;
  polygon.reserve(poly_.size());
  for (const ReadPoint& p : poly_) {
    // A joint adjustment can move a point; check the plane again.
    out_.out_of_range = out_.out_of_range || p.x < db::kPlaneMin || p.x > db::kPlaneMax ||
                        p.y < db::kPlaneMin || p.y > db::kPlaneMax;
    polygon.push_back(db::Point{static_cast<db::Coord>(p.x), static_cast<db::Coord>(p.y)});
  }
  out_.polygons.push_back(std::move(polygon));
  poly_.clear();
}

bool WireBuilder::segment(const ReadPoint& prev, const ReadPoint& cur, const ReadPoint* next) {
  double xmax = static_cast<double>(cur.x);
  double ymax = static_cast<double>(cur.y);
  save_theta_ = theta_;
  theta_ = std::atan2(ymax - static_cast<double>(prev.y), xmax - static_cast<double>(prev.x));
  if (first_) {
    start_polygon(static_cast<double>(prev.x), static_cast<double>(prev.y));
  }
  double phi = theta_;
  if (next != nullptr) {
    phi = std::atan2(static_cast<double>(next->y) - ymax, static_cast<double>(next->x) - xmax);
  } else {
    xmax += width_ * std::cos(theta_);  // the end cap
    ymax += width_ * std::sin(theta_);
  }
  if (!joint_angle(prev, cur, phi, xmax, ymax)) {
    return false;
  }
  const double alpha = 0.5 * (phi - theta_);
  const double delta = (0.5 * (phi + theta_)) - kQuarterTurn;
  const double cwidth = width_ / std::cos(alpha);
  const double xoff = cwidth * std::cos(delta);
  const double yoff = cwidth * std::sin(delta);
  poly_.push_front(ReadPoint{rounded((xmax - xoff) / 2), rounded((ymax - yoff) / 2)});
  poly_.push_back(ReadPoint{rounded((xmax + xoff) / 2), rounded((ymax + yoff) / 2)});
  if (first_ || next == nullptr) {
    close_polygon();
  } else {
    adjust_joint(phi);
  }
  return true;
}

}  // namespace

WireOutline wire_outline(const std::vector<ReadPoint>& path, std::int64_t width) {
  WireOutline out;
  if (path.empty()) {
    return out;
  }
  const std::vector<ReadPoint> points = without_repeats(path);
  WireBuilder builder(width, out);
  // A single-point path uses the point as both ends.
  std::size_t cur = points.size() == 1 ? 0 : 1;
  std::size_t prev = 0;
  for (; cur < points.size(); prev = cur, ++cur) {
    const ReadPoint* next = cur + 1 < points.size() ? &points[cur + 1] : nullptr;
    if (!builder.segment(points[prev], points[cur], next)) {
      break;
    }
  }
  return out;
}

}  // namespace hefesto::cif
