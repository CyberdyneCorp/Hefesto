// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF write: the sections read from the LEF tables (capability def-write
// "Magic-to-LEF layer map", "VIAS section content", "NONDEFAULTRULES
// section", "BLOCKAGES section", "Table enumeration order", "Session state
// after writing"; tasks.md 8.2, 8.5; Q-LEFDEF-91, 92, 98, 99, 100, 101).
//
// Owner: DEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/util/magic_hash_table.hpp"
#include "write_fixture.hpp"

namespace def_write_test {
namespace {

namespace lef = hefesto::lef;

constexpr std::string_view kExtHeader =
    "timestamp 0\nversion 8.3\ntech lefdef\nstyle lefdef\nscale 1000 1000 1\n"
    "resistclasses 1000 500 100 50 30 20\n";

/// A top cell with one wide metal1 port net (rule metal1_width_80).
db::CellDef& wide_net(Session& s, std::string_view name) {
  s.set_widths();
  db::CellDef& top = s.cell(name);
  s.paint(top, "metal1", db::Rect{0, 0, 100, 8});
  s.file("/t/" + std::string{name} + ".ext",
         std::string{kExtHeader} + "port \"A\" 1 0 0 100 8 m1\nnode \"A\" 1 6 0 0 m1 0 0 0 0 0 0 0 0 0 0 0 0\n");
  return top;
}

TEST_CASE("obstruction types map to their route layer", "[cap:def-write][req:Magic-to-LEF layer map]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "obsm1", db::Rect{0, 0, 20, 10});
  REQUIRE(s.write(top, "").ok());
  CHECK(section(s.read("top.def"), "BLOCKAGES") ==
        "BLOCKAGES 1 ;\n   - LAYER metal1\n      RECT ( 0 0 ) ( 200 100 ) ;\nEND BLOCKAGES\n\n");
}

TEST_CASE("no obstruction paint and no floating net: no BLOCKAGES", "[cap:def-write][req:BLOCKAGES section]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 0, 20, 10});
  REQUIRE(s.write(top, "").ok());
  CHECK(s.read("top.def").find("BLOCKAGES") == std::string::npos);
}

// Regression (Q-LEFDEF-101): Magic's blockage collector takes DBSrConnect's
// arguments in the wrong order and crashes on the first tile of a net that
// is neither a port nor special.
TEST_CASE("a floating net's geometry is a blockage, in both modes", "[cap:def-write][req:BLOCKAGES section]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    db::CellDef& top = s.cell("top");
    s.paint(top, "metal2", db::Rect{0, 0, 50, 4});
    s.file("/t/top.ext", std::string{kExtHeader} + "node \"m2_0_0#\" 1 6 0 0 m2 0 0 0 0 0 0 0 0 0 0 0 0\n");
    REQUIRE(s.write(top, "").ok());
    CHECK(section(s.read("top.def"), "BLOCKAGES") ==
          "BLOCKAGES 1 ;\n   - LAYER metal2\n      RECT ( 0 0 ) ( 500 40 ) ;\nEND BLOCKAGES\n\n");
    const diag::Diagnostic* note = s.message("DEF-WRITE-012");
    REQUIRE(note != nullptr);
    CHECK(note->native_only);
  }
}

TEST_CASE("two identical contact regions share one generated via", "[cap:def-write][req:VIAS section content]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "via1", db::Rect{10, 10, 36, 36});
  s.paint(top, "via1", db::Rect{100, 10, 126, 36});
  REQUIRE(s.write(top, "").ok());
  const std::string vias = section(s.read("top.def"), "VIAS");
  CHECK(vias.starts_with("VIAS 1 ;\n   - via1_260_260\n      + RECT metal1 ( -130 -130 ) ( 130 130 )\n"
                         "      + RECT metal2 ( -130 -130 ) ( 130 130 )\n"));
  CHECK(vias.ends_with(" ;\nEND VIAS\n\n"));
}

TEST_CASE("VIAS: a read via with underscores is written uncounted only with magic-compat ON (Q-LEFDEF-91)",
          "[cap:def-write][req:VIAS section content]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    lef::ViaDefinition read;  // a metal type, so the cut type still maps to via1
    read.type = s.type("metal1");
    read.area = db::Rect{-10, -10, 10, 10};
    read.canonical_name = "M1_M2_PR";
    s.editor.lefdef.vias.insert("M1_M2_PR", read);
    db::CellDef& top = s.cell("top");
    s.paint(top, "via1", db::Rect{10, 10, 36, 36});
    REQUIRE(s.write(top, "").ok());
    const std::string vias = section(s.read("top.def"), "VIAS");
    CHECK(vias.starts_with("VIAS 1 ;\n"));
    CHECK((vias.find("   - M1_M2_PR\n") != std::string::npos) == on);
    CHECK(vias.find("   - via1_260_260\n") != std::string::npos);
  }
}

