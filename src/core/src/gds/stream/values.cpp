// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDSII payload values (see hefesto/gds/stream/values.hpp). The string
// encoding ports Magic 8.3.694 calma/CalmaWrite.c calmaOutStringRecord;
// Magic is (c) the Regents of the University of California, distributed
// under its permissive license (THIRD_PARTY_NOTICES.md). Unlike Magic it
// never rewrites the caller's text (Q-MASK-208). Owner: GDSR.

#include "hefesto/gds/stream/values.hpp"

#include <array>

namespace hefesto::gds {

namespace {

constexpr std::size_t kNameLimit = 32;  // CALMANAMELENGTH

std::uint32_t byte_at(std::span<const std::byte> data, std::size_t i) {
  return std::to_integer<std::uint32_t>(data[i]);
}

/// The bytes of `text` that string-limit keeps, and the written length.
struct Window {
  std::size_t offset = 0;
  std::size_t length = 0;
  bool truncated = false;
};

Window limit_window(std::size_t text_length, bool string_limit, bool magic_compat) {
  const std::size_t padded = text_length + (text_length & 1U);
  if (!string_limit) {
    return Window{0, padded, false};
  }
  if (magic_compat) {
    if (padded > kNameLimit) {
      return Window{padded - kNameLimit, kNameLimit, true};  // may end on the pad NUL
    }
    return Window{0, padded, false};
  }
  if (text_length > kNameLimit) {
    return Window{text_length - kNameLimit, kNameLimit, true};
  }
  return Window{0, padded, false};
}

}  // namespace

std::optional<std::int16_t> i2_at(std::span<const std::byte> data, std::size_t index) {
  if (index >= data.size() / 2) {
    return std::nullopt;
  }
  const std::size_t i = index * 2;
  return static_cast<std::int16_t>(static_cast<std::uint16_t>((byte_at(data, i) << 8U) | byte_at(data, i + 1)));
}

std::optional<std::int32_t> i4_at(std::span<const std::byte> data, std::size_t index) {
  if (index >= data.size() / 4) {
    return std::nullopt;
  }
  const std::size_t i = index * 4;
  const std::uint32_t u = (byte_at(data, i) << 24U) | (byte_at(data, i + 1) << 16U) | (byte_at(data, i + 2) << 8U) |
                          byte_at(data, i + 3);
  return static_cast<std::int32_t>(u);
}

std::optional<double> r8_at(std::span<const std::byte> data, std::size_t index) {
  if (index >= data.size() / 8) {
    return std::nullopt;
  }
  return decode_real8(data.subspan(index * 8).first<8>());
}

std::vector<std::int16_t> i2_values(std::span<const std::byte> data) {
  std::vector<std::int16_t> out;
  out.reserve(data.size() / 2);
  for (std::size_t i = 0; i < data.size() / 2; ++i) {
    out.push_back(*i2_at(data, i));
  }
  return out;
}

std::vector<std::int32_t> i4_values(std::span<const std::byte> data) {
  std::vector<std::int32_t> out;
  out.reserve(data.size() / 4);
  for (std::size_t i = 0; i < data.size() / 4; ++i) {
    out.push_back(*i4_at(data, i));
  }
  return out;
}

std::string string_value(std::span<const std::byte> data) {
  std::string out;
  for (const std::byte b : data) {
    if (b == std::byte{0}) {
      break;
    }
    out.push_back(static_cast<char>(b));
  }
  return out;
}

EncodedString encode_string(std::string_view text, const CharTable& table, const StringOptions& options,
                            const compat::CompatSettings& compat) {
  text = text.substr(0, text.find('\0'));  // Magic's strlen
  EncodedString out;
  const Window w = limit_window(text.size(), options.string_limit, compat.magic_compat);
  out.truncated = w.truncated;
  out.data.reserve(w.length);
  for (std::size_t i = 0; i < w.length; ++i) {
    const std::size_t at = w.offset + i;
    const auto c = static_cast<unsigned char>(at < text.size() ? text[at] : '\0');
    unsigned char mapped = 0;
    if (c > 127) {
      out.unprintable = true;
      mapped = 'X';
    } else if (c != 0) {
      mapped = static_cast<unsigned char>(table.map(std::string_view{reinterpret_cast<const char*>(&c), 1})[0]);
      out.changed = out.changed || mapped != c;
    }
    if (!options.lower && mapped >= 'a' && mapped <= 'z') {
      mapped = static_cast<unsigned char>(mapped - 'a' + 'A');
    }
    out.data.push_back(static_cast<std::byte>(mapped));
  }
  // Magic's modified string: every character of the text through the table.
  out.mapped = table.map(text);
  return out;
}

}  // namespace hefesto::gds
