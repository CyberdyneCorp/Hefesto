// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shape repetition detection and CBLOCK compression of oasis::write
// (tasks.md 12.3; Q-OASIS-64, Q-OASIS-67). Owner: OASW.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <random>

#include "hefesto/oasis/write/repetitions.hpp"
#include "oasis_write_fixture.hpp"

using namespace oasis_write_test;  // NOLINT(google-build-using-namespace)

namespace {

oasis::Repetition grid(std::uint8_t type, std::uint64_t columns, std::int64_t dx, std::uint64_t rows,
                       std::int64_t dy) {
  oasis::Repetition r;
  r.type = type;
  r.columns = columns;
  r.rows = rows;
  r.column_step = columns > 1 ? oasis::Point64{dx, 0} : oasis::Point64{};
  r.row_step = rows > 1 ? oasis::Point64{0, dy} : oasis::Point64{};
  return r;
}

std::multiset<std::pair<std::int64_t, std::int64_t>> expanded(const std::vector<oasis::RepetitionGroup>& groups) {
  std::multiset<std::pair<std::int64_t, std::int64_t>> out;
  for (const oasis::RepetitionGroup& g : groups) {
    const oasis::Repetition r = g.repetition.value_or(oasis::Repetition{});
    for (const oasis::Point64& d : oasis::expand(r)) {
      out.emplace(g.origin.x + d.x, g.origin.y + d.y);
    }
  }
  return out;
}

/// Six 1x1 tiles 2 apart along x and 3 along y (one tile each: no merge).
db::CellDef& contact_array(OasFixture& f) {
  db::CellDef& top = f.cell("top");
  for (const int y : {0, 3}) {
    for (const int x : {0, 2, 4}) {
      f.paint(top, f.m1, db::Rect{x, y, x + 1, y + 1});
    }
  }
  return top;
}

}  // namespace

TEST_CASE("a contact array is one type 1 group", "[cap:oasis-write][req:Shape repetition detection]") {
  const std::vector<oasis::Point64> origins{{0, 0}, {20, 0}, {40, 0}, {0, 30}, {20, 30}, {40, 30}};
  const std::vector<oasis::RepetitionGroup> groups = oasis::detect_repetitions(origins);
  REQUIRE(groups.size() == 1);
  CHECK(groups[0] == oasis::RepetitionGroup{{0, 0}, grid(1, 3, 20, 2, 30)});
}

TEST_CASE("irregular rows stay separate", "[cap:oasis-write][req:Shape repetition detection]") {
  const std::vector<oasis::Point64> origins{{50, 0}, {0, 0}, {20, 0}};
  const std::vector<oasis::RepetitionGroup> groups = oasis::detect_repetitions(origins);
  REQUIRE(groups.size() == 2);
  CHECK(groups[0] == oasis::RepetitionGroup{{0, 0}, grid(2, 2, 20, 1, 0)});
  CHECK(groups[1] == oasis::RepetitionGroup{{50, 0}, std::nullopt});
}

TEST_CASE("a column of equal runs is a type 3 group", "[cap:oasis-write][req:Shape repetition detection]") {
  const std::vector<oasis::Point64> origins{{5, 0}, {5, 7}, {5, 14}, {5, 30}};
  const std::vector<oasis::RepetitionGroup> groups = oasis::detect_repetitions(origins);
  REQUIRE(groups.size() == 2);
  CHECK(groups[0] == oasis::RepetitionGroup{{5, 0}, grid(3, 1, 0, 3, 7)});
  CHECK(groups[1] == oasis::RepetitionGroup{{5, 30}, std::nullopt});
}

TEST_CASE("the groups expand to exactly the input origins, duplicates included",
          "[cap:oasis-write][req:Shape repetition detection]") {
  std::mt19937 random(20261120);
  for (int round = 0; round < 50; ++round) {
    std::vector<oasis::Point64> origins;
    const int count = 1 + static_cast<int>(random() % 60);
    for (int i = 0; i < count; ++i) {
      origins.push_back({static_cast<std::int64_t>(random() % 6) * 10, static_cast<std::int64_t>(random() % 5) * 7});
    }
    const std::vector<oasis::RepetitionGroup> groups = oasis::detect_repetitions(origins);
    std::multiset<std::pair<std::int64_t, std::int64_t>> wanted;
    for (const oasis::Point64& p : origins) {
      wanted.emplace(p.x, p.y);
    }
    CHECK(expanded(groups) == wanted);
    CHECK(oasis::detect_repetitions(origins) == groups);  // deterministic
  }
}

