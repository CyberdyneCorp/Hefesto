// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef-tech-section: the table operations (scale, fill_defaults, the
// magic-to-LEF map, add and release) on tables read from base.tech.
//
// Owner: LEFTECH (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "fixture.hpp"

namespace lef = hefesto::lef;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
using lef::LayerClass;
using lef::test::build;

#define SCALE_REQ "[cap:lef-tech-section][req:Scaling the LEF table]"
#define FILL_REQ "[cap:lef-tech-section][req:Filling defaults after grid scaling]"
#define USE_REQ "[cap:lef-tech-section][req:Use of entry classes by readers and writers]"
#define LIFE_REQ "[cap:lef-tech-section][req:LEF table lifecycle]"

TEST_CASE("route values truncate toward zero", SCALE_REQ) {
  auto b = build("routing m5 metal5 M5");
  lef::LayerRecord& r = *b.table.find_mutable("metal5");
  r.width = 5;
  r.spacing = -3;
  r.pitch = 7;
  b.table.scale(1, 2);
  CHECK(r.width == 2);
  CHECK(r.spacing == -1);  // C division, not floor
  CHECK(r.pitch == 3);
  b.table.scale(3, 2);  // trunc(trunc(v * 3) / 2)
  CHECK(r.width == 3);
  CHECK(r.spacing == -1);
  CHECK(r.pitch == 4);
}

TEST_CASE("negative contact coordinates floor", SCALE_REQ) {
  auto b = build("contact pc cont1");
  lef::LayerRecord& r = *b.table.find_mutable("cont1");
  r.contact_area = db::Rect{-3, -3, 3, 3};
  b.table.scale(1, 2);
  CHECK(r.contact_area == db::Rect{-2, -2, 1, 1});
}

TEST_CASE("regression: a cut-flavor area is GeoNullRect and scales to itself", SCALE_REQ) {
  // It was (0, 0, -1, -1), which scaled by 2 became (0, 0, -2, -2); Magic's
  // GeoNullRect is (0, 0, 0, 0) (utils/geometry.c), so it stays put.
  auto b = build("cut m2c via1");
  CHECK(b.table.find("via1")->contact_area == db::Rect{0, 0, 0, 0});
  b.table.scale(2, 1);
  CHECK(b.table.find("via1")->contact_area == db::Rect{0, 0, 0, 0});
  b.table.scale(1, 3);
  CHECK(b.table.find("via1")->contact_area == db::Rect{0, 0, 0, 0});
}

TEST_CASE("a record shared by aliases is scaled once", SCALE_REQ) {
  auto b = build("routing m5 metal5 M5 MET5 m5");
  b.table.scale(2, 1);
  CHECK(b.table.find("MET5")->width == 6);
  CHECK(b.table.find("MET5")->spacing == 8);
  CHECK(b.table.find("m5") == b.table.find("metal5"));
}

TEST_CASE("other classes and unset values are not scaled", SCALE_REQ) {
  auto b = build("masterslice ndiff active\nrouting m1 metal1\ncontact pc cont1", false);
  const lef::LayerRecord before = *b.table.find("active");
  b.table.scale(2, 1);
  CHECK(*b.table.find("active") == before);
  CHECK_FALSE(b.table.find("metal1")->width.has_value());
  CHECK_FALSE(b.table.find("cont1")->contact_area.has_value());
  CHECK(b.table.find("metal1")->pitch == 0);
}

TEST_CASE("regression: a huge route value saturates instead of overflowing", SCALE_REQ) {
  // LefTechScale multiplies in C int (undefined on overflow; fixed in both modes).
  auto b = build("routing m5 metal5");
  b.table.find_mutable("metal5")->width = 2'000'000'000;
  b.table.scale(4, 1);
  CHECK(b.table.find("metal5")->width == std::numeric_limits<db::Coord>::max());
  b.table.scale(0, 1);  // a non-positive factor is ignored
  CHECK(b.table.find("metal5")->width == std::numeric_limits<db::Coord>::max());
}

TEST_CASE("the ON fill replaces only exact sentinels, the spacing into the width", FILL_REQ) {
  auto b = build("routing m1 metal1\nrouting m5 metal5\ncontact pc cont1\ncut m2c via1", true);
  const tech::TileType m1 = b.type("m1");
  const db::Coord width = hefesto::drc::default_width(*b.rules, *b.tech, m1);
  const db::Coord spacing = hefesto::drc::default_spacing(*b.rules, *b.tech, m1, m1);
  const db::Coord pc = hefesto::drc::default_width(*b.rules, *b.tech, b.type("pc"));
  REQUIRE(width == 3);
  REQUIRE(spacing == 4);
  REQUIRE(pc == 2);
  b.table.fill_defaults(*b.rules, *b.tech, hefesto::compat::CompatSettings{true});
  CHECK(b.table.find("metal1")->width == spacing);  // Q-LEFDEF-2: overwritten
  CHECK(b.table.find("metal1")->spacing == -1);
  CHECK(b.table.find("metal5")->width == 3);
  CHECK(b.table.find("metal5")->spacing == 4);
  CHECK(b.table.find("cont1")->contact_area == db::Rect{-pc, -pc, pc, pc});
  CHECK(b.table.find("via1")->contact_area == db::Rect{0, 0, 0, 0});
}

