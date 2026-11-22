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

int quarter_turns(long long rotation) noexcept {
  return static_cast<int>(((rotation % 4) + 4) % 4);
}

// "Orientation transform": any non-zero flip mirrors.
bool flipped(int flip) noexcept {
  return flip != 0;
}

}  // namespace

Point transform_point(const Placement& placement, Point local) noexcept {
  const double x = flipped(placement.flip) ? -local.x : local.x;
  const double y = local.y;
  Point offset;
  switch (placement.rotation) {
    case 0:
      offset = {x, y};
      break;
    case 1:
      offset = {-y, x};
      break;
    case 2:
      offset = {-x, -y};
      break;
    default:  // 3, and every other value, acts as 3
      offset = {y, -x};
      break;
  }
  return {placement.origin.x + offset.x, placement.origin.y + offset.y};
}

// Rotation `(t_rot + (f != 0 and t_rot odd ? r + 2 : r)) mod 4` with r and
// t_rot as stored, the remainder taken as 0..3; flip `t_flip XOR f`.
TextOrientation transform_text(const Placement& placement, int rotation, int flip) noexcept {
  const long long r = placement.rotation;
  const bool odd = rotation % 2 != 0;
  const long long turn = (flipped(placement.flip) && odd) ? r + 2 : r;
  return {quarter_turns(static_cast<long long>(rotation) + turn), flip ^ placement.flip};
}

double transform_arc_start(const Placement& placement, double start, double sweep) noexcept {
  const double r = placement.rotation;
  const double angle = flipped(placement.flip) ? std::fmod(270.0 * r + 180.0 - sweep - start, 360.0)
                                               : std::fmod(start + 270.0 * r, 360.0);
  return angle < 0.0 ? angle + 360.0 : angle;
}

Point box_centre(const BoxRecord& box) noexcept {
  return {(box.p1.x + box.p2.x) / 2.0, (box.p1.y + box.p2.y) / 2.0};
}

}  // namespace hefesto::schem
