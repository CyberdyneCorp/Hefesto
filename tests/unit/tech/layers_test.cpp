// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Capability tech-layers-and-planes: built-in planes and types, the planes,
// types and aliases sections, locked types, name lookup, the type-list
// language, derived tables and name reporting.

#include <catch2/catch_test_macros.hpp>

#include "tech_fixture.hpp"

namespace tech = hefesto::tech;
using tech::LookupError;
using tech::NameForm;
using tech::test::load;
using tech::test::mask_of;
using tech::test::planes_of;
using tech::test::skeleton;
using tech::test::tiny;
using tech::test::tiny_tech;
using tech::test::with;

namespace {

constexpr tech::PlaneId kWell = 6;
constexpr tech::PlaneId kActive = 7;
constexpr tech::PlaneId kMetal1 = 8;
constexpr tech::PlaneId kMetal2 = 9;

tech::TypeMask list(std::string_view text) {
  auto parsed = tiny_tech()->parse_type_list(text);
  REQUIRE(parsed.ok());
  return parsed.value();
}

}  // namespace

TEST_CASE("six built-in planes; technology planes from 6", "[cap:tech-layers-and-planes][req:Built-in planes]") {
  const auto minimum = tech::builtin_technology();
  REQUIRE(minimum->plane_count() == 6);
  CHECK(minimum->plane_name(0) == "router");
  CHECK(minimum->plane_name(1) == "designRuleCheck");
  CHECK(minimum->plane_name(2) == "designRuleError");
  CHECK(minimum->plane_name(5) == "rhint");
  const auto run = load(skeleton("well,w\n", ""));
  REQUIRE(run.ok());
  CHECK(*run.t().find_plane("well").id == 6);
  CHECK(run.t().plane_count() == 7);
}

TEST_CASE("planes lines: long name first, shortest is the short name", "[cap:tech-layers-and-planes][req:planes section]") {
  auto run = load(skeleton("well\nactive,diffusion,polysilicon,a\n", ""));
  REQUIRE(run.ok());
  CHECK(run.t().plane_name(7) == "active");
  CHECK(run.t().plane_name(7, NameForm::short_name) == "a");
  run = load(skeleton("well,w\nactive,w\nmetal1\n", ""));
  CHECK(run.says("Duplicate name: w"));
  CHECK_FALSE(run.ok());
  run = load(skeleton("well active\n", ""));
  CHECK(run.says("Line must contain names for plane"));
  std::string many;
  for (int i = 0; i < 59; ++i) {
    many += "p" + std::to_string(i) + "\n";
  }
  CHECK(load(skeleton(many, "")).says("Too many tile planes (max=64)"));
}

TEST_CASE("nine built-in types with short names", "[cap:tech-layers-and-planes][req:Built-in tile types]") {
  const auto t = tiny_tech();
  CHECK(*t->find_type("EP").id == 3);
  CHECK(t->type_name(3) == "error_p");
  CHECK(t->type_name(7, NameForm::short_name) == "f");
  CHECK(*t->home_plane(tech::kRotate) == tech::kPlaneRHint);
  CHECK_FALSE(t->home_plane(tech::kSpace).has_value());
  CHECK(t->find_type("m").error == LookupError::ambiguous);  // magnet, mag, m1, m2, m2c, metal1, ...
}

TEST_CASE("types lines number types from 9 with home planes", "[cap:tech-layers-and-planes][req:types section]") {
  const auto t = tiny_tech();
  CHECK(t->type_name(9) == "nwell");
  CHECK(t->type_name(9, NameForm::short_name) == "nw");
  CHECK(*t->home_plane(9) == kWell);
  CHECK(t->type_name(10) == "polysilicon");
  CHECK(t->type_name(10, NameForm::short_name) == "p");
  CHECK(*t->home_plane(10) == kActive);

  auto run = load(with(tiny(), "    -well   pwell,pw\n", "    -well   pwell,pw\n    metal3 metal3,m3\n    metal2 foo\n"));
  CHECK(run.says("Unrecognized plane name \"metal3\""));
  CHECK_FALSE(run.ok());
  run = load(with(tiny(), "    -well   pwell,pw\n", "    -well   pwell,pw\n    lonely\n"));
  CHECK(run.says("Line must contain at least 2 fields"));
  CHECK(run.ok());  // not rejected
  run = load(with(tiny(), "    -well   pwell,pw\n", "    -well   pwell,pw\n    m metal3\n"));
  CHECK(run.says("Ambiguous plane name \"m\""));
}

