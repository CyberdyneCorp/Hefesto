// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Orientation transform (schematic-file-format "Orientation transform",
// "Coordinate system"); see transform.hpp.
//
// Owner: FORMAT.

#include "hefesto/schem/file/transform.hpp"

#include <cmath>

namespace hefesto::schem {

namespace {

int quarter_turns(int rotation) noexcept {
  return ((rotation % 4) + 4) % 4;
}

bool flipped(int flip) noexcept {
  return flip != 0;
}

}  // namespace

Point transform_point(const Placement& placement, Point local) noexcept {
  const double x = flipped(placement.flip) ? -local.x : local.x;
  const double y = local.y;
  Point offset;
  switch (quarter_turns(placement.rotation)) {
    case 1:
      offset = {-y, x};
      break;
    case 2:
      offset = {-x, -y};
      break;
    case 3:
      offset = {y, -x};
      break;
    default:
      offset = {x, y};
      break;
  }
  return {placement.origin.x + offset.x, placement.origin.y + offset.y};
}

TextOrientation transform_text(const Placement& placement, int rotation, int flip) noexcept {
  const int r = quarter_turns(placement.rotation);
  const bool f = flipped(placement.flip);
  const int own = quarter_turns(rotation);
  const int turn = (f && own % 2 == 1) ? r + 2 : r;
  return {quarter_turns(own + turn), (flipped(flip) != f) ? 1 : 0};
}

double transform_arc_start(const Placement& placement, double start, double sweep) noexcept {
  const double r = quarter_turns(placement.rotation);
  const double angle = flipped(placement.flip) ? std::fmod(270.0 * r + 180.0 - sweep - start, 360.0)
                                               : std::fmod(start + 270.0 * r, 360.0);
  return angle < 0.0 ? angle + 360.0 : angle;
}

Point box_centre(const BoxRecord& box) noexcept {
  return {(box.p1.x + box.p2.x) / 2.0, (box.p1.y + box.p2.y) / 2.0};
}

}  // namespace hefesto::schem
