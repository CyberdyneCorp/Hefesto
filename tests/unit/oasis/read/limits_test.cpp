// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The input limits of an OASIS read (oasis-read "Input limits"; D1 of the
// user decisions of 2026-11-19, security review M6-S3): the GDS-equivalent
// stream and instance caps, names and strings over 65530 bytes cut,
// numbers above 32767 read as 32767, and polygons over 8191 points
// fractured. Each is a regression test of its cap. Owner: OASR (written by
// the round-4 OASCMD workstream, which the OASIS-read decisions went to).

#include <catch2/catch_test_macros.hpp>

#include "hefesto/oasis/read/limits.hpp"
#include "oasis_read_fixture.hpp"

using namespace oasis_read_test;

namespace {

/// A Manhattan comb: a band [0, 20n] x [0, 10] with n teeth [20i, 20i+10] x
/// [10, 10 + h] above it, as one polygon of 4n + 2 vertices (4n + 3 GDS
/// points with the closing one).
oasis::Element comb(std::int64_t n, std::int64_t h) {
  std::vector<oasis::Point64> rest;
  for (std::int64_t i = 0; i < n; ++i) {
    const std::int64_t x = 20 * i;
    if (i > 0) {
      rest.push_back({x, 10});
    }
    rest.push_back({x, 10 + h});
    rest.push_back({x + 10, 10 + h});
    rest.push_back({x + 10, 10});
  }
  rest.push_back({20 * n, 10});
  rest.push_back({20 * n, 0});
  return polygon(68, 20, 0, 0, std::move(rest));
}

/// The same comb as rectangles.
void comb_rects(Oas& f, std::int64_t n, std::int64_t h) {
  f.add(rect(68, 20, 0, 0, 20 * n, 10));
  for (std::int64_t i = 0; i < n; ++i) {
    f.add(rect(68, 20, 20 * i, 10, 10, h));
  }
}

hefesto::cmd::CommandResult read_with(OasSession& s, const Bytes& bytes, const oasis::ReadLimits& limits) {
  REQUIRE(s.fs.write_atomic("/data/test.oas", std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()})
              .ok());
  s.last = oasis::read(s.editor, std::string{"/data/test.oas"}, s.oasis_options, limits);
  return s.last;
}

}  // namespace

TEST_CASE("input limits: stream and instance caps", "[cap:oasis-read][req:Input limits]") {
  SECTION("the defaults are 2 GiB and 2^28 instances") {
    const oasis::ReadLimits limits;
    CHECK(limits.stream_bytes == (std::uint64_t{1} << 31));
    CHECK(limits.instances == (std::uint64_t{1} << 28));
  }
  SECTION("regression: past the instance cap the rest of the file is not read") {
    Oas f;
    f.cell("c").add(with(rect(68, 20, 0, 0, 10, 10), grid(2, 10, {20, 0})));
    OasSession s;
    (void)read_with(s, f.bytes(), oasis::ReadLimits{std::uint64_t{1} << 31, 4});
    CHECK(s.said("the file has more than 4 element instances"));
    CHECK(s.painted("c", "metal1").size() == 4);
  }
  SECTION("regression: past the stream cap the rest of the file is not read") {
    Oas f;
    f.cell("c").add(with(rect(68, 20, 0, 0, 10, 10), grid(2, 1000, {20, 0})));
    OasSession s;
    (void)read_with(s, f.bytes(), oasis::ReadLimits{2000, std::uint64_t{1} << 28});
    CHECK(s.said("GDS-equivalent stream over 2000 bytes"));
    const std::size_t painted = s.painted("c", "metal1").size();
    CHECK(painted > 0);
    CHECK(painted < 1000);
  }
}

TEST_CASE("input limits: names and strings", "[cap:oasis-read][req:Input limits]") {
  const std::string long_name(70000, 'n');
  SECTION("regression: a cell name over 65530 bytes is cut, and the top cell still loads") {
    Oas f;
    f.cell(long_name).add(rect(68, 20, 0, 0, 10, 10));
    OasSession s;
    const auto result = s.read_oasis(f.bytes());
    CHECK(s.said("cell name of 70000 bytes cut to 65530"));
    const std::string cut = long_name.substr(0, 65530);
    REQUIRE(s.has_cell(cut));
    const auto* top = std::get_if<std::string>(&result.value.data());
    REQUIRE(top != nullptr);
    CHECK(*top == cut);
  }
  SECTION("regression: a text string over 65530 bytes is cut") {
    Oas f;
    f.cell("c").add(text(68, 20, 5, 5, long_name));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.said("text string of 70000 bytes cut to 65530"));
  }
  SECTION("regression: an S_GDS_PROPERTY value over 65530 bytes is cut with a message") {
    Oas f;
    f.cell("A").add(rect(68, 20, 0, 0, 10, 10));
    f.cell("top").add(place("A", 0, 0)).property(gds_property(5, long_name));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.said("S_GDS_PROPERTY value of 70000 bytes cut to 65530"));
  }
}

