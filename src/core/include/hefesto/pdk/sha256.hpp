// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Streaming SHA-256 (FIPS 180-4) for PDK download verification
// (capability hefesto-pdk-manager, "Download integrity verification").
//
// Portable implementation in the core, as design.md D9 of the
// m1-core-viewer change allows ("tar parsing and SHA-256 are implemented in
// the core"). hefesto-licensing D-13 assigns SHA-256 to CryptoKit (Apple)
// and libsodium (Linux) behind the platform-services layer; until that
// service exists this class is the single implementation, and its output is
// identical on every platform by construction (recorded deviation, see the
// PDK workstream report).
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::pdk {

class Sha256 {
 public:
  Sha256() noexcept;

  void update(std::string_view data) noexcept;
  /// Finishes the digest; the object must not be updated afterwards.
  [[nodiscard]] std::array<std::uint8_t, 32> finish() noexcept;
  /// finish() as 64 lower-case hex digits.
  [[nodiscard]] std::string finish_hex();

 private:
  void compress(const std::uint8_t* block) noexcept;

  std::array<std::uint32_t, 8> state_{};
  std::array<std::uint8_t, 64> buffer_{};
  std::size_t buffered_ = 0;
  std::uint64_t total_bytes_ = 0;
};

/// SHA-256 of a file on the local disk, read in chunks (assets can be
/// hundreds of megabytes). PLATFORM-IO-001 when it cannot be read.
[[nodiscard]] diag::Outcome<std::string> sha256_file_hex(const platform::Path& path);

}  // namespace hefesto::pdk
