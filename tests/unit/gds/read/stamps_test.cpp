// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Content-hash timestamps (tasks.md 9.6): the structure table, the FNV-1a
// stamp with the spec's three vectors, child stamps, dates excluded,
// repeated names, reference cycles, deep hierarchies and malformed
// streams. Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "../stream/stream_fixture.hpp"
#include "hefesto/gds/read/stamps.hpp"

namespace gds = hefesto::gds;
using gds_test::Bytes;
using gds_test::Gds;
using gds_test::hex;

namespace {

constexpr std::uint8_t kBgnstr = 0x05;
constexpr std::uint8_t kStrname = 0x06;
constexpr std::uint8_t kEndstr = 0x07;
constexpr std::uint8_t kEndlib = 0x04;

// The records of structure A after STRNAME (spec "Leaf structure stamp").
const Bytes kLeafBody = hex(
    "00 04 08 00  00 06 0d 02 00 01  00 06 0e 02 00 00  00 2c 10 03"
    "00000000 00000000  00000000 0000000a  0000000a 0000000a  0000000a 00000000  00000000 00000000"
    "00 04 11 00  00 04 07 00");
// The records of structure B after STRNAME: an SREF of A at the origin.
const Bytes kParentBody = hex("00 04 0a 00  00 06 12 06 41 00  00 0c 10 03 00 00 00 00 00 00 00 00  00 04 11 00  00 04 07 00");

Gds& structure(Gds& g, std::string_view name, const Bytes& body, int year = 124) {
  g.i2(kBgnstr, {year, 1, 1, 0, 0, 0, year, 1, 1, 0, 0, 0}).str(kStrname, name);
  return g.raw(body);
}

const gds::StructureStamp& named(const gds::StampTable& table, std::string_view name) {
  for (const auto& s : table) {
    if (s.name == name) {
      return s;
    }
  }
  FAIL("no structure " << name);
  return table.front();
}

}  // namespace

TEST_CASE("the spec's leaf, parent and empty stamps", "[cap:gds-read][req:Content-hash timestamps for read-only cells]") {
  Gds g;
  g.library("lib");
  structure(g, "A", kLeafBody);
  const std::uint64_t a_start = g.size() - kLeafBody.size();
  structure(g, "B", kParentBody);
  structure(g, "E", hex("00 04 07 00"));
  g.empty(kEndlib);
  const gds::StampTable table = gds::content_stamps(g.bytes());
  REQUIRE(table.size() == 3);
  CHECK(table[0] == gds::StructureStamp{"A", a_start, a_start + kLeafBody.size(), 143581310});
  CHECK(table[1].name == "B");
  CHECK(table[1].stamp == 1837575894);
  CHECK(table[2].stamp == 1595775465);
  CHECK(gds::lookup_stamp(table, a_start) == 143581310);
  CHECK(gds::lookup_stamp(table, table[1].start) == 1837575894);
  CHECK(gds::lookup_stamp(table, a_start + 1) == 0);
  CHECK(gds::lookup_stamp({}, 0) == 0);
}

TEST_CASE("a parent's stamp follows its child defined later", "[cap:gds-read][req:Content-hash timestamps for read-only cells]") {
  Gds g;
  g.library("lib");
  structure(g, "B", kParentBody);
  structure(g, "A", kLeafBody);
  g.empty(kEndlib);
  const gds::StampTable table = gds::content_stamps(g.bytes());
  CHECK(named(table, "A").stamp == 143581310);
  CHECK(named(table, "B").stamp == 1837575894);
}

TEST_CASE("dates do not change stamps", "[cap:gds-read][req:Content-hash timestamps for read-only cells]") {
  Gds a;
  a.library("lib");
  structure(a, "A", kLeafBody, 100);
  Gds b;
  b.i2(0x00, {600}).i2(0x01, {99, 9, 9, 9, 9, 9, 99, 9, 9, 9, 9, 9}).str(0x02, "lib");
  b.rec(0x03, 5, hex("3E 41 89 37 4B C6 A7 F0  39 44 B8 2F A0 9B 5A 54"));
  structure(b, "A", kLeafBody, 120);
  CHECK(gds::content_stamps(a.bytes())[0].stamp == gds::content_stamps(b.bytes())[0].stamp);
}

TEST_CASE("children resolve to the first definition and repeats are hashed once",
          "[cap:gds-read][req:Content-hash timestamps for read-only cells]") {
  Gds g;
  structure(g, "A", kLeafBody);
  structure(g, "A", hex("00 04 07 00"));  // a second A: references resolve to the first
  // Two SREFs of A in a row: A's stamp is mixed in once (immediate repeats are dropped).
  const Bytes twice = hex(
      "00 04 0a 00  00 06 12 06 41 00  00 0c 10 03 00 00 00 00 00 00 00 00  00 04 11 00"
      "00 04 0a 00  00 06 12 06 41 00  00 0c 10 03 00 00 00 00 00 00 00 00  00 04 11 00  00 04 07 00");
  structure(g, "B", twice);
  const gds::StampTable table = gds::content_stamps(g.bytes());
  REQUIRE(table.size() == 3);
  CHECK(table[0].stamp == 143581310);
  CHECK(table[1].stamp == 1595775465);
  CHECK(table[1].start != table[0].start);
  CHECK(table[2].stamp != 1837575894);
}