TEST_CASE("a written contact array is one RECTANGLE with a repetition",
          "[cap:oasis-write][req:Shape repetition detection]") {
  OasFixture f;
  f.settings.compress = 0;
  db::CellDef& top = contact_array(f);
  Decoded d = decode_oasis(f.write_oas(top));
  std::vector<oasis::Element> e = d.elements("top");
  REQUIRE(e.size() == 1);
  CHECK(e[0].kind == oasis::ElementKind::rectangle);
  CHECK(e[0].size == oasis::Point64{100, 100});
  CHECK(e[0].repetition == grid(1, 3, 200, 2, 300));

  f.settings.repetitions = false;  // the gds-write element order
  d = decode_oasis(f.write_oas(top));
  e = d.elements("top");
  CHECK(e.size() == 6);
  CHECK(std::none_of(e.begin(), e.end(), [](const oasis::Element& x) { return x.repetition.has_value(); }));
}

TEST_CASE("groups of different keys are written by origin", "[cap:oasis-write][req:Shape repetition detection]") {
  OasFixture f;
  f.settings.compress = 0;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 5, 2, 6});  // key A at (0, 500)
  f.paint(top, f.m1, db::Rect{0, 0, 1, 1});  // key B at (0, 0) and (300, 0)
  f.paint(top, f.m1, db::Rect{3, 0, 4, 1});
  const std::vector<oasis::Element> e = decode_oasis(f.write_oas(top)).elements("top");
  REQUIRE(e.size() == 2);
  CHECK(e[0].position == oasis::Point64{0, 0});
  CHECK(e[0].repetition == grid(2, 2, 300, 1, 0));
  CHECK(e[1].position == oasis::Point64{0, 500});
  CHECK_FALSE(e[1].repetition.has_value());
}

TEST_CASE("label rectangles are not candidates", "[cap:oasis-write][req:Shape repetition detection]") {
  OasFixture f;
  f.settings.compress = 0;
  mask::OutputLayer lbl;
  lbl.name = "M1LBL";
  lbl.gds = mask::GdsLayer{49, 10};
  f.style.layers.push_back(lbl);
  f.style.label_layer[f.m1] = 1;
  db::CellDef& top = f.cell("top");
  top.labels().push_back(plain_label("A", f.m1, db::Rect{0, 0, 1, 1}));
  top.labels().push_back(plain_label("B", f.m1, db::Rect{2, 0, 3, 1}));
  const std::vector<oasis::Element> e = decode_oasis(f.write_oas(top)).elements("top");
  REQUIRE(e.size() == 4);
  CHECK(e[0].kind == oasis::ElementKind::text);
  CHECK(e[1].kind == oasis::ElementKind::rectangle);
  CHECK_FALSE(e[1].repetition.has_value());
  CHECK(e[2].kind == oasis::ElementKind::text);
  CHECK(e[3].kind == oasis::ElementKind::rectangle);
}

TEST_CASE("a tiny cell stays uncompressed", "[cap:oasis-write][req:CBLOCK compression]") {
  OasFixture f;
  f.settings.compress = 6;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
  const Decoded d = decode_oasis(f.write_oas(top));
  const auto rect = std::find_if(d.records.begin(), d.records.end(),
                                 [](const oasis::Record& r) { return r.id == oasis::RecordId::rectangle; });
  REQUIRE(rect != d.records.end());
  CHECK_FALSE(rect->in_cblock);
}

TEST_CASE("compressed and uncompressed files decode to the same records", "[cap:oasis-write][req:CBLOCK compression]") {
  OasFixture f;
  db::CellDef& top = f.cell("top");
  for (int i = 0; i < 300; ++i) {
    f.paint(top, f.m1, db::Rect{i * 5, i % 7, i * 5 + 3, i % 7 + 2 + i % 3});
  }
  f.settings.compress = 0;
  const Decoded plain = decode_oasis(f.write_oas(top));
  f.settings.compress = 9;
  const std::string packed_bytes = f.write_oas(top);
  const Decoded packed = decode_oasis(packed_bytes);
  CHECK(packed.any_in_cblock());
  CHECK(packed.cells() == plain.cells());
  CHECK(packed.elements("top") == plain.elements("top"));
  CHECK(packed.names.cellnames == plain.names.cellnames);
  CHECK(packed.names.propstrings == plain.names.propstrings);
  CHECK(packed.names.layernames == plain.names.layernames);
  // The pinned zlib gives the same bytes on every write (and every host).
  CHECK(f.write_oas(top) == packed_bytes);
}
