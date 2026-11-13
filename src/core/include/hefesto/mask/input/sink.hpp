// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ReadSink: the interface between the format parsers (CIF, GDS) and the
// shared read engine (capability mask-input-styles;
// openspec/changes/m5-mask-gds-cif/design.md D4 "Input", D9). The CIF and
// GDS parsers drive it in file order; ReadEngine implements it (read
// planes, scale, rescale, paint-current); tests use a recording sink.
// Coordinates are in the parser's read units, already multiplied by the
// input style's scale where the format says so.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); INPUT owns
// the engine behind it (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/mask/plane/plane.hpp"

namespace hefesto::mask {

/// The geometry of one paint call: a rectangle (CIF B, GDS BOX or a
/// rectangular BOUNDARY) or a polygon (CIF P/W outline, GDS BOUNDARY/PATH).
using ReadShape = std::variant<db::Rect, Polygon>;

/// A label read from the file (CIF 94/95, GDS TEXT).
struct ReadLabel {
  std::string text;
  db::Rect area;                        // a point label has zero width and height
  std::optional<std::size_t> cif_layer; // the input layer it was read on, if any
  std::int32_t size = 0;                // font size in read units (0 = default)
  std::int32_t rotation = 0;            // degrees
  std::int32_t position = 0;            // GEO position (0 = center)
  bool port = false;                    // a port label (GDS pin datatype, CIF port extension)

  friend bool operator==(const ReadLabel&, const ReadLabel&) = default;
};

class ReadSink {
 public:
  ReadSink() = default;
  ReadSink(const ReadSink&) = delete;
  ReadSink& operator=(const ReadSink&) = delete;
  virtual ~ReadSink() = default;

  /// Starts the definition of cell `name` (CIF DS, GDS BGNSTR/STRNAME).
  [[nodiscard]] virtual diag::Status begin_cell(std::string_view name) = 0;
  /// Paints `shape` on input layer `cif_layer` of the current cell.
  [[nodiscard]] virtual diag::Status paint(std::size_t cif_layer, const ReadShape& shape) = 0;
  /// Adds a label to the current cell.
  [[nodiscard]] virtual diag::Status label(const ReadLabel& label) = 0;
  /// Places `child` in the current cell (CIF C, GDS SREF/AREF); `array`
  /// is the identity for a single placement.
  [[nodiscard]] virtual diag::Status call(std::string_view child, const db::Transform& transform,
                                          const db::ArrayInfo& array) = 0;
  /// Sets a property of the current cell (GDS PROPATTR/PROPVALUE, CIF
  /// user extensions).
  [[nodiscard]] virtual diag::Status property(std::string_view key, std::string_view value) = 0;
  /// Ends the current cell (CIF DF, GDS ENDSTR).
  [[nodiscard]] virtual diag::Status end_cell() = 0;
};

}  // namespace hefesto::mask
