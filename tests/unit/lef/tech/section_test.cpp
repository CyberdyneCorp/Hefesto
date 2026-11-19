// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef-tech-section: the reader of the tech `lef` section (grammar, type
// resolution, classes, aliases, defaults) and the table's hash order.
//
// Owner: LEFTECH (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "fixture.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace lef = hefesto::lef;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
using lef::LayerClass;
using lef::test::build;

#define GRAMMAR_REQ "[cap:lef-tech-section][req:Lef section line grammar]"
#define RESOLVE_REQ "[cap:lef-tech-section][req:Magic type resolution in lef lines]"
#define CLASS_REQ "[cap:lef-tech-section][req:Keyword to LEF class mapping]"
#define ALIAS_REQ "[cap:lef-tech-section][req:LEF name aliasing and canonical names]"
#define ROUTE_REQ "[cap:lef-tech-section][req:Default routing parameters]"
#define CONTACT_REQ "[cap:lef-tech-section][req:Default contact area]"

TEST_CASE("an ambiguous keyword prefix ignores the line", GRAMMAR_REQ) {
  const auto b = build("rout m1 metal1");
  CHECK(b.texts() == std::vector<std::string>{"Unknown LEF section keyword: rout.  Line ignored."});
  CHECK(b.codes() == std::vector<std::string>{"LEF-TECH-001"});
  CHECK(b.table.find("metal1") == nullptr);
  CHECK(b.table.size() == 0);
}

TEST_CASE("a line without LEF names is rejected", GRAMMAR_REQ) {
  const auto b = build("routing m1");
  CHECK(b.texts() == std::vector<std::string>{"No LEF layer names present!"});
  CHECK(b.table.size() == 0);
}

TEST_CASE("keywords match case-insensitively by unique prefix, an exact match first", GRAMMAR_REQ) {
  const auto b = build("route m1 metal1\nOB obsm2 metal2\nIG NWELL PWELL\nMasterSlice ndiff active\nnosuch m1 x");
  CHECK(b.texts() == std::vector<std::string>{"Unknown LEF section keyword: nosuch.  Line ignored."});
  REQUIRE(b.table.find("metal1") != nullptr);
  CHECK(b.table.find("metal1")->layer_class == LayerClass::route);
  REQUIRE(b.table.find("metal2") != nullptr);
  CHECK(b.table.find("metal2")->obstruction_type == b.type("obsm2"));
  CHECK(b.table.find("NWELL")->layer_class == LayerClass::ignore);
  CHECK(b.table.find("PWELL") == b.table.find("NWELL"));
  CHECK(b.table.find("active")->layer_class == LayerClass::masterslice);
  CHECK(b.table.find("x") == nullptr);
}

TEST_CASE("section messages carry the line and the lef section", GRAMMAR_REQ) {
  const auto b = build("routing m1");
  REQUIRE(b.diagnostics.size() == 1);
  const auto& d = b.diagnostics[0];
  CHECK(d.severity == hefesto::diag::Severity::error);
  const auto* where = std::get_if<hefesto::diag::SourceLocation>(&d.location);
  REQUIRE(where != nullptr);
  CHECK(where->file == "/t/lef.tech");
  CHECK(where->line > 0);
  REQUIRE(d.related.has_value());
  CHECK(d.related->name == "lef");
}

TEST_CASE("an ignore line needs only LEF names", GRAMMAR_REQ) {
  const auto b = build("ignore PWELL\nignore");
  CHECK(b.diagnostics.empty());
  REQUIRE(b.table.find("PWELL") != nullptr);
  CHECK(b.table.find("PWELL")->layer_class == LayerClass::ignore);
  CHECK(b.table.find("PWELL")->type == tech::kSpace);
  CHECK(b.table.size() == 1);
}

TEST_CASE("two types on an obstruction line: the lower number is the primary", RESOLVE_REQ) {
  const auto b = build("obstruction obsv1,m2c via1\nobstruction obsm1,m1 OBS1");
  CHECK(b.diagnostics.empty());
  const lef::LayerRecord* via1 = b.table.find("via1");
  REQUIRE(via1 != nullptr);
  CHECK(via1->layer_class == LayerClass::cut);  // m2c is a contact
  CHECK(via1->type == tech::kSpace);
  CHECK(via1->obstruction_type == b.type("m2c"));
  CHECK(via1->obstruction_types == (tech::TypeMask::of(b.type("m2c")) | tech::TypeMask::of(b.type("obsv1"))));
  // A route record keeps no secondary type (Magic's route info has none).
  const lef::LayerRecord* obs1 = b.table.find("OBS1");
  REQUIRE(obs1 != nullptr);
  CHECK(obs1->layer_class == LayerClass::route);
  CHECK(obs1->obstruction_type == b.type("m1"));
  CHECK(obs1->obstruction_types == tech::TypeMask::of(b.type("m1")));
}

