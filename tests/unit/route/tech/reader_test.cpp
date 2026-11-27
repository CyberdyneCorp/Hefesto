// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `mzrouter` section reader (m10-plot-route tasks.md 9.1): the grammar
// and Magic's messages, whole-statement validation (Q-LEGACY-31, -39), the
// `search` penalty range check (Q-LEGACY-47) and resolution (Q-LEGACY-32),
// style defaults, file order and find_style. The reader is the same in both
// magic-compat settings; the statements that differ by mode take effect in
// derive_after_load (derive_test.cpp).
//
// Owner: MZTECH.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "route_tech_fixture.hpp"

namespace route = hefesto::route;
namespace diag = hefesto::diag;
using route::test::contact_of;
using route::test::layer_of;
using route::test::load_text;
using route::test::tech_text;
using route::test::texts;
using route::test::type_named;

namespace {

// The styles read from `body`, with the messages to `sink`.
const route::MazeStyles& read(const std::string& body, diag::CollectingSink& sink,
                              std::shared_ptr<const hefesto::tech::Technology>& keep) {
  keep = load_text(tech_text(body), sink);
  return keep->maze_styles();
}

std::vector<std::string> route_messages(const diag::CollectingSink& sink) {
  return texts(sink.snapshot(), "ROUTE-TECH-");
}

}  // namespace

TEST_CASE("layer defaults", "[cap:router-maze][req:mzrouter section grammar]") {
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style irouter\nlayer m2 32 64", sink, tech);
  REQUIRE(styles.styles.size() == 1);
  const route::MazeStyle& style = styles.styles[0];
  REQUIRE(style.layers.size() == 1);
  const route::RouteLayer& m2 = style.layers[0];
  CHECK(m2.route.type == type_named(*tech, "metal2"));
  CHECK(m2.h_cost == 32);
  CHECK(m2.v_cost == 64);
  CHECK(m2.jog_cost == 1);
  CHECK(m2.hint_cost == 1);
  CHECK(m2.over_cost == 1);
  CHECK(m2.route.active);
  CHECK(route_messages(sink).empty());
}

TEST_CASE("a statement before any style line", "[cap:router-maze][req:mzrouter section grammar]") {
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("layer m1 64 32", sink, tech);
  CHECK(styles.styles.empty());
  CHECK(route_messages(sink) == std::vector<std::string>{"Missing style line."});
}

TEST_CASE("a contact with an undeclared layer defines nothing", "[cap:router-maze][req:mzrouter section grammar]") {
  // Q-LEGACY-31: Magic has already attached the contact to metal1 and to the
  // route-type list when the second layer fails.
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style irouter\nlayer m1 64 32\ncontact m2contact metal1 metal2 1024", sink, tech);
  REQUIRE(styles.styles.size() == 1);
  const route::MazeStyle& style = styles.styles[0];
  CHECK(style.contacts.empty());
  CHECK(style.types.size() == 1);
  CHECK(route_messages(sink) == std::vector<std::string>{"route layer must be declared before used in contact."});
}

TEST_CASE("a contact with a bad cost defines nothing", "[cap:router-maze][req:mzrouter section grammar]") {
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read(
      "style irouter\nlayer m1 64 32\nlayer m2 32 64\ncontact m2c m1 m2 x\ncontact m2c m1 m2 0\ncontact m2c m1 m2",
      sink, tech);
  CHECK(styles.styles[0].contacts.empty());
  CHECK(styles.styles[0].types.size() == 2);
  CHECK(route_messages(sink) ==
        std::vector<std::string>{"Cost argument to \"contact\" line must be numeric", "Cost must be > 0",
                                 "Malformed \"contact\" line, should be:  contact layer layer1 layer2 cost"});
}

