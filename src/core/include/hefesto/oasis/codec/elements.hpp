// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The decoded OASIS geometry vocabulary (capability oasis-format;
// openspec/changes/m6-lef-def-oasis/design.md D4, D11), shared by the
// reader, the writer's transcoders and gds/write/vendor_oasis.cpp:
// - Point64: OASIS coordinates, which are 64-bit signed in the file; the
//   reader range-checks them against the database's 32-bit grid;
// - Repetition: the twelve repetition types (0 = reuse the modal one) with
//   their parameters, and their expansion into offsets;
// - Element: one PLACEMENT, TEXT or geometry record after modal resolution.
// ModalState (oasis/codec/modal.hpp) holds the modal variables these are
// resolved against.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASFMT fills
// in the implementation in src/core/src/oasis/codec/ (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hefesto::oasis {

struct Point64 {
  std::int64_t x = 0;
  std::int64_t y = 0;

  friend bool operator==(const Point64&, const Point64&) = default;
};

/// A repetition (OASIS 1.0 section 7.6). `type` 1-3 use the grid fields,
/// 4-11 the explicit `offsets` relative to the first instance.
struct Repetition {
  std::uint8_t type = 0;  // 0: reuse the modal repetition
  std::uint64_t columns = 1;
  std::uint64_t rows = 1;
  Point64 column_step;
  Point64 row_step;
  std::vector<Point64> offsets;

  friend bool operator==(const Repetition&, const Repetition&) = default;
};

/// The offsets of every instance of `repetition`, the first (0, 0)
/// included, in file order. SCAFFOLD: returns {(0, 0)} until OASFMT lands
/// it.
[[nodiscard]] std::vector<Point64> expand(const Repetition& repetition);

enum class ElementKind : std::uint8_t {
  placement,
  text,
  rectangle,
  polygon,
  path,
  trapezoid,
  ctrapezoid,
  circle,
  xgeometry,
};

/// One element after modal resolution.
struct Element {
  ElementKind kind = ElementKind::rectangle;
  std::uint64_t layer = 0;      // layer, or textlayer for TEXT
  std::uint64_t datatype = 0;   // datatype, or texttype for TEXT
  Point64 position;             // x, y
  Point64 size;                 // RECTANGLE/TRAPEZOID width and height
  std::vector<Point64> points;  // POLYGON/PATH point list, relative to position
  std::int64_t half_width = 0;  // PATH
  std::int64_t start_extension = 0;
  std::int64_t end_extension = 0;
  std::int64_t delta_a = 0;  // TRAPEZOID
  std::int64_t delta_b = 0;
  bool vertical = false;
  std::uint8_t ctrapezoid_type = 0;  // CTRAPEZOID 0-25
  std::int64_t radius = 0;           // CIRCLE
  std::string name;                  // PLACEMENT cell or TEXT string
  double magnification = 1.0;        // PLACEMENT
  double angle = 0.0;
  bool flip = false;
  std::optional<Repetition> repetition;

  friend bool operator==(const Element&, const Element&) = default;
};

}  // namespace hefesto::oasis
