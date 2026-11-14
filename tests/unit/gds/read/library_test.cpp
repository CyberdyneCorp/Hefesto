// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The library level of the GDS reader (tasks.md 9.2): the header grammar,
// the library cell name, units and the read scale, coordinate conversion
// with automatic rescaling, and structure timestamps in both magic-compat
// settings (Q-MASK-134). Dates are converted in the pinned zone "UTC0"
// (Q-MASK-132). Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "../stream/stream_fixture.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/read/library.hpp"
#include "hefesto/gds/stream/io.hpp"

namespace gds = hefesto::gds;
namespace diag = hefesto::diag;
using gds_test::Gds;
using gds_test::hex;

namespace {

constexpr std::uint8_t kUnits = 0x03;
constexpr std::uint8_t kBgnstr = 0x05;

struct Parsed {
  diag::Outcome<gds::LibraryHeader> header;
  std::vector<diag::Diagnostic> messages;
  std::uint64_t position;
};

Parsed parse(const Gds& g) {
  gds::MemorySource source{g.bytes()};
  gds::RecordReader reader{source};
  diag::CollectingSink sink;
  auto header = gds::read_library_header(reader, sink);
  return Parsed{std::move(header), sink.take(), reader.position()};
}

std::string error_text(const Parsed& p) {
  REQUIRE_FALSE(p.header.ok());
  return p.header.error().text;
}

gds::Record bgnstr(std::initializer_list<int> values) {
  Gds g;
  g.i2(kBgnstr, values);
  gds::MemorySource source{g.bytes()};
  gds::RecordReader reader{source};
  auto r = reader.next();
  REQUIRE(r.ok());
  return *r.value();
}

}  // namespace

TEST_CASE("the library header is read through UNITS", "[cap:gds-read][req:Library grammar and header processing]") {
  Gds g;
  g.library("lib/my top").empty(0x04);
  const Parsed p = parse(g);
  REQUIRE(p.header.ok());
  CHECK(p.header.value().version == 600);
  CHECK(p.header.value().libname == "lib/my top");
  CHECK(p.header.value().user_units == 0.001);
  CHECK(p.header.value().meters_per_unit == 1e-9);
  CHECK(p.position == g.size() - 4);  // at ENDLIB
  REQUIRE(p.messages.size() == 1);
  CHECK(p.messages[0].code == "GDS-READ-004");
  CHECK(p.messages[0].text == "Library written using GDS-II Release 6.0");
}

TEST_CASE("the release message", "[cap:gds-read][req:Library grammar and header processing]") {
  CHECK(gds::release_message(3) == "Library written using GDS-II Release 3.0");
  CHECK(gds::release_message(599) == "Library written using GDS-II Release 599.0");
  CHECK(gds::release_message(600) == "Library written using GDS-II Release 6.0");
  CHECK(gds::release_message(607) == "Library written using GDS-II Release 6.7");
}

TEST_CASE("header records are skipped where the grammar allows them",
          "[cap:gds-read][req:Library grammar and header processing]") {
  Gds g;
  g.i2(0x00, {5}).i2(0x01, {124, 1, 1, 0, 0, 0, 124, 1, 1, 0, 0, 0});
  g.i2(57, {1}).str(58, "srf").i2(59, {0, 0}).i2(57, {2});  // LIBDIRSIZE, SRFNAME, LIBSECUR
  g.str(0x02, "LIB");
  g.str(0x1f, "ref").str(0x20, "font").i2(0x22, {3}).i2(0x36, {0}).str(0x37, "m").empty(0x38).i2(0x23, {0});
  g.str(0x24, "s");
  g.rec(kUnits, 5, hex("3E 41 89 37 4B C6 A7 F0  39 44 B8 2F A0 9B 5A 54"));
  const Parsed p = parse(g);
  REQUIRE(p.header.ok());
  CHECK(p.header.value().libname == "LIB");
  CHECK(p.messages[0].text == "Library written using GDS-II Release 5.0");
}

TEST_CASE("a missing UNITS record aborts the read", "[cap:gds-read][req:Library grammar and header processing]") {
  Gds g;
  g.i2(0x00, {600}).i2(0x01, {124, 1, 1, 0, 0, 0, 124, 1, 1, 0, 0, 0}).str(0x02, "LIB").i2(kBgnstr, {0});
  const Parsed p = parse(g);
  CHECK(error_text(p) == "Unexpected record type in input: Expected UNITS record but got BGNSTR.");
  CHECK(p.header.error().code == "GDS-READ-001");
}