TEST_CASE("two types on a routing line are rejected", RESOLVE_REQ) {
  const auto b = build("routing m1,m2 metal1");
  CHECK(b.texts() == std::vector<std::string>{"Can only define multiple types for via obstruction layers."});
  CHECK(b.table.find("metal1") == nullptr);
}

TEST_CASE("an unknown magic type rejects the line", RESOLVE_REQ) {
  const auto b = build("routing nosuch metal1");
  REQUIRE_FALSE(b.diagnostics.empty());
  CHECK(b.diagnostics.back().text == "Bad magic layer type \"nosuch\" in LEF layer definition.");
  CHECK(b.diagnostics.back().code == "LEF-TECH-003");
  CHECK(b.diagnostics.front().code == "TECH-LINE-001");  // DBTechNoisyNameMask's own message
  CHECK(b.table.find("metal1") == nullptr);
}

TEST_CASE("three or more types per mode", RESOLVE_REQ) {
  SECTION("magic-compat ON counts them as two, as Magic's loop does") {
    const auto b = build("obstruction obsm1,obsm2,obsv1 OBSX\nrouting m1,m2,m5 metal1", true);
    CHECK(b.texts() == std::vector<std::string>{"Can only define multiple types for via obstruction layers."});
    REQUIRE(b.table.find("OBSX") != nullptr);
    CHECK(b.table.find("OBSX")->obstruction_type == b.type("obsm1"));
    CHECK(b.table.find("metal1") == nullptr);
  }
  SECTION("magic-compat OFF rejects them") {
    const auto b = build("obstruction obsm1,obsm2,obsv1 OBSX\nrouting m1,m2,m5 metal1", false);
    CHECK(b.texts() == std::vector<std::string>{"Too many types in LEF layer definition.",
                                                "Too many types in LEF layer definition."});
    CHECK(b.codes() == std::vector<std::string>{"LEF-TECH-005", "LEF-TECH-005"});
    CHECK(b.table.size() == 0);
  }
}

TEST_CASE("the class of a new record follows the keyword and the contact type", CLASS_REQ) {
  struct Row {
    std::string line;
    LayerClass expected;
    std::string message;
  };
  const Row row = GENERATE(
      Row{"layer m2c via1", LayerClass::cut, ""}, Row{"layer m1 via1", LayerClass::route, ""},
      Row{"route m1 via1", LayerClass::route, ""}, Row{"routing m1 via1", LayerClass::route, ""},
      Row{"routing m2c via1", LayerClass::route, "Attempt to define cut type m2contact as routing."},
      Row{"cut m2c via1", LayerClass::cut, ""},
      Row{"cut m1 via1", LayerClass::cut, "Attempt to define non-cut type metal1 as a cut."},
      Row{"contact pc via1", LayerClass::cut, ""},
      Row{"contact m1 via1", LayerClass::cut, "Attempt to define non-cut type metal1 as a cut."},
      Row{"obstruction m2c via1", LayerClass::cut, ""}, Row{"obstruction m1 via1", LayerClass::route, ""},
      Row{"masterslice ndiff via1", LayerClass::masterslice, ""},
      Row{"masterslice pc via1", LayerClass::masterslice, "Attempt to define cut type polycontact as masterslice."},
      Row{"overlap m1 via1", LayerClass::overlap, ""},
      Row{"overlap m2c via1", LayerClass::overlap, "Attempt to define cut type m2contact as overlap."},
      Row{"bound m1 via1", LayerClass::bound, ""},
      Row{"bound m2c via1", LayerClass::bound, "Attempt to define cut type m2contact as bound."},
      Row{"ignore via1", LayerClass::ignore, ""});
  INFO(row.line);
  const auto b = build(row.line);
  REQUIRE(b.table.find("via1") != nullptr);
  CHECK(b.table.find("via1")->layer_class == row.expected);
  if (row.message.empty()) {
    CHECK(b.diagnostics.empty());
  } else {
    CHECK(b.texts() == std::vector<std::string>{row.message});
  }
}

TEST_CASE("normal and obstruction types of new records", CLASS_REQ) {
  const auto b = build("layer m2c via1\nobstruction m1 OBS_M1\nrouting m2c via2\nignore NWELL");
  CHECK(b.table.find("via1")->type == b.type("m2c"));
  CHECK(b.table.find("via1")->obstruction_type == tech::kSpace);
  CHECK(b.table.find("via1")->contact_area == db::Rect{0, 0, 0, 0});  // cut flavor
  const lef::LayerRecord& obs = *b.table.find("OBS_M1");
  CHECK(obs.layer_class == LayerClass::route);
  CHECK(obs.type == tech::kSpace);
  CHECK(obs.obstruction_type == b.type("m1"));
  CHECK(b.texts() == std::vector<std::string>{"Attempt to define cut type m2contact as routing."});
  CHECK(b.table.find("via2")->layer_class == LayerClass::route);
  CHECK(b.table.find("via2")->type == b.type("m2c"));
  CHECK(b.table.find("NWELL")->type == tech::kSpace);
  CHECK(b.table.find("NWELL")->obstruction_types.empty());
}

