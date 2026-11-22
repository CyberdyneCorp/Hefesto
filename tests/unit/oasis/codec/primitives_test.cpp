// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS primitives (tasks.md 10.1; OASIS-001..004 inputs as unit
// inputs): integers, reals, strings, deltas, point lists, repetitions and
// intervals. Owner: OASFMT.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

#include "codec_fixture.hpp"
#include "hefesto/oasis/codec/elements.hpp"
#include "hefesto/oasis/codec/primitives.hpp"

using oasis_test::hex;
using oasis_test::pt;
namespace oasis = hefesto::oasis;
namespace diag = hefesto::diag;

namespace {

std::vector<std::byte> unsigned_bytes(std::uint64_t v) {
  std::vector<std::byte> out;
  oasis::write_unsigned(out, v);
  return out;
}

std::vector<std::byte> signed_bytes(std::int64_t v) {
  std::vector<std::byte> out;
  oasis::write_signed(out, v);
  return out;
}

std::vector<std::byte> real_bytes(double v) {
  std::vector<std::byte> out;
  oasis::write_real(out, v);
  return out;
}

std::vector<std::byte> delta_bytes(oasis::Point64 d, oasis::DeltaForm form) {
  std::vector<std::byte> out;
  oasis::write_delta(out, d, form);
  return out;
}

template <typename T, typename F>
T decoded(const std::vector<std::byte>& bytes, F read, std::size_t* consumed = nullptr) {
  oasis::ByteCursor cursor{bytes};
  auto r = read(cursor);
  REQUIRE(r.ok());
  if (consumed != nullptr) {
    *consumed = cursor.offset;
  }
  return r.value();
}

std::string error_code(const std::vector<std::byte>& bytes,
                       diag::Outcome<oasis::Repetition> (*read)(oasis::ByteCursor&)) {
  oasis::ByteCursor cursor{bytes};
  auto r = read(cursor);
  REQUIRE_FALSE(r.ok());
  return r.error().code;
}

}  // namespace

TEST_CASE("unsigned integers are little-endian base-128", "[cap:oasis-format][req:Unsigned and signed integers]") {
  CHECK(unsigned_bytes(0) == hex("00"));
  CHECK(unsigned_bytes(1) == hex("01"));
  CHECK(unsigned_bytes(127) == hex("7F"));
  CHECK(unsigned_bytes(128) == hex("80 01"));
  CHECK(unsigned_bytes(300) == hex("AC 02"));
  CHECK(unsigned_bytes(16383) == hex("FF 7F"));
  CHECK(unsigned_bytes(16384) == hex("80 80 01"));
  for (const std::uint64_t v : {std::uint64_t{0}, std::uint64_t{127}, std::uint64_t{128}, std::uint64_t{1} << 63,
                                std::numeric_limits<std::uint64_t>::max()}) {
    CHECK(decoded<std::uint64_t>(unsigned_bytes(v), oasis::read_unsigned) == v);
  }
}

TEST_CASE("signed integers carry the sign in bit 0", "[cap:oasis-format][req:Unsigned and signed integers]") {
  CHECK(signed_bytes(0) == hex("00"));
  CHECK(signed_bytes(1) == hex("02"));
  CHECK(signed_bytes(-1) == hex("03"));
  CHECK(signed_bytes(63) == hex("7E"));
  CHECK(signed_bytes(-64) == hex("81 01"));
  CHECK(signed_bytes(64) == hex("80 01"));
  CHECK(signed_bytes(-200) == hex("91 03"));
  CHECK(decoded<std::int64_t>(hex("01"), oasis::read_signed) == 0);  // negative zero
  const std::int64_t big = std::numeric_limits<std::int64_t>::max();
  CHECK(decoded<std::int64_t>(signed_bytes(big), oasis::read_signed) == big);
  CHECK(decoded<std::int64_t>(signed_bytes(-big), oasis::read_signed) == -big);
}

