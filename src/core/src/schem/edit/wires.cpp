// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Wire routes and junction dots (wires.hpp; capability schematic-editing
// "Wires"). Points touch by NETLIST's "Connection geometry" (touches()).
//
// Owner: EDIT.

#include "hefesto/schem/edit/wires.hpp"

#include <algorithm>
#include <cmath>

#include "hefesto/schem/net/connectivity.hpp"

namespace hefesto::schem {

namespace {

int junction_weight(Point p, const WireRecord& other) noexcept {
  if (p == other.p1 || p == other.p2) return 1;
  return touches(p, other.p1, other.p2) ? 2 : 0;
}

bool is_junction(const Document& document, std::size_t self, Point p) noexcept {
  int count = 0;
  for (std::size_t j = 0; j < document.wires.size() && count <= 1; ++j) {
    if (j != self) count += junction_weight(p, document.wires[j]);
  }
  return count > 1;
}

}  // namespace

std::vector<WireSegment> route_wire(Point from, Point to, WireRoute route) {
  if (from == to) return {};
  if (route == WireRoute::oblique || from.x == to.x || from.y == to.y) return {{from, to}};
  const Point corner = route == WireRoute::horizontal_first ? Point{to.x, from.y} : Point{from.x, to.y};
  return {{from, corner}, {corner, to}};
}

WireRoute next_route(WireRoute route) noexcept {
  switch (route) {
    case WireRoute::horizontal_first:
      return WireRoute::vertical_first;
    case WireRoute::vertical_first:
      return WireRoute::oblique;
    default:
      return WireRoute::horizontal_first;
  }
}

WireRoute orthogonal_route(Point from, Point pointer) noexcept {
  return std::fabs(pointer.x - from.x) >= std::fabs(pointer.y - from.y) ? WireRoute::horizontal_first
                                                                        : WireRoute::vertical_first;
}

std::vector<Point> junction_points(const Document& document) {
  std::vector<Point> points;
  for (std::size_t i = 0; i < document.wires.size(); ++i) {
    for (const Point p : {document.wires[i].p1, document.wires[i].p2}) {
      if (std::find(points.begin(), points.end(), p) == points.end() && is_junction(document, i, p)) {
        points.push_back(p);
      }
    }
  }
  return points;
}

}  // namespace hefesto::schem