TEST_CASE("after a write the generated vias leave the table but keep their keys",
          "[cap:def-write][req:Session state after writing]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "via1", db::Rect{10, 10, 36, 36});
  REQUIRE(s.write(top, "").ok());
  const auto* entry = s.editor.lefdef.vias.find("via1_260_260");
  REQUIRE(entry != nullptr);
  CHECK_FALSE(entry->value.has_value());
  // A second write refills the slot and counts it again.
  REQUIRE(s.write(top, "").ok());
  CHECK(section(s.read("top.def"), "VIAS").starts_with("VIAS 1 ;\n   - via1_260_260\n"));
}

TEST_CASE("the rules a write creates persist with magic-compat ON only (Q-LEFDEF-99)",
          "[cap:def-write][req:Session state after writing][req:NONDEFAULTRULES section]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    REQUIRE(s.write(wide_net(s, "top"), "").ok());
    CHECK(section(s.read("top.def"), "NONDEFAULTRULES").starts_with("NONDEFAULTRULES 1 ;\n  - metal1_width_80\n"));
    db::CellDef& other = s.cell("other");
    s.paint(other, "metal1", db::Rect{0, 0, 10, 3});
    REQUIRE(s.write(other, "").ok());
    CHECK((s.read("other.def").find("  - metal1_width_80\n") != std::string::npos) == on);
    CHECK((s.editor.lefdef.rules.find("metal1_width_80") != nullptr) == on);
  }
}

TEST_CASE("a multi-layer rule read from LEF, per mode (Q-LEFDEF-98)", "[cap:def-write][req:NONDEFAULTRULES section]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    lef::NonDefaultRule w2;
    w2.name = "W2";
    w2.layers.push_back(lef::NonDefaultRuleLayer{"metal1", lef::RuleTarget::layer, 28, 0, 10});
    w2.layers.push_back(lef::NonDefaultRuleLayer{"metal2", lef::RuleTarget::layer, 30, 0, 10});
    s.editor.lefdef.rules.insert("W2", w2);
    db::CellDef& top = s.cell("top");
    REQUIRE(s.write(top, "").ok());
    if (on) {
      CHECK(section(s.read("top.def"), "NONDEFAULTRULES") ==
            "NONDEFAULTRULES 1 ;\n  - W2\n     + LAYER metal1 WIDTH 280 WIREEXT 5\n"
            "     + LAYER metal1 WIDTH 280 WIREEXT 5 ;\nEND NONDEFAULTRULES\n\n");
    } else {
      CHECK(s.read("top.def").find("NONDEFAULTRULES") == std::string::npos);
    }
  }
}

TEST_CASE("def write -anal lists the route layers a rule leaves out", "[cap:def-write][req:NONDEFAULTRULES section]") {
  Session s;
  def::DefWriteOptions o;
  o.anal = true;
  REQUIRE(s.write(wide_net(s, "top"), "", o).ok());
  const std::string rules = section(s.read("top.def"), "NONDEFAULTRULES");
  INFO(rules);
  const std::size_t m2 = rules.find("     + LAYER metal2 WIDTH 40\n");
  const std::size_t m3 = rules.find("     + LAYER metal3 WIDTH 50\n");
  const std::size_t m1 = rules.find("     + LAYER metal1 WIDTH 80 ;\n");
  REQUIRE(m2 != std::string::npos);
  REQUIRE(m3 != std::string::npos);
  REQUIRE(m1 != std::string::npos);
  CHECK(m2 < m1);
  CHECK(m3 < m1);
  CHECK(rules.find("LAYER metal1 WIDTH 30") == std::string::npos);  // metal1 is the rule's own layer
  std::size_t count = 0;
  for (std::size_t at = rules.find("LAYER metal2 "); at != std::string::npos;
       at = rules.find("LAYER metal2 ", at + 1)) {
    ++count;
  }
  CHECK(count == 1);  // aliases met2 and M2 share the record
}

