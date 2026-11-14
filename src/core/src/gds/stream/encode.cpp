// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDSII value encodings (see hefesto/gds/stream/encode.hpp). Ported from
// Magic 8.3.694 calma/CalmaWrite.c (calmaOutR8, calmaMapTableStrict,
// calmaMapTablePermissive, calmaOutDate) and calma/CalmaRdio.c
// (calmaReadR8); Magic is (c) the Regents of the University of California,
// distributed under its permissive license (THIRD_PARTY_NOTICES.md). The
// float paths use explicit double operations in Magic's order (design D22).
// Owner: GDSR.

#include "hefesto/gds/stream/encode.hpp"

#include <cctype>
#include <cmath>

#include "hefesto/gds/stream/dates.hpp"

namespace hefesto::gds {

namespace {

/// The largest magnitude the format holds: exponent 127, mantissa all ones.
constexpr std::array<std::byte, 8> kLargest{std::byte{0x7f}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff},
                                            std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};

/// The mantissa bits of `d` (1/16 <= d < 1) as calmaOutR8 builds them.
std::uint64_t mantissa_bits(double d) {
  std::uint64_t mantissa = 0;
  for (int i = 0; i < 64; ++i) {
    mantissa <<= 1U;
    if (d >= 0.5) {
      mantissa |= 1U;
      d -= 0.5;
    }
    d *= 2.0;
  }
  return mantissa;
}

}  // namespace

std::array<std::byte, 8> encode_real8(double value) {
  std::array<std::byte, 8> out{};
  if (value == 0.0 || std::isnan(value)) {
    return out;  // NaN has no GDSII form; it is written as zero
  }
  const bool negative = value < 0.0;
  if (std::isinf(value)) {
    out = kLargest;  // Magic loops forever on an infinity; saturate instead
    out[0] |= negative ? std::byte{0x80} : std::byte{0};
    return out;
  }
  double d = negative ? -value : value;
  int exponent = 64;
  while (d >= 1.0) {
    d /= 16.0;
    ++exponent;
  }
  while (d < 0.0625) {
    d *= 16.0;
    --exponent;
  }
  if (exponent < 0) {
    return out;  // below the smallest magnitude (16^-64): zero
  }
  if (exponent > 127) {
    out = kLargest;  // above the largest magnitude: saturate
  } else {
    const std::uint64_t mantissa = mantissa_bits(d);
    out[0] = static_cast<std::byte>(exponent);
    for (unsigned i = 1; i < 8; ++i) {
      out[i] = static_cast<std::byte>(0xffU & (mantissa >> (64U - 8U * i)));
    }
  }
  out[0] |= negative ? std::byte{0x80} : std::byte{0};
  return out;
}

double decode_real8(std::span<const std::byte, 8> bytes) {
  int exponent = std::to_integer<int>(bytes[0]);
  const bool negative = (exponent & 0x80) != 0;
  exponent = (exponent & 0x7f) - 64;
  double mantissa = 0.0;
  for (std::size_t i = 7; i > 0; --i) {
    mantissa += std::to_integer<int>(bytes[i]);
    mantissa /= 256.0;
  }
  double d = mantissa;
  for (; exponent > 0; --exponent) {
    d *= 16.0;
  }
  for (; exponent < 0; ++exponent) {
    d /= 16.0;
  }
  return negative ? -d : d;
}

CharTable::CharTable(Kind kind, const compat::CompatSettings& compat) : kind_(kind) {
  for (std::size_t c = 0; c < table_.size(); ++c) {
    const bool printable = c >= 0x20 && c < 0x7f;
    if (!printable) {
      table_[c] = '\0';
    } else if (kind == Kind::permissive) {
      table_[c] = static_cast<char>(c);
    } else {
      const bool keep = std::isalnum(static_cast<int>(c)) != 0 || c == '$' || c == '_';
      table_[c] = keep ? static_cast<char>(c) : '_';
    }
  }
  if (kind == Kind::permissive && compat.magic_compat) {
    table_['%'] = '&';  // Magic's permissive table swaps them (Q-MASK-200 ON)
    table_['&'] = '%';
  }
}

std::string CharTable::map(std::string_view text) const {
  std::string out{text};
  for (char& c : out) {
    const auto index = static_cast<unsigned char>(c);
    if (index < table_.size()) {
      c = table_[index];
    }
  }
  return out;
}

std::array<std::int16_t, 6> encode_date(std::int64_t time, std::string_view zone) {
  const auto local = to_local_time(time, zone);
  if (!local) {
    return {};
  }
  // calmaOutI2 keeps the low 16 bits of each struct tm field.
  const auto i2 = [](std::int64_t v) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(static_cast<std::uint64_t>(v) & 0xffffU));
  };
  return {i2(local->year), i2(local->month + 1), i2(local->day), i2(local->hour), i2(local->minute),
          i2(local->second)};
}

}  // namespace hefesto::gds