TEST_CASE("a routing layer and an obstruction share one LEF name", ALIAS_REQ) {
  const auto b = build("routing m1 metal1\nobstruction obsm1 metal1");
  CHECK(b.diagnostics.empty());
  const lef::LayerRecord& r = *b.table.find("metal1");
  CHECK(r.type == b.type("m1"));
  CHECK(r.obstruction_type == b.type("obsm1"));
  CHECK(r.canonical_name == "metal1");
  CHECK(r.layer_class == LayerClass::route);
}

TEST_CASE("a second obstruction line on a cut sets its secondary type", ALIAS_REQ) {
  const auto b = build("cut m2c via1\nobstruction obsm1,obsm2 via1");
  CHECK(b.diagnostics.empty());
  const lef::LayerRecord& r = *b.table.find("via1");
  CHECK(r.layer_class == LayerClass::cut);
  CHECK(r.type == b.type("m2c"));
  CHECK(r.obstruction_type == b.type("obsm1"));
  CHECK(r.obstruction_types == (tech::TypeMask::of(b.type("obsm1")) | tech::TypeMask::of(b.type("obsm2"))));
}

TEST_CASE("a duplicate mapping is reported and the name keeps its type", ALIAS_REQ) {
  const auto b = build("routing m1 metal1\nrouting m2 metal1");
  CHECK(b.texts() == std::vector<std::string>{"LEF name metal1 already used for magic type metal1"});
  CHECK(b.codes() == std::vector<std::string>{"LEF-TECH-008"});
  CHECK(b.table.find("metal1")->type == b.type("m1"));
}

TEST_CASE("the canonical name is the first new name of the line", ALIAS_REQ) {
  const auto b = build("routing m1 metal1\nrouting m2 metal1 M1 MET1");
  CHECK(b.texts() == std::vector<std::string>{"LEF name metal1 already used for magic type metal1"});
  const lef::LayerRecord* m1 = b.table.find("M1");
  REQUIRE(m1 != nullptr);
  CHECK(m1 == b.table.find("MET1"));
  CHECK(m1->canonical_name == "M1");
  CHECK(m1->type == b.type("m2"));
  CHECK(b.table.find("metal1")->canonical_name == "metal1");
  CHECK(b.table.size() == 3);
}

TEST_CASE("LEF names are case-sensitive keys", ALIAS_REQ) {
  const auto b = build("routing m1 metal1\nrouting m2 METAL1");
  CHECK(b.diagnostics.empty());
  CHECK(b.table.find("metal1") != b.table.find("METAL1"));
  CHECK(b.table.find("METAL1")->type == b.type("m2"));
}

TEST_CASE("a name bound to an ignore record is silently kept", ALIAS_REQ) {
  const auto b = build("ignore NWELL\nrouting m1 NWELL\nobstruction obsm1 NWELL");
  CHECK(b.diagnostics.empty());
  CHECK(b.table.find("NWELL")->layer_class == LayerClass::ignore);
  CHECK(b.table.find("NWELL")->type == tech::kSpace);
}

TEST_CASE("regression: an ignore line never rebinds a mapped name", ALIAS_REQ) {
  // Magic reads LefTechLine's uninitialized isObstruction and mtype here
  // (undefined behaviour, fixed in both modes): the name is reported and kept.
  const bool compat = GENERATE(true, false);
  const auto b = build("obstruction obsm1 OBS1\nignore OBS1", compat);
  CHECK(b.texts() == std::vector<std::string>{"LEF name OBS1 already used for magic type obsm1"});
  CHECK(b.table.find("OBS1")->layer_class == LayerClass::route);
  CHECK(b.table.find("OBS1")->type == tech::kSpace);
  CHECK(b.table.find("OBS1")->obstruction_type == b.type("obsm1"));
}

TEST_CASE("regression: a duplicate obstruction-only name names its obstruction type", ALIAS_REQ) {
  // Magic prints DBTypeLongNameTbl[-1] here (out of bounds; fixed in both modes).
  const auto b = build("obstruction obsm1 OBS1\nobstruction obsm2 OBS1");
  CHECK(b.texts() == std::vector<std::string>{"LEF name OBS1 already used for magic type obsm1"});
  CHECK(b.table.find("OBS1")->obstruction_type == b.type("obsm1"));
}