// Regression (Q-LEFDEF-100): -anal dereferences the slots an earlier write
// emptied.
TEST_CASE("def write -anal after a write that emptied via slots", "[cap:def-write][req:NONDEFAULTRULES section]") {
  Session s;
  db::CellDef& first = s.cell("first");
  s.paint(first, "via1", db::Rect{10, 10, 36, 36});
  REQUIRE(s.write(first, "").ok());
  def::DefWriteOptions o;
  o.anal = true;
  REQUIRE(s.write(wide_net(s, "top"), "", o).ok());
  CHECK(section(s.read("top.def"), "NONDEFAULTRULES").find("+ LAYER metal2 WIDTH 40") != std::string::npos);
}

// Regression: a rule entry that names no LEF record made Magic dereference
// a null lefInfo.
TEST_CASE("a rule entry with no LEF record is written by name", "[cap:def-write][req:NONDEFAULTRULES section]") {
  Session s;
  lef::NonDefaultRule r;
  r.name = "R";
  r.layers.push_back(lef::NonDefaultRuleLayer{"nosuch", lef::RuleTarget::none, 10, 0, 0});
  s.editor.lefdef.rules.insert("R", r);
  db::CellDef& top = s.cell("top");
  REQUIRE(s.write(top, "").ok());
  CHECK(section(s.read("top.def"), "NONDEFAULTRULES") ==
        "NONDEFAULTRULES 1 ;\n  - R\n     + LAYER nosuch WIDTH 100 ;\nEND NONDEFAULTRULES\n\n");
  CHECK(s.message("DEF-WRITE-012") != nullptr);
}

// Regression: a via whose route residue has no LEF name made Magic take
// strlen(NULL) of the layer name. ndc gets a cut record here, and its
// first residue ndiff has none, so the record that starts on it has no
// route layer.
TEST_CASE("a via whose route residue has no LEF name writes no layer name, in both modes",
          "[cap:def-write][req:Route record assembly]") {
  std::string tech_text = data_file("lefdef.tech");
  const std::string pc = "    cut pc cont mcon\n";
  const std::size_t at = tech_text.find(pc);
  REQUIRE(at != std::string::npos);
  tech_text.insert(at + pc.size(), "    cut ndc ndcont\n");
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on, tech_text);
    s.set_widths();
    db::CellDef& top = s.cell("top");
    s.paint(top, "ndc", db::Rect{0, 0, 6, 6});
    s.file("/t/top.ext", std::string{kExtHeader} +
                             "port \"A\" 1 0 0 6 6 ndc\nnode \"A\" 1 6 0 0 ndc 0 0 0 0 0 0 0 0 0 0 0 0\n");
    REQUIRE(s.write(top, "").ok());
    CHECK(section(s.read("top.def"), "NETS") ==
          "NETS 1 ;\n   - A ( PIN A )\n      + ROUTED  ( 30 30 ) ndcont_60_60  ;\nEND NETS\n\n");
    const diag::Diagnostic* note = s.message("DEF-WRITE-012");
    REQUIRE(note != nullptr);
    CHECK(note->native_only);
    CHECK(note->text.starts_with("No LEF route layer for magic type \"ndiffusion\" of a via"));
  }
}

