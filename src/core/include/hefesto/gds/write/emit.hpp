// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The element stream of one GDS structure (capability gds-write;
// openspec/changes/m5-mask-gds-cif/design.md D12): what the structure body
// writer and the element emitters (gds/write/polygons.hpp,
// gds/write/labels.hpp) write through. An ElementWriter wraps the write's
// RecordWriter with the structure's context (cell, output style, `gds`
// settings, compat, output scale factors) and gives the record forms
// CalmaWrite.c uses:
// - int2: an I2 record holding the low 16 bits of a value (calmaOutI2);
// - real8, string_record (calmaOutStringRecord: the character table, `gds lower`,
//   `string-limit`, with Magic's warnings, never changing the source text,
//   Q-MASK-208), empty records;
// - xy: an XY record whose values are checked against the signed 32-bit
//   range; a value outside it fails the element with GDS-WRITE-004 naming
//   the cell and the value (Q-MASK-222, a fix in both modes);
// - boundary: BOUNDARY, LAYER, DATATYPE, XY, ENDEL.
// Every point handed to xy/boundary is already in output units (the caller
// multiplies by OutputScale::write or ::paint in 64-bit arithmetic).
//
// Owner: GDSW (design D2).

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/command/settings.hpp"
#include "hefesto/gds/stream/encode.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "hefesto/gds/stream/values.hpp"
#include "hefesto/mask/style/style.hpp"

namespace hefesto::tech {
class Technology;
}

namespace hefesto::gds {

/// A point in output (database) units, before the I4 range check.
struct OutputPoint {
  std::int64_t x = 0;
  std::int64_t y = 0;

  friend bool operator==(const OutputPoint&, const OutputPoint&) = default;
};

/// "Output scale factors" of one structure.
struct OutputScale {
  /// Wscale: multiplies internal units (reference positions, array
  /// pitches, label positions and label rectangles).
  std::int64_t write = 1;
  /// Pscale: multiplies mask-plane coordinates (already in output units).
  std::int64_t paint = 1;

  friend bool operator==(const OutputScale&, const OutputScale&) = default;
};

/// What the emitters of one structure read.
struct StructureContext {
  const db::CellDef* cell = nullptr;       // the cell whose structure is written
  const mask::OutputStyle* style = nullptr;
  const GdsSettings* settings = nullptr;   // the write's `gds` settings
  compat::CompatSettings compat;
  OutputScale scale;
  const tech::Technology* technology = nullptr;  // type names in warnings (may be null)
};

class ElementWriter {
 public:
  /// Writes to `records` (which must outlive the writer); warnings go to
  /// `sink` (nullptr: dropped).
  ElementWriter(RecordWriter& records, const StructureContext& context, diag::DiagnosticSink* sink);

  [[nodiscard]] const StructureContext& context() const noexcept { return context_; }
  [[nodiscard]] const db::CellDef& cell() const noexcept { return *context_.cell; }
  [[nodiscard]] const mask::OutputStyle& style() const noexcept { return *context_.style; }
  [[nodiscard]] const GdsSettings& settings() const noexcept { return *context_.settings; }
  [[nodiscard]] const compat::CompatSettings& compat() const noexcept { return context_.compat; }
  [[nodiscard]] const OutputScale& scale() const noexcept { return context_.scale; }
  /// The character table of the style (`calma-permissive-labels`).
  [[nodiscard]] const CharTable& table() const noexcept { return table_; }
  /// The underlying record writer, for records without a helper here.
  [[nodiscard]] RecordWriter& records() noexcept { return records_; }
  /// Where warnings go (nullptr: dropped).
  [[nodiscard]] diag::DiagnosticSink* sink() const noexcept { return sink_; }

  /// A record without data (BOUNDARY, TEXT, SREF, AREF, ENDEL, ...).
  [[nodiscard]] diag::Status empty(RecordType type);
  /// An I2 record holding the low 16 bits of `value` (calmaOutI2, as Magic
  /// writes LAYER, DATATYPE, TEXTTYPE, PRESENTATION, STRANS and PROPATTR).
  [[nodiscard]] diag::Status int2(RecordType type, std::int32_t value);
  /// A BITARRAY record of 16 bits (STRANS, PRESENTATION).
  [[nodiscard]] diag::Status bits(RecordType type, std::uint16_t value);
  /// An R8 record of one value.
  [[nodiscard]] diag::Status real8(RecordType type, double value);
  /// A string record (calmaOutStringRecord) with Magic's warnings.
  [[nodiscard]] diag::Status string_record(RecordType type, std::string_view text);
  /// An XY record; fails with GDS-WRITE-004 when a value is outside I4.
  [[nodiscard]] diag::Status xy(std::span<const OutputPoint> points);
  /// LAYER `layer.layer` and then `second` (DATATYPE or TEXTTYPE) holding
  /// `layer.datatype`.
  [[nodiscard]] diag::Status layer_records(RecordType second, const mask::GdsLayer& layer);
  /// BOUNDARY, LAYER, DATATYPE, XY of `points`, ENDEL.
  [[nodiscard]] diag::Status boundary(const mask::GdsLayer& layer, std::span<const OutputPoint> points);

  /// A warning of the write (GDS-WRITE-006, or `code`).
  void warn(std::string text, std::string_view code = {});

  /// The value range check of Q-MASK-222: `value` when it fits in I4, else
  /// the GDS-WRITE-004 error naming the cell and `value`.
  [[nodiscard]] diag::Outcome<std::int32_t> checked_i4(std::int64_t value) const;

 private:
  RecordWriter& records_;
  StructureContext context_;
  diag::DiagnosticSink* sink_;
  CharTable table_;
};

/// Saturating 64-bit product and sum for output arithmetic: a saturated
/// value is far outside I4, so ElementWriter's range check rejects it
/// (Q-MASK-222) instead of the arithmetic overflowing.
[[nodiscard]] std::int64_t saturating_mul(std::int64_t a, std::int64_t b) noexcept;
[[nodiscard]] std::int64_t saturating_add(std::int64_t a, std::int64_t b) noexcept;

/// The StringOptions of `style` and `settings` (string-limit, `gds lower`).
[[nodiscard]] StringOptions string_options(const mask::OutputStyle& style, const GdsSettings& settings) noexcept;
/// The character table of `style` under `compat`.
[[nodiscard]] CharTable char_table(const mask::OutputStyle& style, const compat::CompatSettings& compat);

}  // namespace hefesto::gds
