// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS record encoder (capability oasis-format;
// openspec/changes/m6-lef-def-oasis/design.md D4, D11): minimal integers
// (Q-OASIS-1), modal field omission, the name tables in strict or
// non-strict layout, the CBLOCK writer (raw DEFLATE from the bundled zlib,
// D3) and the END record with its validation signature.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASFMT fills
// in the implementation in src/core/src/oasis/codec/ (design D2).

#pragma once

#include <cstdint>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/oasis/codec/elements.hpp"
#include "hefesto/oasis/codec/modal.hpp"
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
  explicit Encoder(gds::ByteSink& sink, EncoderOptions options = {}) : sink_(&sink), options_(options) {}

  /// The magic bytes and the START record. SCAFFOLD: the four methods below
  /// fail with CORE-NOTIMPL-006 until OASFMT lands them.
  [[nodiscard]] diag::Status begin();
  /// A CELL record by name.
  [[nodiscard]] diag::Status begin_cell(std::string_view name);
  /// One element, with the modal fields it shares omitted.
  [[nodiscard]] diag::Status element(const Element& value);
  /// The name tables, the END record and its signature.
  [[nodiscard]] diag::Status finish();

  [[nodiscard]] const ModalState& modal() const noexcept { return modal_; }
  [[nodiscard]] const EncoderOptions& options() const noexcept { return options_; }

 private:
  [[maybe_unused]] gds::ByteSink* sink_;  // written by OASFMT's implementation
  EncoderOptions options_;
  ModalState modal_;
};

}  // namespace hefesto::oasis
