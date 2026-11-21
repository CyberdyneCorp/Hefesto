// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The DEF-write side of the via code: contact regions (Q-LEFDEF-111),
// generated via names and records (Q-LEFDEF-105, 106), registration, and
// the VIAS RECT lines (Q-LEFDEF-107, 115) (capability def-write;
// openspec/changes/m6-lef-def-oasis tasks.md 4.5). The cells are painted
// directly; DEF write itself lands with DEFW.
//
// Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/drc/rules/query.hpp"
#include "hefesto/lef/via/regions.hpp"
#include "via_fixture.hpp"

namespace hefesto::lef {
namespace {

/// A cell of via.tech, with paint helpers and a DEF-write via context.
struct Layout {
  std::shared_ptr<const tech::Technology> technology;
  std::shared_ptr<const drc::RuleSet> rules;
  db::CellDef cell;
  compat::CompatSettings compat;
  diag::CollectingSink sink;
  std::map<std::string, std::string> names{{"m2contact", "via1"}, {"metal1", "metal1"}, {"metal2", "metal2"}};

  explicit Layout(bool magic_compat = true, std::string_view drc_body = {})
      : technology(test::via_tech(drc_body)),
        rules(drc_body.empty() ? nullptr : test::rules_of(*technology)),
        cell("top", technology->plane_count()),
        compat(test::compat_mode(magic_compat)) {}

  [[nodiscard]] tech::TileType type(std::string_view name) const { return test::type_of(*technology, name); }

  void paint(std::string_view type_name, const db::Rect& r) {
    REQUIRE(db::paint_type(cell, *technology, r, type(type_name)).ok());
  }

  [[nodiscard]] DefViaContext context(float scale = 10.0F) {
    DefViaContext c{*technology, rules.get(), {}, scale, compat, sink};
    c.lef_name = [this](tech::TileType t) -> std::optional<std::string> {
      const auto found = names.find(std::string{technology->type_name(t)});
      return found == names.end() ? std::nullopt : std::optional{found->second};
    };
    return c;
  }

