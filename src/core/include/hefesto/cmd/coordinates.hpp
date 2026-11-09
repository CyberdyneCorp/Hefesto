// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The coordinate units shared by the layout (db) and view commands: one term
// of Magic's cmdScaleCoord (commands/CmdSubrs.c). A coordinate is a number
// with an optional unit suffix:
//   l            lambda (v * L1 / L0 of the grid scale)
//   i            internal units
//   g            grid units of the current window (pitch, plus the origin
//                for an absolute coordinate); 0 when no window is known
//   nm um mm cm  metric: v / (10*s / (e*k)) with the CIF output style's
//   u micron...  scale factor s and expander e, the divisor in single
//   cu centi...  precision (cmd-coordinates-and-units "Unit suffixes")
// and no suffix means the session's default unit (`snap`). An unknown suffix
// reports Magic's message and takes the value as internal units. The
// arithmetic forms of cmdScaleCoord (`{1+2}`, `3*2l`, "Coordinate
// expressions") are not parsed here yet (M2). Owner: INTEGRATION.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

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
  /// when no window is known (grid units then count as internal units, as
  /// Magic does without a window).
  std::optional<std::array<std::int64_t, 4>> grid;
};

enum class Axis : std::uint8_t { x, y };

/// The coordinate in internal units, unrounded (callers round half away
/// from zero, or require an integral value). CMD-USAGE-001 when the word does
/// not start with a number. An unknown unit is reported to `sink` (Magic's
/// "Unknown ..." text, CMD-USAGE-001 as a warning-level note) and the value
/// is taken as internal units. `relative`: a size or offset, so grid units
/// add no grid origin.
[[nodiscard]] diag::Outcome<double> parse_coordinate(std::string_view text, const CoordinateUnits& units,
                                                     Axis axis = Axis::x, bool relative = false,
                                                     diag::DiagnosticSink* sink = nullptr);

}  // namespace hefesto::cmd
