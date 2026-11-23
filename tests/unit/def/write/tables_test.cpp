// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF write: the sections read from the LEF tables (capability def-write
// "Magic-to-LEF layer map", "VIAS section content", "NONDEFAULTRULES
// section", "BLOCKAGES section", "Table enumeration order", "Session state
// after writing"; tasks.md 8.2, 8.5; Q-LEFDEF-91, 92, 98, 99, 100, 101).
//
// Owner: DEFW (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

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
    s.editor.lefdef.layers.note_key("M1_M2_PR");  // as the LEF and DEF readers do (Q-LEFDEF-135)
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
// a null lefInfo (Q-LEFDEF-140, fixed in both modes). With magic-compat OFF
// a read rule is not written by def write (Q-LEFDEF-99), so nothing reaches
// the entry.
TEST_CASE("a rule entry with no LEF record is written by name, per mode",
          "[cap:def-write][req:NONDEFAULTRULES section]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    lef::NonDefaultRule r;
    r.name = "R";
    r.layers.push_back(lef::NonDefaultRuleLayer{"nosuch", lef::RuleTarget::none, 10, 0, 0});
    s.editor.lefdef.rules.insert("R", r);
    db::CellDef& top = s.cell("top");
    REQUIRE(s.write(top, "").ok());
    CHECK(section(s.read("top.def"), "NONDEFAULTRULES") ==
          (on ? "NONDEFAULTRULES 1 ;\n  - R\n     + LAYER nosuch WIDTH 100 ;\nEND NONDEFAULTRULES\n\n" : ""));
    CHECK((s.message("DEF-WRITE-012") != nullptr) == on);
  }
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

/// The LEF names of the tech `lef` section of `tech_text`, in the order the
/// section inserts them (the names follow the keyword and the magic type).
std::vector<std::string> tech_keys(const std::string& tech_text) {
  std::vector<std::string> keys;
  std::istringstream in(tech_text);
  bool inside = false;
  for (std::string line; std::getline(in, line);) {
    std::istringstream words(line);
    std::vector<std::string> t;
    for (std::string w; words >> w;) {
      t.push_back(w);
    }
    if (t.empty()) {
      continue;
    }
    if (!inside) {
      inside = t.size() == 1 && t[0] == "lef";
      continue;
    }
    if (t[0] == "end") {
      break;
    }
    if (t.size() > 2) {
      keys.insert(keys.end(), t.begin() + 2, t.end());
    }
  }
  return keys;
}

/// Magic's LefInfo enumeration order after inserting `history` (HashFind
/// of each name, in order) into one string hash table of 32 buckets.
std::vector<std::string> lefinfo_order(const std::vector<std::string>& history) {
  hefesto::util::MagicHashTable<int> info(32);
  for (const std::string& key : history) {
    (void)info.find_or_insert(key);
  }
  return info.keys();
}

/// The keys of the tech section, then `vias`, in Magic's LefInfo order.
std::vector<std::string> info_keys(const std::vector<std::string>& vias) {
  std::vector<std::string> history = tech_keys(data_file("lefdef.tech"));
  history.insert(history.end(), vias.begin(), vias.end());
  return lefinfo_order(history);
}

/// The record names of a VIAS section, in order.
std::vector<std::string> via_names(const std::string& vias) {
  std::vector<std::string> names;
  std::istringstream in(vias);
  for (std::string line; std::getline(in, line);) {
    if (line.starts_with("   - ")) {
      names.push_back(line.substr(5));
    }
  }
  return names;
}

TEST_CASE("a cut type maps to the first via-class record in LefInfo order (Q-LEFDEF-92)",
          "[cap:def-write][req:Magic-to-LEF layer map]") {
  Session s;
  lef::ViaDefinition read;
  read.type = s.type("via1");
  read.area = db::Rect{-10, -10, 10, 10};
  read.canonical_name = "M1M2_PR";
  s.editor.lefdef.layers.note_key("M1M2_PR");  // as the LEF and DEF readers do (Q-LEFDEF-135)
  s.editor.lefdef.vias.insert("M1M2_PR", read);
  std::string first;
  for (const std::string& key : info_keys({"M1M2_PR"})) {
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
  for (const std::string& key : info_keys({"via1_260_260", "via2_200_200"})) {
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

/// A LEF library of `count` vias V_<i>_r on metal1, with a LAYER m3 (a new
/// route layer) after the first half and a NONDEFAULTRULE naming the
/// unknown layer `nosuch` at the end; `history` gets the LefInfo keys the
/// read inserts, in order.
std::string interleaved_lef(int count, std::vector<std::string>& history) {
  std::string lef = "VERSION 5.7 ;\n";
  for (int i = 0; i < count; ++i) {
    const std::string name = "V_" + std::to_string(i) + "_r";
    lef += "VIA " + name + " DEFAULT\n  LAYER metal1 ;\n    RECT -0.1 -0.1 0.1 0.1 ;\nEND " + name + "\n";
    history.push_back(name);
    if (i == count / 2) {
      lef += "LAYER m3\n  TYPE ROUTING ;\n  WIDTH 0.5 ;\nEND m3\n";
      history.emplace_back("m3");
    }
  }
  lef += "NONDEFAULTRULE R1\n  LAYER nosuch\n    WIDTH 0.5 ;\n  END nosuch\nEND R1\n";
  history.emplace_back("nosuch");
  return lef + "END LIBRARY\n";
}

/// The name a write created: its VIAS record starting with `via1_`.
std::string created_via(const std::string& def_text) {
  for (const std::string& name : via_names(section(def_text, "VIAS"))) {
    if (name.starts_with("via1_")) {
      return name;
    }
  }
  return {};
}

// E1 (2026-11-20, Q-LEFDEF-135): one key order for the layers and the vias.
// The LEF read interleaves via names, a new layer and a rule's unknown
// layer name; three writes insert one generated via name each; the last
// write regenerates all three. With more than 96 keys the table has grown
// once, so the order depends on the whole insertion history.
TEST_CASE("table enumeration order: one insertion history for layers and vias, growth included, per mode",
          "[cap:def-write][req:Table enumeration order]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    std::vector<std::string> history = tech_keys(data_file("lefdef.tech"));
    s.file("/t/lib.lef", interleaved_lef(80, history));
    REQUIRE(s.run("lef read /t/lib.lef").ok());
    db::CellDef& top = s.cell("top");
    std::vector<std::string> generated;
    for (const db::Coord w : {26, 28, 30}) {
      const std::string name = "c" + std::to_string(w);
      db::CellDef& one = s.cell(name);
      s.paint(one, "via1", db::Rect{0, 0, w, w});
      REQUIRE(s.write(one, "").ok());
      generated.push_back(created_via(s.read(name + ".def")));
      REQUIRE_FALSE(generated.back().empty());
      s.paint(top, "via1", db::Rect{100 * w, 0, 100 * w + w, w});
    }
    history.insert(history.end(), generated.begin(), generated.end());
    REQUIRE(history.size() > 96);  // the 97th key grows the table
    REQUIRE(s.write(top, "").ok());
    std::vector<std::string> expected;
    for (const std::string& key : lefinfo_order(history)) {
      const bool mine = std::find(generated.begin(), generated.end(), key) != generated.end();
      if (mine || (on && key.starts_with("V_"))) {
        expected.push_back(key);
      }
    }
    CHECK(via_names(section(s.read("top.def"), "VIAS")) == expected);
  }
}

}  // namespace
}  // namespace def_write_test
