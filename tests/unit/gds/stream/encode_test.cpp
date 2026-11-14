// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDSII value encodings (tasks.md 9.1): eight-byte reals (the MASK-GW-040
// vectors), payload values, and string records with the strict and
// permissive character tables, `lower` and string-limit, per
// magic-compat setting (Q-MASK-200, 207). Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include "hefesto/gds/stream/encode.hpp"
#include "hefesto/gds/stream/values.hpp"
#include "stream_fixture.hpp"

namespace gds = hefesto::gds;
namespace compat = hefesto::compat;
using gds_test::hex;

namespace {

std::array<std::byte, 8> eight(std::string_view text) {
  const auto bytes = hex(text);
  REQUIRE(bytes.size() == 8);
  std::array<std::byte, 8> out{};
  std::copy(bytes.begin(), bytes.end(), out.begin());
  return out;
}

const compat::CompatSettings kOn{true, true};
const compat::CompatSettings kOff{false, true};

gds::EncodedString encode(std::string_view text, gds::CharTable::Kind kind, const compat::CompatSettings& c,
                          gds::StringOptions options = {}) {
  const gds::CharTable table{kind, c};
  return gds::encode_string(text, table, options, c);
}

}  // namespace

TEST_CASE("R8 decoding of meters per DB unit, negative and zero values",
          "[cap:gds-read][req:Record framing and primitive data types]") {
  CHECK(std::abs(gds::decode_real8(eight("39 44 B8 2F A0 9B 5A 54")) - 1.0e-9) < 1e-24);
  CHECK(gds::decode_real8(eight("C1 10 00 00 00 00 00 00")) == -1.0);
  CHECK(gds::decode_real8(eight("00 00 00 00 00 00 00 00")) == 0.0);
  CHECK(gds::decode_real8(eight("41 10 00 00 00 00 00 00")) == 1.0);
  // Garbage exponents still decode to a finite value (no UB, no hang).
  CHECK(std::isfinite(gds::decode_real8(eight("7F FF FF FF FF FF FF FF"))));
  CHECK(gds::decode_real8(eight("00 00 00 00 00 00 00 01")) > 0.0);
}

TEST_CASE("eight-byte reals encode the standard values exactly", "[cap:gds-write][req:Eight-byte real encoding]") {
  const std::pair<double, const char*> vectors[] = {
      {0.001, "3E 41 89 37 4B C6 A7 F0"}, {1e-9, "39 44 B8 2F A0 9B 5A 54"},  {0.0001, "3D 68 DB 8B AC 71 0C B4"},
      {1e-10, "38 6D F3 7F 67 5E F6 EC"}, {1.0, "41 10 00 00 00 00 00 00"},   {0.5, "40 80 00 00 00 00 00 00"},
      {90.0, "42 5A 00 00 00 00 00 00"},  {180.0, "42 B4 00 00 00 00 00 00"}, {270.0, "43 10 E0 00 00 00 00 00"},
      {-1.0, "C1 10 00 00 00 00 00 00"},  {0.1, "40 19 99 99 99 99 99 9A"},   {0.0, "00 00 00 00 00 00 00 00"},
  };
  for (const auto& [value, bytes] : vectors) {
    INFO(value);
    CHECK(gds::encode_real8(value) == eight(bytes));
    CHECK(gds::decode_real8(eight(bytes)) == value);
  }
}

TEST_CASE("eight-byte reals saturate values outside the format", "[cap:gds-write][req:Eight-byte real encoding]") {
  // Magic loops forever on an infinity; Hefesto saturates (and writes NaN as zero).
  CHECK(gds::encode_real8(std::numeric_limits<double>::infinity()) == eight("7F FF FF FF FF FF FF FF"));
  CHECK(gds::encode_real8(-std::numeric_limits<double>::infinity()) == eight("FF FF FF FF FF FF FF FF"));
  CHECK(gds::encode_real8(std::numeric_limits<double>::quiet_NaN()) == eight("00 00 00 00 00 00 00 00"));
  CHECK(gds::encode_real8(1e300) == eight("7F FF FF FF FF FF FF FF"));
  CHECK(gds::encode_real8(1e-300) == eight("00 00 00 00 00 00 00 00"));
  CHECK(gds::encode_real8(std::numeric_limits<double>::denorm_min()) == eight("00 00 00 00 00 00 00 00"));
}

TEST_CASE("payload values are big-endian two's complement", "[cap:gds-read][req:Record framing and primitive data types]") {
  const auto data = hex("FF FF FF 38 00 31");
  CHECK(gds::i4_at(data, 0) == -200);
  CHECK(gds::i2_at(data, 2) == 49);
  CHECK(gds::i2_at(data, 0) == -1);
  CHECK_FALSE(gds::i2_at(data, 3).has_value());
  CHECK_FALSE(gds::i4_at(data, 1).has_value());
  CHECK_FALSE(gds::r8_at(data, 0).has_value());
  CHECK(gds::i2_values(hex("00 01 00 02 FF")) == std::vector<std::int16_t>{1, 2});
  CHECK(gds::i4_values(hex("00 00 00 05 00")) == std::vector<std::int32_t>{5});
}

