// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS file validation (capability oasis-format "Validation signature";
// openspec/changes/m6-lef-def-oasis/design.md D11, Q-OASIS-2 (a)): the END
// record's validation scheme and signature. CRC32 uses zlib's crc32 and
// CHECKSUM32 is the byte sum, over the bytes gdstk covers (from the first
// magic byte up to and including the validation-scheme byte), stored least
// significant byte first.
//
// Owner: OASFMT (design D2; the header was created by the M6 scaffold).

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

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

/// The signature of `bytes` under `scheme` (0 for none).
[[nodiscard]] std::uint32_t signature(ValidationScheme scheme, std::span<const std::byte> bytes) noexcept;

/// Continues the signature `previous` of earlier bytes over `bytes`.
[[nodiscard]] std::uint32_t update_signature(ValidationScheme scheme, std::uint32_t previous,
                                             std::span<const std::byte> bytes) noexcept;

/// Reads `source` from its start, locates the END record in the last 256
/// bytes and checks its signature. An unreadable or truncated file, a file
/// whose END record is not the last 256 bytes, and an unknown validation
/// scheme are errors.
[[nodiscard]] diag::Outcome<ValidationResult> validate(gds::ByteSource& source);

}  // namespace hefesto::oasis
