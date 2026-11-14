// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The pure element arithmetic of the GDS reader (capability gds-read
// "Structure reference transforms", "Magnified instances", "PATH
// elements", "Path-to-polygon conversion", "TEXT element parsing";
// openspec/changes/m5-mask-gds-cif/design.md D11, D22): the Manhattan
// rounding of SREF/AREF angles and their transform, the continued-fraction
// magnification and the magnified copy's name, path end extensions, the
// path outline (CIFPaintWirePath with the GDS end-cap flag and the
// Q-MASK-116/118 choices) and the TEXT presentation anchor. The structure
// and element handlers of gds::read (src/core/src/gds/read/) call these;
// the unit tests pin them, the float paths on every leg (design D22).
//
// Owner: GDSR (design D2).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/transform.hpp"
#include "hefesto/mask/plane/plane.hpp"

namespace hefesto::gds {

/// The Manhattan angle of an ANGLE value (calmaReadTransform).
struct ManhattanAngle {
  std::int32_t degrees = 0;     // 0, 90, 180 or 270
  std::int32_t truncated = 0;   // (int)a reduced into [0, 360]; reported when it was not Manhattan
  bool rounded = false;         // "Non-Manhattan angle (<truncated>) in transform"
};

/// (int)a, reduced by 360 into [0, 360] (360 becomes 0), then rounded to
/// the nearest quarter turn: [0,45) 0, [45,135) 90, [135,225) 180,
/// [225,315) 270, [315,360) 0. A non-finite value is 0.
[[nodiscard]] ManhattanAngle manhattan_angle(double angle) noexcept;

/// The use transform of STRANS flags `flags` (0x8000 mirrors about the x
/// axis first) and counterclockwise `degrees`, without translation.
[[nodiscard]] db::Transform strans_transform(std::uint16_t flags, std::int32_t degrees) noexcept;

/// calmaMagFraction: magnification `mag` (> 0) as num / den with den at
/// most 10000, by continued fractions.
struct MagFraction {
  std::int64_t num = 1;
  std::int64_t den = 1;
  bool exact = true;  // false: "Magnification <mag> of cell <s> approximated as num/den"

  friend bool operator==(const MagFraction&, const MagFraction&) = default;
};
inline constexpr std::int64_t kMaxMagDenominator = 10000;  // CALMA_MAG_MAXDEN
[[nodiscard]] MagFraction mag_fraction(double mag) noexcept;

/// The name of a magnified copy of `sname` with total magnification
/// num / den: "%.6g" of the ratio with '.' made 'p' ("cell_mag1p5").
[[nodiscard]] std::string magnified_name(std::string_view sname, std::int64_t num, std::int64_t den);

/// A point of a path centerline or outline in doubled (centerline) or
/// plain read units, kept in 64 bits until it is range-checked.
struct PathPoint {
  std::int64_t x = 0;
  std::int64_t y = 0;

  friend bool operator==(const PathPoint&, const PathPoint&) = default;
};

/// "PATH elements" item 5: moves the first point by `begin` and the last by
/// `end` (doubled read units). magic-compat ON (Q-MASK-114): each
/// coordinate moves away from its neighbour's by the extension when they
/// differ, and only positive extensions apply; OFF: along the segment
/// direction, rounded half away from zero, negative values shortening.
void extend_path(std::vector<PathPoint>& points, std::int64_t begin, std::int64_t end, bool magic_compat);

/// The outlines of a path (CIFPaintWirePath): centerline `points` in
/// doubled read units, half width `width` (= W, doubled units), with the
/// half-width end caps only when `endcap`. magic-compat ON keeps the head
/// duplicate (Q-MASK-116), uses 90 degrees as 1.5708 and a first saved
/// angle of 0 (Q-MASK-118); OFF removes every duplicate, uses pi/2 and the
/// first segment's angle.
struct PathOutline {
  /// Closed outlines in read units, in the order they are painted.
  std::vector<mask::Polygon> polygons;
  /// "Warning: direction reversal in path at (x, y)." or "duplicate point".
  std::vector<std::string> warnings;
  /// "Error: mitre limit exceeded at wire junction at (x, y)."; the outline
  /// in progress is dropped and the rest of the path ignored.
  std::string mitre_message;
  bool truncated = false;
  /// A computed point left the usable plane; the polygons were dropped.
  bool out_of_range = false;
};
[[nodiscard]] PathOutline path_outline(const std::vector<PathPoint>& points, std::int64_t width, bool endcap,
                                       bool magic_compat);

/// The label position of a TEXT PRESENTATION anchor nibble (southeast for
/// the undefined nibbles).
[[nodiscard]] db::Position presentation_position(std::int32_t presentation) noexcept;

}  // namespace hefesto::gds