TEST_CASE("structures without STRNAME and records outside structures are not hashed",
          "[cap:gds-read][req:Content-hash timestamps for read-only cells]") {
  Gds g;
  g.library("lib");
  g.i2(kBgnstr, {0}).raw(kLeafBody);  // no STRNAME: not tabulated
  structure(g, "A", kLeafBody);
  g.i2(0x0d, {5});  // stray record between structures
  structure(g, "E", hex("00 04 07 00"));
  g.empty(kEndlib);
  structure(g, "AFTER", hex("00 04 07 00"));  // past ENDLIB
  const gds::StampTable table = gds::content_stamps(g.bytes());
  REQUIRE(table.size() == 2);
  CHECK(table[0].stamp == 143581310);
  CHECK(table[1].stamp == 1595775465);
}

TEST_CASE("a reference cycle is folded without following it",
          "[cap:gds-read][req:Content-hash timestamps for read-only cells]") {
  const auto sref = [](char child) {
    Bytes b = hex("00 04 0a 00  00 06 12 06");
    b.push_back(static_cast<std::byte>(child));
    b.push_back(std::byte{0});
    const Bytes rest = hex("00 0c 10 03 00 00 00 00 00 00 00 00  00 04 11 00  00 04 07 00");
    b.insert(b.end(), rest.begin(), rest.end());
    return b;
  };
  Gds g;
  structure(g, "P", sref('P'));  // refers to itself
  structure(g, "Q", sref('R'));
  structure(g, "R", sref('Q'));
  const gds::StampTable table = gds::content_stamps(g.bytes());
  REQUIRE(table.size() == 3);
  for (const auto& s : table) {
    CHECK(s.stamp != 0);
  }
  CHECK(table == gds::content_stamps(g.bytes()));  // deterministic
}

TEST_CASE("a deep hierarchy does not exhaust the stack", "[cap:gds-read][req:Content-hash timestamps for read-only cells]") {
  // S0 references S1, S1 references S2, ... 50000 levels.
  constexpr int kDepth = 50000;
  Gds g;
  for (int i = 0; i < kDepth; ++i) {
    const std::string child = "S" + std::to_string(i + 1);
    Gds body;
    body.empty(0x0a).str(0x12, child).i4(0x10, {0, 0}).empty(0x11).empty(kEndstr);
    structure(g, "S" + std::to_string(i), body.bytes());
  }
  const gds::StampTable table = gds::content_stamps(g.bytes());
  REQUIRE(table.size() == kDepth);
  CHECK(table.front().stamp != 0);
}

TEST_CASE("a cut-off stream keeps the structures it has", "[cap:gds-read][req:Content-hash timestamps for read-only cells]") {
  Gds g;
  g.library("lib");
  structure(g, "A", kLeafBody);
  structure(g, "B", hex("00 04 0a 00  00 06 12 06 41 00  00 0c 10 03 00 00"));  // cut inside XY
  const gds::StampTable table = gds::content_stamps(g.bytes());
  REQUIRE(table.size() == 2);
  CHECK(table[0].stamp == 143581310);
  CHECK(table[1].end == 0);
  CHECK(table[1].stamp != 0);
  // A zero-length record ends the scan.
  Gds z;
  structure(z, "A", hex("00 00 08 00"));
  const gds::StampTable cut = gds::content_stamps(z.bytes());
  REQUIRE(cut.size() == 1);
  CHECK(cut[0].end == 0);
  CHECK(cut[0].stamp != 1595775465);  // nothing hashed, not even ENDSTR
}

TEST_CASE("arbitrary bytes scan without crashing", "[cap:gds-read][req:Content-hash timestamps for read-only cells]") {
  std::uint32_t state = 99;
  for (int round = 0; round < 300; ++round) {
    Bytes bytes;
    const std::size_t size = (state = state * 1103515245U + 12345U) % 400;
    for (std::size_t i = 0; i < size; ++i) {
      state = state * 1103515245U + 12345U;
      // Bias towards small lengths and the interesting record types.
      const std::uint32_t r = state >> 16U;
      bytes.push_back(static_cast<std::byte>(i % 4 == 0 ? 0 : i % 4 == 1 ? r % 16 : i % 4 == 2 ? r % 20 : r));
    }
    const gds::StampTable table = gds::content_stamps(bytes);
    for (const auto& s : table) {
      CHECK(s.stamp >= 1);
      CHECK(s.stamp <= 0x7fffffffU);
    }
  }
}