TEST_CASE("a rejected types line adds no names (Q-TECH-11)", "[cap:tech-layers-and-planes][req:types section]") {
  const auto run = load(skeleton("a\n", "a x,y\nnoplane z,w\n"));
  CHECK_FALSE(run.ok());
  // Same file but valid, to inspect: z must not exist once its line was rejected.
  tech::TechnologyBuilder builder;
  REQUIRE(builder.add_plane("a").ok());
  REQUIRE(builder.add_type("a", "x,y").ok());
  CHECK_FALSE(builder.add_type("noplane", "z,w").ok());
  CHECK_FALSE(builder.add_type("a", "q,x").ok());  // duplicate x
  REQUIRE(builder.add_type("a", "q").ok());
  // finalize() reports the earlier rejected lines.
  CHECK_FALSE(builder.finalize().ok());
}

TEST_CASE("locked types are not in the active set", "[cap:tech-layers-and-planes][req:Locked types]") {
  const auto t = tiny_tech();
  const auto pwell = tech::test::type_of(*t, "pwell");
  CHECK(t->locked_by_default(pwell));
  CHECK_FALSE(t->active_types().test(pwell));
  CHECK(t->active_types().test(tech::test::type_of(*t, "nwell")));
  CHECK(t->active_types().test(19));  // stacked pc+m2c: components active
  CHECK(t->active_types().test(tech::kMagnet));
  CHECK_FALSE(t->active_types().test(tech::kErrorP));
  CHECK(t->tech_active_types() == t->active_types());
}

TEST_CASE("unique-prefix lookup", "[cap:tech-layers-and-planes][req:Name lookup by unique prefix]") {
  const auto t = tiny_tech();
  CHECK(*t->find_type("p").id == 10);
  CHECK(t->find_type("po").error == LookupError::ambiguous);
  CHECK(t->find_type("ndif").error == LookupError::ambiguous);
  CHECK_FALSE(t->find_type("cp").ok());
  CHECK(t->find_type("cp").error == LookupError::unknown);
  CHECK(*t->find_type("CP").id == 1);
  CHECK(*t->find_type("polys").id == 10);
  CHECK(t->find_type("").error == LookupError::ambiguous);
  CHECK(*t->find_plane("metal2").id == kMetal2);
  CHECK(t->find_plane("metal").error == LookupError::ambiguous);
  // magic-compat OFF: candidates that all denote one type resolve (Q-TECH-14).
  const auto off = load(tiny(), false);
  CHECK(*off.t().find_type("ndif").id == 11);
  CHECK(off.t().find_type("po").error == LookupError::ambiguous);
}

TEST_CASE("single type-name resolution with plane qualifiers", "[cap:tech-layers-and-planes][req:Single type-name resolution]") {
  const auto t = tiny_tech();
  CHECK(*t->find_type("pc/metal1").id == 15);
  CHECK(*t->find_type("pc/active").id == 15);
  CHECK_FALSE(t->find_type("poly/metal1").ok());
  CHECK_FALSE(t->find_type("pc/nosuch").ok());
  CHECK(*t->find_type("pc+m2c/metal2").id == 19);
}

