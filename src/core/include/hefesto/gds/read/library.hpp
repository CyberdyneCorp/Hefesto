// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The library level of the GDS reader (capability gds-read "Library
// grammar and header processing", "Units and input scale", "Coordinate
// conversion and automatic rescaling", "Structure timestamps";
// openspec/changes/m5-mask-gds-cif/design.md D11): the header records up to
// UNITS, the library cell name, the read scale num/den, the conversion of
// XY values to input units with automatic rescaling, the BGNSTR creation
// date and the ENDLIB check. The structure and element handlers of the
// read (tasks.md 9.3-9.5) build on these; the rescaling of planes and style
// distances that a conversion asks for is the read engine's (INPUT).
//
// Owner: GDSR (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/stream/records.hpp"

namespace hefesto::gds {

/// Magic's printable name of a record type (calmaRecordName); a number
/// when the type has no name.
[[nodiscard]] std::string record_name(std::uint8_t type);

/// "Unexpected record type in input: Expected <wanted> record but got
/// <got>." (calmaUnexpected), code GDS-READ-001.
[[nodiscard]] diag::Diagnostic unexpected_record(RecordType wanted, std::uint8_t got);

/// The header of a library: HEADER, BGNLIB, LIBNAME and UNITS.
struct LibraryHeader {
  std::int16_t version = 0;      // HEADER
  std::string libname;           // LIBNAME as written (up to the first NUL)
  double user_units = 0.0;       // UNITS: user units per DB unit (ignored)
  double meters_per_unit = 0.0;  // UNITS: meters per DB unit (mpd)

  friend bool operator==(const LibraryHeader&, const LibraryHeader&) = default;
};

/// "Library written using GDS-II Release <v>" for a HEADER version.
[[nodiscard]] std::string release_message(std::int16_t version);

/// Reads the library header (steps 1-4 of "Library grammar and header
/// processing"): HEADER, BGNLIB, the skipped LIBDIRSIZE/SRFNAME/LIBSECUR,
/// LIBNAME, the skipped FORMAT/MASK/ENDMASKS/REFLIBS/FONTS/ATTRTABLE/
/// STYPTABLE/GENERATIONS and UNITS. The release message goes to `sink`
/// (GDS-READ-004). A wrong record (GDS-READ-001) or the end of the stream
/// (GDS-READ-002) is the error; the reader is then past the bad record.
[[nodiscard]] diag::Outcome<LibraryHeader> read_library_header(RecordReader& reader, diag::DiagnosticSink& sink);

/// Step 5: consumes the record after the last structure, which must be
/// ENDLIB (GDS-READ-001 otherwise, GDS-READ-002 at the end of the stream).
[[nodiscard]] diag::Status read_library_end(RecordReader& reader);

/// The library cell name of step 3.
struct LibraryName {
  std::optional<std::string> name;  // nullopt with `gds library yes` or an empty LIBNAME
  bool modified = false;            // a space or '/' was changed ("Library name modified ...")
};

/// Spaces become '_' and only the part after the last '/' is kept; with
/// `library` (the `gds library` option) the name is discarded.
[[nodiscard]] LibraryName library_cell_name(std::string_view libname, bool library);

/// The read scale: an input-unit value is `DB value * num / den`.
struct ReadScale {
  std::int64_t num = 1;
  std::int64_t den = 1;

  friend bool operator==(const ReadScale&, const ReadScale&) = default;
};

/// The largest num or den a scale holds. Magic's int conversion of a huge
/// or non-finite ratio is undefined; Hefesto clamps to [1, kMaxScale], so
/// that every XY product fits 64 bits.
inline constexpr std::int64_t kMaxScale = std::int64_t{1} << 30;

/// calmaParseUnits: cu = mpd * 1e8 * multiplier; num = (int)(cu + 0.5),
/// den = 1 when cu >= 1, else num = 1, den = (int)(1 / cu + 0.5).
[[nodiscard]] ReadScale units_scale(double meters_per_unit, std::int64_t multiplier);

/// The result of converting one XY value.
struct Conversion {
  std::int64_t value = 0;    // in input units
  std::int64_t rescale = 1;  // r > 1: num was multiplied by r ("units rescaled by factor of r / 1")
  bool rounded = false;      // "calma units at max scale; value rounded"

  friend bool operator==(const Conversion&, const Conversion&) = default;
};

/// calmaReadX/calmaReadY: converts XY values at the current scale and
/// rescales num when a value falls between input units and num * r stays
/// within the rescale limit (`cif limit`, at most 100). After a conversion
/// with rescale r the caller multiplies every input-unit quantity it holds
/// by r: the read planes, saved flatten planes, S, the multiplier and the
/// operator distances (the read engine), and the points of the element
/// read so far (the element handler).
class CoordinateScale {
 public:
  static constexpr std::int64_t kDefaultLimit = 100;  // CIFMAXRESCALE

  explicit CoordinateScale(ReadScale scale, std::int64_t limit = kDefaultLimit);

  /// `v` times the coordinate multiplier `k` (1 for BOUNDARY, BOX, TEXT,
  /// SREF and AREF points, 2 for PATH centerlines, 0 for points that are
  /// consumed without storing) in input units.
  [[nodiscard]] Conversion convert(std::int32_t v, std::int64_t k);

  [[nodiscard]] ReadScale scale() const noexcept { return scale_; }
  /// Replaces the scale (a magnified copy reads at a temporary scale),
  /// clamped to [1, kMaxScale] as the constructor does.
  void set_scale(ReadScale scale) noexcept;

 private:
  ReadScale scale_;
  std::int64_t limit_;
};

/// The creation date of a BGNSTR record (calmaReadStampRecord).
struct StructureDate {
  std::int64_t time = 0;      // 0 when the format is unknown
  bool known_format = false;  // false: "Unknown timestamp format; setting timestamp to zero." (GDS-READ-005)
};

/// "Unknown timestamp format;  setting timestamp to zero." (GDS-READ-005).
[[nodiscard]] diag::Diagnostic unknown_timestamp_format();

/// The creation time of `bgnstr` (its first six I2 values) in the host
/// zone or the pinned `zone`, by Q-MASK-134 (see gds::decode_date).
[[nodiscard]] StructureDate structure_date(const Record& bgnstr, std::string_view zone, bool magic_compat);

/// The timestamp a structure gives its cell.
struct CellStamp {
  std::int64_t value = 0;
  bool fixed = false;  // CDFIXEDSTAMP: later edits keep the stamp

  friend bool operator==(const CellStamp&, const CellStamp&) = default;
};

/// The cell timestamp of a structure dated `date` under the session date
/// stamp `session_stamp` (nullopt: `datestamp yes`, use the GDS date; 0:
/// `datestamp no`; N: N, and a nonzero N fixes the stamp).
[[nodiscard]] CellStamp cell_stamp(std::int64_t date, std::optional<std::int64_t> session_stamp);

}  // namespace hefesto::gds