TEST_CASE("a layer that fails a check defines nothing", "[cap:router-maze][req:mzrouter section grammar]") {
  // Q-LEGACY-39 (2): Magic leaves the half-built route layer in its
  // route-type list.
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style irouter\n"
                            "layer m1 64 32\n"
                            "layer m2c 1 1\n"     // m2contact's home plane is metal1
                            "layer m2 0 64\n"     // hCost
                            "layer m2 32 x\n"     // non-numeric
                            "layer m2 32 64 1 1 -3\n"
                            "layer m2 32\n"
                            "layer m2 32 64 1 1 1 1\n"
                            "layer m2 32 64 7 8 9",
                            sink, tech);
  const route::MazeStyle& style = styles.styles[0];
  REQUIRE(style.layers.size() == 2);
  CHECK(style.types.size() == 2);
  const route::RouteLayer* m2 = layer_of(style, type_named(*tech, "metal2"));
  REQUIRE(m2 != nullptr);
  CHECK(m2->jog_cost == 7);
  CHECK(m2->hint_cost == 8);
  CHECK(m2->over_cost == 9);
  const std::string malformed =
      "Malformed \"layer\" line, should be:  \"layer name hCost vCost [jogCost [hintCost [overCost]]]\"";
  CHECK(route_messages(sink) ==
        std::vector<std::string>{"Attempt to define two route layers on same plane:  m2contact and metal1",
                                 "hCost must be > 0", "Cost arguments to \"layer\" line must be numeric",
                                 "overCost must be > 0", malformed, malformed});
}

TEST_CASE("a bare style line skips to the next style", "[cap:router-maze][req:mzrouter section grammar]") {
  // Q-LEGACY-39 (1): Magic copies the missing argument (NULL).
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style\nlayer m1 64 32\nbogus\nstyle irouter\nlayer m2 32 64", sink, tech);
  REQUIRE(styles.styles.size() == 1);
  CHECK(styles.styles[0].name == "irouter");
  CHECK(styles.styles[0].layers.size() == 1);
  CHECK(route_messages(sink) == std::vector<std::string>{"\"style\" lines need a style name"});
}

TEST_CASE("unknown keywords are reported", "[cap:router-maze][req:mzrouter section grammar]") {
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  (void)read("style irouter\nLayer m1 64 32", sink, tech);
  CHECK(route_messages(sink) == std::vector<std::string>{"Unrecognized keyword: \"Layer\""});
}

TEST_CASE("notactive marks route types inactive", "[cap:router-maze][req:mzrouter section grammar]") {
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style irouter\nlayer m1 64 32\nlayer poly 128 128\ncontact pc m1 poly 2056\n"
                            "notactive poly pcontact\nnotactive m1 metal3\nnotactive",
                            sink, tech);
  const route::MazeStyle& style = styles.styles[0];
  CHECK(layer_of(style, type_named(*tech, "metal1"))->route.active);  // its line also named metal3
  CHECK_FALSE(layer_of(style, type_named(*tech, "poly"))->route.active);
  CHECK_FALSE(contact_of(style, type_named(*tech, "pc"))->route.active);
  CHECK(route_messages(sink) == std::vector<std::string>{"Unrecognized route type: \"metal3\"",
                                                         "Bad form on mzroute notactive.",
                                                         "Usage: notactive routeType1 ... [routeTypen]"});
}

TEST_CASE("spacing statements are kept in file order", "[cap:router-maze][req:mzrouter section grammar]") {
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style irouter\nlayer m1 64 32\nlayer m2 32 64\n"
                            "spacing m1 poly 5 SUB 7\n"
                            "spacing m2 m3 NIL\n"
                            "spacing m2 nosuch 1 m1 2\n"  // an unknown type: reported, the line defines nothing
                            "spacing m2 m1 -2\n"
                            "spacing m2 m1 x\n"
                            "spacing m2 m1\n"
                            "spacing m3 m1 1",
                            sink, tech);
  const route::MazeStyle& style = styles.styles[0];
  REQUIRE(style.spacings.size() == 3);
  CHECK(style.spacings[0].type == type_named(*tech, "poly"));
  CHECK(style.spacings[0].value == 5);
  CHECK_FALSE(style.spacings[0].subcell);
  CHECK(style.spacings[1].subcell);
  CHECK(style.spacings[1].value == 7);
  CHECK(style.spacings[2].type == type_named(*tech, "metal3"));
  CHECK(style.spacings[2].value == -1);
  CHECK(style.spacings[2].route == route::RouteTypeRef{route::RouteTypeRef::Kind::layer, 1});
  CHECK(route_messages(sink) ==
        std::vector<std::string>{"Unrecognized layer (type): \"nosuch\"", "Bad spacing value: -2",
                                 "Valid values are nonnegative integers and \"NIL\".", "Bad spacing value: x",
                                 "Valid values are nonnegative integers and \"NIL\".", "Bad form on mzroute spacing.",
                                 "Usage: spacing routeType type1 spacing1 ... [typen spacingn]",
                                 "Unrecognized route type: \"m3\""});
}