TEST_CASE("non-minimal integers are accepted and a 65th bit overflows",
          "[cap:oasis-format][req:Unsigned and signed integers]") {
  std::size_t consumed = 0;
  CHECK(decoded<std::uint64_t>(hex("80 80 00"), oasis::read_unsigned, &consumed) == 0);
  CHECK(consumed == 3);
  CHECK(decoded<std::uint64_t>(hex("81 80 80 80 80 80 80 80 80 80 80 00"), oasis::read_unsigned) == 1);
  // 2^63 is the 10th byte's single bit; a second bit there needs 65 bits.
  CHECK(decoded<std::uint64_t>(hex("80 80 80 80 80 80 80 80 80 01"), oasis::read_unsigned) == std::uint64_t{1} << 63);
  for (const auto* overflow : {"80 80 80 80 80 80 80 80 80 02", "80 80 80 80 80 80 80 80 80 80 01"}) {
    const std::vector<std::byte> bytes = hex(overflow);
    oasis::ByteCursor cursor{bytes};
    auto r = oasis::read_unsigned(cursor);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().code == "OASIS-CODEC-005");
    CHECK(r.error().text == "integer overflow");
  }
  const std::vector<std::byte> truncated = hex("80 80");
  oasis::ByteCursor cursor{truncated};
  auto r = oasis::read_unsigned(cursor);
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().code == "OASIS-CODEC-002");
}

TEST_CASE("reals of types 0-7 decode exactly", "[cap:oasis-format][req:Real numbers]") {
  CHECK(decoded<double>(hex("00 E8 07"), oasis::read_real) == 1000.0);
  CHECK(decoded<double>(hex("01 05"), oasis::read_real) == -5.0);
  CHECK(decoded<double>(hex("02 04"), oasis::read_real) == 0.25);
  CHECK(decoded<double>(hex("03 02"), oasis::read_real) == -0.5);
  CHECK(decoded<double>(hex("04 03 02"), oasis::read_real) == 1.5);
  CHECK(decoded<double>(hex("05 01 03"), oasis::read_real) == -1.0 / 3.0);
  CHECK(decoded<double>(hex("06 00 00 C0 3F"), oasis::read_real) == 1.5);
  CHECK(decoded<double>(hex("07 00 00 00 00 00 00 F8 3F"), oasis::read_real) == 1.5);
  for (const auto* bad : {"02 00", "05 01 00", "08 00"}) {
    const std::vector<std::byte> bytes = hex(bad);
    oasis::ByteCursor cursor{bytes};
    auto r = oasis::read_real(cursor);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().code == "OASIS-CODEC-006");
  }
  const std::vector<std::byte> zero = hex("02 00");
  oasis::ByteCursor cursor{zero};
  CHECK(oasis::read_real(cursor).error().text == "division by zero in real");
}

TEST_CASE("the writer picks the narrowest real form", "[cap:oasis-format][req:Real numbers]") {
  CHECK(real_bytes(1000.0) == hex("00 E8 07"));
  CHECK(real_bytes(-3.0) == hex("01 03"));
  CHECK(real_bytes(0.5) == hex("02 02"));
  CHECK(real_bytes(-0.001) == hex("03 E8 07"));
  CHECK(real_bytes(1.5) == hex("07 00 00 00 00 00 00 F8 3F"));
  for (const double v : {0.1, 1e300, -2.75, 1.0 / 3.0, 4503599627370497.0}) {
    CHECK(decoded<double>(real_bytes(v), oasis::read_real) == v);
  }
}

TEST_CASE("strings are a length and bytes, checked by kind", "[cap:oasis-format][req:Strings and character sets]") {
  CHECK(oasis_test::str("1.0") == hex("03 31 2E 30"));
  CHECK(decoded<std::string>(hex("03 31 2E 30"), oasis::read_string) == "1.0");
  CHECK(oasis::legal_string(oasis::StringKind::a_string, "a b~"));
  CHECK_FALSE(oasis::legal_string(oasis::StringKind::a_string, "a\tb"));
  CHECK_FALSE(oasis::legal_string(oasis::StringKind::n_string, "my cell"));
  CHECK_FALSE(oasis::legal_string(oasis::StringKind::n_string, ""));
  CHECK(oasis::legal_string(oasis::StringKind::n_string, "top"));
  CHECK(oasis::legal_string(oasis::StringKind::b_string, std::string{"\0\xff", 2}));
  const std::vector<std::byte> short_string = hex("05 41 42");
  oasis::ByteCursor cursor{short_string};
  CHECK(oasis::read_string(cursor).error().code == "OASIS-CODEC-002");
}