TEST_CASE("wrong or missing header records are errors", "[cap:gds-read][req:Library grammar and header processing]") {
  {
    Gds g;
    g.empty(0x01);
    CHECK(error_text(parse(g)) == "Unexpected record type in input: Expected HEADER record but got BGNLIB.");
  }
  {
    Gds g;
    g.i2(0x00, {600}).empty(0x02);
    CHECK(error_text(parse(g)) == "Unexpected record type in input: Expected BGNLIB record but got LIBNAME.");
  }
  {
    Gds g;
    g.i2(0x00, {600}).i2(0x01, {0}).rec(200, 0, {});
    CHECK(error_text(parse(g)) == "Unexpected record type in input: Expected LIBNAME record but got 200.");
  }
  const auto eof_code = [](const Gds& g) {
    const Parsed p = parse(g);
    REQUIRE_FALSE(p.header.ok());
    CHECK(p.header.error().text == "Unexpected EOF.");
    return p.header.error().code;
  };
  CHECK(eof_code(Gds{}) == "GDS-READ-002");
  Gds no_value;
  no_value.empty(0x00);  // HEADER without its value
  CHECK(eof_code(no_value) == "GDS-READ-002");
  Gds no_bgnlib;
  no_bgnlib.i2(0x00, {600});
  CHECK(eof_code(no_bgnlib) == "GDS-READ-002");
  Gds short_units;
  short_units.i2(0x00, {600}).i2(0x01, {0}).str(0x02, "L").rec(kUnits, 5, hex("3E 41 89 37 4B C6 A7 F0"));
  CHECK(eof_code(short_units) == "GDS-READ-002");
}

TEST_CASE("the record after the structures must be ENDLIB", "[cap:gds-read][req:Library grammar and header processing]") {
  const auto end = [](const Gds& g) {
    gds::MemorySource source{g.bytes()};
    gds::RecordReader reader{source};
    return gds::read_library_end(reader);
  };
  CHECK(end(Gds{}.empty(0x04)).ok());
  const auto wrong = end(Gds{}.empty(0x07));
  REQUIRE_FALSE(wrong.ok());
  CHECK(wrong.error().text == "Unexpected record type in input: Expected ENDLIB record but got ENDSTR.");
  const auto eof = end(Gds{});
  REQUIRE_FALSE(eof.ok());
  CHECK(eof.error().code == "GDS-READ-002");
}

TEST_CASE("the library cell name is sanitized", "[cap:gds-read][req:Library grammar and header processing]") {
  const auto name = gds::library_cell_name("lib/my top", false);
  CHECK(name.name == "my_top");
  CHECK(name.modified);
  const auto plain = gds::library_cell_name("top", false);
  CHECK(plain.name == "top");
  CHECK_FALSE(plain.modified);
  CHECK(gds::library_cell_name("a b", false).name == "a_b");
  CHECK(gds::library_cell_name("a/b/", false).name == "");
  CHECK_FALSE(gds::library_cell_name("top", true).name.has_value());  // `gds library yes`
  CHECK_FALSE(gds::library_cell_name("", false).name.has_value());
}

TEST_CASE("record names follow Magic's table", "[cap:gds-read][req:Library grammar and header processing]") {
  CHECK(gds::record_name(0) == "HEADER");
  CHECK(gds::record_name(5) == "BGNSTR");
  CHECK(gds::record_name(56) == "ENDMASKS");
  CHECK(gds::record_name(59) == "LIBSECUR");
  CHECK(gds::record_name(60) == "60");
  CHECK(gds::record_name(255) == "255");
}

TEST_CASE("units give the read scale", "[cap:gds-read][req:Units and input scale]") {
  CHECK(gds::units_scale(1e-9, 1) == gds::ReadScale{1, 10});   // nanometer GDS, centimicron style
  CHECK(gds::units_scale(1e-9, 10) == gds::ReadScale{1, 1});   // nanometer GDS, nanometer style
  CHECK(gds::units_scale(1e-10, 1) == gds::ReadScale{1, 100}); // angstrom GDS
  CHECK(gds::units_scale(1e-8, 1) == gds::ReadScale{1, 1});
  CHECK(gds::units_scale(1e-6, 1) == gds::ReadScale{100, 1});
  CHECK(gds::units_scale(2.5e-9, 1) == gds::ReadScale{1, 4});  // 1 / 0.25
}

TEST_CASE("degenerate units are clamped, not undefined", "[cap:gds-read][req:Units and input scale]") {
  CHECK(gds::units_scale(0.0, 1) == gds::ReadScale{1, gds::kMaxScale});
  CHECK(gds::units_scale(-1e-9, 1) == gds::ReadScale{1, 1});
  CHECK(gds::units_scale(1e30, 1) == gds::ReadScale{gds::kMaxScale, 1});
  CHECK(gds::units_scale(std::numeric_limits<double>::quiet_NaN(), 1) == gds::ReadScale{1, 1});
  CHECK(gds::units_scale(std::numeric_limits<double>::infinity(), 1) == gds::ReadScale{gds::kMaxScale, 1});
}

TEST_CASE("an off-grid coordinate rescales", "[cap:gds-read][req:Coordinate conversion and automatic rescaling]") {
  gds::CoordinateScale scale{gds::ReadScale{1, 10}};
  CHECK(scale.convert(20, 1) == gds::Conversion{2, 1, false});
  CHECK(scale.convert(15, 1) == gds::Conversion{3, 2, false});  // r = 10 / gcd(10, 15) = 2
  CHECK(scale.scale() == gds::ReadScale{2, 10});
  CHECK(scale.convert(15, 1) == gds::Conversion{3, 1, false});  // now on the grid: 15 * 2 / 10
  CHECK(scale.convert(-7, 1) == gds::Conversion{-7, 5, false}); // r = 10 / gcd(10, 14) = 5
  CHECK(scale.scale() == gds::ReadScale{10, 10});
}