TEST_CASE("width statements are kept", "[cap:router-maze][req:mzrouter section grammar]") {
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style irouter\nlayer m2 32 64\nwidth m2 4\nwidth m2 5 9\nwidth m2 0\nwidth m2 5 x\n"
                            "width m2",
                            sink, tech);
  const route::MazeStyle& style = styles.styles[0];
  REQUIRE(style.widths.size() == 2);
  CHECK(style.widths[0].width == 4);
  CHECK(style.widths[0].length == 4);
  CHECK(style.widths[1].width == 5);
  CHECK(style.widths[1].length == 9);
  CHECK(route_messages(sink) ==
        std::vector<std::string>{"Bad width: 0", "Width must be a positive integer.", "Bad minimum length: x",
                                 "Length must be a positive integer.", "Bad form on mzroute width.",
                                 "Usage: width <routeType> <width> [<length>]"});
}

TEST_CASE("the search statement and its penalty resolution", "[cap:router-maze][req:mzrouter section grammar]") {
  // Q-LEGACY-32: the penalty is stored as floor(p * 2) with exponent 1.
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style a\nsearch 200 4000 1.3\nstyle b\nsearch 0 x 2\nstyle c\nsearch 1 1", sink, tech);
  REQUIRE(styles.styles.size() == 3);
  CHECK(styles.styles[0].window_rate == 200);
  CHECK(styles.styles[0].window_width == 4000);
  CHECK(styles.styles[0].penalty.mantissa == 2);
  CHECK(styles.styles[0].penalty.exponent == 1);
  // b: a bad value leaves every search parameter at its default.
  CHECK(styles.styles[1].window_rate == 500);
  CHECK(styles.styles[1].window_width == 10000);
  CHECK(styles.styles[1].penalty.mantissa == 2048);
  CHECK(route_messages(sink) == std::vector<std::string>{"Bad rate: 0", "Rate must be a positive integer.",
                                                         "Bad width: x", "Width must be a positive integer.",
                                                         "Bad form on search.",
                                                         "Usage: search <rate> <width> <penalty>"});
}

TEST_CASE("a negative or non-numeric search penalty is rejected", "[cap:router-maze][req:mzrouter section grammar]") {
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style a\nsearch 200 4000 -1\nsearch 200 4000 abc", sink, tech);
  CHECK(styles.styles[0].window_rate == 500);
  CHECK(styles.styles[0].penalty.mantissa == 2048);
  CHECK(route_messages(sink) ==
        std::vector<std::string>{"Bad penalty value: -1.000000", "Penalty must be non-negative decimal.",
                                 "Bad penalty value: abc", "Penalty must be non-negative decimal."});
}

TEST_CASE("an out-of-range search penalty defines nothing", "[cap:router-maze][req:Maze search safety and limits]") {
  // Q-LEGACY-47: Magic converts (int)(p * 2) unchecked (undefined
  // behaviour); Hefesto rejects it in both settings and the whole statement
  // is ignored.
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style a\nsearch 200 4000 1073741824\nsearch 300 5000 1e30\nsearch 400 6000 nan\n"
                            "search 7 8 1073741760",
                            sink, tech);
  const route::MazeStyle& style = styles.styles[0];
  CHECK(style.window_rate == 7);
  CHECK(style.window_width == 8);
  CHECK(style.penalty.mantissa == 2147483520);  // the largest accepted value, 2^30 - 64, times 2
  CHECK(route_messages(sink) == std::vector<std::string>{"Penalty value out of range: 1073741824",
                                                         "Penalty value out of range: 1e30",
                                                         "Penalty value out of range: nan"});
}

