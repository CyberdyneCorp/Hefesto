// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// gds-write "Library header and trailer" (Q-MASK-201), "Date records"
// (Q-MASK-202, 203), "Complete annotated example" and "Compressed output".
// Owner: GDSW.

#include <catch2/catch_test_macros.hpp>

#include <iomanip>
#include <sstream>

#include "write_fixture.hpp"

namespace gds_write_test {
namespace {

std::string hex(const std::string& bytes, std::size_t from = 0, std::size_t count = std::string::npos) {
  std::ostringstream out;
  const std::size_t end = count == std::string::npos ? bytes.size() : std::min(bytes.size(), from + count);
  for (std::size_t i = from; i < end; ++i) {
    if (i > from) {
      out << ' ';
    }
    out << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
        << static_cast<int>(static_cast<unsigned char>(bytes[i]));
  }
  return out.str();
}

constexpr std::string_view kEpoch = "00 46 00 01 00 01 00 00 00 00 00 00";  // t = 0 under UTC
constexpr std::string_view kNowDate = "00 7B 00 0B 00 0E 00 16 00 0D 00 14";  // 2023-11-14 22:13:20
constexpr std::string_view kNanoUnits = "00 14 03 05 3E 41 89 37 4B C6 A7 F0 39 44 B8 2F A0 9B 5A 54";
constexpr std::string_view kAngstromUnits = "00 14 03 05 3D 68 DB 8B AC 71 0C B4 38 6D F3 7F 67 5E F6 EC";

// The reference case: root `top` with metal1 (0,0)-(4,2) and no labels.
db::CellDef& reference_case(Fixture& f) {
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
  return top;
}

TEST_CASE("the reference case is written byte for byte", "[cap:gds-write][req:Complete annotated example]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    const std::string bytes = f.write(reference_case(f));
    REQUIRE(f.last.ok());
    REQUIRE(bytes.size() == 170);
    const std::string mod{on ? kNowDate : kEpoch};
    const std::string expected = "00 06 00 02 00 03 00 1C 01 02 " + std::string{kEpoch} + " " + mod +
                                 " 00 08 02 06 74 6F 70 00 " + std::string{kNanoUnits} + " 00 1C 05 02 " +
                                 std::string{kEpoch} + " " + mod +
                                 " 00 08 06 06 74 6F 70 00 00 04 08 00 00 06 0D 02 00 31 00 06 0E 02 00 00 "
                                 "00 2C 10 03 00 00 00 00 00 00 00 00 00 00 01 90 00 00 00 00 00 00 01 90 00 00 "
                                 "00 C8 00 00 00 00 00 00 00 C8 00 00 00 00 00 00 00 00 00 04 11 00 00 04 07 00 "
                                 "00 04 04 00";
    CHECK(hex(bytes) == expected);
  }
}

TEST_CASE("the header bytes of a nanometer style", "[cap:gds-write][req:Library header and trailer]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  const std::string bytes = f.write(top);
  REQUIRE(f.last.ok());
  CHECK(hex(bytes, 0, 6) == "00 06 00 02 00 03");
  CHECK(hex(bytes, 6, 16) == "00 1C 01 02 " + std::string{kEpoch});
  CHECK(hex(bytes, 34, 8) == "00 08 02 06 74 6F 70 00");
  CHECK(hex(bytes, 42, 20) == kNanoUnits);
  CHECK(hex(bytes, bytes.size() - 4) == "00 04 04 00");
}

TEST_CASE("angstrom units, and a nanometer write after an angstrom write",
          "[cap:gds-write][req:Library header and trailer]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& top = f.cell("top");
    f.style.options |= mask::style_option::angstroms;
    f.style.scale.expander = 10;  // 10 angstroms per mask unit, so the scales stay whole
    std::string bytes = f.write(top);
    REQUIRE(f.last.ok());
    CHECK(hex(bytes, 42, 20) == kAngstromUnits);
    // Q-MASK-201 (fix in both modes): UNITS follows the current style.
    f.style.options &= ~mask::style_option::angstroms;
    f.style.scale.expander = 1;
    bytes = f.write(top);
    REQUIRE(f.last.ok());
    CHECK(hex(bytes, 42, 20) == kNanoUnits);
  }
}

TEST_CASE("a fixed date stamp and the modification date", "[cap:gds-write][req:Date records]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& top = f.cell("top");
    f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
    f.options.settings.datestamp = 1700000000;
    const std::vector<Rec> recs = decode(f.write(top));
    REQUIRE(f.last.ok());
    const std::vector<std::int16_t> created{123, 11, 14, 22, 13, 20};
    const std::vector<std::int16_t> modified = on ? created : std::vector<std::int16_t>{70, 1, 1, 0, 0, 0};
    for (const gds::RecordType t : {gds::RecordType::bgnlib, gds::RecordType::bgnstr}) {
      for (const Rec& r : recs) {
        if (r.record() == t) {
          const std::vector<std::int16_t> v = r.i2();
          REQUIRE(v.size() == 12);
          CHECK(std::vector<std::int16_t>(v.begin(), v.begin() + 6) == created);
          CHECK(std::vector<std::int16_t>(v.begin() + 6, v.end()) == modified);
        }
      }
    }
  }
}

TEST_CASE("without a date stamp the cell timestamp is the creation date", "[cap:gds-write][req:Date records]") {
  Fixture f;
  db::CellDef& inv = f.cell("inv");
  inv.set_timestamp(1600000000);
  f.options.settings.datestamp.reset();
  const std::vector<Rec> recs = decode(f.write(inv));
  REQUIRE(f.last.ok());
  const std::vector<std::int16_t> expected{120, 9, 13, 12, 26, 40};
  for (const Rec& r : recs) {
    if (r.record() == gds::RecordType::bgnstr || r.record() == gds::RecordType::bgnlib) {
      const std::vector<std::int16_t> v = r.i2();
      CHECK(std::vector<std::int16_t>(v.begin(), v.begin() + 6) == expected);
    }
  }
}

TEST_CASE("dates follow the pinned zone", "[cap:gds-write][req:Date records]") {
  // Q-MASK-203: local time in both modes; the epoch is 1969-12-31 21:00 at UTC-3.
  const auto fields = gds::structure_dates(0, 0, compat::CompatSettings{false}, "<-03>3");
  CHECK(std::vector<std::int16_t>(fields.begin(), fields.end()) ==
        std::vector<std::int16_t>{69, 12, 31, 21, 0, 0, 69, 12, 31, 21, 0, 0});
}

TEST_CASE("compressed and plain outputs agree", "[cap:gds-write][req:Compressed output]") {
  Fixture f;
  db::CellDef& top = reference_case(f);
  const std::string plain = f.write(top, "/plain.gds");
  REQUIRE(f.last.ok());
  f.options.settings.compress = 9;
  const std::string packed = f.write(top, "/packed.gds.gz");
  REQUIRE(f.last.ok());
  REQUIRE(packed.size() >= 2);
  CHECK(static_cast<unsigned char>(packed[0]) == 0x1f);
  CHECK(static_cast<unsigned char>(packed[1]) == 0x8b);
  auto source = gds::open_source(f.fs, "/packed.gds.gz");
  REQUIRE(source.ok());
  std::string inflated;
  std::array<std::byte, 256> buffer{};
  for (;;) {
    auto n = source.value()->read(buffer);
    REQUIRE(n.ok());
    if (n.value() == 0) {
      break;
    }
    for (std::size_t i = 0; i < n.value(); ++i) {
      inflated.push_back(static_cast<char>(buffer[i]));
    }
  }
  CHECK(inflated == plain);
}

}  // namespace
}  // namespace gds_write_test
