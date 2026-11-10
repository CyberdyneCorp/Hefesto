// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The coordinate parser shared by every editing command (capability
// cmd-coordinates-and-units: "Numeric literal parsing", "Unit suffixes",
// "Default unit of un-suffixed values", "Coordinate expressions") and the
// reported value formatting and pointer snapping of the same capability.
// A coordinate argument is a left-to-right expression of operands joined by
// `+ - * /`; each operand is a number with an optional unit suffix:
//   l            lambda (v * L1 / L0 of the grid scale)
//   i            internal units
//   g            grid units of the current window (pitch, plus the origin
//                for an absolute coordinate); an error without a window
//   nm um mm cm  metric: v / (10*s / (e*k)) with the CIF output style's
//   u micron...  scale factor s and expander e, the divisor in single
//   cu centi...  precision; an error without an output style
// and no suffix means the effective input unit (`units`, or `snap` while
// `units` is default). Each operand is rounded half away from zero before
// the 32-bit integer arithmetic. The magic-compat rules of Q-CMDEDIT-1/2
// decide the unit of an operand written directly before an operator and
// whether a suffixed operand ends the expression.
// (Magic ref: commands/CmdSubrs.c:122-315, dbwind/DBWprocs.c:657-980,
// dbwind/DBWtools.c:1095-1162)
// Owner: EDIT (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cmd/units.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"

namespace hefesto::cmd {

/// The unit of a coordinate without a suffix (Magic's DBWSnapToGrid).
enum class DefaultUnit : std::uint8_t { internal, lambda, user, microns };

struct CoordinateUnits {
  std::int64_t internal_per = 1;  // grid scale: `internal_per` internal units ...
  std::int64_t lambda_per = 1;    // ... equal `lambda_per` lambda
  DefaultUnit default_unit = DefaultUnit::lambda;
  /// The CIF output style's scale factor s and expander e (CIFGetOutputScale);
  /// std::nullopt when the technology has none (Magic then uses a divisor of 1).
  std::optional<std::array<std::int64_t, 2>> cif_scale;
  /// The window grid {x0, y0, x1, y1} (spacing = x1-x0, y1-y0); std::nullopt
  /// when no layout window is known (a grid operand is then an error,
  /// Q-CMDEDIT-4/5).
  std::optional<std::array<std::int64_t, 4>> grid;
  /// The `magic-compat` setting (Q-CMDEDIT-1, Q-CMDEDIT-2).
  bool magic_compat = true;
};

enum class Axis : std::uint8_t { x, y };

/// How one coordinate argument is read: the axis (user-grid pitch and
/// origin), whether it is a size or offset (`relative`: no grid origin) and
/// Magic's `scale` (the result in 1/scale internal units, e.g. 8 for label
/// sizes and offsets).
struct DistanceRequest {
  Axis axis = Axis::x;
  bool relative = false;
  int scale = 1;
};

/// Magic's cmdScaleCoord with Hefesto's fixes: the value of the expression
/// `text` in internal units (times `request.scale`), as a 32-bit integer.
/// An argument that does not start with a number reports "Coordinate value
/// cannot be parsed:  assuming 0" to `sink` and yields the value
/// accumulated so far. Unknown suffixes and ignored text after a suffix
/// (magic-compat ON) are warnings to `sink`. Fails (CMD-USAGE-001) on a
/// metric operand without an output style, a grid operand without a
/// window, and a division by zero.
[[nodiscard]] diag::Outcome<std::int32_t> parse_distance(std::string_view text, const CoordinateUnits& units,
                                                         const DistanceRequest& request,
                                                         diag::DiagnosticSink* sink = nullptr);

/// parse_distance() for callers that need a number: fails (CMD-USAGE-001)
/// when the word does not start with a number instead of assuming 0. The
/// result is integral (each operand is rounded).
[[nodiscard]] diag::Outcome<double> parse_coordinate(std::string_view text, const CoordinateUnits& units,
                                                     Axis axis = Axis::x, bool relative = false,
                                                     diag::DiagnosticSink* sink = nullptr);

/// What a reported value measures ("Reported value formatting").
enum class ValueKind : std::uint8_t { length, area };

/// Formats `value` (internal units, or internal units squared for an area)
/// for a report according to `units` ("Reported value formatting": single
/// precision scaling, `%.3f`/`%.6f` with trailing zeros trimmed, the unit
/// suffix when the print flag is set; `user` without a grid formats as
/// `default`). `axis` picks the user-grid pitch of a length.
[[nodiscard]] std::string format_value(std::int64_t value, const UnitsState& units, const CoordinateUnits& scale,
                                       ValueKind kind = ValueKind::length, Axis axis = Axis::x);

/// The reference grid of pointer snapping: origin and pitch per axis.
struct SnapGrid {
  db::Point origin;
  db::Coord pitch_x = 1;
  db::Coord pitch_y = 1;
};

/// "Pointer snapping": the nearest grid point on each axis (ties go to the
/// upper line). Values typed on the command line are never snapped.
[[nodiscard]] db::Point snap_point(db::Point p, const SnapGrid& grid);

}  // namespace hefesto::cmd