TEST_CASE("a name with a space warns and is used as it is", "[cap:oasis-format][req:Strings and character sets]") {
  oasis_test::File f;
  f.add("03").add(oasis_test::str("my cell"));
  const auto d = oasis_test::decode(f.end());
  REQUIRE_FALSE(d.error);
  CHECK(d.text("OASIS-CODEC-007") == "illegal character in n-string \"my cell\"");
  CHECK(d.names.cellnames.at(0) == "my cell");
}

TEST_CASE("the four delta forms", "[cap:oasis-format][req:Delta encodings]") {
  using F = oasis::DeltaForm;
  CHECK(decoded<oasis::Point64>(hex("2A"), [](auto& c) { return oasis::read_delta(c, F::two); }) == pt(-10, 0));
  CHECK(decoded<oasis::Point64>(hex("55"), [](auto& c) { return oasis::read_delta(c, F::three); }) == pt(-10, 10));
  CHECK(decoded<oasis::Point64>(hex("03"), [](auto& c) { return oasis::read_delta(c, F::one_vertical); }) == pt(0, -1));
  CHECK(delta_bytes(pt(3, -5), F::g) == hex("0D 0B"));
  CHECK(delta_bytes(pt(-7, -7), F::g) == hex("7C"));
  CHECK(delta_bytes(pt(0, 0), F::g) == hex("00"));
  CHECK(decoded<oasis::Point64>(hex("0D 0B"), [](auto& c) { return oasis::read_delta(c, F::g); }) == pt(3, -5));
  CHECK(decoded<oasis::Point64>(hex("7C"), [](auto& c) { return oasis::read_delta(c, F::g); }) == pt(-7, -7));
  for (const auto d : {pt(5, 0), pt(0, -9), pt(-4, 0), pt(0, 3)}) {
    CHECK(decoded<oasis::Point64>(delta_bytes(d, F::two), [](auto& c) { return oasis::read_delta(c, F::two); }) == d);
  }
  for (const auto d : {pt(6, 6), pt(-2, 2), pt(-8, -8), pt(1, -1)}) {
    CHECK(decoded<oasis::Point64>(delta_bytes(d, F::three), [](auto& c) { return oasis::read_delta(c, F::three); }) ==
          d);
  }
}

TEST_CASE("a delta component outside 32 bits is a coordinate overflow", "[cap:oasis-format][req:Delta encodings]") {
  const std::vector<std::byte> bytes = signed_bytes(std::int64_t{1} << 31);
  oasis::ByteCursor cursor{bytes};
  auto r = oasis::read_delta(cursor, oasis::DeltaForm::one_horizontal);
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().code == "OASIS-CODEC-008");
  CHECK(r.error().text == "coordinate overflow");
}

TEST_CASE("point lists of each type decode to their vertices", "[cap:oasis-format][req:Point lists]") {
  const auto list = [](std::string_view bytes) {
    return decoded<oasis::PointList>(hex(bytes), oasis::read_point_list);
  };
  CHECK(list("00 02 50 3C").vertices == std::vector{pt(40, 0), pt(40, 30)});
  CHECK(list("01 04 28 14 15 28").vertices == std::vector{pt(0, 20), pt(10, 20), pt(10, 10), pt(30, 10)});
  CHECK(list("02 02 28 29").vertices == std::vector{pt(10, 0), pt(10, 10)});
  CHECK(list("03 02 50 55").vertices == std::vector{pt(10, 0), pt(0, 10)});
  CHECK(list("04 02 0D 0B 00").vertices == std::vector{pt(3, -5), pt(3, -5)});
  // Type 5 second differences: (10,0), (0,10), (-10,-10) give (0,0) (10,0) (20,10) (20,10).
  CHECK(list("05 03 A0 01 A2 01 AC 01").vertices == std::vector{pt(10, 0), pt(20, 10), pt(20, 10)});
  const std::vector<std::byte> bad_type = hex("06 01 00");
  oasis::ByteCursor cursor{bad_type};
  CHECK(oasis::read_point_list(cursor).error().code == "OASIS-CODEC-009");
  const std::vector<std::byte> empty = hex("00 00");
  oasis::ByteCursor empty_cursor{empty};
  CHECK(oasis::read_point_list(empty_cursor).error().text == "invalid point list (count 0)");
}

