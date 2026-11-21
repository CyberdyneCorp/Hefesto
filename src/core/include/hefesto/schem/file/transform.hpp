// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The coordinate system and orientation transform of schematic instances
// (capability schematic-file-format "Coordinate system", "Orientation
// transform"). Coordinates are unitless, x right, y down.
//
// Owner: FORMAT.

#pragma once

#include "hefesto/schem/file/document.hpp"

namespace hefesto::schem {

/// An instance placement: origin, rotation (taken modulo 4) and flip (any
/// non-zero value flips). The flip is applied before the rotation; each
/// rotation step turns the symbol 90 degrees clockwise on screen.
struct Placement {
  Point origin;
  int rotation = 0;
  int flip = 0;
};

/// The schematic point of the symbol-local point `local`.
[[nodiscard]] Point transform_point(const Placement& placement, Point local) noexcept;

/// A text's rotation and flip after placement.
struct TextOrientation {
  int rotation = 0;
  int flip = 0;

  friend bool operator==(const TextOrientation&, const TextOrientation&) = default;
};

/// The orientation of a text with `rotation`/`flip` inside a symbol placed
/// with `placement`: rotation `(t_rot + (f = 1 and t_rot odd ? r + 2 : r))
/// mod 4`, flip `t_flip XOR f`.
[[nodiscard]] TextOrientation transform_text(const Placement& placement, int rotation, int flip) noexcept;

/// The start angle in degrees of an arc with `start`/`sweep` after placement
/// (the sweep is unchanged and the centre transforms as a point), in
/// [0, 360) for finite input.
[[nodiscard]] double transform_arc_start(const Placement& placement, double start, double sweep) noexcept;

/// The connection point of a pin box: the centre of the box.
[[nodiscard]] Point box_centre(const BoxRecord& box) noexcept;

}  // namespace hefesto::schem