TEST_CASE("input limits: numbers above 32767", "[cap:oasis-read][req:Input limits]") {
  SECTION("regression: a layer above 32767 is read as 32767 with a message") {
    Oas f;
    f.cell("c").add(rect(40000, 7, 0, 0, 10, 10));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.said("element on layer 40000, datatype 7: a number above 32767 is read as 32767"));
    CHECK(s.said("layer=32767 type=7"));
  }
  SECTION("regression: a datatype above 32767 is reported once per element") {
    Oas f;
    f.cell("c").add(with(rect(68, 70000, 0, 0, 10, 10), grid(2, 3, {20, 0})));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.count("a number above 32767 is read as 32767") == 1);
  }
  SECTION("regression: an S_GDS_PROPERTY attribute above 32767 is read as 32767 with a message") {
    Oas f;
    f.cell("A").add(rect(68, 20, 0, 0, 10, 10));
    f.cell("top").add(place("A", 0, 0)).property(gds_property(40000, "x"));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.said("S_GDS_PROPERTY attribute outside -32768 to 32767 is read as 32767"));
  }
  // Q-OASIS-45 (approved 2026-11-20 as proposed): the nearest INT2 value.
  // Regression: an attribute below -32768 was read as 32767, flipping its sign.
  SECTION("an S_GDS_PROPERTY attribute below -32768 is read as -32768 with a message") {
    Oas f;
    f.cell("A").add(rect(68, 20, 0, 0, 10, 10));
    oasis::Property p = gds_property(0, "x");
    p.values[0].kind = oasis::PropertyValue::Kind::signed_integer;
    p.values[0].signed_value = -40000;
    f.cell("top").add(place("A", 0, 0)).property(p);
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.said("S_GDS_PROPERTY attribute outside -32768 to 32767 is read as -32768"));
    CHECK_FALSE(s.said("is read as 32767"));
  }
  SECTION("32767 itself is read without a message") {
    Oas f;
    f.cell("c").add(rect(32767, 32767, 0, 0, 10, 10));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK_FALSE(s.said("is read as 32767"));
  }
}

TEST_CASE("input limits: polygons over 8191 points", "[cap:oasis-read][req:Input limits]") {
  SECTION("regression: a Manhattan polygon over 8191 points is fractured into the same paint") {
    for (const std::int64_t h : {20, 100000}) {  // wide and tall bounding boxes
      Oas f;
      f.cell("c").add(comb(2100, h));  // 8403 points
      Oas g;
      g.cell("c");
      comb_rects(g, 2100, h);
      OasSession s;
      (void)s.read_oasis(f.bytes());
      OasSession r;
      (void)r.read_oasis(g.bytes());
      INFO("tooth height " << h);
      CHECK(s.said("polygon with 8403 points fractured into"));
      CHECK(s.painted("c", "metal1") == r.painted("c", "metal1"));
    }
  }
  SECTION("a polygon of 8191 points is one BOUNDARY") {
    Oas f;
    f.cell("c").add(comb(2047, 20));  // 8191 points
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK_FALSE(s.said("fractured"));
    CHECK(s.area("c", "metal1") == 2047.0 * 20 * 10 + 2047.0 * 10 * 20);
  }
  SECTION("each instance of a repeated polygon is fractured the same way") {
    Oas f;
    f.cell("c").add(with(comb(2100, 20), grid(2, 2, {50000, 0})));
    Oas g;
    g.cell("c");
    comb_rects(g, 2100, 20);
    OasSession s;
    (void)s.read_oasis(f.bytes());
    OasSession r;
    (void)r.read_oasis(g.bytes());
    CHECK(s.count("fractured into") == 1);
    CHECK(s.area("c", "metal1") == 2 * r.area("c", "metal1"));
  }
  SECTION("regression: a polygon whose vertices a cut cannot separate is reported and dropped") {
    std::vector<oasis::Point64> rest;
    for (int i = 0; i < 2100; ++i) {
      rest.push_back({10, 0});
      rest.push_back({10, 10});
      rest.push_back({0, 10});
      rest.push_back({0, 0});
    }
    Oas f;
    f.cell("c").add(polygon(68, 20, 0, 0, rest)).add(rect(68, 20, 0, 100, 10, 10));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.said("cannot be fractured"));
    CHECK(s.painted("c", "metal1") == std::vector<db::Rect>{{0, 100, 10, 110}});
  }
}
