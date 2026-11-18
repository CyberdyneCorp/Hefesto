// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS record decoder (capability oasis-format;
// openspec/changes/m6-lef-def-oasis/design.md D4, D11): reads records over
// a gds::ByteSource and yields them typed, never touching the database:
// - integers, reals, strings, deltas, point lists, repetitions, intervals;
// - ModalState with the reset rules (Q-OASIS-5);
// - the name tables and their reference numbers; references stay refnums
//   in the records and are resolved through names() once the whole file is
//   read (forward references, Q-OASIS-34 in oasis-read);
// - CBLOCK inflation, spans across the end included (Q-OASIS-9), under the
//   inflate cap (security review M6-S4);
// - the strict-mode checks (Q-OASIS-7) and validation (Q-OASIS-2), both
//   evaluated at END (the table offsets of a strict file are in END).
//
// next() fails on an error that stops decoding (the error is returned again
// by every later call). Warnings, and the errors that drop one element or
// are reported after the file was scanned, are kept in diagnostics(); a
// dropped element's record has no `element`.
//
// Owner: OASFMT (design D2; the header was created by the M6 scaffold).

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/oasis/codec/elements.hpp"
#include "hefesto/oasis/codec/modal.hpp"
#include "hefesto/oasis/codec/primitives.hpp"
#include "hefesto/oasis/codec/property.hpp"
#include "hefesto/oasis/codec/validate.hpp"

namespace hefesto::gds {
class ByteSource;
}

namespace hefesto::oasis {

/// The record IDs of OASIS 1.0 (PAD = 0 .. XGEOMETRY = 33, CBLOCK = 34).
enum class RecordId : std::uint8_t {
  pad = 0,
  start = 1,
  end = 2,
  cellname_implicit = 3,
  cellname = 4,
  textstring_implicit = 5,
  textstring = 6,
  propname_implicit = 7,
  propname = 8,
  propstring_implicit = 9,
  propstring = 10,
  layername_geometry = 11,
  layername_text = 12,
  cell_by_reference = 13,
  cell_by_name = 14,
  xyabsolute = 15,
  xyrelative = 16,
  placement = 17,
  placement_transform = 18,
  text = 19,
  rectangle = 20,
  polygon = 21,
  path = 22,
  trapezoid = 23,
  trapezoid_a = 24,
  trapezoid_b = 25,
  ctrapezoid = 26,
  circle = 27,
  property = 28,
  property_repeat = 29,
  xname_implicit = 30,
  xname = 31,
  xelement = 32,
  xgeometry = 33,
  cblock = 34,
};

/// The six name tables, in the order of the table-offsets structure.
enum class NameTable : std::uint8_t { cellname, textstring, propname, propstring, layername, xname };

/// The upper-case name of `table`, e.g. "CELLNAME".
[[nodiscard]] std::string_view name_table_name(NameTable table) noexcept;

/// One table-entry of the table-offsets structure.
struct TableEntry {
  bool strict = false;
  std::uint64_t offset = 0;  // 0: no table, or position not given

  friend bool operator==(const TableEntry&, const TableEntry&) = default;
};

/// The table-offsets structure, indexed by NameTable.
using TableOffsets = std::array<TableEntry, 6>;

/// The START record.
struct StartInfo {
  std::string version;
  double unit = 0.0;
  bool offsets_at_end = false;
  TableOffsets tables{};  // as read from START (all zero when at END)

  friend bool operator==(const StartInfo&, const StartInfo&) = default;
};

/// The END record.
struct EndInfo {
  std::optional<TableOffsets> tables;  // present iff START's offset-flag != 0
  std::uint64_t validation_scheme = 0;
  ValidationResult validation;  // scheme none when unknown
  std::uint64_t length = 0;     // from the record ID through the signature

  friend bool operator==(const EndInfo&, const EndInfo&) = default;
};

/// A LAYERNAME record (11: geometry, 12: text).
struct LayerName {
  std::string name;
  bool text = false;
  Interval layers;
  Interval datatypes;

  friend bool operator==(const LayerName&, const LayerName&) = default;
};

/// An XNAME record.
struct XName {
  std::uint64_t attribute = 0;
  std::string name;  // b-string

  friend bool operator==(const XName&, const XName&) = default;
};

/// What a PROPERTY record attaches to (oasis-format "PROPERTY records").
enum class PropertyTarget : std::uint8_t { file, name_record, cell, element };

/// One decoded record. Geometry, PLACEMENT, TEXT and extension elements
/// carry their element after modal resolution; name records their name and
/// refnum; CELL its name or refnum; PROPERTY its property and target.
struct Record {
  RecordId id = RecordId::pad;
  /// The file offset of the record, or of the CBLOCK holding its first
  /// byte (in the stream after gzip decompression).
  std::uint64_t offset = 0;
  bool in_cblock = false;
  std::uint64_t cblock_position = 0;  // the first byte's position in the CBLOCK's data
  std::optional<Element> element;
  std::string name;
  std::optional<std::uint64_t> reference;
  std::optional<StartInfo> start;
  std::optional<EndInfo> end;
  std::optional<Property> property;
  PropertyTarget target = PropertyTarget::file;
  std::optional<LayerName> layer_name;
  std::optional<XName> xname;

  friend bool operator==(const Record&, const Record&) = default;
};

/// The file's name tables, by reference number.
struct NameTables {
  std::map<std::uint64_t, std::string> cellnames;
  std::map<std::uint64_t, std::string> textstrings;
  std::map<std::uint64_t, std::string> propnames;
  std::map<std::uint64_t, std::string> propstrings;
  std::map<std::uint64_t, XName> xnames;
  std::vector<LayerName> layernames;
};

struct DecoderOptions {
  /// The strict-mode checks of Q-OASIS-7.
  bool check_strict = true;
  /// The cap on the bytes one CBLOCK may inflate to.
  std::uint64_t inflate_limit = std::uint64_t{1} << 30;
};

class Decoder {
 public:
  /// Decodes `source` from its current position (the first magic byte),
  /// which must outlive the decoder.
  explicit Decoder(gds::ByteSource& source, DecoderOptions options = {});
  ~Decoder();
  Decoder(const Decoder&) = delete;
  Decoder& operator=(const Decoder&) = delete;
  Decoder(Decoder&&) noexcept;
  Decoder& operator=(Decoder&&) noexcept;

  /// The next record; nullopt after END.
  [[nodiscard]] diag::Outcome<std::optional<Record>> next();

  [[nodiscard]] gds::ByteSource& source() const noexcept;
  [[nodiscard]] const ModalState& modal() const noexcept;
  [[nodiscard]] const NameTables& names() const noexcept;
  [[nodiscard]] const DecoderOptions& options() const noexcept;
  /// The START record, once decoded.
  [[nodiscard]] const std::optional<StartInfo>& start() const noexcept;
  /// The warnings and non-fatal errors so far, in order.
  [[nodiscard]] const std::vector<diag::Diagnostic>& diagnostics() const noexcept;
  /// Moves the diagnostics out.
  [[nodiscard]] std::vector<diag::Diagnostic> take_diagnostics();

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace hefesto::oasis
