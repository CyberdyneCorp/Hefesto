// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS record decoder (capability oasis-format;
// openspec/changes/m6-lef-def-oasis/design.md D4, D11): reads records over
// a gds::ByteSource and yields them typed, never touching the database:
// - integers, reals, strings, deltas, point lists, repetitions, intervals;
// - ModalState with the reset rules (Q-OASIS-5);
// - the name tables, resolved in two passes with the table offsets
//   (Q-OASIS-8);
// - CBLOCK inflation, spans across the end included (Q-OASIS-9), under the
//   M5-S1-style inflate cap;
// - the strict-mode checks (Q-OASIS-7) and validation (Q-OASIS-2).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASFMT fills
// in the implementation in src/core/src/oasis/codec/ (design D2).

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/oasis/codec/elements.hpp"
#include "hefesto/oasis/codec/modal.hpp"

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

/// One decoded record. Geometry, PLACEMENT and TEXT records carry their
/// element after modal resolution; name records their name.
struct Record {
  RecordId id = RecordId::pad;
  std::uint64_t offset = 0;  // in the decompressed stream
  std::optional<Element> element;
  std::string name;
  std::optional<std::uint64_t> reference;

  friend bool operator==(const Record&, const Record&) = default;
};

/// The file's name tables, by reference number.
struct NameTables {
  std::map<std::uint64_t, std::string> cellnames;
  std::map<std::uint64_t, std::string> textstrings;
  std::map<std::uint64_t, std::string> propnames;
  std::map<std::uint64_t, std::string> propstrings;
};

struct DecoderOptions {
  /// The strict-mode checks of Q-OASIS-7.
  bool check_strict = true;
  /// The cap on the bytes one CBLOCK may inflate to.
  std::uint64_t inflate_limit = std::uint64_t{1} << 30;
};

class Decoder {
 public:
  /// Decodes `source`, which must outlive the decoder.
  explicit Decoder(gds::ByteSource& source, DecoderOptions options = {}) : source_(&source), options_(options) {}

  /// The next record; nullopt after END. SCAFFOLD: fails with
  /// CORE-NOTIMPL-006 until OASFMT lands it.
  [[nodiscard]] diag::Outcome<std::optional<Record>> next();

  [[nodiscard]] const ModalState& modal() const noexcept { return modal_; }
  [[nodiscard]] const NameTables& names() const noexcept { return names_; }
  [[nodiscard]] const DecoderOptions& options() const noexcept { return options_; }

 private:
  [[maybe_unused]] gds::ByteSource* source_;  // read by OASFMT's implementation
  DecoderOptions options_;
  ModalState modal_;
  NameTables names_;
};

}  // namespace hefesto::oasis
