// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The library level of the GDS reader (see hefesto/gds/read/library.hpp).
// Ported from Magic 8.3.694 calma/CalmaRead.c (CalmaReadFile,
// calmaParseUnits, calmaUnexpected, calmaRecordName), calma/CalmaRdio.c
// (calmaReadI2Record, calmaReadStringRecord, calmaReadStampRecord,
// calmaSkipSet, calmaSkipExact) and calma/CalmaRdpt.c (calmaReadX,
// calmaReadY); Magic is (c) the Regents of the University of California,
// distributed under its permissive license (THIRD_PARTY_NOTICES.md).
//
// Records are read whole, so a record whose payload is longer than its
// type needs is skipped as one record (Magic reads only the values it
// needs and would take the rest as the next header). Owner: GDSR.

#include "hefesto/gds/read/library.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <numeric>

#include "hefesto/gds/stream/dates.hpp"
#include "hefesto/gds/stream/values.hpp"

namespace hefesto::gds {

namespace {

constexpr std::array<std::string_view, 60> kRecordNames{
    "HEADER",     "BGNLIB",   "LIBNAME",   "UNITS",     "ENDLIB",      "BGNSTR",     "STRNAME",   "ENDSTR",
    "BOUNDARY",   "PATH",     "SREF",      "AREF",      "TEXT",        "LAYER",      "DATATYPE",  "WIDTH",
    "XY",         "ENDEL",    "SNAME",     "COLROW",    "TEXTNODE",    "NODE",       "TEXTTYPE",  "PRESENTATION",
    "SPACING",    "STRING",   "STRANS",    "MAG",       "ANGLE",       "UINTEGER",   "USTRING",   "REFLIBS",
    "FONTS",      "PATHTYPE", "GENERATIONS", "ATTRTABLE", "STYPTABLE", "STRTYPE",    "ELFLAGS",   "ELKEY",
    "LINKTYPE",   "LINKKEYS", "NODETYPE",  "PROPATTR",  "PROPVALUE",   "BOX",        "BOXTYPE",   "PLEX",
    "BGNEXTN",    "ENDEXTN",  "TAPENUM",   "TAPECODE",  "STRCLASS",    "RESERVED",   "FORMAT",    "MASK",
    "ENDMASKS",   "LIBDIRSIZE", "SRFNAME", "LIBSECUR"};

// Record types past ENDMASKS that the header may skip.
constexpr std::uint8_t kLibdirsize = 57;
constexpr std::uint8_t kSrfname = 58;
constexpr std::uint8_t kLibsecur = 59;

diag::Diagnostic unexpected_eof() {
  return diag::make("GDS-READ-002", "Unexpected EOF.");
}

/// The next record, or GDS-READ-002 at the end of the stream.
diag::Outcome<Record> next_record(RecordReader& reader) {
  auto record = reader.next();
  if (!record.ok()) {
    return record.error();
  }
  if (!record.value()) {
    return unexpected_eof();
  }
  return std::move(*record.value());
}

/// The next record, which must be of type `wanted`.
diag::Outcome<Record> expect(RecordReader& reader, RecordType wanted) {
  auto record = next_record(reader);
  if (record.ok() && record.value().type != static_cast<std::uint8_t>(wanted)) {
    return unexpected_record(wanted, record.value().type);
  }
  return record;
}

/// calmaSkipSet: consumes records while their type is one of `types`.
diag::Status skip_set(RecordReader& reader, std::initializer_list<std::uint8_t> types) {
  for (;;) {
    auto record = reader.peek();
    if (!record.ok()) {
      return record.error();
    }
    if (!record.value() || std::find(types.begin(), types.end(), record.value()->type) == types.end()) {
      return diag::Done{};
    }
    static_cast<void>(reader.next());  // the look-ahead record: cannot fail
  }
}

std::uint8_t type_of(RecordType t) {
  return static_cast<std::uint8_t>(t);
}

diag::Outcome<std::int16_t> read_version(RecordReader& reader) {
  auto header = expect(reader, RecordType::header);
  if (!header.ok()) {
    return header.error();
  }
  const auto version = i2_at(header.value().data, 0);
  if (!version) {
    return unexpected_eof();
  }
  return *version;
}

diag::Status read_units(RecordReader& reader, LibraryHeader& out) {
  auto units = expect(reader, RecordType::units);
  if (!units.ok()) {
    return units.error();
  }
  const auto user = r8_at(units.value().data, 0);
  const auto meters = r8_at(units.value().data, 1);
  if (!user || !meters) {
    return unexpected_eof();
  }
  out.user_units = *user;
  out.meters_per_unit = *meters;
  return diag::Done{};
}

/// (int)(x + 0.5) clamped to [1, kMaxScale]; NaN gives 1.
std::int64_t round_scale(double x) {
  const double rounded = x + 0.5;
  if (!(rounded >= 1.0)) {
    return 1;
  }
  if (rounded >= static_cast<double>(kMaxScale)) {
    return kMaxScale;
  }
  return static_cast<std::int64_t>(rounded);
}

}  // namespace

std::string record_name(std::uint8_t type) {
  return type < kRecordNames.size() ? std::string{kRecordNames[type]} : std::to_string(type);
}

diag::Diagnostic unexpected_record(RecordType wanted, std::uint8_t got) {
  return diag::make("GDS-READ-001", "Unexpected record type in input: Expected " + record_name(type_of(wanted)) +
                                        " record but got " + record_name(got) + ".");
}

std::string release_message(std::int16_t version) {
  if (version < 600) {
    return "Library written using GDS-II Release " + std::to_string(version) + ".0";
  }
  return "Library written using GDS-II Release " + std::to_string(version / 100) + "." +
         std::to_string(version % 100);
}

diag::Outcome<LibraryHeader> read_library_header(RecordReader& reader, diag::DiagnosticSink& sink) {
  LibraryHeader out;
  auto version = read_version(reader);
  if (!version.ok()) {
    return version.error();
  }
  out.version = version.value();
  sink.emit(diag::make("GDS-READ-004", release_message(out.version)));
  if (auto bgnlib = expect(reader, RecordType::bgnlib); !bgnlib.ok()) {
    return bgnlib.error();
  }
  if (auto status = skip_set(reader, {kLibdirsize, kSrfname, kLibsecur}); !status.ok()) {
    return status.error();
  }
  auto libname = expect(reader, RecordType::libname);
  if (!libname.ok()) {
    return libname.error();
  }
  out.libname = string_value(libname.value().data);
  const auto skipped = {type_of(RecordType::format),    type_of(RecordType::mask),
                        type_of(RecordType::endmasks),  type_of(RecordType::reflibs),
                        type_of(RecordType::fonts),     type_of(RecordType::attrtable),
                        type_of(RecordType::styptable), type_of(RecordType::generations)};
  if (auto status = skip_set(reader, skipped); !status.ok()) {
    return status.error();
  }
  if (auto status = read_units(reader, out); !status.ok()) {
    return status.error();
  }
  return out;
}

diag::Status read_library_end(RecordReader& reader) {
  auto record = expect(reader, RecordType::endlib);
  if (!record.ok()) {
    return record.error();
  }
  return diag::Done{};
}

LibraryName library_cell_name(std::string_view libname, bool library) {
  LibraryName out;
  if (library || libname.empty()) {
    return out;
  }
  std::string name{libname};
  if (name.find(' ') != std::string::npos) {
    std::replace(name.begin(), name.end(), ' ', '_');
    out.modified = true;
  }
  if (const auto slash = name.rfind('/'); slash != std::string::npos) {
    name.erase(0, slash + 1);
    out.modified = true;
  }
  out.name = std::move(name);
  return out;
}

ReadScale units_scale(double meters_per_unit, std::int64_t multiplier) {
  const double cu = meters_per_unit * 1.0e8 * static_cast<double>(multiplier);
  if (cu >= 1.0) {
    return ReadScale{round_scale(cu), 1};
  }
  return ReadScale{1, round_scale(1.0 / cu)};
}

CoordinateScale::CoordinateScale(ReadScale scale, std::int64_t limit) : scale_(scale), limit_(limit) {
  scale_.num = std::clamp<std::int64_t>(scale_.num, 1, kMaxScale);
  scale_.den = std::clamp<std::int64_t>(scale_.den, 1, kMaxScale);
}

Conversion CoordinateScale::convert(std::int32_t v, std::int64_t k) {
  Conversion out;
  std::int64_t w = std::int64_t{v} * (scale_.num * k);
  if (k != 0 && w % scale_.den != 0) {
    const std::int64_t r = scale_.den / std::gcd(scale_.den, w < 0 ? -w : w);
    if (scale_.num * r > limit_) {
      out.rounded = true;
      w += w < 0 ? -((scale_.den - 1) >> 1) : (scale_.den >> 1);
    } else {
      scale_.num *= r;
      out.rescale = r;
      w *= r;
    }
  }
  out.value = w / scale_.den;
  return out;
}

diag::Diagnostic unknown_timestamp_format() {
  return diag::make("GDS-READ-005", "Unknown timestamp format;  setting timestamp to zero.");
}

StructureDate structure_date(const Record& bgnstr, std::string_view zone, bool magic_compat) {
  if (bgnstr.data.size() != 24) {
    return StructureDate{};
  }
  std::array<std::int16_t, 6> fields{};
  for (std::size_t i = 0; i < fields.size(); ++i) {
    fields[i] = *i2_at(bgnstr.data, i);
  }
  return StructureDate{decode_date(fields, zone, magic_compat), true};
}

CellStamp cell_stamp(std::int64_t date, std::optional<std::int64_t> session_stamp) {
  if (!session_stamp) {
    return CellStamp{date, false};
  }
  return CellStamp{*session_stamp, *session_stamp != 0};
}

}  // namespace hefesto::gds