TEST_CASE("at the rescale limit values are rounded", "[cap:gds-read][req:Coordinate conversion and automatic rescaling]") {
  gds::CoordinateScale scale{gds::ReadScale{60, 7}};
  CHECK(scale.convert(1, 1) == gds::Conversion{9, 1, true});  // (60 + 3) / 7
  CHECK(scale.scale() == gds::ReadScale{60, 7});
  gds::CoordinateScale half{gds::ReadScale{51, 2}};
  CHECK(half.convert(1, 1) == gds::Conversion{26, 1, true});    // 25.5 -> 26
  CHECK(half.convert(-1, 1) == gds::Conversion{-25, 1, true});  // -25.5 -> -25
  // A smaller `cif limit` rounds earlier.
  gds::CoordinateScale limited{gds::ReadScale{1, 10}, 1};
  CHECK(limited.convert(15, 1) == gds::Conversion{2, 1, true});  // (15 + 5) / 10
}

TEST_CASE("path centerlines double and unmapped points are consumed",
          "[cap:gds-read][req:Coordinate conversion and automatic rescaling]") {
  gds::CoordinateScale scale{gds::ReadScale{1, 10}};
  CHECK(scale.convert(15, 2) == gds::Conversion{3, 1, false});  // 30 / 10
  CHECK(scale.convert(7, 0) == gds::Conversion{0, 1, false});
  CHECK(scale.scale() == gds::ReadScale{1, 10});
  // Extreme values stay exact in 64 bits.
  gds::CoordinateScale big{gds::ReadScale{gds::kMaxScale, 1}};
  CHECK(big.convert(INT32_MIN, 2).value == std::int64_t{INT32_MIN} * gds::kMaxScale * 2);
  gds::CoordinateScale tiny{gds::ReadScale{1, gds::kMaxScale}};
  CHECK(tiny.convert(INT32_MAX, 1).rounded);
  // A temporary scale is clamped as the constructor clamps, so products
  // stay within 64 bits (UBSan on the linux-asan leg).
  gds::CoordinateScale copy{gds::ReadScale{1, 1}};
  copy.set_scale(gds::ReadScale{std::int64_t{1} << 40, 0});
  CHECK(copy.scale() == gds::ReadScale{gds::kMaxScale, 1});
  CHECK(copy.convert(INT32_MIN, 2).value == std::int64_t{INT32_MIN} * gds::kMaxScale * 2);
  copy.set_scale(gds::ReadScale{-5, std::int64_t{1} << 40});
  CHECK(copy.scale() == gds::ReadScale{1, gds::kMaxScale});
}

TEST_CASE("structure timestamps by magic-compat setting", "[cap:gds-read][req:Structure timestamps]") {
  const gds::Record full_year = bgnstr({2024, 1, 1, 0, 0, 0, 2024, 1, 1, 0, 0, 0});
  CHECK(gds::structure_date(full_year, "UTC0", false).time == 1704067200);
  const auto on = gds::structure_date(full_year, "UTC0", true);
  CHECK(on.known_format);
  CHECK(on.time == 1532669056);  // 3924-01-01 truncated to 32 bits
  const gds::Record magic = bgnstr({124, 1, 15, 10, 30, 0, 125, 2, 2, 2, 2, 2});
  for (const bool compat : {true, false}) {
    CHECK(gds::structure_date(magic, "UTC0", compat).time == 1705314600);
  }
  // A date in another zone, with month and day normalized as mktime does.
  CHECK(gds::structure_date(bgnstr({124, 1, 15, 7, 30, 0, 0, 0, 0, 0, 0, 0}), "<-03>3", true).time == 1705314600);
  CHECK(gds::structure_date(bgnstr({123, 13, 15, 10, 30, 0, 0, 0, 0, 0, 0, 0}), "UTC0", true).time == 1705314600);
}

TEST_CASE("a BGNSTR of another length has timestamp zero", "[cap:gds-read][req:Structure timestamps]") {
  const gds::StructureDate date = gds::structure_date(bgnstr({124, 1, 15, 10, 30, 0}), "UTC0", true);
  CHECK_FALSE(date.known_format);
  CHECK(date.time == 0);
  CHECK(gds::unknown_timestamp_format().text == "Unknown timestamp format;  setting timestamp to zero.");
  CHECK(gds::unknown_timestamp_format().code == "GDS-READ-005");
}

TEST_CASE("the datestamp option sets the cell stamp", "[cap:gds-read][req:Structure timestamps]") {
  CHECK(gds::cell_stamp(1705314600, std::nullopt) == gds::CellStamp{1705314600, false});  // datestamp yes
  CHECK(gds::cell_stamp(1705314600, 0) == gds::CellStamp{0, false});                     // datestamp no
  CHECK(gds::cell_stamp(1705314600, 1700000000) == gds::CellStamp{1700000000, true});    // datestamp N: fixed
}
