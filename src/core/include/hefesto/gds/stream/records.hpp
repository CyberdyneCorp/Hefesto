// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDSII records (capabilities gds-read, gds-write;
// openspec/changes/m5-mask-gds-cif/design.md D4 "GDS", D10): the record
// reader with one-record look-ahead (CalmaRdio.c's READRH/UNREADRH), which
// reports each record's offset in the decompressed stream, and the record
// writer (CalmaWrite.c's calmaOutRH and friends), which buffers 64 KB.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GDSR fills
// in the implementation in src/core/src/gds/stream/records.cpp (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/gds/stream/io.hpp"

namespace hefesto::gds {

/// The GDSII record types used by Magic (calma/calmaInt.h), by number.
enum class RecordType : std::uint8_t {
  header = 0x00, bgnlib = 0x01, libname = 0x02, units = 0x03, endlib = 0x04, bgnstr = 0x05,
  strname = 0x06, endstr = 0x07, boundary = 0x08, path = 0x09, sref = 0x0a, aref = 0x0b,
  text = 0x0c, layer = 0x0d, datatype = 0x0e, width = 0x0f, xy = 0x10, endel = 0x11,
  sname = 0x12, colrow = 0x13, textnode = 0x14, node = 0x15, texttype = 0x16, presentation = 0x17,
  spacing = 0x18, string = 0x19, strans = 0x1a, mag = 0x1b, angle = 0x1c, uinteger = 0x1d,
  ustring = 0x1e, reflibs = 0x1f, fonts = 0x20, pathtype = 0x21, generations = 0x22,
  attrtable = 0x23, styptable = 0x24, strtype = 0x25, elflags = 0x26, elkey = 0x27,
  linktype = 0x28, linkkeys = 0x29, nodetype = 0x2a, propattr = 0x2b, propvalue = 0x2c,
  box = 0x2d, boxtype = 0x2e, plex = 0x2f, bgnextn = 0x30, endextn = 0x31, tapenum = 0x32,
  tapecode = 0x33, strclass = 0x34, reserved = 0x35, format = 0x36, mask = 0x37, endmasks = 0x38,
};

/// The GDSII data types of a record header's second byte.
enum class DataType : std::uint8_t { none = 0, bitarray = 1, int2 = 2, int4 = 3, real4 = 4, real8 = 5, string = 6 };

/// One record: its header fields, payload and position.
struct Record {
  std::uint8_t type = 0;          // RecordType value (unknown types are kept)
  std::uint8_t data_type = 0;     // DataType value
  std::vector<std::byte> data;    // payload without the 4-byte header
  std::uint64_t offset = 0;       // of the header, in the decompressed stream

  friend bool operator==(const Record&, const Record&) = default;
};

class RecordReader {
 public:
  /// Reads from `source`, which must outlive the reader.
  explicit RecordReader(ByteSource& source) : source_(source) {}

  /// The next record without consuming it; nullopt at end of stream.
  [[nodiscard]] diag::Outcome<std::optional<Record>> peek();
  /// The next record, consumed; nullopt at end of stream.
  [[nodiscard]] diag::Outcome<std::optional<Record>> next();
  /// Pushes back the record last returned by next() (UNREADRH; one only).
  void unread(Record record);
  /// The offset of the next record in the decompressed stream.
  [[nodiscard]] std::uint64_t position() const noexcept;
  /// Moves to `offset` (decompressed) and drops the look-ahead.
  [[nodiscard]] diag::Status seek(std::uint64_t offset);

 private:
  ByteSource& source_;
  std::optional<Record> lookahead_;
};

class RecordWriter {
 public:
  /// Writes to `sink`, which must outlive the writer.
  explicit RecordWriter(ByteSink& sink) : sink_(sink) {}

  /// Writes one record (header and payload).
  [[nodiscard]] diag::Status write(RecordType type, DataType data_type, std::span<const std::byte> payload);
  /// Convenience forms in GDSII big-endian encodings.
  [[nodiscard]] diag::Status write_empty(RecordType type);
  [[nodiscard]] diag::Status write_int2(RecordType type, std::span<const std::int16_t> values);
  [[nodiscard]] diag::Status write_int4(RecordType type, std::span<const std::int32_t> values);
  [[nodiscard]] diag::Status write_real8(RecordType type, std::span<const double> values);
  /// A string padded to even length with a NUL.
  [[nodiscard]] diag::Status write_string(RecordType type, std::string_view text);
  /// Flushes the 64 KB buffer to the sink.
  [[nodiscard]] diag::Status flush();

 private:
  ByteSink& sink_;
  std::vector<std::byte> buffer_;
};

}  // namespace hefesto::gds
