// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `wire segment` geometry and input (wiring-tool "wire segment"): the
// centerline-path-to-polygon conversion shared with CIF/GDS wire paths, and
// the coordinate file reader. Ported from Magic VLSI 8.3.694 (fc5daa9e)
// cif/CIFrdpt.c:339-603 (CIFPaintWirePath) and commands/CmdTZ.c:2036-2112,
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md). The floating-point steps (atan2, the 1.5708
// right angle, round half away from zero) are Magic's. Owner: WIRE.

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <deque>
#include <string>

#include "hefesto/wiring/diag_codes.hpp"
#include "hefesto/wiring/wiring.hpp"

namespace hefesto::wiring {

namespace {

constexpr double kRightAngle = 1.5708;  // Magic's "90 degrees"

db::Coord rnd(double v) {
  constexpr auto kMax = static_cast<double>(INT32_MAX);
  return static_cast<db::Coord>(std::clamp(std::round(v), -kMax, kMax));
}

db::Point half_point(double x, double y) {
  return db::Point{rnd(x / 2), rnd(y / 2)};
}

void emit(diag::DiagnosticSink* sink, std::string_view code, std::string text) {
  if (sink != nullptr) {
    sink->emit(diag::make(code, std::move(text)));
  }
}

// Repeated points are removed as Magic does: consecutive duplicates among
// the second and later points (the head is never compared).
std::vector<db::Point> without_repeats(std::span<const db::Point> points) {
  std::vector<db::Point> out;
  for (std::size_t i = 0; i < points.size(); ++i) {
    if (i < 2 || points[i] != out.back()) {
      out.push_back(points[i]);
    }
  }
  return out;
}

// One run of CIFPaintWirePath's loop state.
class PathBuilder {
 public:
  PathBuilder(std::span<const db::Point> path, db::Coord width, bool endcap, diag::DiagnosticSink* sink)
      : path_(path), w_(static_cast<double>(width)), endcap_(endcap), sink_(sink) {}

  std::vector<std::vector<db::Point>> run() {
    std::size_t prev = 0;
    for (std::size_t cur = path_.size() == 1 ? 0 : 1; cur < path_.size(); prev = cur++) {
      if (!step(prev, cur)) {
        break;  // mitre limit: the route is truncated, the open polygon dropped
      }
    }
    return std::move(out_);
  }

 private:
  // Starts a polygon at segment start (x, y) with direction theta_.
  void start(double x, double y) {
    if (endcap_) {
      x -= w_ * std::cos(theta_);
      y -= w_ * std::sin(theta_);
    }
    const double xoff = w_ * std::cos(theta_ - kRightAngle);
    const double yoff = w_ * std::sin(theta_ - kRightAngle);
    poly_.clear();
    poly_.push_front(half_point(x + xoff, y + yoff));
    poly_.push_front(half_point(x - xoff, y - yoff));
    first_ = false;
  }

  // The sharp-bend check; false when the route must be truncated.
  bool check_bend(std::size_t prev, std::size_t cur, double& phi, double& x, double& y) {
    const double test = std::fabs(std::cos(0.5 * (phi - theta_)));
    if (test >= 0.25) {
      return true;
    }
    const db::Point p = path_[cur];
    if (test >= 1.0e-10) {
      emit(sink_, codes::kPathMitre,
           "Error: mitre limit exceeded at wire junction at (" + std::to_string(p.x) + ", " + std::to_string(p.y) +
               ").\nRoute has been truncated.");
      return false;
    }
    const bool duplicate = path_[prev] == p;
    emit(sink_, codes::kPathReversal,
         std::string{"Warning: "} + (duplicate ? "duplicate point" : "direction reversal") + " in path at (" +
             std::to_string(p.x / 2) + ", " + std::to_string(p.y / 2) + ").");
    phi = theta_;
    end_cap(x, y);
    first_ = true;
    return true;
  }

  void end_cap(double& x, double& y) const {
    if (endcap_) {
      x += w_ * std::cos(theta_);
      y += w_ * std::sin(theta_);
    }
  }

  // Moves the joint vertex that makes a cut-out wedge to the intersection
  // of the previous and next edges.
  void fix_wedge(double save_theta, double phi) {
    const db::Point ret = poly_.back();
    db::Point& save = poly_[poly_.size() - 2];
    const db::Point front = poly_[0];
    db::Point& second = poly_[1];
    // Differences in double: the vertices may lie anywhere in the int range.
    const auto dx = [](const db::Point& a, const db::Point& b) { return static_cast<double>(a.x) - b.x; };
    const auto dy = [](const db::Point& a, const db::Point& b) { return static_cast<double>(a.y) - b.y; };
    const double a1 = std::fabs(std::atan2(dy(ret, save), dx(ret, save)) - theta_);
    const double a2 = std::fabs(std::atan2(dy(front, second), dx(front, second)) - theta_);
    const double d1 = std::cos(save_theta) * std::sin(phi) - std::sin(save_theta) * std::cos(phi);
    const auto move = [&](db::Point& v, const db::Point& from) {
      if (std::fabs(d1) > 1.0e-4) {
        const double r2 = (std::sin(phi) * dx(from, v) - std::cos(phi) * dy(from, v)) / d1;
        v.x = rnd(static_cast<double>(v.x) + rnd(r2 * std::cos(save_theta)));
        v.y = rnd(static_cast<double>(v.y) + rnd(r2 * std::sin(save_theta)));
      }
    };
    if (a1 > 0.1 && a1 < 6.1) {
      move(save, ret);
    } else if (a2 > 0.1 && a2 < 6.1) {
      move(second, front);
    }
  }

  bool step(std::size_t prev, std::size_t cur) {
    double xmin = path_[prev].x;
    double ymin = path_[prev].y;
    double xmax = path_[cur].x;
    double ymax = path_[cur].y;
    const double save_theta = theta_;
    theta_ = std::atan2(ymax - ymin, xmax - xmin);
    if (first_) {
      start(xmin, ymin);
    }
    const bool last = cur + 1 >= path_.size();
    double phi = theta_;
    if (!last) {
      phi = std::atan2(path_[cur + 1].y - ymax, path_[cur + 1].x - xmax);
    } else {
      end_cap(xmax, ymax);
    }
    if (!check_bend(prev, cur, phi, xmax, ymax)) {
      return false;
    }
    const double alpha = 0.5 * (phi - theta_);
    const double delta = 0.5 * (phi + theta_) - kRightAngle;
    const double cwidth = w_ / std::cos(alpha);
    const double xoff = cwidth * std::cos(delta);
    const double yoff = cwidth * std::sin(delta);
    poly_.push_front(half_point(xmax - xoff, ymax - yoff));
    poly_.push_back(half_point(xmax + xoff, ymax + yoff));
    if (first_ || last) {
      out_.emplace_back(poly_.begin(), poly_.end());
      poly_.clear();
    } else {
      fix_wedge(save_theta, phi);
    }
    return true;
  }

  std::span<const db::Point> path_;
  double w_;
  bool endcap_;
  diag::DiagnosticSink* sink_;
  bool first_ = true;
  double theta_ = 0;
  std::deque<db::Point> poly_;  // fronts: left side reversed; back: right side
  std::vector<std::vector<db::Point>> out_;
};

bool is_space(char c) {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

// The next "line" as fgets(line, 128, file) returns it: up to 127 bytes,
// ending after a newline.
std::string_view next_fgets_line(std::string_view& text) {
  constexpr std::size_t kMax = 127;
  std::size_t n = std::min(text.size(), kMax);
  const std::size_t nl = text.substr(0, n).find('\n');
  if (nl != std::string_view::npos) {
    n = nl + 1;
  }
  const std::string_view line = text.substr(0, n);
  text.remove_prefix(n);
  return line;
}

// The x and y words of one line (CmdWire's pointer walk); empty when absent.
std::pair<std::string_view, std::string_view> coordinate_words(std::string_view line) {
  std::size_t i = 0;
  while (i < line.size() && is_space(line[i])) {
    ++i;
  }
  const std::size_t xs = i;
  while (i < line.size() && !is_space(line[i])) {
    ++i;
  }
  const std::string_view x = line.substr(xs, i - xs);
  if (i >= line.size()) {
    return {x, {}};  // the x word ends the text: no y
  }
  ++i;
  while (i < line.size() && is_space(line[i])) {
    ++i;
  }
  const std::size_t ys = i;
  while (i < line.size() && !is_space(line[i])) {
    ++i;
  }
  return {x, line.substr(ys, i - ys)};
}

}  // namespace

std::vector<std::vector<db::Point>> wire_path_polygons(std::span<const db::Point> points, db::Coord width,
                                                       bool endcap, diag::DiagnosticSink* sink) {
  if (points.empty()) {
    return {};
  }
  const std::vector<db::Point> path = without_repeats(points);
  return PathBuilder{path, width, endcap, sink}.run();
}

db::Coord scale_coord(std::string_view text, const cmd::CoordinateUnits& units, bool relative, bool is_x, int scale,
                      diag::DiagnosticSink* sink) {
  const auto value = cmd::parse_coordinate(text, units, is_x ? cmd::Axis::x : cmd::Axis::y, relative, sink);
  if (!value.ok()) {
    emit(sink, codes::kWarning, "Coordinate value cannot be parsed:  assuming 0");
    return 0;
  }
  return rnd(value.value() * static_cast<double>(scale));
}

diag::Outcome<std::vector<db::Point>> read_segment_points(std::string_view text, std::string_view file_label,
                                                          const cmd::CoordinateUnits& units,
                                                          diag::DiagnosticSink* sink) {
  std::vector<db::Point> points;
  for (std::size_t n = 1; !text.empty(); ++n) {
    const auto [x, y] = coordinate_words(next_fgets_line(text));
    if (x.empty() || y.empty()) {
      return diag::make(codes::kFileCoordinate,
                        "Bad coordinate pair at " + std::string{file_label} + " line " + std::to_string(n));
    }
    points.push_back(db::Point{scale_coord(x, units, false, true, 2, sink), scale_coord(y, units, false, false, 2, sink)});
  }
  return points;
}

}  // namespace hefesto::wiring
