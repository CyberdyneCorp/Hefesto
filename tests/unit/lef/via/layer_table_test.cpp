// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Via redefinition over a LEF layer table read from a tech `lef` section
// (LEFTECH's lef::read_layer_section) instead of an empty table: the round-2
// integration test of LEFVIA's store against LEFTECH's aliases
// (openspec/changes/m6-lef-def-oasis tasks.md 4.1), including the
// canonical-name move of Q-LEFDEF-8 (LayerTable::redefine_alias).
//
// Owner: INTEGRATION (cross-workstream test; design D2).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/lef/tech/lifecycle.hpp"
#include "hefesto/lef/via/store.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "via_fixture.hpp"

namespace hefesto::lef {
namespace {

using test::Context;

/// via.tech with a lef section: m2contact under three names, metal1 under
/// two, metal2 under one.
constexpr std::string_view kLefSection = R"(
lef
    contact m2contact via1 V1 VIA12
    routing metal1 met1 M1
    routing metal2 met2
end
)";

std::shared_ptr<const tech::Technology> aliased_tech() {
  std::string text = test::via_tech_text();
  text += "\ndrc\nend\n";
  text += kLefSection;
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text, "/t/via.tech", fs, tech::LoadOptions{}, sink);
  for (const auto& d : sink.take()) {
    UNSCOPED_INFO(d.code << ": " << d.text);
  }
  REQUIRE(loaded.ok());
  return loaded.value();
}

LayerTable layer_table(const tech::Technology& technology) {
  diag::CollectingSink sink;
  LayerTable table = read_layer_section(technology, nullptr, compat::CompatSettings{}, sink);
  REQUIRE(sink.take().empty());
  return table;
}

TEST_CASE("the tech lef section's aliases are counted as shared names",
          "[cap:lef-read][req:Via redefinition]") {
  const auto technology = aliased_tech();
  const LayerTable layers = layer_table(*technology);
  CHECK(shared_names(layers, "via1") == 3);
  CHECK(shared_names(layers, "VIA12") == 3);
  CHECK(shared_names(layers, "M1") == 2);
  CHECK(shared_names(layers, "met2") == 1);
  CHECK(shared_names(layers, "nosuch") == 0);
}

TEST_CASE("a LEF VIA named after one alias of a tech lef cut gets its own via record",
          "[cap:lef-read][req:Via redefinition]") {
  Context c{aliased_tech()};
  LayerTable layers = layer_table(*c.technology);
  const LayerRecord* layer = layers.find("V1");
  REQUIRE(layer != nullptr);
  const LayerRecord before = *layer;
  REQUIRE(before.canonical_name == "via1");

  ViaTable vias;
  ViaDefinition* via = begin_via(vias, layers, "V1", ViaSection::lef_via, c.via());
  REQUIRE(via != nullptr);
  CHECK(c.texts() == std::vector<std::string>{"Cut type \"V1\" redefined."});
  // Q-LEFDEF-8 fix: class VIA, no types, an unset area, named after itself.
  CHECK(via->layer_class == LayerClass::cut);
  CHECK_FALSE(via->type.has_value());
  CHECK(via->area == db::Rect{});
  CHECK(via->canonical_name == "V1");

  // The other names keep the tech layer record unchanged; V1 left it.
  CHECK(layers.find("via1") == layers.find("VIA12"));
  CHECK(*layers.find("via1") == before);
  CHECK(layers.find("V1") == nullptr);
  CHECK(shared_names(layers, "via1") == 2);
}

TEST_CASE("redefining the canonical name of a shared tech lef record moves it to another alias",
          "[cap:lef-read][req:Via redefinition]") {
  // Q-LEFDEF-8 (fix, both modes): Magic takes the first name of the whole
  // table that differs, which can be an unrelated layer (met2, M1, ...).
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Context c{aliased_tech(), on};
    LayerTable layers = layer_table(*c.technology);
    const LayerRecord* cut = layers.find("VIA12");
    REQUIRE(cut != nullptr);
    REQUIRE(cut->canonical_name == "via1");
    // The first other alias of the record in hash order.
    std::string expected;
    layers.enumerate([&](std::string_view name, const LayerRecord& record) {
      if (expected.empty() && &record == cut && name != "via1") {
        expected = std::string{name};
      }
    });
    REQUIRE((expected == "V1" || expected == "VIA12"));

    ViaTable vias;
    ViaDefinition* via = begin_via(vias, layers, "via1", ViaSection::lef_via, c.via());
    REQUIRE(via != nullptr);
    CHECK(via->layer_class == LayerClass::cut);
    CHECK(via->canonical_name == "via1");
    CHECK(layers.find("via1") == nullptr);
    REQUIRE(layers.find("V1") == layers.find("VIA12"));
    CHECK(layers.find("V1")->canonical_name == expected);
    CHECK(layers.find("V1")->layer_class == LayerClass::cut);
    // Every remaining name of every layer still names an alias of its own record.
    layers.enumerate([&](std::string_view, const LayerRecord& record) {
      CHECK(layers.find(record.canonical_name) == &record);
    });
  }
}

TEST_CASE("LayerTable::redefine_alias keeps a canonical name that was not redefined",
          "[cap:lef-read][req:Via redefinition]") {
  Context c{aliased_tech()};
  LayerTable layers = layer_table(*c.technology);
  layers.redefine_alias("M1");
  REQUIRE(layers.find("met1") != nullptr);
  CHECK(layers.find("met1")->canonical_name == "met1");
  CHECK(layers.find("M1") == nullptr);
  layers.redefine_alias("nosuch");  // no record: nothing happens
  CHECK(layers.find("met1") != nullptr);
}

TEST_CASE("a DEF via named after a single-name tech lef route keeps the layer's class",
          "[cap:def-read][req:Via redefinition]") {
  Context c{aliased_tech()};
  LayerTable layers = layer_table(*c.technology);
  ViaTable vias;
  ViaDefinition* via = begin_via(vias, layers, "met2", ViaSection::def_via, c.via());
  REQUIRE(via != nullptr);
  CHECK(c.texts() == std::vector<std::string>{"Composite via \"met2\" redefined."});
  CHECK(via->layer_class == LayerClass::route);
  CHECK_FALSE(via->type.has_value());
  CHECK(via->canonical_name == "met2");
  add_via_rect(*via, c.type("metal2"), db::Rect{-10, -10, 10, 10}, c.via());
  CHECK(vias.find("met2")->value->type == c.type("metal2"));
}

TEST_CASE("a VIARULE named after a tech lef layer is skipped", "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  Context c{aliased_tech()};
  LayerTable layers = layer_table(*c.technology);
  ViaTable vias;
  CHECK(begin_via(vias, layers, "VIA12", ViaSection::lef_viarule, c.via()) == nullptr);
  CHECK(vias.size() == 0);
  CHECK(c.texts().empty());
}

}  // namespace
}  // namespace hefesto::lef
