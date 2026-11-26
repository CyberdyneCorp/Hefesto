// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The decoded OASIS geometry vocabulary (capability oasis-format;
// openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D4, D11), shared by the
// reader, the writer's transcoders and gds/write/vendor_oasis.cpp:
// - Point64: OASIS coordinates, which are 64-bit signed in the file; the
//   reader range-checks them against the database's 32-bit grid;
// - Repetition: the twelve repetition types (0 = reuse the modal one) with
//   their parameters, and their lazy expansion into offsets;
// - Element: one PLACEMENT, TEXT, geometry or extension record after modal
//   resolution;
// - outline: the vertices of a RECTANGLE, POLYGON, TRAPEZOID or CTRAPEZOID.
// ModalState (oasis/codec/modal.hpp) holds the modal variables these are
// resolved against.
//
// Owner: OASFMT (design D2; the header was created by the M6 scaffold).

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

/// A repetition (oasis-format "Repetitions"). Every decoded repetition has
/// type 1-11 (type 0 is resolved to the modal repetition):
/// - grid types 1, 2, 3, 8 and 9 use `columns` x `rows` instances, offset
///   `i * column_step + j * row_step` (row j outer, column i inner); the
///   unused step of types 2, 3 and 9 is (0, 0) and the unused count is 1;
/// - list types 4-7, 10 and 11 hold the displacements d_1 .. d_{n-1} of the
///   instances after the first in `offsets` (already multiplied by `grid`,
///   which is nonzero for types 5, 7 and 11 only); `columns` and `rows`
///   stay 1.
/// The counts come from the file and are never expanded eagerly: use
/// instance_count and offset_at, or expand once the count is bounded.
struct Repetition {
  std::uint8_t type = 0;  // 0: reuse the modal repetition
  std::uint64_t columns = 1;
  std::uint64_t rows = 1;
  Point64 column_step;
  Point64 row_step;
  std::vector<Point64> offsets;
  std::uint64_t grid = 0;

  friend bool operator==(const Repetition&, const Repetition&) = default;
};

/// The number of instances of `repetition` (saturates at 2^64 - 1); 1 for
/// type 0.
[[nodiscard]] std::uint64_t instance_count(const Repetition& repetition) noexcept;

/// The offset of instance `index` (< instance_count) in file order. The
/// decoder has checked that every offset fits in 64 bits.
[[nodiscard]] Point64 offset_at(const Repetition& repetition, std::uint64_t index) noexcept;

/// The offsets of every instance of `repetition`, the first (0, 0)
/// included, in file order. It allocates instance_count() points, so the
/// caller bounds the count first (design D21).
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
  xelement,
};

/// One element after modal resolution. Coordinates are absolute (XYRELATIVE
/// already applied).
struct Element {
  ElementKind kind = ElementKind::rectangle;
  std::uint64_t layer = 0;      // layer, or textlayer for TEXT
  std::uint64_t datatype = 0;   // datatype, or texttype for TEXT
  Point64 position;             // x, y
  Point64 size;                 // RECTANGLE/TRAPEZOID/CTRAPEZOID width and height
  /// POLYGON: every vertex after the implicit first one (0, 0), relative to
  /// `position`, the vertex synthesized for a type 0/1 list included; PATH:
  /// the centerline P1..Pn after P0 = (0, 0).
  std::vector<Point64> points;
  /// The point-list type 0-5 read from the file; for the encoder, the type
  /// to write (nullopt: the narrowest type that represents `points`).
  std::optional<std::uint8_t> point_list_type;
  std::int64_t half_width = 0;  // PATH
  std::int64_t start_extension = 0;
  std::int64_t end_extension = 0;
  std::int64_t delta_a = 0;  // TRAPEZOID
  std::int64_t delta_b = 0;
  bool vertical = false;
  /// CTRAPEZOID 0-25. Its `size` holds only the dimensions the type uses;
  /// the unused one is 0.
  std::uint8_t ctrapezoid_type = 0;
  std::int64_t radius = 0;  // CIRCLE
  /// PLACEMENT cell or TEXT string given inline (empty when by reference).
  std::string name;
  /// PLACEMENT CELLNAME or TEXT TEXTSTRING reference number, when the record
  /// referenced the name by refnum (resolve it with NameTables).
  std::optional<std::uint64_t> reference;
  double magnification = 1.0;  // PLACEMENT
  double angle = 0.0;          // degrees, counterclockwise
  bool flip = false;
  std::optional<Repetition> repetition;
  std::uint64_t attribute = 0;  // XELEMENT, XGEOMETRY
  std::string payload;          // XELEMENT, XGEOMETRY b-string, unchanged

  friend bool operator==(const Element&, const Element&) = default;
};

/// The outline of a RECTANGLE, POLYGON, TRAPEZOID or CTRAPEZOID relative to
/// its position, first vertex included, with consecutive coincident
/// vertices of a trapezoid merged (oasis-format "TRAPEZOID records",
/// "CTRAPEZOID record"). Empty for other kinds, for an invalid CTRAPEZOID
/// type and when a vertex does not fit in 64 bits.
[[nodiscard]] std::vector<Point64> outline(const Element& element);

/// Whether CTRAPEZOID `type` reads the width (types 0-19 and 22-25) and the
/// height (types 0-15, 20, 21 and 24).
[[nodiscard]] bool ctrapezoid_uses_width(std::uint32_t type) noexcept;
[[nodiscard]] bool ctrapezoid_uses_height(std::uint32_t type) noexcept;

}  // namespace hefesto::oasis
