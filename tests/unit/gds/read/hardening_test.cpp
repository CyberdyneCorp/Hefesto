// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Malformed GDS input read through gds::read (design R9, D21): recursion
// caps (look-ahead chains, self-magnifying structures, reference cycles),
// array copy caps, coordinates beyond the plane, truncated streams, block
// padding and random bytes. Every case must end without a crash, sanitizer
// report or unbounded allocation; the caps report GDS-READ-011. Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include <random>

#include "read_fixture.hpp"

using namespace gds_read_test;

namespace {

void square(GdsFile& f, std::int32_t x = 0, std::int32_t size = 10) {
  f.boundary(68, 20, {{x, 0}, {x, size}, {x + size, size}, {x + size, 0}, {x, 0}});
}

bool has_code(const Session& s, std::string_view code) {
  return std::any_of(s.last.diagnostics.begin(), s.last.diagnostics.end(),
                     [&](const diag::Diagnostic& d) { return d.code == code; });
}

}  // namespace

TEST_CASE("a self-magnifying structure is capped", "[cap:gds-read][req:Magnified instances]") {
  Session s;
  GdsFile f;
  f.library().begin("A");
  square(f);
  f.sref("A", {0, 0}, RefSpec{0, 2.0, {}, {}}).end();
  f.begin("top").sref("A", {0, 0}, RefSpec{0, 2.0, {}, {}}).end().end_library();
  s.read(f);
  CHECK(has_code(s, "GDS-READ-011"));
  CHECK(s.has_cell("A_mag2"));
}

TEST_CASE("an enormous magnification is capped", "[cap:gds-read][req:Magnified instances]") {
  Session s;
  GdsFile f;
  f.library().begin("A");
  square(f);
  f.end().begin("top").sref("A", {0, 0}, RefSpec{0, 1e12, {}, {}}).end().end_library();
  s.read(f);
  CHECK(has_code(s, "GDS-READ-011"));
  REQUIRE(uses(s.cell("top")).size() == 1);
  CHECK(uses(s.cell("top")).front()->def == &s.cell("A"));
}

TEST_CASE("a long look-ahead chain is capped", "[cap:gds-read][req:Post-order reading and file repositioning]") {
  Session s;
  s.options.settings.ordering = true;
  constexpr int kChain = 600;
  GdsFile f;
  f.library();
  for (int i = 0; i < kChain; ++i) {
    f.begin("S" + std::to_string(i)).sref("S" + std::to_string(i + 1), {0, 0}).end();
  }
  f.begin("S" + std::to_string(kChain));
  square(f);
  f.end().end_library();
  s.read(f);
  CHECK(has_code(s, "GDS-READ-011"));
  for (int i = 0; i <= kChain; ++i) {
    INFO("S" << i);
    CHECK(s.cell("S" + std::to_string(i)).flags().available);
  }
}

TEST_CASE("reference cycles are refused", "[cap:gds-read][req:Use placement rules]") {
  Session s;
  s.options.settings.ordering = true;
  GdsFile f;
  f.library().begin("A").sref("B", {0, 0}).end().begin("B").sref("A", {0, 0}).end().end_library();
  s.read(f);
  CHECK(s.said("(Use skipped)"));
}

TEST_CASE("flatten copies into a huge array are capped", "[cap:gds-read][req:Flattening small cells on input]") {
  Session s;
  s.options.settings.flatten = true;
  GdsFile f;
  f.library().begin("VIA");
  square(f, 0, 2);
  f.end().begin("top").aref("VIA", 32767, 32767, {{0, 0}, {32767 * 4, 0}, {0, 32767 * 4}}).end().end_library();
  s.read(f);
  CHECK(has_code(s, "GDS-READ-011"));
}

TEST_CASE("coordinates beyond the plane are clamped", "[cap:gds-read][req:Coordinate conversion and automatic rescaling]") {
  Session s;
  GdsFile f;
  f.library().begin("top");
  f.boundary(68, 20, {{INT32_MIN, INT32_MIN}, {INT32_MIN, INT32_MAX}, {INT32_MAX, INT32_MAX}, {INT32_MAX, INT32_MIN},
                      {INT32_MIN, INT32_MIN}});
  f.box(68, 20, {{INT32_MIN, 0}, {INT32_MIN, 5}, {INT32_MAX, 5}, {INT32_MAX, 0}, {INT32_MIN, 0}});
  f.path(68, 20, PathSpec{2, INT32_MAX, INT32_MAX, INT32_MIN}, {{INT32_MIN, 0}, {INT32_MAX, 0}});
  f.text(68, 5, {INT32_MAX, INT32_MIN}, "far", TextSpec{0, INT32_MAX, true, 1e300, 1e300});
  f.sref("top", {INT32_MAX, INT32_MAX});
  f.end().end_library();
  s.read(f);
  CHECK(s.said("Very large point in path"));
  CHECK(s.has_cell("top"));
}

TEST_CASE("truncated and padded streams", "[cap:gds-read][req:Record framing and primitive data types]") {
  GdsFile f;
  f.library().begin("A");
  square(f);
  f.end().begin("B").sref("C", {0, 0});
  SECTION("truncated inside a record") {
    Session s;
    gds_test::Bytes bytes = f.bytes();
    bytes.resize(bytes.size() - 6);  // ENDEL and half of the XY data
    gds::MemorySource source{bytes};
    s.last = gds::read(s.editor, source, s.options);
    CHECK(s.said("Unexpected end of file inside the record"));
    CHECK(s.cell("A").flags().available);
  }
  SECTION("block padding after a missing look-ahead target") {
    Session s;
    s.options.settings.ordering = true;
    GdsFile padded = f;
    padded.end().end_library();
    gds_test::Bytes bytes = padded.bytes();
    bytes.resize(bytes.size() + 2048, std::byte{0});
    gds::MemorySource source{bytes};
    s.last = gds::read(s.editor, source, s.options);
    CHECK(s.has_cell("C"));
    CHECK(s.said("cell C was used but not defined."));
  }
}

TEST_CASE("random bytes never crash the reader", "[cap:gds-read][req:Record framing and primitive data types]") {
  std::mt19937 rng(20261114);
  GdsFile header;
  header.library().begin("A");
  for (int round = 0; round < 64; ++round) {
    Session s;
    gds_test::Bytes bytes = header.bytes();
    const std::size_t extra = rng() % 512;
    for (std::size_t i = 0; i < extra; ++i) {
      bytes.push_back(static_cast<std::byte>(rng() & 0xffU));
    }
    gds::MemorySource source{bytes};
    s.last = gds::read(s.editor, source, s.options);
    CHECK(s.has_cell("A"));
  }
}
