// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `irouter` style derived from the `lef` section (m10-plot-route
// tasks.md 9.3; Q-LEGACY-37 (b), J2): with magic-compat OFF only, on the
// three PDKs (skipped when their data set is not fetched), on scmos with
// its `irouter` style removed, and on the test technology.
//
// Owner: MZTECH.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "reference_data.hpp"
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

constexpr std::string_view kLef = R"(
lef
    masterslice ndiff ndiff
    routing m1 MET1 m1
    cut m2c VIA12 via
    routing m2 MET2
    routing m3 MET3
    cut m3c VIA23
    cut pc CONT
    routing poly POLY
end
)";

struct Expected {
  std::vector<std::string_view> layers;    // magic type names, bottom first
  std::vector<std::string_view> contacts;  // magic type names, in order
  std::string_view no_width_rule = {};       // a layer the default DRC style gives no width rule
};

// Checks a derived style against the layer and contact names expected.
void check_derived(const hefesto::tech::Technology& tech, const route::MazeStyle& style, const Expected& expected) {
  CHECK(style.name == "irouter");
  CHECK(style.derived);
  REQUIRE(style.layers.size() == expected.layers.size());
  for (std::size_t i = 0; i < expected.layers.size(); ++i) {
    INFO("layer " << expected.layers[i]);
    const route::RouteLayer& layer = style.layers[i];
    CHECK(layer.route.type == type_named(tech, expected.layers[i]));
    CHECK(layer.h_cost == (i % 2 == 0 ? 32 : 64));
    CHECK(layer.v_cost == (i % 2 == 0 ? 64 : 32));
    CHECK(layer.jog_cost == 256);
    CHECK(layer.hint_cost == 1);
    CHECK(layer.over_cost == 1);
    CHECK(layer.route.active);
  }
  REQUIRE(style.contacts.size() == expected.contacts.size());
  for (std::size_t i = 0; i < expected.contacts.size(); ++i) {
    INFO("contact " << expected.contacts[i]);
    const route::RouteContact& contact = style.contacts[i];
    CHECK(contact.route.type == type_named(tech, expected.contacts[i]));
    CHECK(contact.cost == 1024);
    CHECK(contact.route.active);
    CHECK(contact.layer1 < contact.layer2);
  }
  CHECK(style.types.size() == style.layers.size() + style.contacts.size());
  // Every other parameter keeps the style defaults.
  CHECK(style.penalty.mantissa == 2048);
  CHECK(style.window_rate == 500);
  CHECK(style.window_width == 10000);
  CHECK(style.bloom_limit == 0);
}

// Loads a pinned PDK's tech file, or nullptr when it is not fetched.
std::shared_ptr<const hefesto::tech::Technology> pdk(const hefesto::test::PinnedPdk& set) {
  const std::optional<fs::path> path = hefesto::test::pinned_tech(set);
  return path ? route::test::load_file(*path) : nullptr;
}

// Both settings on a PDK: OFF derives `expected`, widths from DRC; ON
// derives nothing (the section is empty, so `iroute` keeps Magic's
// missing-style error).
void check_pdk(const hefesto::test::PinnedPdk& set, const Expected& expected) {
  const auto tech = pdk(set);
  if (tech == nullptr) {
    SKIP("the pinned " << set.variant << " data set is not fetched (scripts/fetch_pdk.py)");
  }
  CHECK(route::find_style(tech->maze_styles(), "irouter") == nullptr);
  const route::MazeStyles off = derived_styles(*tech, false);
  const route::MazeStyle* style = route::find_style(off, "irouter");
  REQUIRE(style != nullptr);
  check_derived(*tech, *style, expected);
  for (const route::RouteLayer& layer : style->layers) {
    INFO("width of " << tech->type_name(layer.route.type));
    const bool ruled = expected.no_width_rule.empty() ||
                       layer.route.type != type_named(*tech, expected.no_width_rule);
    CHECK((layer.route.width > 0) == ruled);
    CHECK(layer.route.spacing.size() == tech->type_count());
  }
  const route::MazeStyles on = derived_styles(*tech, true);
  CHECK(route::find_style(on, "irouter") == nullptr);
}

}  // namespace

TEST_CASE("sky130A with magic-compat off and on", "[cap:router-maze-iroute][req:Derived irouter style]") {
  check_pdk(hefesto::test::kSky130A, Expected{{"li", "m1", "m2", "m3", "m4", "m5", "mrdl"},
                                              {"mcon", "m2c", "m3c", "via3", "via4"},
                                              "mrdl"});
}

TEST_CASE("gf180mcuD with magic-compat off and on", "[cap:router-maze-iroute][req:Derived irouter style]") {
  check_pdk(hefesto::test::kGf180mcuD, Expected{{"metal1", "metal2", "metal3", "metal4", "metal5"},
                                                {"m2contact", "m3contact", "via3", "via4"}});
}

