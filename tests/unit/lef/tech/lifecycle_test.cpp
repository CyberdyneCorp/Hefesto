// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef-tech-section: the table through `tech load` and `scalegrid` in a
// command session: the post-load scaling and fill per grid ratio and mode,
// the rescale hook, and the table's lifecycle.
//
// Owner: LEFTECH (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <optional>
#include <string>

#include "fixture.hpp"

namespace lef = hefesto::lef;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
using lef::test::Session;

#define FILL_REQ "[cap:lef-tech-section][req:Filling defaults after grid scaling]"
#define SCALE_REQ "[cap:lef-tech-section][req:Scaling the LEF table]"
#define LIFE_REQ "[cap:lef-tech-section][req:LEF table lifecycle]"

namespace {

constexpr std::string_view kLines = "routing m1 metal1\nrouting m5 metal5\ncontact pc cont1\ncut m2c via1";

std::optional<db::Coord> some(db::Coord v) {
  return v;
}

db::Rect square(db::Coord w) {
  return db::Rect{-w, -w, w, w};
}

// Loads kLines at internal:lambda = n:d.
void load_at(Session& s, std::int64_t n, std::int64_t d) {
  s.editor.grid = tech::GridScale{n, d};
  s.load(kLines);
  REQUIRE(s.editor.grid == tech::GridScale{n, d});
}

}  // namespace

TEST_CASE("1:1 grid keeps the sentinels with magic-compat ON", FILL_REQ) {
  Session s(true);
  load_at(s, 1, 1);
  CHECK(s.record("metal1").width == -1);
  CHECK(s.record("metal1").spacing == -1);
  CHECK(s.record("metal5").width == 3);
  CHECK(s.record("metal5").spacing == 4);
  CHECK(s.record("cont1").contact_area == db::Rect{-1, -1, -1, -1});
  CHECK(s.record("via1").contact_area == db::Rect{0, 0, -1, -1});
}

TEST_CASE("1:1 grid fills every default with magic-compat OFF", FILL_REQ) {
  Session s(false);
  load_at(s, 1, 1);
  CHECK(s.record("metal1").width == s.default_width("m1"));
  CHECK(s.record("metal1").spacing == s.default_spacing("m1"));
  CHECK(s.record("metal1").width == 3);
  CHECK(s.record("metal1").spacing == 4);
  CHECK(s.record("cont1").contact_area == square(s.default_width("pc")));
}

TEST_CASE("3:2 grid fills the route width with the default spacing (ON)", FILL_REQ) {
  Session s(true);
  load_at(s, 3, 2);
  // trunc(-1 * 3 / 2) = -1: filled; the spacing overwrites the width.
  CHECK(s.record("metal1").width == s.default_spacing("m1"));
  CHECK(s.record("metal1").spacing == -1);
  CHECK(s.record("metal5").width == 4);  // 3 * 3 / 2
  CHECK(s.record("metal5").spacing == 6);
  // floor(-1 * 3 / 2) = -2: no longer the sentinel, never filled.
  CHECK(s.record("cont1").contact_area == db::Rect{-2, -2, -2, -2});
}

TEST_CASE("3:2 grid fills width and spacing separately (OFF)", FILL_REQ) {
  Session s(false);
  load_at(s, 3, 2);
  CHECK(s.record("metal1").width == s.default_width("m1"));
  CHECK(s.record("metal1").spacing == s.default_spacing("m1"));
  CHECK(s.record("metal5").width == 4);
  CHECK(s.record("cont1").contact_area == square(s.default_width("pc")));
}

TEST_CASE("2:1 grid leaves the scaled sentinel (ON) and fills it (OFF)", FILL_REQ) {
  const bool compat = GENERATE(true, false);
  Session s(compat);
  load_at(s, 2, 1);
  if (compat) {
    CHECK(s.record("metal1").width == -2);
    CHECK(s.record("metal1").spacing == -2);
    CHECK(s.record("cont1").contact_area == db::Rect{-2, -2, -2, -2});
  } else {
    CHECK(s.record("metal1").width == s.default_width("m1"));
    CHECK(s.record("metal1").spacing == s.default_spacing("m1"));
    CHECK(s.record("cont1").contact_area == square(s.default_width("pc")));
  }
  CHECK(s.record("metal5").width == 6);
  CHECK(s.record("metal5").spacing == 8);
}

