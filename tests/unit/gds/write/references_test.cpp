// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// gds-write "Cell reference elements" and "Array flattening on output".
// Owner: GDSW.

#include <catch2/catch_test_macros.hpp>

#include <iomanip>
#include <sstream>

#include "write_fixture.hpp"

namespace gds_write_test {
namespace {

std::string hex(const std::string& bytes) {
  std::ostringstream out;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    out << (i > 0 ? " " : "") << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
        << static_cast<int>(static_cast<unsigned char>(bytes[i]));
  }
  return out.str();
}

// The reference elements of `top` (SREF/AREF to ENDEL), each as records.
std::vector<std::vector<Rec>> references(const std::vector<Rec>& recs) {
  std::vector<std::vector<Rec>> out;
  const std::vector<Rec> top = structure(recs, "top");
  for (std::size_t i = 0; i < top.size(); ++i) {
    if (top[i].record() != gds::RecordType::sref && top[i].record() != gds::RecordType::aref) {
      continue;
    }
    std::vector<Rec> element;
    for (std::size_t k = i; k < top.size(); ++k) {
      element.push_back(top[k]);
      if (top[k].record() == gds::RecordType::endel) {
        break;
      }
    }
    out.push_back(element);
  }
  return out;
}

const Rec* find(const std::vector<Rec>& element, gds::RecordType type) {
  for (const Rec& r : element) {
    if (r.record() == type) {
      return &r;
    }
  }
  return nullptr;
}

std::vector<std::vector<Rec>> write_use(Fixture& f, db::Transform t, db::ArrayInfo array, const std::string& id) {
  db::CellDef& sub = f.cell("sub");
  f.paint(sub, f.m1, db::Rect{0, 0, 4, 4});
  db::CellDef& top = f.cell("top");
  f.place(top, sub, t, array, id);
  const std::vector<Rec> recs = decode(f.write(top));
  REQUIRE(f.last.ok());
  return references(recs);
}

TEST_CASE("the bytes of a rotated SREF", "[cap:gds-write][req:Cell reference elements]") {
  Fixture f;
  db::CellDef& sub = f.cell("sub");
  db::CellDef& top = f.cell("top");
  f.place(top, sub, db::Transform{0, 1, 10, -1, 0, 20}, {}, "sub_0");
  const std::string bytes = f.write(top);
  REQUIRE(f.last.ok());
  const std::string expected =
      "00 04 0A 00 00 08 12 06 73 75 62 00 00 06 1A 01 00 00 00 0C 1C 05 43 10 E0 00 00 00 00 00 "
      "00 0C 10 03 00 00 03 E8 00 00 07 D0 00 06 2B 02 00 3D 00 0A 2C 06 73 75 62 5F 30 00 00 04 11 00";
  CHECK(hex(bytes).find(expected) != std::string::npos);
}

TEST_CASE("STRANS and ANGLE of the eight orientations", "[cap:gds-write][req:Cell reference elements]") {
  struct Case {
    db::Transform t;
    std::int16_t strans;
    std::optional<double> angle;
  };
  const std::vector<Case> cases{
      {db::Transform{1, 0, 0, 0, 1, 0}, 0, std::nullopt},     {db::Transform{0, -1, 0, 1, 0, 0}, 0, 90.0},
      {db::Transform{-1, 0, 0, 0, -1, 0}, 0, 180.0},          {db::Transform{0, 1, 0, -1, 0, 0}, 0, 270.0},
      {db::Transform{1, 0, 0, 0, -1, 0}, -32768, std::nullopt}, {db::Transform{-1, 0, 0, 0, 1, 0}, -32768, 180.0},
      {db::Transform{0, 1, 0, 1, 0, 0}, -32768, 90.0},        {db::Transform{0, -1, 0, -1, 0, 0}, -32768, 270.0},
  };
  for (const Case& c : cases) {
    Fixture f;
    const auto refs = write_use(f, c.t, {}, "u");
    REQUIRE(refs.size() == 1);
    const Rec* strans = find(refs[0], gds::RecordType::strans);
    REQUIRE(strans != nullptr);
    CHECK(strans->i2() == std::vector<std::int16_t>{c.strans});
    const Rec* angle = find(refs[0], gds::RecordType::angle);
    REQUIRE((angle != nullptr) == c.angle.has_value());
    if (angle != nullptr) {
      CHECK(gds::r8_at(angle->data, 0) == c.angle);
    }
  }
}

TEST_CASE("AREF geometry and the array-limits property", "[cap:gds-write][req:Cell reference elements]") {
  Fixture f;
  auto refs = write_use(f, {}, db::ArrayInfo{0, 2, 0, 1, 10, 20}, "a1");
  REQUIRE(refs.size() == 1);
  CHECK(refs[0].front().record() == gds::RecordType::aref);
  CHECK(find(refs[0], gds::RecordType::colrow)->i2() == std::vector<std::int16_t>{3, 2});
  CHECK(find(refs[0], gds::RecordType::xy)->i4() == std::vector<std::int32_t>{0, 0, 3000, 0, 0, 4000});
  CHECK(use_names(refs[0]) == std::vector<std::string>{"a1"});
  std::size_t propattrs = 0;
  for (const Rec& r : refs[0]) {
    propattrs += r.record() == gds::RecordType::propattr ? 1U : 0U;
  }
  CHECK(propattrs == 1);

  Fixture g;
  refs = write_use(g, {}, db::ArrayInfo{1, 3, 0, 1, 10, 20}, "a1");
  REQUIRE(refs.size() == 1);
  const std::vector<Rec>& e = refs[0];
  REQUIRE(e.size() >= 4);
  const std::size_t n = e.size();
  CHECK(e[n - 3].record() == gds::RecordType::propattr);
  CHECK(e[n - 3].i2() == std::vector<std::int16_t>{99});
  CHECK(e[n - 2].text() == "1_3_0_1");
}

TEST_CASE("a mirrored reference has no ANGLE", "[cap:gds-write][req:Cell reference elements]") {
  Fixture f;
  const auto refs = write_use(f, db::Transform{1, 0, 0, 0, -1, 0}, {}, "m");
  REQUIRE(refs.size() == 1);
  const std::string strans = hex(std::string{reinterpret_cast<const char*>(find(refs[0], gds::RecordType::strans)->data.data()), 2});
  CHECK(strans == "80 00");
  CHECK(find(refs[0], gds::RecordType::angle) == nullptr);
}

TEST_CASE("an array dimension outside I2 fails the write", "[cap:gds-write][req:Cell reference elements]") {
  Fixture f;
  db::CellDef& sub = f.cell("sub");
  db::CellDef& top = f.cell("top");
  f.place(top, sub, {}, db::ArrayInfo{0, 40000, 0, 0, 1, 1}, "wide");
  (void)f.write(top);
  CHECK_FALSE(f.last.ok());
  CHECK(f.has_code("GDS-WRITE-004"));
}

TEST_CASE("a flattened array writes one SREF per element", "[cap:gds-write][req:Array flattening on output]") {
  Fixture f;
  f.options.settings.arrays = true;
  auto refs = write_use(f, {}, db::ArrayInfo{0, 1, 0, 0, 10, 0}, "a");
  REQUIRE(refs.size() == 2);
  CHECK(find(refs[0], gds::RecordType::xy)->i4() == std::vector<std::int32_t>{0, 0});
  CHECK(find(refs[1], gds::RecordType::xy)->i4() == std::vector<std::int32_t>{1000, 0});
  for (const auto& e : refs) {
    CHECK(e.front().record() == gds::RecordType::sref);
    CHECK(find(e, gds::RecordType::propattr) == nullptr);
  }

  // x outer, y inner, with the use's rotation applied to the pitches.
  Fixture g;
  g.options.settings.arrays = true;
  refs = write_use(g, db::Transform{0, -1, 0, 1, 0, 0}, db::ArrayInfo{0, 1, 0, 1, 10, 20}, "r");
  REQUIRE(refs.size() == 4);
  const std::vector<std::vector<std::int32_t>> expected{{0, 0}, {-2000, 0}, {0, 1000}, {-2000, 1000}};
  for (std::size_t i = 0; i < refs.size(); ++i) {
    CHECK(find(refs[i], gds::RecordType::xy)->i4() == expected[i]);
    CHECK(find(refs[i], gds::RecordType::angle) != nullptr);
  }

  // A plain use writes no PROPATTR in this mode either.
  Fixture h;
  h.options.settings.arrays = true;
  refs = write_use(h, {}, {}, "single");
  REQUIRE(refs.size() == 1);
  CHECK(find(refs[0], gds::RecordType::propattr) == nullptr);
}

}  // namespace
}  // namespace gds_write_test
