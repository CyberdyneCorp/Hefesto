// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASR-private: the pieces of oasis::read (capability oasis-read;
// openspec/changes/m6-lef-def-oasis/design.md D12).
//
// The reader is GDS-equivalent by construction. Pass 1 (index.cpp) decodes
// the whole file with the oasis-format Decoder and indexes the name
// tables, the complete cells, their placements and the file properties.
// Pass 2 (transcode*.cpp) decodes it again and writes, for every element
// instance, the GDS-equivalent element this capability assigns to it into
// an in-memory GDS stream: one structure per CELL record, in file order.
// gds::read then reads that stream, so every rule oasis-read inherits from
// gds-read (layer mapping, units and rescaling, structures, duplicates,
// post-order reading, flattening, magnified copies, placement rules,
// instance properties, labels and ports, read-only views, messages) is the
// M5 reader's own code, reached through its public entry point (design
// D17), never copied. read.cpp then turns the GDS_* vendor properties into
// OASIS_FILE/OASIS_CELL, assigns the OASIS content stamps and selects the
// top cell; messages.cpp gives the messages their OASIS byte positions and
// applies the `oasis warning` level.
//
// Owner: OASR (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/mask/input/warnings.hpp"
#include "hefesto/oasis/codec/decoder.hpp"
#include "hefesto/oasis/read/limits.hpp"
#include "hefesto/oasis/read/read.hpp"

namespace hefesto::gds {
class ByteSource;
}

namespace hefesto::mask {
class InputStyle;
}

namespace hefesto::oasis::read_detail {

/// GDS record limits that bound a GDS-equivalent element (oasis-read
/// "Input limits"). The stream and instance caps are oasis::ReadLimits.
inline constexpr std::size_t kMaxRecordPayload = 0xffff - 4;            // 65531 bytes
inline constexpr std::size_t kMaxPoints = kMaxRecordPayload / 8;        // 8191 XY points, the closing one included
inline constexpr std::size_t kMaxString = kMaxRecordPayload & ~std::size_t{1};  // 65530, padded even
inline constexpr std::int64_t kMaxGdsInt2 = 32767;

/// Where a record is in the decompressed stream (oasis-read "Read
/// diagnostics"): its file offset, or the file offset of the CBLOCK holding
/// it and its position in the CBLOCK's data.
struct StreamPosition {
  std::uint64_t offset = 0;
  bool in_cblock = false;
  std::uint64_t inner = 0;

  [[nodiscard]] static StreamPosition of(const Record& r) noexcept { return {r.offset, r.in_cblock, r.cblock_position}; }
  friend bool operator==(const StreamPosition&, const StreamPosition&) = default;
};

/// "1234", or "1000+56" inside a CBLOCK.
[[nodiscard]] std::string position_text(const StreamPosition& p);

/// A name given inline or by CELLNAME reference number.
struct NameRef {
  std::string name;
  std::optional<std::uint64_t> reference;
};

/// The resolved cell name: inline, from CELLNAME, or `#<n>` (Q-OASIS-34).
[[nodiscard]] std::string cell_name(const NameTables& names, const NameRef& ref);

/// A string property value as text: a string, or a PROPSTRING reference
/// resolved (nullopt for numbers and undefined references).
[[nodiscard]] std::optional<std::string> string_value(const NameTables& names, const PropertyValue& value);

/// The name of a PROPERTY record (inline or by PROPNAME reference).
[[nodiscard]] std::string property_name(const NameTables& names, const Property& property);

/// One CELL record that pass 1 saw complete.
struct IndexedCell {
  std::string name;
  StreamPosition position;
  std::vector<std::string> children;  // the cells its PLACEMENTs name, in record order
};

/// Pass 1 (oasis-read "Two-pass decoding and name resolution").
struct FileIndex {
  double unit = 0.0;
  NameTables names;
  std::vector<IndexedCell> cells;       // complete cells, in file order
  std::vector<std::string> top_cells;   // S_TOP_CELL values, in record order
  std::vector<diag::Diagnostic> notes;  // the decoder's warnings and non-fatal errors
  /// The error that stopped decoding, prefixed with its cell and position.
  std::optional<diag::Diagnostic> fatal;
  bool started = false;                 // START decoded: the file is OASIS
};

/// Decodes `source` from its start.
[[nodiscard]] FileIndex index_file(gds::ByteSource& source);

/// oasis-read "Top cell selection" over the cells of `index`.
[[nodiscard]] std::optional<std::string> top_cell(const FileIndex& index);

/// The message counters of one read (gds-read "Read diagnostics", the
/// CalmaReadError, CIFReadError and CIFReadWarning counters under the
/// `oasis warning` level), fed by OASR's own messages and by the GDS
/// reader's, whose byte positions are mapped back to OASIS positions.
class ReadMessages {
 public:
  explicit ReadMessages(mask::WarningSettings warnings) : warnings_(std::move(warnings)) {}

  /// A read error: "Error while reading cell "<cell>" (byte position <p>): ".
  void counted(std::string_view cell, const StreamPosition& position, diag::Diagnostic d);
  /// A message printed whatever the level.
  void print(diag::Diagnostic d);

  /// The GDS-equivalent stream's offsets and the OASIS positions they come
  /// from, in increasing order of offset.
  using PositionMap = std::vector<std::pair<std::uint64_t, StreamPosition>>;
  /// Takes over the messages of gds::read (run at the default level).
  void forward(std::vector<diag::Diagnostic> gds_messages, const PositionMap& positions);

  [[nodiscard]] std::vector<diag::Diagnostic> take() { return std::move(printed_); }
  [[nodiscard]] const std::string& redirected() const noexcept { return redirected_; }

 private:
  enum class Kind : std::uint8_t { counted, detail, cif_error, cif_warning, print };
  void route(Kind kind, diag::Diagnostic d);
  [[nodiscard]] bool admit(std::int64_t count, std::string_view limit_text);
  void emit(diag::Diagnostic d);

  mask::WarningSettings warnings_;
  std::int64_t calma_errors_ = 0;
  std::int64_t cif_errors_ = 0;
  std::int64_t cif_warnings_ = 0;
  std::vector<diag::Diagnostic> printed_;
  std::string redirected_;
};

/// Pass 2's output.
struct Transcoded {
  std::vector<std::byte> gds;
  ReadMessages::PositionMap positions;
  /// The STRNAME end offset (GDS_START) of each structure and the OASIS
  /// cell it was made from.
  std::vector<std::pair<std::uint64_t, std::string>> structures;
};

struct TranscodeOptions {
  std::int32_t circle_points = 64;
  /// The cifinput style: a RECTANGLE on an unmapped pair is written as a
  /// BOX, whose message gds-read words "in box" (oasis-read "Layer and
  /// datatype mapping" rule 3); a mapped one is a BOUNDARY, which takes
  /// part in pin-shape association ("RECTANGLE elements").
  const mask::InputStyle* style = nullptr;
  std::string top_cell;  // the LIBNAME, so gds-read's library cell rule loads it
  ReadLimits limits;     // the stream and instance caps (OASIS-READ-007)
};

/// Writes the GDS-equivalent stream of the cells of `index`, decoding
/// `source` again from its start.
[[nodiscard]] Transcoded transcode(gds::ByteSource& source, const FileIndex& index, const TranscodeOptions& options,
                                   ReadMessages& messages);

}  // namespace hefesto::oasis::read_detail