TEST_CASE("1:2 grid fills the contact area and zeroes the route sentinel (ON)", FILL_REQ) {
  const bool compat = GENERATE(true, false);
  Session s(compat);
  load_at(s, 1, 2);
  const db::Coord pc = s.default_width("pc");
  REQUIRE(pc == 1);  // 2 lambda at 1 internal unit per 2 lambda
  CHECK(s.record("cont1").contact_area == square(pc));
  if (compat) {
    CHECK(s.record("metal1").width == 0);  // trunc(-1 / 2): not filled
    CHECK(s.record("metal1").spacing == 0);
  } else {
    CHECK(s.record("metal1").width == s.default_width("m1"));
    CHECK(s.record("metal1").spacing == s.default_spacing("m1"));
  }
  CHECK(s.record("metal5").width == 1);
  CHECK(s.record("metal5").spacing == 2);
}

TEST_CASE("scalegrid 1 2 rescales the table and never refills (LEFDEF-004)", SCALE_REQ) {
  const bool compat = GENERATE(true, false);
  Session s(compat);
  load_at(s, 1, 1);
  REQUIRE(s.run("scalegrid 1 2").ok());
  REQUIRE(s.editor.grid == tech::GridScale{2, 1});
  CHECK(s.record("metal5").width == 6);
  CHECK(s.record("metal5").spacing == 8);
  CHECK(s.record("metal5").pitch == 0);
  if (compat) {
    CHECK(s.record("metal1").width == -2);
    CHECK(s.record("metal1").spacing == -2);
    CHECK(s.record("cont1").contact_area == db::Rect{-2, -2, -2, -2});
  } else {
    CHECK(s.record("metal1").width == 6);
    CHECK(s.record("metal1").spacing == 8);
    CHECK(s.record("cont1").contact_area == db::Rect{-4, -4, 4, 4});
  }
  REQUIRE(s.run("scalegrid 2 1").ok());
  CHECK(s.record("metal5").width == 3);
  CHECK(s.record("metal1").width == (compat ? some(-1) : some(3)));
}

TEST_CASE("the OFF refill after scalegrid fills what a reader left unset", SCALE_REQ) {
  Session s(false);
  load_at(s, 1, 1);
  lef::LayerRecord added;
  added.layer_class = lef::LayerClass::route;
  added.type = s.type("m1");
  s.editor.lefdef.layers.add("METAL1X", added);
  REQUIRE(s.run("scalegrid 1 2").ok());
  CHECK(s.record("METAL1X").width == s.default_width("m1"));  // from the rescaled rules
  CHECK(s.record("METAL1X").width == 6);
}

TEST_CASE("the table starts empty before any technology load", LIFE_REQ) {
  hefesto::editor::EditorState editor;
  CHECK(editor.lefdef.layers.size() == 0);
  CHECK(editor.lefdef.layers.find("metal1") == nullptr);
}

TEST_CASE("a technology reload clears LEF-defined vias and rules", LIFE_REQ) {
  Session s;
  load_at(s, 1, 1);
  lef::LayerRecord via;
  via.layer_class = lef::LayerClass::cut;
  via.type = s.type("m2c");
  s.editor.lefdef.layers.add("M1M2_PR", via);
  s.editor.lefdef.vias.insert("M1M2_PR", lef::ViaDefinition{});
  s.editor.lefdef.rules.insert("NDR", lef::NonDefaultRule{});
  REQUIRE(s.run("box 0 0 10 10").ok());  // other commands keep the table
  CHECK(s.editor.lefdef.layers.find("M1M2_PR") != nullptr);
  REQUIRE(s.run("tech load /t/lef.tech -noprompt").ok());
  CHECK(s.editor.lefdef.layers.find("M1M2_PR") == nullptr);
  CHECK(s.editor.lefdef.vias.size() == 0);
  CHECK(s.editor.lefdef.rules.size() == 0);
  CHECK(s.editor.lefdef.layers.find("metal1") != nullptr);
}

TEST_CASE("a failed technology load keeps the previous table", LIFE_REQ) {
  Session s;
  load_at(s, 1, 1);
  s.file("/t/bad.tech", "tech\n    format 35\n    bad\nend\n");
  CHECK_FALSE(s.run("tech load /t/bad.tech -noprompt").ok());
  CHECK(s.editor.lefdef.layers.find("metal1") != nullptr);
}

TEST_CASE("lef section messages are printed while the technology is read", LIFE_REQ) {
  Session s;
  s.load("rout m1 metal1\nrouting m1 metal1");
  bool found = false;
  for (const auto& d : s.messages) {
    if (d.code == "LEF-TECH-001") {
      found = true;
      CHECK(d.text == "Unknown LEF section keyword: rout.  Line ignored.");
    }
  }
  CHECK(found);
  CHECK(s.record("metal1").type == s.type("m1"));
}