TEST_CASE("the OFF fill gives every unset value its own default", FILL_REQ) {
  auto b = build("routing m1 metal1\ncontact pc cont1\ncut m2c via1", false);
  b.table.fill_defaults(*b.rules, *b.tech, hefesto::compat::CompatSettings{false});
  CHECK(b.table.find("metal1")->width == 3);
  CHECK(b.table.find("metal1")->spacing == 4);
  CHECK(b.table.find("cont1")->contact_area == db::Rect{-2, -2, 2, 2});
  CHECK(b.table.find("via1")->contact_area == db::Rect{0, 0, 0, 0});
}

TEST_CASE("regression: an obstruction-only record is filled with 0", FILL_REQ) {
  // Magic fills it from DRCGetDefaultLayerWidth(-1) and
  // DRCGetDefaultLayerSpacing(-1, -1), an out-of-bounds read (fixed in both
  // modes): the record has no normal type, so its defaults are 0.
  const bool compat = GENERATE(true, false);
  auto b = build("obstruction m1 OBS_M1\nobstruction pc OBS_PC", compat);
  b.table.fill_defaults(*b.rules, *b.tech, hefesto::compat::CompatSettings{compat});
  CHECK(b.table.find("OBS_M1")->width == 0);
  CHECK(b.table.find("OBS_M1")->spacing == (compat ? std::optional<db::Coord>{-1} : std::optional<db::Coord>{0}));
  CHECK(b.table.find("OBS_PC")->contact_area == db::Rect{0, 0, 0, 0});
}

TEST_CASE("an obstruction type maps to the route name", USE_REQ) {
  const auto b = build("route m1 met1\nobstruction obsm1 met1");
  const lef::LayerRecord* r = b.table.magic_to_lef(b.type("obsm1"));
  REQUIRE(r != nullptr);
  CHECK(r->canonical_name == "met1");
  CHECK(b.table.magic_to_lef(b.type("m1")) == r);
}

TEST_CASE("the magic-to-LEF map picks the class by contact type", USE_REQ) {
  const auto b = build("masterslice ndiff active\nrouting m2c via1\ncut m2c VIA12\nbound m1 BOUND\noverlap m5 OVL");
  CHECK(b.table.magic_to_lef(b.type("ndiff"))->canonical_name == "active");
  CHECK(b.table.magic_to_lef(b.type("m2c"))->canonical_name == "VIA12");  // a contact needs a cut record
  CHECK(b.table.magic_to_lef(b.type("m1")) == nullptr);                   // bound is never mapped
  CHECK(b.table.magic_to_lef(b.type("m5")) == nullptr);                   // nor overlap
  CHECK(b.table.magic_to_lef(tech::kSpace) == nullptr);
}

TEST_CASE("a via definition can capture a cut type, in enumeration order", USE_REQ) {
  // Q-LEFDEF-92 (compat): the first cut record in Magic's hash order wins.
  auto b = build("cut m2c via1");
  lef::LayerRecord via;
  via.layer_class = LayerClass::cut;
  via.type = b.type("m2c");
  b.table.add("M1M2_PR", via);
  std::string first;
  b.table.enumerate([&first](std::string_view name, const lef::LayerRecord&) {
    if (first.empty()) {
      first = std::string{name};
    }
  });
  CHECK(b.table.magic_to_lef(b.type("m2c"))->canonical_name == first);
}

TEST_CASE("an ignore record is found and carries no type", USE_REQ) {
  const auto b = build("ignore NWELL");
  const lef::LayerRecord* r = b.table.find("NWELL");
  REQUIRE(r != nullptr);
  CHECK(r->layer_class == LayerClass::ignore);
  CHECK(r->type == tech::kSpace);
  CHECK(r->obstruction_types.empty());
}

TEST_CASE("a released name stays in the table with no record", LIFE_REQ) {
  auto b = build("cut m2c via1");
  lef::LayerRecord generated;
  generated.layer_class = LayerClass::cut;
  b.table.add("via1_gen", generated);
  CHECK(b.table.find("via1_gen")->canonical_name == "via1_gen");
  b.table.release("via1_gen");
  CHECK(b.table.find("via1_gen") == nullptr);
  int visited = 0;
  b.table.enumerate([&visited](std::string_view, const lef::LayerRecord&) { ++visited; });
  CHECK(visited == 1);
  CHECK(b.table.size() == 2);
  b.table.scale(2, 1);  // a released record is not scaled (nor crashes)
}

TEST_CASE("add rebinds a name to a new record", LIFE_REQ) {
  auto b = build("routing m1 metal1 M1");
  lef::LayerRecord replacement;
  replacement.layer_class = LayerClass::route;
  replacement.canonical_name = "MX";
  b.table.add("M1", replacement);
  CHECK(b.table.find("M1")->canonical_name == "MX");
  CHECK(b.table.find("metal1")->canonical_name == "metal1");
  CHECK(b.table.find_mutable("nosuch") == nullptr);
}
