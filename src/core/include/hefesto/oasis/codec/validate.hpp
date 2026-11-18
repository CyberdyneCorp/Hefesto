// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS file validation (capability oasis-format "Validation";
// openspec/changes/m6-lef-def-oasis/design.md D11, Q-OASIS-2 (a)): the END
// record's validation scheme and signature. CRC32 uses zlib's crc32 and
// CHECKSUM32 is the byte sum, over the bytes gdstk covers (from the start of
// the file up to the signature), in its byte order.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASFMT fills
// in the implementation in src/core/src/oasis/codec/ (design D2).

#pragma once

#include <cstdint>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::gds {
class ByteSource;
}

namespace hefesto::oasis {

/// validation-scheme of the END record: 0 none, 1 CRC32, 2 CHECKSUM32.
enum class ValidationScheme : std::uint8_t { none = 0, crc32 = 1, checksum32 = 2 };

/// The outcome of checking a file's END record.
struct ValidationResult {
  ValidationScheme scheme = ValidationScheme::none;
  std::uint32_t stored = 0;    // the signature in the file
  std::uint32_t computed = 0;  // the signature of the bytes read

  [[nodiscard]] bool ok() const noexcept { return scheme == ValidationScheme::none || stored == computed; }
  friend bool operator==(const ValidationResult&, const ValidationResult&) = default;
};

/// Reads `source` from its start to the END record and checks its
/// signature. An unreadable or truncated file is an error. SCAFFOLD: fails
/// with CORE-NOTIMPL-006 until OASFMT lands it.
[[nodiscard]] diag::Outcome<ValidationResult> validate(gds::ByteSource& source);

}  // namespace hefesto::oasis