  [[nodiscard]] std::vector<db::Rect> regions() const {
    std::vector<db::Rect> out;
    for (const ContactRegion& r : contact_regions(cell, *technology, compat)) {
      CHECK(r.type == type("m2contact"));
      out.push_back(r.area);
    }
    return out;
  }
};

TEST_CASE("a contact stored as two tiles is one region", "[cap:def-write][req:Contact region identification]") {
  for (const bool compat : {true, false}) {
    Layout l{compat};
    l.paint("m2contact", db::Rect{10, 10, 36, 36});
    l.paint("metal1", db::Rect{36, 20, 50, 40});  // splits the contact tile at y = 20
    CHECK(l.regions() == std::vector<db::Rect>{{10, 10, 36, 36}});
  }
}

TEST_CASE("an L-shaped contact: the chain bounding box with magic-compat ON, exact rectangles OFF",
          "[cap:def-write][req:Contact region identification]") {
  // Arm on the right: the walk up from the right-top neighbour (Magic's RT)
  // reaches it, so the bounding box overshoots.
  for (const bool compat : {true, false}) {
    Layout l{compat};
    l.paint("m2contact", db::Rect{10, 10, 36, 20});
    l.paint("m2contact", db::Rect{26, 20, 36, 36});
    const std::vector<db::Rect> expected =
        compat ? std::vector<db::Rect>{{10, 10, 36, 36}} : std::vector<db::Rect>{{10, 10, 36, 20}, {26, 20, 36, 36}};
    CHECK(l.regions() == expected);
  }
  // Arm on the left: RT of the lower tile is space, so Magic's region is the
  // lower tile and the arm (whose bottom neighbour is the contact) is lost.
  for (const bool compat : {true, false}) {
    Layout l{compat};
    l.paint("m2contact", db::Rect{10, 10, 36, 20});
    l.paint("m2contact", db::Rect{10, 20, 20, 36});
    const std::vector<db::Rect> expected =
        compat ? std::vector<db::Rect>{{10, 10, 36, 20}} : std::vector<db::Rect>{{10, 10, 36, 20}, {10, 20, 20, 36}};
    CHECK(l.regions() == expected);
  }
}

TEST_CASE("separate contacts are separate regions", "[cap:def-write][req:Contact region identification]") {
  Layout l;
  l.paint("m2contact", db::Rect{0, 0, 4, 4});
  l.paint("m2contact", db::Rect{20, 0, 24, 4});
  const auto regions = l.regions();
  REQUIRE(regions.size() == 2);
  CHECK(l.regions().size() == 2);
}

TEST_CASE("plain generated via naming", "[cap:def-write][req:Generated via definitions]") {
  Layout l;
  l.paint("m2contact", db::Rect{10, 10, 36, 36});
  const DefViaContext c = l.context();
  const auto regions = contact_regions(l.cell, *l.technology, l.compat);
  REQUIRE(regions.size() == 1);
  const auto via = region_via(l.cell, regions.front(), c);
  REQUIRE(via.has_value());
  CHECK(via->name == "via1_260_260");
  CHECK(via_name(l.cell, regions.front(), c) == "via1_260_260");
  CHECK(via->record.type == l.type("m2contact"));
  CHECK(via->record.area == db::Rect{-26, -26, 26, 26});
  CHECK(via->record.ref_count == 0);
  CHECK(via->record.layer_class == LayerClass::cut);
  CHECK(via->record.shapes.empty());
  CHECK(via->position_key == "m1_10_10");

  ViaTable vias;
  LayerTable layers;
  const DefVias registered = register_vias(l.cell, vias, layers, c);
  CHECK(registered.count == 1);
  CHECK(registered.created == std::vector<std::string>{"via1_260_260"});
  CHECK(registered.positions.at("m1_10_10") == "via1_260_260");
  CHECK(vias.find("via1_260_260")->value == via->record);
}

TEST_CASE("two identical vias share one definition", "[cap:def-write][req:Generated via definitions]") {
  Layout l;
  l.paint("m2contact", db::Rect{10, 10, 36, 36});
  l.paint("m2contact", db::Rect{100, 10, 126, 36});
  ViaTable vias;
  LayerTable layers;
  const DefVias registered = register_vias(l.cell, vias, layers, l.context());
  CHECK(registered.count == 1);
  CHECK(registered.positions.size() == 2);
  CHECK(registered.positions.at("m1_100_10") == "via1_260_260");
  // A second write reuses the record without counting it.
  CHECK(register_vias(l.cell, vias, layers, l.context()).count == 0);
  CHECK(layers.size() == 1);  // the name is noted once in the shared key order (Q-LEFDEF-135)
}

TEST_CASE("a contact type with no LEF name gives no via", "[cap:def-write][req:Generated via definitions]") {
  Layout l;
  l.names.erase("m2contact");
  l.paint("m2contact", db::Rect{10, 10, 36, 36});
  ViaTable vias;
  LayerTable layers;
  const DefVias registered = register_vias(l.cell, vias, layers, l.context());
  CHECK(registered.count == 0);
  CHECK(registered.positions.empty());
}

constexpr std::string_view kDirectional =
    "surround m2contact metal1 1 absence_illegal \"s\"\nsurround m2contact metal1 1 2 directional \"dir\"";

TEST_CASE("a directional surround adds the orientation suffix", "[cap:def-write][req:Generated via definitions]") {
  Layout probe{true, kDirectional};
  const std::int32_t s = drc::default_surround(*probe.rules, *probe.technology, probe.type("m2contact"),
                                               probe.type("metal1"));
  const std::int32_t d = drc::directional_surround(*probe.rules, probe.type("m2contact"), probe.type("metal1"));
  REQUIRE(d > 0);
  CHECK(drc::directional_surround(*probe.rules, probe.type("m2contact"), probe.type("metal2")) == 0);
  {
    // metal1 extends far around the contact: no other tile in the probe.
    Layout l{true, kDirectional};
    l.paint("metal1", db::Rect{-50, -50, 100, 100});
    l.paint("m2contact", db::Rect{10, 10, 36, 36});
    const auto regions = contact_regions(l.cell, *l.technology, l.compat);
    REQUIRE(regions.size() == 1);
    const auto via = region_via(l.cell, regions.front(), l.context());
    REQUIRE(via.has_value());
    CHECK(via->name == "via1_260_260_hx");
    // The metal1 residue rectangle: A grown by 2s, then by 2d in x.
    const db::Rect a{-26, -26, 26, 26};
    const std::vector<ViaShape> shapes{
        {l.type("metal1"), db::Rect{a.x0 - 2 * s - 2 * d, a.y0 - 2 * s, a.x1 + 2 * s + 2 * d, a.y1 + 2 * s}}};
    CHECK(via->record.shapes == shapes);
  }
  {
    // metal1 only over the contact: space in the probe gives 'v'.
    Layout l{true, kDirectional};
    l.paint("m2contact", db::Rect{10, 10, 36, 36});
    const auto regions = contact_regions(l.cell, *l.technology, l.compat);
    const auto via = region_via(l.cell, regions.front(), l.context());
    CHECK(via->name == "via1_260_260_vx");
    const db::Rect a{-26, -26, 26, 26};
    CHECK(via->record.shapes.front().area ==
          db::Rect{a.x0 - 2 * s, a.y0 - 2 * s - 2 * d, a.x1 + 2 * s, a.y1 + 2 * s + 2 * d});
  }
}

TEST_CASE("the directional probe is asymmetric with magic-compat ON only",
          "[cap:def-write][req:Generated via definitions]") {
  Layout probe{true, kDirectional};
  const std::int32_t s = drc::default_surround(*probe.rules, *probe.technology, probe.type("m2contact"),
                                               probe.type("metal1"));
  const std::int32_t d = drc::directional_surround(*probe.rules, probe.type("m2contact"), probe.type("metal1"));
  REQUIRE(s > 0);
  // metal1 starts just inside the OFF probe (xl − d − s) but outside the ON
  // probe (xl − d + s).
  for (const bool compat : {true, false}) {
    Layout l{compat, kDirectional};
    l.paint("metal1", db::Rect{10 - d - s + 1, -50, 100, 100});
    l.paint("m2contact", db::Rect{10, 10, 36, 36});
    const auto regions = contact_regions(l.cell, *l.technology, l.compat);
    const auto via = region_via(l.cell, regions.front(), l.context());
    CHECK(via->name == (compat ? "via1_260_260_hx" : "via1_260_260_vx"));
  }
}

/// The cut operator of the def-write scenarios: z' = 170, p' = 170, b' = 50
/// at s = 10 (scale factor 1, so c = 10).
mask::OutputStyle cut_style(const Layout& l) {
  return test::squares_style(l.type("m2contact"), 1, 50, 170, 170);
}

ViaDefinition via_record(const Layout& l, db::Coord half) {
  ViaDefinition via;
  via.type = l.type("m2contact");
  via.area = db::Rect{-half, -half, half, half};
  return via;
}

TEST_CASE("a single centred cut", "[cap:def-write][req:VIAS section content]") {
  for (const bool compat : {true, false}) {
    Layout l{compat};
    const mask::OutputStyle style = cut_style(l);
    const std::vector<ViaRect> rects = via_rects(via_record(l, 26), "via1_260_260", &style, l.context());
    const std::vector<ViaRect> expected{{"metal1", -130, -130, 130, 130},
                                        {"metal2", -130, -130, 130, 130},
                                        {"via1", -85, -85, 85, 85}};
    CHECK(rects == expected);
    // Q-LEFDEF-107: Magic's inverted y test fires here (bottom >= Y1).
    std::vector<std::string> texts;
    for (const auto& d : l.sink.take()) {
      texts.push_back(d.text);
    }
    CHECK(texts == (compat ? std::vector<std::string>{"Warning: via size is 170 but area height is 260!"}
                           : std::vector<std::string>{}));
  }
}

TEST_CASE("a two-by-two cut array", "[cap:def-write][req:VIAS section content]") {
  Layout l;
  const mask::OutputStyle style = cut_style(l);
  const std::vector<ViaRect> rects = via_rects(via_record(l, 80), "v", &style, l.context());
  REQUIRE(rects.size() == 6);
  const std::vector<ViaRect> cuts{rects.begin() + 2, rects.end()};
  const std::vector<ViaRect> expected{{"via1", -255, -255, -85, -85},
                                      {"via1", 85, -255, 255, -85},
                                      {"via1", -255, 85, -85, 255},
                                      {"via1", 85, 85, 255, 255}};
  CHECK(cuts == expected);
  CHECK(l.sink.take().empty());
}

TEST_CASE("the width warning of a cut wider than its via", "[cap:def-write][req:VIAS section content]") {
  for (const bool compat : {true, false}) {
    Layout l{compat};
    const mask::OutputStyle style = cut_style(l);
    // Area 10 x 10 DEF units: X2 − X1 = 100 < z' = 170; left = −85 < −50.
    const auto rects = via_rects(via_record(l, 10), "v", &style, l.context());
    std::vector<std::string> texts;
    for (const auto& d : l.sink.take()) {
      texts.push_back(d.text);
    }
    std::vector<std::string> expected{"Warning: via size is 170 but area width is 100!"};
    if (!compat) {
      expected.emplace_back("Warning: via size is 170 but area height is 100!");
    }
    CHECK(texts == expected);
    CHECK(rects.back() == ViaRect{"via1", -85, -85, 85, 85});
  }
}

TEST_CASE("without a cut operator one cut covers the area", "[cap:def-write][req:VIAS section content]") {
  Layout l;
  const auto rects = via_rects(via_record(l, 26), "v", nullptr, l.context());
  REQUIRE(rects.size() == 3);
  CHECK(rects.back() == ViaRect{"via1", -130, -130, 130, 130});
}

TEST_CASE("a residue with no LEF name is skipped with a warning",
          "[cap:def-write][req:VIAS section content]") {
  // Q-LEFDEF-115 fix: Magic prints "(null)".
  Layout l;
  l.names.erase("metal2");
  const auto rects = via_rects(via_record(l, 26), "via1_260_260", nullptr, l.context());
  REQUIRE(rects.size() == 2);
  CHECK(rects.front().layer == "metal1");
  const auto diagnostics = l.sink.take();
  REQUIRE(diagnostics.size() == 1);
  CHECK(diagnostics.front().code == "LEF-VIA-011");
  CHECK(diagnostics.front().text ==
        "No LEF layer corresponding to magic type \"metal2\" of via \"via1_260_260\"; RECT skipped.");
}

TEST_CASE("a residue's surround rectangle replaces the area", "[cap:def-write][req:VIAS section content]") {
  Layout l;
  ViaDefinition via = via_record(l, 26);
  via.shapes.push_back(ViaShape{l.type("metal1"), db::Rect{-30, -26, 30, 26}});
  const auto rects = via_rects(via, "v", nullptr, l.context());
  CHECK(rects.front() == ViaRect{"metal1", -150, -130, 150, 130});
}

}  // namespace
}  // namespace hefesto::lef
