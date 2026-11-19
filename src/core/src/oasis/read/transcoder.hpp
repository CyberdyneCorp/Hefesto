// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASR-private: pass 2 of the OASIS reader, the GDS-equivalent stream
// (capability oasis-read: the mapping of each OASIS element instance to its
// GDS-equivalent element). transcode.cpp holds the cell and record loop and
// the GDS record builder, transcode_shapes.cpp the geometry and TEXT
// elements, transcode_placements.cpp the PLACEMENT elements.
//
// Owner: OASR (openspec/changes/m6-lef-def-oasis/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "hefesto/gds/stream/records.hpp"
#include "internal.hpp"

namespace hefesto::oasis::read_detail {

/// GDSII records appended to a byte vector, so every record's offset is
/// known (the position map and GDS_START need them).
class GdsBuilder {
 public:
  void record(gds::RecordType type, gds::DataType data, std::span<const std::byte> payload);
  void empty(gds::RecordType type);
  void int2(gds::RecordType type, std::span<const std::int16_t> values);
  void int4(gds::RecordType type, std::span<const std::int32_t> values);
  void real8(gds::RecordType type, std::span<const double> values);
  /// A string record; `text` is at most kMaxString bytes.
  void string(gds::RecordType type, std::string_view text);

  [[nodiscard]] std::uint64_t size() const noexcept { return bytes_.size(); }
  [[nodiscard]] std::vector<std::byte> take() { return std::move(bytes_); }

 private:
  std::vector<std::byte> bytes_;
};

/// A coordinate pair of a GDS XY record.
struct Xy {
  std::int32_t x = 0;
  std::int32_t y = 0;
};

/// `base + offset`, or nullopt outside the signed 32-bit range.
[[nodiscard]] std::optional<std::int32_t> coordinate(std::int64_t base, std::int64_t offset) noexcept;

/// A layer, datatype or attribute number as a GDS INT2: numbers above
/// 32767 become 32767, which matches only a `*` wildcard, as any number
/// above 255 does (Q-OASIS-39).
[[nodiscard]] std::int16_t int2_number(std::uint64_t value) noexcept;

class Transcoder {
 public:
  Transcoder(const FileIndex& index, const TranscodeOptions& options, ReadMessages& messages);

  [[nodiscard]] Transcoded run(gds::ByteSource& source);

 private:
  struct Placement {
    Element element;
    StreamPosition position;
    std::string child;  // the placed cell's name, as written
    std::vector<std::pair<std::int16_t, std::string>> properties;  // S_GDS_PROPERTY pairs
  };

  // --- records and cells (transcode.cpp) ---
  void library_begin();
  /// Takes one decoded record; false once the read must stop.
  bool take(const Record& r);
  bool take_cell(const Record& r);
  void begin_cell(const Record& r);
  void end_cell();
  void element(const Element& e);
  void unsupported(std::string_view kind);
  void property(const Record& r);
  void error(std::string_view code, std::string text);
  /// Whether another instance fits under kMaxStreamBytes (reported once).
  bool room();

  // --- geometry and TEXT (transcode_shapes.cpp) ---
  void shape(const Element& e);
  void boundary(const Element& e, std::span<const Point64> outline, Point64 base);
  void path(const Element& e, Point64 base);
  void text(const Element& e, Point64 base);
  /// The position of instance `i` of `e`; nullopt (reported) past 64 bits.
  [[nodiscard]] std::optional<Point64> instance_base(const Element& e, std::uint64_t i);
  /// "coordinate overflow", once per element.
  void overflow();
  /// The TEXT string of `e`, cut to kMaxString.
  [[nodiscard]] std::string text_string(const Element& e);
  /// The vertices of `outline` (relative) at `base`, closed when `close`.
  [[nodiscard]] std::optional<std::vector<std::int32_t>> points(std::span<const Point64> outline, Point64 base,
                                                                bool close);
  [[nodiscard]] std::vector<Point64> shape_outline(const Element& e);

  // --- PLACEMENT (transcode_placements.cpp) ---
  void flush_placement();
  void placement_properties(const Property& p);
  void reference(const Placement& p, Point64 base);
  /// One AREF for a regular repetition with axis-aligned vectors in the
  /// child's coordinates; false when the repetition is not one.
  bool array(const Placement& p);
  void separate_uses(const Placement& p);
  void reference_head(gds::RecordType type, const Placement& p);
  void reference_tail(const Placement& p, std::span<const std::int32_t> xy,
                      std::optional<std::string> limits);
  [[nodiscard]] std::string child_name(const Element& e);

  const FileIndex& index_;
  TranscodeOptions options_;
  ReadMessages& messages_;
  GdsBuilder gds_;
  Transcoded out_;
  std::size_t next_cell_ = 0;  // index_.cells entries begun so far
  bool in_cell_ = false;
  bool capped_ = false;
  std::string cell_;            // the cell being transcoded
  StreamPosition position_;     // the record being transcoded
  std::optional<Placement> pending_;
  std::unordered_set<std::string> reported_;  // unsupported record kinds of this cell
  bool xname_reported_ = false;
  bool overflow_reported_ = false;  // for the element being transcoded
  std::string text_;                // its TEXT string
  std::uint64_t instances_ = 0;     // element instances and uses written (kMaxInstances)
};

}  // namespace hefesto::oasis::read_detail