TEST_CASE("a padded string ends at its first NUL", "[cap:gds-read][req:Record framing and primitive data types]") {
  CHECK(gds::string_value(hex("41 42 43 00")) == "ABC");
  CHECK(gds::string_value(hex("41 00 42 43")) == "A");
  CHECK(gds::string_value(hex("41 42")) == "AB");
  CHECK(gds::string_value({}).empty());
}

TEST_CASE("string records pad odd lengths and map with the strict table",
          "[cap:gds-write][req:String records and character mapping]") {
  const auto top = encode("top", gds::CharTable::Kind::strict, kOn);
  CHECK(top.data == hex("74 6F 70 00"));
  CHECK_FALSE(top.changed);
  const auto label = encode("A<0>", gds::CharTable::Kind::strict, kOn);
  CHECK(label.data == hex("41 5F 30 5F"));
  CHECK(label.changed);
  CHECK(label.mapped == "A_0_");
  // `$` and `_` are kept; control characters and DEL become NUL bytes.
  CHECK(encode("$_\x01\x7f", gds::CharTable::Kind::strict, kOn).data == hex("24 5F 00 00"));
  // A byte above 127 is written as X.
  const auto high = encode("a\xe9", gds::CharTable::Kind::strict, kOn);
  CHECK(high.data == hex("61 58"));
  CHECK(high.unprintable);
  // `gds lower no` upper-cases letters on output.
  CHECK(encode("ab", gds::CharTable::Kind::strict, kOn, {false, false}).data == hex("41 42"));
  // An embedded NUL ends the string (Magic's strlen).
  CHECK(encode(std::string_view{"ab\0cd", 5}, gds::CharTable::Kind::strict, kOn).data == hex("61 62"));
}

TEST_CASE("the permissive table swaps percent and ampersand only with magic-compat ON",
          "[cap:gds-write][req:String records and character mapping]") {
  const auto on = encode("a%b", gds::CharTable::Kind::permissive, kOn);
  CHECK(on.data == hex("61 26 62 00"));
  CHECK(on.changed);
  CHECK(encode("a&b", gds::CharTable::Kind::permissive, kOn).data == hex("61 25 62 00"));
  const auto off = encode("a%b", gds::CharTable::Kind::permissive, kOff);
  CHECK(off.data == hex("61 25 62 00"));
  CHECK_FALSE(off.changed);
  CHECK(encode("A<0> x", gds::CharTable::Kind::permissive, kOff).data == gds_test::text_bytes("A<0> x"));
  const gds::CharTable table{gds::CharTable::Kind::permissive, kOn};
  CHECK(table.kind() == gds::CharTable::Kind::permissive);
  CHECK(table.map("50%&") == "50&%");
}

TEST_CASE("string-limit keeps the tail of a long string", "[cap:gds-write][req:String records and character mapping]") {
  const gds::StringOptions limit{true, true};
  for (const auto& c : {kOn, kOff}) {
    const auto even = encode("abcdefghijklmnopqrstuvwxyz0123456789", gds::CharTable::Kind::strict, c, limit);
    CHECK(gds_test::as_text(even.data) == "efghijklmnopqrstuvwxyz0123456789");
    CHECK(even.truncated);
  }
  const auto odd_on = encode("bcdefghijklmnopqrstuvwxyz0123456789", gds::CharTable::Kind::strict, kOn, limit);
  CHECK(gds_test::as_text(odd_on.data) == std::string{"fghijklmnopqrstuvwxyz0123456789"} + '\0');
  const auto odd_off = encode("bcdefghijklmnopqrstuvwxyz0123456789", gds::CharTable::Kind::strict, kOff, limit);
  CHECK(gds_test::as_text(odd_off.data) == "efghijklmnopqrstuvwxyz0123456789");
  // Within the limit nothing changes; without the option no limit applies.
  const auto short_text = encode("abc", gds::CharTable::Kind::strict, kOn, limit);
  CHECK(short_text.data == hex("61 62 63 00"));
  CHECK_FALSE(short_text.truncated);
  CHECK(encode(std::string(40, 'a'), gds::CharTable::Kind::strict, kOn).data.size() == 40);
  // 31 characters pad to 32 and are kept whole in both modes.
  for (const auto& c : {kOn, kOff}) {
    CHECK_FALSE(encode(std::string(31, 'a'), gds::CharTable::Kind::strict, c, limit).truncated);
  }
}
