// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// derive_after_load (m10-plot-route tasks.md 9.2): widths, lengths and
// spacings from the DRC rules, the explicit spacings, the SUBCELL maxima,
// spacing tables over the defined tile types only (Q-LEGACY-35), and the
// `width` statement per magic-compat setting (Q-LEGACY-33), on the test
// technology and on Magic's scmos.
//
// Owner: MZTECH.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>

#include "route/tech/detail.hpp"
#include "route_tech_fixture.hpp"

namespace route = hefesto::route;
namespace diag = hefesto::diag;
namespace fs = std::filesystem;
using route::test::contact_of;
using route::test::derived_styles;
using route::test::layer_of;
using route::test::load_text;
using route::test::tech_text;
using route::test::type_named;

namespace {

constexpr const char* kIrouter =
    "style irouter\n"
    "layer m2 32 64 256 1\n"
    "layer m1 64 32 256 1\n"
    "layer poly 128 128 512 1\n"
    "contact m2contact metal1 metal2 1024\n"
    "contact pcontact metal1 poly 2056\n";

std::shared_ptr<const hefesto::tech::Technology> scmos() {
  const fs::path path{HEFESTO_SCMOS_TECH};
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    return nullptr;
  }
  return route::test::load_file(path);
}

}  // namespace

TEST_CASE("widths, lengths and spacings come from DRC",
          "[cap:router-maze][req:Width and spacing derivation from DRC]") {
  diag::CollectingSink sink;
  const auto tech = load_text(tech_text(kIrouter), sink);
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    const route::MazeStyles styles = derived_styles(*tech, compat);
    const route::MazeStyle& style = styles.styles.at(0);
    const auto m1 = type_named(*tech, "metal1");
    const auto m2 = type_named(*tech, "metal2");
    const auto poly = type_named(*tech, "poly");
    const route::RouteLayer* l2 = layer_of(style, m2);
    REQUIRE(l2 != nullptr);
    CHECK(l2->route.width == 3);
    CHECK(l2->route.length == 3);
    CHECK(l2->route.spacing.at(m2) == 4);
    CHECK(l2->route.spacing.at(m1) == 0);  // no rule
    CHECK(layer_of(style, m1)->route.spacing.at(m1) == 3);
    CHECK(layer_of(style, poly)->route.width == 2);
    CHECK(layer_of(style, poly)->route.spacing.at(type_named(*tech, "ndiff")) == 1);
    const route::RouteContact* via = contact_of(style, type_named(*tech, "m2c"));
    REQUIRE(via != nullptr);
    CHECK(via->route.width == 2);
    CHECK(via->route.length == 2);
    CHECK(contact_of(style, type_named(*tech, "pc"))->route.width == 0);  // no width rule
    // SUBCELL: the largest spacing of the type.
    CHECK(l2->route.subcell_spacing == 4);
    CHECK(layer_of(style, poly)->route.subcell_spacing == 1);
  }
}

TEST_CASE("spacing tables cover the defined tile types only",
          "[cap:router-maze][req:Width and spacing derivation from DRC]") {
  // Q-LEGACY-35: Magic fills TT_MAXTYPES entries; Hefesto one per defined type.
  diag::CollectingSink sink;
  const auto tech = load_text(tech_text(kIrouter), sink);
  const route::MazeStyles styles = derived_styles(*tech, true);
  for (const route::RouteTypeRef ref : styles.styles.at(0).types) {
    CHECK(styles.styles.at(0).route_type(ref).spacing.size() == tech->type_count());
  }
}

