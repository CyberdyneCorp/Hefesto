// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF geometry statements shared by the LEF reader's sections (capability
// lef-read "LEF unit conversion", "PORT and OBS geometry"; design D6):
// LefReadRect. LAYER sections read it today; PORT, OBS and VIA geometry
// (tasks.md 4.1, 5.4) use it too.
//
// Owner: LEFR (design D2).

#pragma once

#include <optional>

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

}  // namespace hefesto::lef