// Regression: a `.ext` node layer that is not a type of the technology made
// Magic index DBConnectTbl[-1], for a net's geometry and for the blockage
// pass.
TEST_CASE("a .ext node layer the technology lacks writes no geometry, in both modes",
          "[cap:def-write][req:Net geometry discovery][req:BLOCKAGES section]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    s.set_widths();
    db::CellDef& top = s.cell("top");
    s.paint(top, "metal1", db::Rect{0, 0, 100, 3});
    s.paint(top, "metal2", db::Rect{0, 20, 100, 24});
    s.file("/t/top.ext", std::string{kExtHeader} +
                             "port \"A\" 1 0 0 100 3 m1\n"
                             "node \"A\" 1 6 0 0 nosuch 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "node \"m2_0_20#\" 1 6 0 20 nosuch 0 0 0 0 0 0 0 0 0 0 0 0\n");
    REQUIRE(s.write(top, "").ok());
    const std::string text = s.read("top.def");
    CHECK(section(text, "NETS") == "NETS 1 ;\n   - A ( PIN A ) ;\nEND NETS\n\n");
    CHECK(text.find("BLOCKAGES") == std::string::npos);
    const diag::Diagnostic* note = s.message("DEF-WRITE-012");
    REQUIRE(note != nullptr);
    CHECK(note->native_only);
    CHECK(note->text == "Node A has layer \"nosuch\", which is not a type of this technology; its geometry is not "
                        "written.");
  }
}

/// The keys of the layer table, then `vias`, in Magic's LefInfo order.
std::vector<std::string> info_keys(Session& s, const std::vector<std::string>& vias) {
  hefesto::util::MagicHashTable<int> info(32);
  std::vector<std::string> layer_keys;
  s.editor.lefdef.layers.enumerate([&](std::string_view name, const lef::LayerRecord&) {
    layer_keys.emplace_back(name);
  });
  for (auto it = layer_keys.rbegin(); it != layer_keys.rend(); ++it) {
    info.insert(*it, 0);
  }
  for (const std::string& v : vias) {
    info.insert(v, 1);
  }
  return info.keys();
}

TEST_CASE("a cut type maps to the first via-class record in LefInfo order (Q-LEFDEF-92)",
          "[cap:def-write][req:Magic-to-LEF layer map]") {
  Session s;
  lef::ViaDefinition read;
  read.type = s.type("via1");
  read.area = db::Rect{-10, -10, 10, 10};
  read.canonical_name = "M1M2_PR";
  s.editor.lefdef.vias.insert("M1M2_PR", read);
  std::string first;
  for (const std::string& key : info_keys(s, {"M1M2_PR"})) {
    if (key == "M1M2_PR" || key == "via1" || key == "via" || key == "V1") {
      first = key == "M1M2_PR" ? key : "via1";
      break;
    }
  }
  db::CellDef& top = s.cell("top");
  s.paint(top, "via1", db::Rect{10, 10, 36, 36});
  REQUIRE(s.write(top, "").ok());
  CHECK(section(s.read("top.def"), "VIAS").find("   - " + first + "_260_260\n") != std::string::npos);
}

TEST_CASE("table enumeration order: VIAS follow Magic's single LefInfo hash",
          "[cap:def-write][req:Table enumeration order]") {
  // The order Magic's LefInfo would give: the layer keys of the tech
  // section, then the generated vias, in one string hash table of 32
  // buckets.
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "via1", db::Rect{10, 10, 36, 36});
  s.paint(top, "via2", db::Rect{100, 10, 120, 30});
  s.paint(top, "metal1", db::Rect{200, 0, 210, 10});
  REQUIRE(s.write(top, "").ok());
  std::vector<std::string> expected;
  for (const std::string& key : info_keys(s, {"via1_260_260", "via2_200_200"})) {
    if (key.find('_') != std::string::npos) {
      expected.push_back("   - " + key + "\n");
    }
  }
  const std::string vias = section(s.read("top.def"), "VIAS");
  REQUIRE(expected.size() == 2);
  const std::size_t a = vias.find(expected[0]);
  const std::size_t b = vias.find(expected[1]);
  REQUIRE(a != std::string::npos);
  REQUIRE(b != std::string::npos);
  CHECK(a < b);
}

}  // namespace
}  // namespace def_write_test