TEST_CASE("explicit spacings override the derived ones",
          "[cap:router-maze][req:Width and spacing derivation from DRC]") {
  diag::CollectingSink sink;
  const auto tech = load_text(tech_text(std::string{kIrouter} +
                                        "spacing m2 m2 7 m1 NIL\n"
                                        "spacing m2 m2 9\n"  // the first statement for a pair wins (Magic's list order)
                                        "spacing m1 SUBCELL 2\n"
                                        "spacing poly m3 12\n"),
                                sink);
  const route::MazeStyles styles = derived_styles(*tech, true);
  const route::MazeStyle& style = styles.styles.at(0);
  const route::RouteLayer* l2 = layer_of(style, type_named(*tech, "metal2"));
  CHECK(l2->route.spacing.at(type_named(*tech, "metal2")) == 7);
  CHECK(l2->route.spacing.at(type_named(*tech, "metal1")) == -1);
  CHECK(l2->route.subcell_spacing == 7);  // the maximum after the explicit spacings
  // An explicit SUBCELL spacing overrides the maximum.
  CHECK(layer_of(style, type_named(*tech, "metal1"))->route.subcell_spacing == 2);
  const route::RouteLayer* poly = layer_of(style, type_named(*tech, "poly"));
  CHECK(poly->route.spacing.at(type_named(*tech, "metal3")) == 12);
  CHECK(poly->route.subcell_spacing == 12);
}

TEST_CASE("without DRC rules every width and spacing is 0",
          "[cap:router-maze][req:Width and spacing derivation from DRC]") {
  diag::CollectingSink sink;
  const auto tech = load_text(tech_text(std::string{kIrouter} + "spacing m2 m2 5\n"), sink);
  route::MazeStyles styles = tech->maze_styles();
  route::derive_after_load(styles, *tech, nullptr, hefesto::lef::LayerTable{}, true, sink);
  const route::RouteLayer* l2 = layer_of(styles.styles.at(0), type_named(*tech, "metal2"));
  CHECK(l2->route.width == 0);
  CHECK(l2->route.spacing.at(type_named(*tech, "metal2")) == 5);
  CHECK(l2->route.spacing.at(type_named(*tech, "metal1")) == 0);
  CHECK(l2->route.subcell_spacing == 5);
}

TEST_CASE("derivation starts again from the written styles",
          "[cap:router-maze][req:Width and spacing derivation from DRC]") {
  diag::CollectingSink sink;
  const auto tech = load_text(tech_text(kIrouter), sink);
  route::MazeStyles first = derived_styles(*tech, true);
  route::MazeStyles second = first;
  // A second run on its own output (a rescale on the session copy) gives
  // the same values.
  const auto rules = hefesto::drc::read_rules(*tech, hefesto::compat::CompatSettings{}, sink);
  route::derive_after_load(second, *tech, rules.rules.get(), hefesto::lef::LayerTable{}, true, sink);
  CHECK(second.styles.at(0).layers.at(0).route.width == first.styles.at(0).layers.at(0).route.width);
  CHECK(second.styles.at(0).layers.at(0).route.spacing == first.styles.at(0).layers.at(0).route.spacing);
  CHECK(tech->maze_styles().styles.at(0).layers.at(0).route.width == 0);  // the technology's copy is untouched
}

TEST_CASE("width statement with magic-compat on: DRC wins",
          "[cap:router-maze][req:Maze router behaviour with magic-compat off]") {
  // Q-LEGACY-33 ON: MZAfterTech overwrites the statement with the DRC width.
  diag::CollectingSink sink;
  const auto tech = load_text(tech_text(std::string{kIrouter} + "width m2 4\nwidth m2contact 6 8\n"), sink);
  const route::MazeStyles styles = derived_styles(*tech, true);
  const route::MazeStyle& style = styles.styles.at(0);
  CHECK(layer_of(style, type_named(*tech, "metal2"))->route.width == 3);
  CHECK(layer_of(style, type_named(*tech, "metal2"))->route.length == 3);
  CHECK(contact_of(style, type_named(*tech, "m2c"))->route.width == 2);
  CHECK(contact_of(style, type_named(*tech, "m2c"))->route.length == 2);
}