TEST_CASE("route types are kept in file order per style", "[cap:router-maze][req:mzrouter section grammar]") {
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style irouter\nlayer m2 32 64 256 1\nlayer m1 64 32 256 1\nlayer poly 128 128 512 1\n"
                            "contact m2contact metal1 metal2 1024\ncontact pcontact metal1 poly 2056\n"
                            "style garouter\nlayer m2 32 64 256 1\nlayer m1 64 32 256 1\n"
                            "contact m2contact metal1 metal2 1024",
                            sink, tech);
  REQUIRE(styles.styles.size() == 2);
  const route::MazeStyle& ir = styles.styles[0];
  CHECK(ir.name == "irouter");
  REQUIRE(ir.layers.size() == 3);
  CHECK(ir.layers[0].route.type == type_named(*tech, "metal2"));
  CHECK(ir.layers[2].route.type == type_named(*tech, "poly"));
  REQUIRE(ir.contacts.size() == 2);
  CHECK(ir.contacts[0].layer1 == 1);  // metal1
  CHECK(ir.contacts[0].layer2 == 0);  // metal2
  CHECK(ir.contacts[1].cost == 2056);
  using Kind = route::RouteTypeRef::Kind;
  CHECK(ir.types == std::vector<route::RouteTypeRef>{{Kind::layer, 0},
                                                      {Kind::layer, 1},
                                                      {Kind::layer, 2},
                                                      {Kind::contact, 0},
                                                      {Kind::contact, 1}});
  CHECK(styles.styles[1].name == "garouter");
  CHECK(styles.styles[1].contacts.size() == 1);
  CHECK(route_messages(sink).empty());
}

TEST_CASE("a style starts with Magic's defaults", "[cap:router-maze][req:Style defaults and lookup]") {
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style irouter\nlayer m1 64 32", sink, tech);
  const route::MazeStyle& style = styles.styles[0];
  CHECK(style.window_rate == 500);
  CHECK(style.window_width == 10000);
  CHECK(style.penalty.mantissa == 2048);  // 1024.0
  CHECK(style.penalty.exponent == 1);
  CHECK(style.bloom_delta_cost == 1);
  CHECK(style.bounds_increment == -1);
  CHECK(style.estimate);
  CHECK(style.expand_endpoints);
  CHECK_FALSE(style.top_hints_only);
  CHECK(style.max_walk_length == -1);
  CHECK(style.verbosity == 1);
  CHECK(style.bloom_limit == 0);
  CHECK_FALSE(style.derived);
}

TEST_CASE("styles are looked up by exact name", "[cap:router-maze][req:Style defaults and lookup]") {
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style irouter\nlayer m1 64 32\nstyle garouter\nlayer m2 32 64", sink, tech);
  REQUIRE(route::find_style(styles, "irouter") != nullptr);
  CHECK(route::find_style(styles, "irouter")->layers[0].route.type == type_named(*tech, "metal1"));
  CHECK(route::find_style(styles, "garouter") == &styles.styles[1]);
  CHECK(route::find_style(styles, "irout") == nullptr);
  CHECK(route::find_style(styles, "IROUTER") == nullptr);
}

TEST_CASE("regression: of two styles with the same name the last one is found",
          "[cap:router-maze][req:Style defaults and lookup]") {
  // Magic's mzTechStyle prepends and MZFindStyle walks from the head, so the
  // later block wins (Q-LEGACY-107). find_style used to return the first.
  diag::CollectingSink sink;
  std::shared_ptr<const hefesto::tech::Technology> tech;
  const auto styles = read("style irouter\nlayer m1 64 32\nstyle irouter\nlayer m2 32 64", sink, tech);
  REQUIRE(styles.styles.size() == 2);
  CHECK(route::find_style(styles, "irouter") == &styles.styles[1]);
  CHECK(route::find_style(styles, "irouter")->layers.at(0).route.type == type_named(*tech, "metal2"));
}

TEST_CASE("a technology without an mzrouter section has no style", "[cap:router-maze][req:Style defaults and lookup]") {
  diag::CollectingSink sink;
  const auto tech = load_text(tech_text(""), sink);
  CHECK(tech->maze_styles().styles.empty());
  CHECK(route::read_mzrouter_section(nullptr, *tech, sink).styles.empty());
}
