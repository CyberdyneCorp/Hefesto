// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Test helpers of the GDS stream and read tests: GDSII bytes built by hand
// (independent of RecordWriter), hex parsing and byte comparison.
// Owner: GDSR.

#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace gds_test {

using Bytes = std::vector<std::byte>;

/// Bytes from hex text; spaces and '|' are ignored ("00 04 11 00").
inline Bytes hex(std::string_view text) {
  Bytes out;
  int high = -1;
  for (const char c : text) {
    int v = -1;
    if (c >= '0' && c <= '9') {
      v = c - '0';
    } else if (c >= 'a' && c <= 'f') {
      v = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      v = c - 'A' + 10;
    }
    if (v < 0) {
      continue;
    }
    if (high < 0) {
      high = v;
    } else {
      out.push_back(static_cast<std::byte>(high * 16 + v));
      high = -1;
    }
  }
  return out;
}

inline Bytes text_bytes(std::string_view text) {
  Bytes out;
  for (const char c : text) {
    out.push_back(static_cast<std::byte>(c));
  }
  return out;
}

inline std::string as_text(const Bytes& bytes) {
  std::string out;
  for (const std::byte b : bytes) {
    out.push_back(static_cast<char>(b));
  }
  return out;
}

/// A GDSII stream built record by record.
class Gds {
 public:
  /// A record with raw payload bytes.
  Gds& rec(std::uint8_t type, std::uint8_t data_type, const Bytes& payload) {
    const std::size_t length = payload.size() + 4;
    put(static_cast<std::uint32_t>(length >> 8U));
    put(static_cast<std::uint32_t>(length));
    put(type);
    put(data_type);
    bytes_.insert(bytes_.end(), payload.begin(), payload.end());
    return *this;
  }
  Gds& empty(std::uint8_t type) { return rec(type, 0, {}); }
  Gds& i2(std::uint8_t type, std::initializer_list<int> values) {
    Bytes p;
    for (const int v : values) {
      p.push_back(static_cast<std::byte>((static_cast<unsigned>(v) >> 8U) & 0xffU));
      p.push_back(static_cast<std::byte>(static_cast<unsigned>(v) & 0xffU));
    }
    return rec(type, 2, p);
  }
  Gds& i4(std::uint8_t type, std::initializer_list<std::int32_t> values) {
    Bytes p;
    for (const std::int32_t v : values) {
      const auto u = static_cast<std::uint32_t>(v);
      for (const unsigned shift : {24U, 16U, 8U, 0U}) {
        p.push_back(static_cast<std::byte>((u >> shift) & 0xffU));
      }
    }
    return rec(type, 3, p);
  }
  Gds& str(std::uint8_t type, std::string_view text) {
    Bytes p = text_bytes(text);
    if (p.size() % 2 != 0) {
      p.push_back(std::byte{0});
    }
    return rec(type, 6, p);
  }
  Gds& raw(const Bytes& b) {
    bytes_.insert(bytes_.end(), b.begin(), b.end());
    return *this;
  }

  /// HEADER 600, BGNLIB, LIBNAME `name`, UNITS (0.001, 1e-9).
  Gds& library(std::string_view name) {
    i2(0x00, {600});
    i2(0x01, {124, 1, 1, 0, 0, 0, 124, 1, 1, 0, 0, 0});
    str(0x02, name);
    return rec(0x03, 5, hex("3E 41 89 37 4B C6 A7 F0  39 44 B8 2F A0 9B 5A 54"));
  }

  [[nodiscard]] const Bytes& bytes() const { return bytes_; }
  [[nodiscard]] std::size_t size() const { return bytes_.size(); }

 private:
  void put(std::uint32_t v) { bytes_.push_back(static_cast<std::byte>(v & 0xffU)); }

  Bytes bytes_;
};

}  // namespace gds_test