TEST_CASE("width statement honoured with magic-compat off",
          "[cap:router-maze][req:Maze router behaviour with magic-compat off]") {
  // Q-LEGACY-33 OFF: the statement overrides the DRC width and length.
  diag::CollectingSink sink;
  const auto tech = load_text(tech_text(std::string{kIrouter} + "width m2 4\nwidth m2contact 6 8\n"), sink);
  const route::MazeStyles styles = derived_styles(*tech, false);
  const route::MazeStyle& style = styles.styles.at(0);
  CHECK(layer_of(style, type_named(*tech, "metal2"))->route.width == 4);
  CHECK(layer_of(style, type_named(*tech, "metal2"))->route.length == 4);
  CHECK(contact_of(style, type_named(*tech, "m2c"))->route.width == 6);
  CHECK(contact_of(style, type_named(*tech, "m2c"))->route.length == 8);
  CHECK(layer_of(style, type_named(*tech, "metal1"))->route.width == 3);  // no statement
}

TEST_CASE("apply_width_statements is the Q-LEGACY-33 branch point",
          "[cap:router-maze][req:Maze router behaviour with magic-compat off]") {
  route::MazeStyle style;
  style.layers.resize(1);
  style.layers[0].route.width = 3;
  style.layers[0].route.length = 3;
  style.types.push_back(route::RouteTypeRef{});
  style.widths.push_back(route::WidthStatement{route::RouteTypeRef{}, 5, 7});
  route::apply_width_statements(style, true);
  CHECK(style.layers[0].route.width == 3);
  route::apply_width_statements(style, false);
  CHECK(style.layers[0].route.width == 5);
  CHECK(style.layers[0].route.length == 7);
}

TEST_CASE("scmos: the irouter and garouter styles", "[cap:router-maze][req:mzrouter section grammar]") {
  const auto tech = scmos();
  if (tech == nullptr) {
    SKIP("scmos.tech is generated from build/ref/magic");
  }
  const route::MazeStyles& written = tech->maze_styles();
  REQUIRE(written.styles.size() == 2);
  const route::MazeStyle* ir = route::find_style(written, "irouter");
  REQUIRE(ir != nullptr);
  CHECK(ir->layers.size() == 3);
  CHECK(ir->contacts.size() == 2);
  CHECK_FALSE(layer_of(*ir, type_named(*tech, "poly"))->route.active);
  CHECK_FALSE(contact_of(*ir, type_named(*tech, "pcontact"))->route.active);
  CHECK(layer_of(*ir, type_named(*tech, "metal1"))->route.active);
  CHECK(route::find_style(written, "garouter")->layers.size() == 2);
}

TEST_CASE("scmos: width from DRC", "[cap:router-maze][req:Width and spacing derivation from DRC]") {
  const auto tech = scmos();
  if (tech == nullptr) {
    SKIP("scmos.tech is generated from build/ref/magic");
  }
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    const route::MazeStyles styles = derived_styles(*tech, compat);
    const route::MazeStyle* ir = route::find_style(styles, "irouter");
    REQUIRE(ir != nullptr);
    const auto m1 = type_named(*tech, "metal1");
    const auto m2 = type_named(*tech, "metal2");
    const route::RouteLayer* l2 = layer_of(*ir, m2);
    CHECK(l2->route.width == 3);  // width allMetal2 3
    CHECK(l2->route.length == 3);
    CHECK(l2->route.spacing.at(m2) == 4);
    CHECK(layer_of(*ir, m1)->route.width == 3);
    CHECK(layer_of(*ir, m1)->route.spacing.at(m1) == 3);
    CHECK(layer_of(*ir, type_named(*tech, "poly"))->route.width == 2);
    CHECK(contact_of(*ir, type_named(*tech, "m2contact"))->route.width == 4);
    CHECK(contact_of(*ir, type_named(*tech, "pcontact"))->route.width == 4);
    const int widest = *std::max_element(l2->route.spacing.begin(), l2->route.spacing.end());
    CHECK(l2->route.subcell_spacing == std::max(0, widest));
    CHECK(l2->route.spacing.size() == tech->type_count());
    // scmos has an irouter style: nothing is derived in either setting.
    CHECK(styles.styles.size() == 2);
  }
}