TEST_CASE("repetitions keep their parameters and expand in file order", "[cap:oasis-format][req:Repetitions]") {
  const auto rep = [](std::string_view bytes) {
    return decoded<oasis::Repetition>(hex(bytes), oasis::read_repetition);
  };
  const oasis::Repetition grid = rep("01 01 00 0A 14");
  CHECK(grid.columns == 3);
  CHECK(grid.rows == 2);
  CHECK(grid.column_step == pt(10, 0));
  CHECK(grid.row_step == pt(0, 20));
  CHECK(oasis::expand(grid) == std::vector{pt(0, 0), pt(10, 0), pt(20, 0), pt(0, 20), pt(10, 20), pt(20, 20)});
  CHECK(oasis::expand(rep("04 01 05 07")) == std::vector{pt(0, 0), pt(5, 0), pt(12, 0)});
  CHECK(oasis::expand(rep("09 00 48")) == std::vector{pt(0, 0), pt(4, 4)});
  CHECK(oasis::expand(rep("02 00 07")) == std::vector{pt(0, 0), pt(7, 0)});
  CHECK(oasis::expand(rep("03 01 05")) == std::vector{pt(0, 0), pt(0, 5), pt(0, 10)});
  CHECK(oasis::expand(rep("05 01 0A 01 02")) == std::vector{pt(0, 0), pt(10, 0), pt(30, 0)});
  CHECK(oasis::expand(rep("06 01 03 00")) == std::vector{pt(0, 0), pt(0, 3), pt(0, 3)});  // zero spacing kept
  CHECK(oasis::expand(rep("07 01 02 04 01")) == std::vector{pt(0, 0), pt(0, 8), pt(0, 10)});
  CHECK(oasis::expand(rep("08 00 00 10 12")) == std::vector{pt(0, 0), pt(1, 0), pt(0, 1), pt(1, 1)});
  CHECK(oasis::expand(rep("0A 01 10 12")) == std::vector{pt(0, 0), pt(1, 0), pt(1, 1)});
  CHECK(oasis::expand(rep("0B 01 05 10 12")) == std::vector{pt(0, 0), pt(5, 0), pt(5, 5)});
  CHECK(error_code(hex("0C"), oasis::read_repetition) == "OASIS-CODEC-010");
  for (const auto* zero_grid : {"05 00 00 01 01", "07 00 00 01 01", "0B 00 00 08 08"}) {
    CHECK(error_code(hex(zero_grid), oasis::read_repetition) == "OASIS-CODEC-010");
  }
}

TEST_CASE("huge repetition counts are never expanded eagerly", "[cap:oasis-format][req:Repetitions]") {
  // 2^62 x 2^62 instances: the count saturates, and offsets are computed on demand.
  std::vector<std::byte> bytes = hex("01");
  oasis::write_unsigned(bytes, (std::uint64_t{1} << 62) - 2);
  oasis::write_unsigned(bytes, (std::uint64_t{1} << 62) - 2);
  oasis::write_unsigned(bytes, 0);
  oasis::write_unsigned(bytes, 0);
  const auto r = decoded<oasis::Repetition>(bytes, oasis::read_repetition);
  CHECK(oasis::instance_count(r) == std::numeric_limits<std::uint64_t>::max());
  CHECK(oasis::offset_at(r, 12345) == pt(0, 0));
  // A spacing whose farthest offset needs more than 64 bits drops the element.
  std::vector<std::byte> far = hex("02");
  oasis::write_unsigned(far, (std::uint64_t{1} << 40));
  oasis::write_unsigned(far, 2000000000);
  CHECK(error_code(far, oasis::read_repetition) == "OASIS-CODEC-008");
}

