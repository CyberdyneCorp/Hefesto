// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Records of embedded symbol blocks; see block_symbol.hpp.
//
// Owner: FORMAT.

#include "block_symbol.hpp"

#include <utility>

#include "hefesto/schem/file/properties.hpp"

namespace hefesto::schem::file_detail {

namespace {

Point linear(const Placement& placement, Point p) noexcept {
  return transform_point({{0.0, 0.0}, placement.rotation, placement.flip}, p);
}

template <typename Record, typename Place>
void move_all(std::vector<Record>& from, std::vector<Record>& to, Place place) {
  for (Record& record : from) {
    place(record);
    to.push_back(std::move(record));
  }
  from.clear();
}

}  // namespace

void order_endpoints(Point& p1, Point& p2) noexcept {
  if (p2.x < p1.x || (p1.x == p2.x && p2.y < p1.y)) {
    std::swap(p1, p2);
  }
}

void order_corners(Point& p1, Point& p2) noexcept {
  if (p2.x < p1.x) {
    std::swap(p1.x, p2.x);
  }
  if (p2.y < p1.y) {
    std::swap(p1.y, p2.y);
  }
}

bool symbol_ignored(std::string_view props) {
  const auto value = get_token(props, "symbol_ignore");
  return value.has_value() && is_true(*value);
}

bool floater_text(const TextRecord& text) {
  return get_token(text.props, "name").has_value() || get_token(text.props, "floater").has_value();
}

bool graph_box(const BoxRecord& box) {
  const auto flags = get_token(box.props, "flags");
  return flags.has_value() && flags->find("graph") != std::string::npos;
}

Placement compose(const Placement& outer, const Placement& inner) noexcept {
  const Point ex = linear(outer, linear(inner, {1.0, 0.0}));
  const Point ey = linear(outer, linear(inner, {0.0, 1.0}));
  Placement out{transform_point(outer, inner.origin), 0, 0};
  for (int flip = 0; flip < 2; ++flip) {
    for (int rotation = 0; rotation < 4; ++rotation) {
      const Placement candidate{{0.0, 0.0}, rotation, flip};
      if (linear(candidate, {1.0, 0.0}) == ex && linear(candidate, {0.0, 1.0}) == ey) {
        out.rotation = rotation;
        out.flip = flip;
      }
    }
  }
  return out;
}

void merge_nested(Document& nested, const Placement& at, Document& parent) {
  move_all(nested.lines, parent.lines, [&](LineRecord& r) {
    r.p1 = transform_point(at, r.p1);
    r.p2 = transform_point(at, r.p2);
    order_endpoints(r.p1, r.p2);
  });
  move_all(nested.boxes, parent.boxes, [&](BoxRecord& r) {
    r.p1 = transform_point(at, r.p1);
    r.p2 = transform_point(at, r.p2);
    order_corners(r.p1, r.p2);
    if (r.layer == kPinLayer) {
      r.layer = kNestedPinLayer;
    }
  });
  move_all(nested.arcs, parent.arcs, [&](ArcRecord& r) {
    r.centre = transform_point(at, r.centre);
    r.start_angle = transform_arc_start(at, r.start_angle, r.sweep_angle);
  });
  move_all(nested.polygons, parent.polygons, [&](PolygonRecord& r) {
    for (Point& point : r.points) {
      point = transform_point(at, point);
    }
  });
  move_all(nested.texts, parent.texts, [&](TextRecord& r) {
    r.origin = transform_point(at, r.origin);
    const TextOrientation orientation = transform_text(at, r.rotation, r.flip);
    r.rotation = orientation.rotation;
    r.flip = orientation.flip;
  });
  move_all(nested.instances, parent.instances, [&](InstanceRecord& r) {
    const Placement placed = compose(at, {r.origin, r.rotation, r.flip});
    r.origin = placed.origin;
    r.rotation = placed.rotation;
    r.flip = placed.flip;
  });
}

}  // namespace hefesto::schem::file_detail
