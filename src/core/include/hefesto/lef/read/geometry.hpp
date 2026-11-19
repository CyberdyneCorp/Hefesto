// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF geometry statements shared by the LEF reader's sections (capability
// lef-read "LEF unit conversion", "PORT and OBS geometry"; design D6):
// LefReadRect, LefReadLefPoint and LefReadPolygon, used by the LAYER, VIA,
// PORT and OBS bodies.
//
// Owner: LEFR (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/lef/lex/statements.hpp"

namespace hefesto::lef {

/// LefReadRect: reads `x1 y1 x2 y2`, each point optionally in parentheses,
/// converts the corners with `scale` (round(v / scale); a caller reading
/// via geometry passes scale / 2) and returns the canonical rectangle.
/// - a missing or non-numeric value or a missing `)` reports "Bad port
///   geometry: RECT requires 4 values." (LEF-READ-001) and answers nullopt;
/// - when `layer_known` is false it reports "No layer defined for RECT."
///   and answers the empty rectangle (0, 0, 0, 0), as Magic does;
/// - a corner that does not fit an internal coordinate answers nullopt
///   without a message (Magic's int cast is undefined there).
std::optional<db::Rect> read_rect(Lexer& lexer, const MessageTarget& messages, bool layer_known, float scale);

/// read_rect whose first token was already read (`first`; nullopt: the
/// input ended), for a RECT whose first token was checked for MASK or
/// ITERATE.
std::optional<db::Rect> read_rect(Lexer& lexer, const MessageTarget& messages, bool layer_known, float scale,
                                  std::optional<Token> first);

/// LefReadLefPoint's answer.
enum class PointRead : std::uint8_t {
  point,    // 0: a point was read
  none,     // -1: the next token was `;` (consumed) or the input ended
  invalid,  // 1: a value was not a number or `)` was missing
};

/// LefReadLefPoint: `x y` or `( x y )`, as two binary32 values.
PointRead read_point(Lexer& lexer, float& x, float& y);

/// LefReadPolygon: `x y` pairs up to `;`, each converted to round(v /
/// scale) plus `offset`. A bad value reports "Bad X value in <what>." or
/// "Bad Y value in <what>." and skips the statement; a missing y reports
/// "Missing Y value in <what> point!" (LEF-READ-001). The points read
/// before an error are kept; a point that does not fit an internal
/// coordinate is dropped (Magic's int conversion is undefined there).
std::vector<db::Point> read_points(Lexer& lexer, const MessageTarget& messages, float scale, db::Point offset,
                                   std::string_view what = "polygon");

}  // namespace hefesto::lef