TEST_CASE("security fix M6-S3: a point list above 2^24 points is read, not stored, and dropped (approved cap)",
          "[cap:oasis-format][req:Point lists]") {
  // Q-OASIS-14 (approved 2026-11-21), both modes: 2^24 + 1 one-byte 2-deltas.
  const std::uint64_t count = oasis::kMaxListEntries + 1;
  std::vector<std::byte> bytes = hex("02");
  oasis::write_unsigned(bytes, count);
  const std::size_t list_size = bytes.size() + count;
  bytes.resize(list_size, std::byte{0x28});
  bytes.push_back(std::byte{0x7F});  // the next field, not part of the list
  oasis::ByteCursor cursor{bytes};
  const auto r = oasis::read_point_list(cursor);
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().code == "OASIS-CODEC-030");
  CHECK(r.error().text == "point list of 16777217 points, more than 16777216; the element is dropped");
  CHECK(cursor.offset == list_size);
  CHECK(diag::make("OASIS-CODEC-030", "").severity == diag::Severity::error);
}

TEST_CASE("security fix M6-S3: a list repetition above 2^24 displacements is read, not stored, and dropped "
          "(approved cap)",
          "[cap:oasis-format][req:Repetitions]") {
  // Q-OASIS-14 (approved 2026-11-21), both modes: n' = 2^24 gives 2^24 + 1 displacements.
  for (const std::uint8_t type : std::initializer_list<std::uint8_t>{4, 5, 6, 7, 10, 11}) {
    CAPTURE(type);
    std::vector<std::byte> bytes{std::byte{type}};
    oasis::write_unsigned(bytes, oasis::kMaxListEntries);
    if (type == 5 || type == 7 || type == 11) {
      bytes.push_back(std::byte{1});  // grid
    }
    const std::size_t size = bytes.size() + oasis::kMaxListEntries + 1;
    bytes.resize(size, std::byte{0x02});  // one-byte steps (unsigned, or a one-byte g-delta)
    bytes.push_back(std::byte{0x7F});
    oasis::ByteCursor cursor{bytes};
    const auto r = oasis::read_repetition(cursor);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().code == "OASIS-CODEC-030");
    CHECK(r.error().text == "repetition of type " + std::to_string(type) +
                                " lists more than 16777216 displacements; the element is dropped");
    CHECK(cursor.offset == size);
  }
  // The grid types keep counts only, so the cap does not apply to them.
  std::vector<std::byte> grid = hex("02");
  oasis::write_unsigned(grid, oasis::kMaxListEntries * 4);
  grid.push_back(std::byte{0x05});
  CHECK(oasis::instance_count(decoded<oasis::Repetition>(grid, oasis::read_repetition)) ==
        oasis::kMaxListEntries * 4 + 2);
}

TEST_CASE("intervals of types 0-4", "[cap:oasis-format][req:Intervals]") {
  const auto interval = [](std::string_view bytes) {
    return decoded<oasis::Interval>(hex(bytes), oasis::read_interval);
  };
  CHECK(interval("00") == oasis::Interval{0, 0, 0xffffffffU});
  CHECK(interval("01 05") == oasis::Interval{1, 0, 5});
  CHECK(interval("02 05") == oasis::Interval{2, 5, 0xffffffffU});
  CHECK(interval("03 31") == oasis::Interval{3, 49, 49});
  CHECK(interval("04 02 07") == oasis::Interval{4, 2, 7});
  CHECK(interval("04 07 02").empty());
  const std::vector<std::byte> bad = hex("05");
  oasis::ByteCursor cursor{bad};
  CHECK(oasis::read_interval(cursor).error().code == "OASIS-CODEC-011");
}

TEST_CASE("an inverted LAYERNAME interval warns", "[cap:oasis-format][req:Intervals]") {
  oasis_test::File f;
  f.add("0B").add(oasis_test::str("M1")).add("04 07 02 00");
  const auto d = oasis_test::decode(f.end());
  REQUIRE_FALSE(d.error);
  CHECK(d.has("OASIS-CODEC-012"));
  REQUIRE(d.names.layernames.size() == 1);
  CHECK(d.names.layernames[0].layers.empty());
}
