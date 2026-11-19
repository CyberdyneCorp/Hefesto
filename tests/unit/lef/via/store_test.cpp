// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shared via table: LEF VIA/VIARULE and DEF VIAS definitions,
// redefinition and via rectangles (capabilities lef-read, def-read;
// openspec/changes/m6-lef-def-oasis tasks.md 4.1). The rectangles are
// hand-built in half internal units, as the LEF and DEF readers will pass
// them.
//
// Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>
#include <vector>

#include "hefesto/lef/via/store.hpp"
#include "via_fixture.hpp"

namespace hefesto::lef {
namespace {

using test::Context;

/// The VIA of the lef-read example: met1, via, met2 RECTs at s = 0.005.
void read_m1m2(ViaDefinition& via, Context& c) {
  add_via_rect(via, c.type("metal1"), db::Rect{-64, -52, 64, 52}, c.via());
  add_via_rect(via, c.type("m2contact"), db::Rect{-30, -30, 30, 30}, c.via());
  add_via_rect(via, c.type("metal2"), db::Rect{-60, -60, 60, 60}, c.via());
}

TEST_CASE("a contact rectangle becomes the via's primary record",
          "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  Context c{test::via_tech()};
  ViaTable vias;
  const LayerTable layers;
  ViaDefinition* via = begin_via(vias, layers, "M1M2_PR", ViaSection::lef_via, c.via());
  REQUIRE(via != nullptr);
  CHECK(via->layer_class == LayerClass::cut);
  CHECK_FALSE(via->type.has_value());
  CHECK(via->area == db::Rect{});
  CHECK(via->canonical_name == "M1M2_PR");
  read_m1m2(*via, c);
  CHECK(via->type == c.type("m2contact"));
  CHECK(via->area == db::Rect{-30, -30, 30, 30});
  const std::vector<ViaShape> extras{{c.type("metal2"), db::Rect{-60, -60, 60, 60}},
                                     {c.type("metal1"), db::Rect{-64, -52, 64, 52}}};
  CHECK(via->shapes == extras);
  CHECK(c.texts().empty());
}

TEST_CASE("a VIARULE after a VIA of the same name is skipped",
          "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  Context c{test::via_tech()};
  ViaTable vias;
  const LayerTable layers;
  read_m1m2(*begin_via(vias, layers, "V12", ViaSection::lef_via, c.via()), c);
  const ViaDefinition before = *vias.find("V12")->value;
  CHECK(begin_via(vias, layers, "V12", ViaSection::lef_viarule, c.via()) == nullptr);
  CHECK(*vias.find("V12")->value == before);
  CHECK(c.texts().empty());
  // A new VIARULE name is read like a VIA.
  CHECK(begin_via(vias, layers, "RULE1", ViaSection::lef_viarule, c.via()) != nullptr);
}

TEST_CASE("a rectangle on a layer that did not resolve adds no geometry",
          "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  Context c{test::via_tech()};
  ViaTable vias;
  ViaDefinition* via = begin_via(vias, LayerTable{}, "V", ViaSection::lef_via, c.via());
  add_via_rect(*via, std::nullopt, db::Rect{0, 0, 2, 2}, c.via());
  CHECK(via->area == db::Rect{});
  CHECK(via->shapes.empty());
  CHECK_FALSE(via->type.has_value());
}

TEST_CASE("a later contact rectangle is not swapped in over a contact primary",
          "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  Context c{test::via_tech()};
  ViaTable vias;
  ViaDefinition* via = begin_via(vias, LayerTable{}, "V", ViaSection::lef_via, c.via());
  add_via_rect(*via, c.type("m2contact"), db::Rect{-2, -2, 2, 2}, c.via());
  add_via_rect(*via, c.type("m3contact"), db::Rect{-4, -4, 4, 4}, c.via());
  CHECK(via->type == c.type("m2contact"));
  CHECK(via->shapes == std::vector<ViaShape>{{c.type("m3contact"), db::Rect{-4, -4, 4, 4}}});
}

TEST_CASE("reading the same technology LEF twice redefines the via with the same geometry",
          "[cap:lef-read][req:Via redefinition]") {
  Context c{test::via_tech()};
  ViaTable vias;
  const LayerTable layers;
  read_m1m2(*begin_via(vias, layers, "V12", ViaSection::lef_via, c.via()), c);
  const ViaDefinition first = *vias.find("V12")->value;
  ViaDefinition* again = begin_via(vias, layers, "V12", ViaSection::lef_via, c.via());
  REQUIRE(again != nullptr);
  CHECK(again->shapes.empty());
  CHECK_FALSE(again->type.has_value());
  CHECK(again->area == db::Rect{});
  CHECK(c.texts() == std::vector<std::string>{"Cut type \"V12\" redefined."});
  read_m1m2(*again, c);
  CHECK(*vias.find("V12")->value == first);
  CHECK(vias.size() == 1);
}

TEST_CASE("redefinition keeps the record's class", "[cap:lef-read][req:Via redefinition]") {
  Context c{test::via_tech()};
  ViaTable vias;
  ViaDefinition* via = begin_via(vias, LayerTable{}, "V", ViaSection::lef_via, c.via());
  via->layer_class = LayerClass::route;
  via->generated = true;
  ViaDefinition* again = begin_via(vias, LayerTable{}, "V", ViaSection::lef_via, c.via());
  CHECK(again->layer_class == LayerClass::route);
  CHECK(again->canonical_name == "V");
  CHECK_FALSE(again->generated);
}

TEST_CASE("a DEF via overrides a LEF via of the same name", "[cap:def-read][req:Via redefinition]") {
  Context c{test::via_tech()};
  ViaTable vias;
  const LayerTable layers;
  read_m1m2(*begin_via(vias, layers, "V12", ViaSection::lef_via, c.via()), c);
  ViaDefinition* def_via = begin_via(vias, layers, "V12", ViaSection::def_via, c.via());
  REQUIRE(def_via != nullptr);
  CHECK(c.texts() == std::vector<std::string>{"Composite via \"V12\" redefined."});
  CHECK(def_via->shapes.empty());
  add_via_rect(*def_via, c.type("metal1"), db::Rect{-20, -20, 20, 20}, c.via());
  CHECK(vias.find("V12")->value->type == c.type("metal1"));
  CHECK(vias.find("V12")->value->area == db::Rect{-20, -20, 20, 20});
}

TEST_CASE("a three-rectangle DEF via has the contact as its primary type",
          "[cap:def-read][req:Rectangle-based DEF vias]") {
  // oscale = 1: doubled units are twice the DEF values.
  Context c{test::via_tech()};
  ViaTable vias;
  ViaDefinition* via = begin_via(vias, LayerTable{}, "V12", ViaSection::def_via, c.via());
  add_via_rect(*via, c.type("metal1"), db::Rect{-200, -200, 200, 200}, c.via());
  add_via_rect(*via, c.type("m2contact"), db::Rect{-100, -100, 100, 100}, c.via());
  add_via_rect(*via, c.type("metal2"), db::Rect{-240, -160, 240, 160}, c.via());
  CHECK(via->type == c.type("m2contact"));
  CHECK(via->area == db::Rect{-100, -100, 100, 100});
  const std::vector<ViaShape> extras{{c.type("metal2"), db::Rect{-240, -160, 240, 160}},
                                     {c.type("metal1"), db::Rect{-200, -200, 200, 200}}};
  CHECK(via->shapes == extras);
}

// Regression: every RECT was inserted at the head of the extra list, so a
// via with 10^5 RECTs (a few MB of hostile LEF or DEF) cost the square of
// that. The builder appends and orders once; the list matches
// add_via_rect's, including a contact promoted midway and rectangles a
// later commit adds.
TEST_CASE("the via rectangle builder gives add_via_rect's list at linear cost",
          "[cap:def-read][req:Rectangle-based DEF vias]") {
  Context c{test::via_tech()};
  const auto rect_at = [](int i) { return db::Rect{-i - 1, -i - 1, i + 1, i + 1}; };
  const auto type_at = [&c](int i) { return i == 7 ? c.type("m2contact") : c.type(i % 2 == 0 ? "metal1" : "metal2"); };
  ViaDefinition one_by_one;
  ViaDefinition built;
  ViaRectBuilder builder{built};
  for (int i = 0; i < 20; ++i) {
    add_via_rect(one_by_one, type_at(i), rect_at(i), c.via());
    builder.add(type_at(i), rect_at(i), c.via());
    if (i == 11) {
      builder.commit();  // a commit midway orders only what came before
    }
  }
  builder.commit();
  builder.commit();  // a second commit changes nothing
  CHECK(built == one_by_one);

  ViaDefinition large;
  ViaRectBuilder many{large};
  constexpr int kRects = 200'000;
  for (int i = 0; i < kRects; ++i) {
    many.add(c.type("metal1"), rect_at(i), c.via());
  }
  many.commit();
  REQUIRE(large.shapes.size() == kRects - 1);
  CHECK(large.area == rect_at(0));
  CHECK(large.shapes.front().area == rect_at(kRects - 1));  // the last RECT heads the list
  CHECK(large.shapes.back().area == rect_at(1));
}

TEST_CASE("a name that is no layer of the LEF table shares no record", "[cap:lef-read][req:Via redefinition]") {
  CHECK(shared_names(LayerTable{}, "via1") == 0);
}

TEST_CASE("via areas scale like DBScalePoint; the extra list only with magic-compat OFF",
          "[cap:lef-tech-section][req:Scaling the LEF table]") {
  const bool compat = GENERATE(true, false);
  ViaTable vias;
  ViaDefinition via;
  via.area = db::Rect{-3, -3, 3, 3};
  via.shapes.push_back(ViaShape{1, db::Rect{-5, -1, 5, 1}});
  vias.insert("V", via);
  scale_via_areas(vias, 1, 2, compat);
  const ViaDefinition& v = *vias.find("V")->value;
  CHECK(v.area == db::Rect{-2, -2, 1, 1});  // floor
  CHECK(v.shapes.front().area == (compat ? db::Rect{-5, -1, 5, 1} : db::Rect{-3, -1, 2, 0}));
  scale_via_areas(vias, 0, 2, compat);  // not a grid factor: unchanged
  CHECK(vias.find("V")->value->area == db::Rect{-2, -2, 1, 1});
}

TEST_CASE("LEF-VIA messages carry Magic's dialect, not the reader's",
          "[cap:lef-read][req:Cut growing]") {
  // LefGrowVia reports with LEF_WARNING even inside `def read`; the DEF
  // VIAS messages use DEF_WARNING / DEF_ERROR.
  CHECK(via_message_dialect("LEF-VIA-001") == Dialect::lef);
  CHECK(via_message_dialect("LEF-VIA-003") == Dialect::lef);
  CHECK(via_message_dialect("LEF-VIA-004") == Dialect::lef);
  CHECK(via_message_dialect("LEF-VIA-002") == Dialect::def);
  CHECK(via_message_dialect("LEF-VIA-005") == Dialect::def);
  CHECK(via_message_dialect("LEF-VIA-006") == Dialect::def);
  CHECK_FALSE(via_message_dialect("LEF-VIA-008").has_value());
}

}  // namespace
}  // namespace hefesto::lef