TEST_CASE("aliases: single-type names, multi-type sets, shadowing", "[cap:tech-layers-and-planes][req:Type aliases]") {
  const auto t = tiny_tech();
  CHECK(list("allm1") == mask_of({13, 15, 16, 17, 19}));
  auto run = load(with(tiny(), "    allm1  *m1\n", "    allm1  *m1\n    diff ndiff\n    poly ndiff\n    allm1 *m1\n"));
  REQUIRE(run.ok());
  CHECK(*run.t().find_type("diff").id == 11);
  CHECK(run.t().type_names(11) == std::vector<std::string>{"ndiff", "ndiffusion"});  // the alias is not listed
  CHECK(run.says("Type alias \"poly\" shadows a defined type"));
  CHECK(run.says("Type \"allm1\" is already defined and cannot be an alias"));
  // types-section alias lines
  run = load(with(tiny(), "    -well   pwell,pw\n", "    -well   pwell,pw\n    alias x *m1\n    alias y\n"));
  CHECK(run.says("contains the wildcard character \"*\" (alias ignored)"));
  CHECK(run.says("Line must contain at least 2 fields"));  // Q-TECH-12
  CHECK(run.ok());
  // Single-name resolution consults aliases only when the name lookup fails.
  run = load(with(tiny(), "    allm1  *m1\n", "    allm1  *m1\n    metals metal1,metal2\n    onem metal1,space\n"));
  REQUIRE(run.ok());
  CHECK(run.t().find_type("metals").error == LookupError::ambiguous);
  CHECK(*run.t().find_type("onem").id == 13);
}

TEST_CASE("type-list language", "[cap:tech-layers-and-planes][req:Type-list language]") {
  const auto t = tiny_tech();
  CHECK(list("m1") == mask_of({13}));
  CHECK(list("pc") == mask_of({15, 19}));
  CHECK(list("*m1") == mask_of({13, 15, 16, 17, 19}));
  CHECK(list("~(pc,ndc)/active") == mask_of({0, 10, 11, 12}));
  CHECK(list("0").empty());
  CHECK(list("0,poly") == mask_of({10}));  // "0" is an unknown name here (Q-TECH-15)
  CHECK(list("(poly,ndiff") == mask_of({10, 11}));
  // magic-compat OFF: 0 is the empty set anywhere.
  const auto off = load(tiny(), false);
  CHECK(off.t().parse_type_list("0,poly").value() == mask_of({10}));
}

TEST_CASE("type lists build a plane set", "[cap:tech-layers-and-planes][req:Type-list language]") {
  // The plane set is visible through the connect/paint rules; check it via the connectivity of a qualified list.
  const auto run = load(with(tiny(), "    compose nfet poly ndiff\n",
                             "    compose nfet poly ndiff\n    paint pc nwell space\n"));
  REQUIRE(run.ok());
  // `space` has P = every plane but router: the rule writes space on every tech plane of pc.
  CHECK(run.t().paint_result(kActive, 15, 9) == 0);
  CHECK(run.t().paint_result(kMetal1, 15, 9) == 0);
}

TEST_CASE("derived type and plane tables", "[cap:tech-layers-and-planes][req:Derived type and plane tables]") {
  const auto t = tiny_tech();
  CHECK(t->plane_types(kMetal1) == mask_of({0, 13, 15, 16, 17, 19}));
  CHECK(t->home_plane_types(kMetal1) == mask_of({13, 17, 19}));
  CHECK(t->plane_types(tech::kPlaneRouter).empty());
  CHECK(t->type_planes(0) == ~tech::plane_bit(0));
  CHECK(t->type_planes(15) == planes_of({kActive, kMetal1}));
  CHECK(t->type_planes(19) == planes_of({kActive, kMetal1, kMetal2}));
  CHECK(t->type_planes(tech::kErrorP) == planes_of({tech::kPlaneDrcError}));
}

TEST_CASE("short and long names", "[cap:tech-layers-and-planes][req:Short and long name reporting]") {
  const auto t = tiny_tech();
  CHECK(t->type_name(17) == "m2contact");
  CHECK(t->type_name(17, NameForm::short_name) == "m2c");
  CHECK(t->type_name(19, NameForm::short_name) == "pc+m2c");
  CHECK(t->type_name(11, NameForm::short_name) == "ndiff");
  CHECK(t->plane_name(8, NameForm::short_name) == "m1");
}