TEST_CASE("enumeration follows Magic's string hash order", ALIAS_REQ) {
  // The M1 hash model (hefesto-output-determinism "Magic string hash
  // table"): HashInit(&LefInfo, 32, ...), each name pushed at the head of its
  // bucket in the order the lines name them; buckets visited in order.
  const std::vector<std::string> names = {"metal1", "MET1", "m1",   "metal2", "MET2", "via",  "VIA1",
                                          "via12",  "cont2", "OBS1", "NWELL", "PWELL", "li1", "mcon"};
  const auto b = build(
      "routing m1 metal1 MET1 m1\nrouting m2 metal2 MET2\ncut m2c via VIA1 via12 cont2\nobstruction obsm1 OBS1\n"
      "ignore NWELL PWELL\nrouting m5 li1\ncontact pc mcon");
  CHECK(b.diagnostics.empty());
  std::vector<std::string> visited;
  b.table.enumerate([&visited](std::string_view name, const lef::LayerRecord&) { visited.emplace_back(name); });

  const std::size_t size = hefesto::util::magic_table_size(32);
  const unsigned shift = hefesto::util::magic_table_shift(size);
  std::vector<std::vector<std::string>> buckets(size);
  for (const std::string& name : names) {
    auto& chain = buckets[hefesto::util::magic_bucket(hefesto::util::magic_string_hash(name), shift, size)];
    chain.insert(chain.begin(), name);
  }
  std::vector<std::string> expected;
  for (const auto& chain : buckets) {
    expected.insert(expected.end(), chain.begin(), chain.end());
  }
  CHECK(visited == expected);
  CHECK(b.table.size() == names.size());
}

TEST_CASE("a routing layer without DRC rules gets width 3 and spacing 4", ROUTE_REQ) {
  const bool compat = GENERATE(true, false);
  const auto b = build("routing m5 metal5", compat);
  const lef::LayerRecord& r = *b.table.find("metal5");
  CHECK(r.width == 3);
  CHECK(r.spacing == 4);
  CHECK(r.pitch == 0);
  CHECK(r.direction == lef::RouteDirection::horizontal);
}

TEST_CASE("a routing layer with DRC rules is left to the fill, per mode", ROUTE_REQ) {
  SECTION("magic-compat ON keeps Magic's -1 sentinels") {
    const auto b = build("routing m1 metal1\nobstruction m1 OBS_M1", true);
    CHECK(b.table.find("metal1")->width == -1);
    CHECK(b.table.find("metal1")->spacing == -1);
    CHECK(b.table.find("OBS_M1")->width == -1);  // the obstruction type's rules
  }
  SECTION("magic-compat OFF leaves them unset") {
    const auto b = build("routing m1 metal1", false);
    CHECK_FALSE(b.table.find("metal1")->width.has_value());
    CHECK_FALSE(b.table.find("metal1")->spacing.has_value());
    CHECK(b.table.find("metal1")->pitch == 0);
  }
  SECTION("without DRC rules every default is 0, so 3 and 4") {
    const auto b = build("routing m1 metal1", true, "drc\nend\n");
    CHECK(b.table.find("metal1")->width == 3);
    CHECK(b.table.find("metal1")->spacing == 4);
  }
}

TEST_CASE("a cut-flavor entry has a null contact area", CONTACT_REQ) {
  const auto b = build("cut m2c via1");
  const lef::LayerRecord& r = *b.table.find("via1");
  CHECK(r.contact_area == db::Rect{0, 0, 0, 0});
  CHECK(r.contact_area->is_null());
  CHECK(r.obstruction_types.empty());
}

TEST_CASE("a contact-flavor entry waits for the fill, per mode", CONTACT_REQ) {
  SECTION("magic-compat ON: the sentinel area, which is null") {
    const auto b = build("contact pc cont1\nobstruction m2c OBSV", true);
    CHECK(b.table.find("cont1")->contact_area == db::Rect{-1, -1, -1, -1});
    CHECK(b.table.find("cont1")->contact_area->is_null());
    CHECK(b.table.find("OBSV")->contact_area == db::Rect{-1, -1, -1, -1});
  }
  SECTION("magic-compat OFF: unset") {
    const auto b = build("contact pc cont1", false);
    CHECK_FALSE(b.table.find("cont1")->contact_area.has_value());
  }
}

TEST_CASE("the scaffold overload reads with no rules and magic-compat ON", ROUTE_REQ) {
  const auto b = build("");
  hefesto::diag::CollectingSink sink;
  const tech::RawSection section{"lef", {tech::RawLine{{"routing", "m1", "metal1"}, "/t/x.tech", 3},
                                         tech::RawLine{{"contact", "pc", "cont1"}, "/t/x.tech", 4}}};
  const lef::LayerTable table = lef::LayerTable::from_section(section, *b.tech, sink);
  CHECK(sink.take().empty());
  CHECK(table.find("metal1")->width == 3);
  CHECK(table.find("cont1")->contact_area == db::Rect{-1, -1, -1, -1});
}
