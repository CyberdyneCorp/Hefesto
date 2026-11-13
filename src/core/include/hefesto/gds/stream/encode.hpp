// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDSII value encodings (capabilities gds-read, gds-write;
// openspec/changes/m5-mask-gds-cif/design.md D10, D22):
// - the eight-byte excess-64 real (gds-write "Eight-byte real encoding";
//   the MASK-GW-040 vectors are its unit test), written with explicit
//   double operations in Magic's order;
// - the strict and permissive character tables (Q-MASK-200: magic-compat
//   ON swaps `%` and `&`, which IHP output depends on);
// - BGNLIB/BGNSTR dates in local time (Q-MASK-203), with the pinned-zone
//   override of Q-MASK-132 for tests.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GDSR fills
// in the implementation in src/core/src/gds/stream/encode.cpp (design D2).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/compat/compat_settings.hpp"

namespace hefesto::gds {

/// A value in GDSII eight-byte real format (big-endian).
[[nodiscard]] std::array<std::byte, 8> encode_real8(double value);
/// The value of eight bytes in GDSII eight-byte real format.
[[nodiscard]] double decode_real8(std::span<const std::byte, 8> bytes);

/// The character map applied to names and label text on output
/// (calmaMapTableStrict / calmaMapTablePermissive).
class CharTable {
 public:
  enum class Kind : std::uint8_t { strict, permissive };

  CharTable(Kind kind, const compat::CompatSettings& compat);

  /// `text` with every character mapped.
  [[nodiscard]] std::string map(std::string_view text) const;
  [[nodiscard]] Kind kind() const noexcept { return kind_; }

 private:
  Kind kind_;
  std::array<char, 128> table_{};
};

/// The six INT2 date fields (year, month, day, hour, minute, second) of
/// `time` (seconds since the epoch) in the local time zone, or in `zone`
/// (a TZ string) when not empty (Q-MASK-132).
[[nodiscard]] std::array<std::int16_t, 6> encode_date(std::int64_t time, std::string_view zone = {});

}  // namespace hefesto::gds
