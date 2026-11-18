// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS record encoder (capability oasis-format, and the record choices
// of oasis-write "File layout", "Reference numbers and name tables",
// "Modal field omission", "CBLOCK compression", "Strict and non-strict
// modes", "Standard properties", "END record and validation";
// openspec/changes/m6-lef-def-oasis/design.md D4, D11):
// - minimal integers (Q-OASIS-1) and the narrowest real form;
// - modal field omission (coordinates always absolute);
// - strict mode: CELL, PLACEMENT, TEXT, PROPERTY names and string values by
//   refnum, the name tables after the cells (PROPNAME, PROPSTRING,
//   LAYERNAME, TEXTSTRING, CELLNAME, each CELLNAME with S_CELL_OFFSET) and
//   their offsets in END; PROPNAME 0-3 are the four standard names;
// - non-strict mode: names inline, no tables, LAYERNAME after the file
//   properties;
// - CBLOCKs per cell body and per table at `compress` 1-9 (1 MiB chunks,
//   raw DEFLATE from the bundled zlib with the Q-OASIS-64 parameters);
// - the END record padded to 256 bytes with its validation signature.
//
// Call order: begin, then file_property/standard_properties and
// layer_name, then begin_cell with its element and property calls per
// cell, then finish. The encoder never calls the sink's finish().
//
// Owner: OASFMT (design D2; the header was created by the M6 scaffold).

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/oasis/codec/elements.hpp"
#include "hefesto/oasis/codec/modal.hpp"
#include "hefesto/oasis/codec/property.hpp"
#include "hefesto/oasis/codec/validate.hpp"

namespace hefesto::gds {
class ByteSink;
}

namespace hefesto::oasis {

struct EncoderOptions {
  bool strict = true;                                     // strict-mode name tables
  ValidationScheme validation = ValidationScheme::crc32;  // the END signature
  std::int32_t compress = 6;                              // CBLOCK DEFLATE level; 0 = no CBLOCK
  double unit = 1000.0;                                   // START unit (grid steps per micron)
};

class Encoder {
 public:
  /// Encodes to `sink`, which must outlive the encoder.
  explicit Encoder(gds::ByteSink& sink, EncoderOptions options = {});
  ~Encoder();
  Encoder(const Encoder&) = delete;
  Encoder& operator=(const Encoder&) = delete;
  Encoder(Encoder&&) noexcept;
  Encoder& operator=(Encoder&&) noexcept;

  /// The magic bytes and the START record.
  [[nodiscard]] diag::Status begin();
  /// The standard file properties of oasis-write "Standard properties":
  /// S_MAX_SIGNED_INTEGER_WIDTH = 4, S_MAX_UNSIGNED_INTEGER_WIDTH = 4 and,
  /// when `top_cell` is given, S_TOP_CELL.
  [[nodiscard]] diag::Status standard_properties(std::optional<std::string_view> top_cell);
  /// A file-level PROPERTY, before the first cell.
  [[nodiscard]] diag::Status file_property(const Property& property);
  /// The LAYERNAME pair (11 and 12) naming `layer`/`datatype` exactly; an
  /// identical pair is written once. Before the first cell.
  [[nodiscard]] diag::Status layer_name(std::string_view name, std::uint32_t layer, std::uint32_t datatype);
  /// A CELL record: by refnum in strict mode, by name otherwise.
  [[nodiscard]] diag::Status begin_cell(std::string_view name);
  /// One element, with the modal fields it shares omitted. Names are given
  /// in `name` (a `reference` alone is an error).
  [[nodiscard]] diag::Status element(const Element& value);
  /// A PROPERTY attached to the preceding element or cell, written in full.
  [[nodiscard]] diag::Status property(const Property& value);
  /// The name tables, the END record and its signature.
  [[nodiscard]] diag::Status finish();

  [[nodiscard]] gds::ByteSink& sink() const noexcept;
  [[nodiscard]] const ModalState& modal() const noexcept;
  [[nodiscard]] const EncoderOptions& options() const noexcept;

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace hefesto::oasis