TEST_CASE("ihp-sg13g2 with magic-compat off and on", "[cap:router-maze-iroute][req:Derived irouter style]") {
  check_pdk(hefesto::test::kIhpSg13g2,
            Expected{{"m1", "m2", "m3", "m4", "m5", "m6", "m7"},
                     {"via1", "via2", "via3", "via4", "via5", "via6"}});
}

TEST_CASE("the derived style from a lef section, magic-compat off",
          "[cap:router-maze-iroute][req:Derived irouter style]") {
  diag::CollectingSink sink;
  const auto tech = load_text(tech_text("style garouter\nlayer m1 1 1", kLef), sink);
  const route::MazeStyles styles = derived_styles(*tech, false);
  REQUIRE(styles.styles.size() == 2);
  CHECK(styles.styles[0].name == "garouter");
  const route::MazeStyle& style = styles.styles[1];  // a derived style comes last
  // poly comes last although its cut line precedes it; pc connects poly and
  // metal1, both derived, so it is a contact.
  check_derived(*tech, style, Expected{{"metal1", "metal2", "metal3", "poly"}, {"m2c", "m3c", "pc"}});
  const route::RouteContact* pc = contact_of(style, type_named(*tech, "pc"));
  CHECK(pc->layer1 == 0);  // metal1
  CHECK(pc->layer2 == 3);  // poly
  // Widths and spacings come from DRC as for a written style.
  CHECK(layer_of(style, type_named(*tech, "metal2"))->route.width == 3);
  CHECK(layer_of(style, type_named(*tech, "metal2"))->route.spacing.at(type_named(*tech, "metal2")) == 4);
  CHECK(contact_of(style, type_named(*tech, "m2c"))->route.width == 2);
}

TEST_CASE("nothing is derived with magic-compat on", "[cap:router-maze-iroute][req:Derived irouter style]") {
  diag::CollectingSink sink;
  const auto tech = load_text(tech_text("style garouter\nlayer m1 1 1", kLef), sink);
  const route::MazeStyles styles = derived_styles(*tech, true);
  REQUIRE(styles.styles.size() == 1);
  CHECK(route::find_style(styles, "irouter") == nullptr);
  route::MazeStyles direct = tech->maze_styles();
  CHECK_FALSE(route::maybe_derive_irouter(direct, *tech, hefesto::lef::LayerTable{}, true));
  CHECK(direct.styles.size() == 1);
}

TEST_CASE("a written irouter style takes precedence", "[cap:router-maze-iroute][req:Derived irouter style]") {
  diag::CollectingSink sink;
  const auto tech = load_text(tech_text("style irouter\nlayer m2 5 5", kLef), sink);
  for (const bool compat : {true, false}) {
    const route::MazeStyles styles = derived_styles(*tech, compat);
    REQUIRE(styles.styles.size() == 1);
    CHECK_FALSE(styles.styles[0].derived);
    CHECK(styles.styles[0].layers.size() == 1);
  }
}

TEST_CASE("no routing line, no derived style", "[cap:router-maze-iroute][req:Derived irouter style]") {
  diag::CollectingSink sink;
  const auto tech = load_text(tech_text("", "\nlef\n    cut m2c VIA12\n    masterslice ndiff ndiff\nend\n"), sink);
  for (const bool compat : {true, false}) {
    CHECK(derived_styles(*tech, compat).styles.empty());
  }
}

TEST_CASE("scmos with its irouter style removed", "[cap:router-maze-iroute][req:Derived irouter style]") {
  const fs::path path{HEFESTO_SCMOS_TECH};
  std::string text = route::test::read_file(path);
  if (text.empty()) {
    SKIP("scmos.tech is generated from build/ref/magic");
  }
  const std::size_t from = text.find("style irouter");
  const std::size_t to = text.find("style garouter");
  REQUIRE(from != std::string::npos);
  REQUIRE(to != std::string::npos);
  text.erase(from, to - from);
  diag::CollectingSink sink;
  {
    // scmos has no lef section: nothing to derive from in either setting.
    const auto tech = load_text(text, sink);
    for (const bool compat : {true, false}) {
      const route::MazeStyles styles = derived_styles(*tech, compat);
      CHECK(styles.styles.size() == 1);
      CHECK(route::find_style(styles, "irouter") == nullptr);
    }
  }
  // With a lef section appended, OFF derives metal1, metal2 and m2contact.
  text += "\nlef\n    routing m1 MET1\n    routing m2 MET2\n    cut m2c VIA12\nend\n";
  const auto tech = load_text(text, sink);
  const route::MazeStyles off = derived_styles(*tech, false);
  const route::MazeStyle* style = route::find_style(off, "irouter");
  REQUIRE(style != nullptr);
  check_derived(*tech, *style, Expected{{"metal1", "metal2"}, {"m2contact"}});
  CHECK(layer_of(*style, type_named(*tech, "metal2"))->route.width == 3);
  CHECK(contact_of(*style, type_named(*tech, "m2contact"))->route.width == 4);
  CHECK(route::find_style(derived_styles(*tech, true), "irouter") == nullptr);
}
