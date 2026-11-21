// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// def-read: NONDEFAULTRULES (Q-LEFDEF-75 fix), BLOCKAGES (Q-LEFDEF-76,
// 68 fixes), section count checks, diagnostics with error limits, and read
// completion with its undo step (tasks.md 7.6; Q-LEFDEF-22).
//
// Owner: DEFR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "def_fixture.hpp"
#include "hefesto/db/property.hpp"

namespace def_read_test {
namespace {

std::string rules(std::string_view body, int count = 1) {
  return "DESIGN top ;\nNONDEFAULTRULES " + std::to_string(count) + " ;\n" + std::string{body} +
         "\nEND NONDEFAULTRULES\n";
}

std::string blockages(std::string_view body, int count = 1) {
  return "DESIGN top ;\nBLOCKAGES " + std::to_string(count) + " ;\n" + std::string{body} +
         "\nEND BLOCKAGES\nEND DESIGN\n";
}

const lef::NonDefaultRule& rule(Session& s, std::string_view name) {  // by value: GCC 14 -Wdangling-reference
  const auto* entry = s.editor.lefdef.rules.find(std::string{name});
  INFO("rule " << name);
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.has_value());
  return *entry->value;
}

// --- NONDEFAULTRULES section --------------------------------------------------------

TEST_CASE("rule widths", "[cap:def-read][req:NONDEFAULTRULES section]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    REQUIRE(s.read(rules("- W2 + LAYER met1 WIDTH 280 SPACING 280 + LAYER met2 WIDTH 300 ;") +
                   "NETS 1 ;\n- n + NONDEFAULTRULE W2 + ROUTED met1 ( 0 0 ) ( 1000 0 ) ;\nEND NETS\nEND DESIGN\n")
                .ok());
    CHECK(s.painted("top", "metal1") == std::vector<db::Rect>{db::Rect{-140, -140, 1140, 140}});
    const lef::NonDefaultRule& w2 = rule(s, "W2");
    REQUIRE(w2.layers.size() == 2);
    CHECK(w2.layers[0].name == "met2");  // the last rule read first
    CHECK(w2.layers[0].width == 300);
    CHECK(w2.layers[1].width == 280);
    CHECK(w2.layers[1].spacing == 280);
    CHECK(w2.layers[1].target == lef::RuleTarget::layer);
    CHECK(s.said("  Processed 1 non-default rules total."));
  }
}

TEST_CASE("rule values without a layer, WIREEXT and DIAGWIDTH", "[cap:def-read][req:NONDEFAULTRULES section]") {
  Session s;
  REQUIRE(s.read(rules("- R + WIDTH 10 + LAYER met1 WIREEXT 5 DIAGWIDTH 3 + HARDSPACING + MINCUTS via1 2 "
                       "+ PROPERTY a b + VIA via1 + FOO ;") +
                 "END DESIGN\n")
              .ok());
  CHECK(s.said("WIDTH specified without layer."));
  CHECK(s.said("No rule for non-default width."));
  CHECK(s.said("Non-default extension at via not implemented."));
  CHECK(s.said("Unknown non-default rule property \"FOO\" in NONDEFAULTRULE definition; ignoring."));
  const lef::NonDefaultRule& r = rule(s, "R");
  CHECK(r.hard_spacing);
  REQUIRE(r.layers.size() == 2);
  CHECK(r.layers[1].extension == 10);  // doubled units
}

// The name takes a key with no record in the shared key order, as Magic's
// HashFind does (Q-LEFDEF-135).
TEST_CASE("an unknown rule layer is reported and not added (Q-LEFDEF-75)",
          "[cap:def-read][req:NONDEFAULTRULES section]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    const std::size_t names = s.editor.lefdef.layers.size();
    REQUIRE(s.read(rules("- R + LAYER met9 WIDTH 10 ;") + "END DESIGN\n").ok());
    CHECK(s.said("Unknown layer \"met9\" in NONDEFAULTRULE definition."));
    CHECK(s.said("No layer for non-default width."));
    CHECK(s.editor.lefdef.layers.size() == names + 1);
    CHECK(s.editor.lefdef.layers.find("met9") == nullptr);
    CHECK(s.editor.lefdef.vias.find("met9") == nullptr);
    CHECK(rule(s, "R").layers.front().target == lef::RuleTarget::none);
  }
}

TEST_CASE("a rule read again replaces the earlier one", "[cap:def-read][req:NONDEFAULTRULES section]") {
  Session s;
  REQUIRE(s.read(rules("- W + LAYER met1 WIDTH 280 ;\n- W + LAYER met2 WIDTH 300 ;", 2) + "END DESIGN\n").ok());
  REQUIRE(rule(s, "W").layers.size() == 1);
  CHECK(rule(s, "W").layers[0].name == "met2");
}

// --- BLOCKAGES section --------------------------------------------------------------

TEST_CASE("layer blockage", "[cap:def-read][req:BLOCKAGES section]") {
  Session s;
  REQUIRE(s.read(blockages("- LAYER met1 RECT ( 0 0 ) ( 500 500 ) ;")).ok());
  CHECK(s.painted("top", "obsm1") == std::vector<db::Rect>{db::Rect{0, 0, 500, 500}});
  CHECK(s.said("  Processed 1 blockage."));
}

TEST_CASE("no blockage option and annotation skip the section", "[cap:def-read][req:BLOCKAGES section]") {
  Session s;
  s.options.no_blockage = true;
  REQUIRE(s.read(blockages("- LAYER met1 RECT ( 0 0 ) ( 500 500 ) ;")).ok());
  CHECK(s.painted("top", "obsm1").empty());
  Session a;
  a.options.annotate = true;
  REQUIRE(a.read(blockages("- LAYER met1 RECT ( 0 0 ) ( 500 500 ) ;")).ok());
  CHECK(a.painted("top", "obsm1").empty());
}

TEST_CASE("placement blockages are not painted (Q-LEFDEF-76)", "[cap:def-read][req:BLOCKAGES section]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    REQUIRE(s.read(blockages("- LAYER met2 RECT ( 0 0 ) ( 10 10 ) ;\n"
                             "- PLACEMENT RECT ( 100 100 ) ( 200 200 ) ;\n"
                             "- LAYER met2 + COMPONENT u1 RECT ( 20 0 ) ( 30 10 ) RECT ( 40 0 ) ( 50 10 ) ;",
                             3))
                .ok());
    CHECK(s.said("Unknown blockage property \"PLACEMENT\" in BLOCKAGES definition; ignoring."));
    CHECK(s.said("Unknown blockage property \"+\" in BLOCKAGES definition; ignoring."));
    CHECK(s.said("Blockage without a layer (a placement blockage) is not painted."));
    CHECK(s.painted("top", "metal2").size() == 3);
    CHECK_FALSE(s.covers("top", "metal2", db::Rect{100, 100, 200, 200}));
    CHECK(s.said("  Processed 3 blockages."));
  }
}

TEST_CASE("blockage on an ignored or unknown layer, and a bad RECT", "[cap:def-read][req:BLOCKAGES section]") {
  Session s;
  REQUIRE(s.read(blockages("- LAYER met9 RECT ( 0 0 ) ( 10 10 ) ;\n- LAYER met1 RECT ( 0 0 ) ( 10 ;\n"
                           "- LAYER met1 RECT ( 0 0 ) ( 5 5 ) ;",
                           3))
              .ok());
  CHECK(s.said("No layer defined for RECT."));
  CHECK(s.said("Bad port geometry: RECT requires 4 values."));
  CHECK(s.painted("top", "obsm1") == std::vector<db::Rect>{db::Rect{0, 0, 5, 5}});
}

// --- Section count checks -----------------------------------------------------------

TEST_CASE("count mismatch", "[cap:def-read][req:Section count checks]") {
  Session s;
  REQUIRE(s.read("DESIGN top ;\nNETS 3 ;\n- a ;\n- b ;\nEND NETS\nEND DESIGN\n").ok());
  CHECK(s.said("Number of nets read (2) does not match the number declared (3)."));
}

TEST_CASE("missing section ends and unknown statements", "[cap:def-read][req:Section count checks]") {
  Session s;
  REQUIRE(s.read("DESIGN top ;\nNETS 0 ;\nfoo ;\nEND NET\nEND NETS\nPINS x ;\nEND PINS\n"
                 "COMPONENTS 0 ;\nEND COMP\nEND COMPONENTS\nEND DESIGN\n")
              .ok());
  CHECK(s.said("Unknown keyword \"foo\" in NET definition; ignoring."));
  CHECK(s.said("Net END statement missing."));
  CHECK(s.said("  Processed 0 pins total."));  // a count that is not a number is 0
  CHECK(s.said("Component END statement missing."));
}

// --- Diagnostics and error limits ---------------------------------------------------

TEST_CASE("error summary and prefixes", "[cap:def-read][req:Diagnostics and error limits]") {
  Session s;
  REQUIRE(s.read("DESIGN top ;\nEND FOO\nEND BAR\nEND DESIGN\n").ok());
  CHECK(s.said("DEF Read: encountered 2 errors total."));
  CHECK_FALSE(s.said("warnings total."));
  CHECK(std::any_of(s.messages.begin(), s.messages.end(), [](const diag::Diagnostic& d) {
    return d.text == "DEF read, Line 2 (Error): END statement out of context." && d.code == "DEF-READ-001" &&
           d.severity == diag::Severity::error;
  }));
}

TEST_CASE("only the first 100 errors are printed", "[cap:def-read][req:Diagnostics and error limits]") {
  Session s;
  std::string text = "DESIGN top ;\n";
  for (int i = 0; i < 105; ++i) {
    text += "END X\n";
  }
  REQUIRE(s.read(text + "END DESIGN\n").ok());
  CHECK(s.count("END statement out of context.") == 100);
  CHECK(s.count("DEF Read:  Further errors will not be reported.") == 1);
  CHECK(s.said("DEF Read: encountered 105 errors total."));
  // The counters are reset for the next read.
  REQUIRE(s.read("DESIGN top ;\nEND X\nEND DESIGN\n").ok());
  CHECK(s.said("DEF Read: encountered 1 error total."));
}

// --- Read completion ----------------------------------------------------------------

TEST_CASE("completion: modified, labels attached, box recomputed", "[cap:def-read][req:Read completion]") {
  Session s;
  s.options.labels = true;
  REQUIRE(s.read("DESIGN top ;\nNETS 1 ;\n- n1 + ROUTED met1 ( 0 0 ) ( 1000 0 ) ;\nEND NETS\nEND DESIGN\n").ok());
  db::CellDef& top = s.cell("top");
  CHECK(top.flags().modified);
  CHECK(top.bbox() == db::Rect{-70, -70, 1070, 70});
  CHECK_FALSE(s.said("This action cannot be undone."));
}

TEST_CASE("one undo reverts a DEF read that created its cell", "[cap:def-read][req:Read completion]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    db::CellDef* inv = s.editor.cells.create("inv", s.editor.technology->plane_count()).value();
    inv->flags().available = true;
    s.file("/d/top.def",
           "DESIGN top ;\nDIEAREA ( 0 0 ) ( 2000 2000 ) ;\nCOMPONENTS 1 ;\n- u1 inv + PLACED ( 0 0 ) N ;\n"
           "END COMPONENTS\nPINS 1 ;\n- a + LAYER met1 ( 0 0 ) ( 10 10 ) + PLACED ( 0 0 ) N ;\nEND PINS\n"
           "NETS 1 ;\n- n1 + ROUTED met1 ( 0 0 ) ( 1000 0 ) ;\nEND NETS\nEND DESIGN\n");
    REQUIRE(s.run("defr read /d/top").ok());
    REQUIRE(s.editor.cells.find("top") != nullptr);
    REQUIRE(s.run("undo").ok());
    CHECK(s.editor.cells.find("top") == nullptr);
    REQUIRE(s.run("redo").ok());
    db::CellDef& top = s.cell("top");
    CHECK(top.uses().size() == 1);
    CHECK(top.labels().size() == 1);
    CHECK(top.properties().find("FIXED_BBOX") != nullptr);
    CHECK(s.extent("top", "metal1") == db::Rect{-70, -70, 1070, 70});
    REQUIRE(s.run("undo").ok());
    CHECK(s.editor.cells.find("top") == nullptr);
  }
}

TEST_CASE("one undo reverts a DEF read into an existing cell", "[cap:def-read][req:Read completion]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    s.file("/d/a.def", "DESIGN top ;\nDIEAREA ( 0 0 ) ( 10 10 ) ;\nEND DESIGN\n");
    s.file("/d/b.def", "DESIGN top ;\nDIEAREA ( 0 0 ) ( 20 20 ) ;\nNETS 1 ;\n- n1 + ROUTED met1 ( 0 0 ) ( 1000 0 ) ;\n"
                       "END NETS\nEND DESIGN\n");
    REQUIRE(s.run("defr read /d/a").ok());
    REQUIRE(s.run("defr read /d/b").ok());
    CHECK(s.extent("top", "metal1") == db::Rect{-70, -70, 1070, 70});
    REQUIRE(s.run("undo").ok());
    CHECK(s.painted("top", "metal1").empty());
    const auto* die = std::get_if<db::DimensionList>(s.cell("top").properties().find("FIXED_BBOX"));
    REQUIRE(die != nullptr);
    CHECK(die->values == std::vector<std::int32_t>{0, 0, 10, 10});
    REQUIRE(s.run("redo").ok());
    CHECK(s.extent("top", "metal1") == db::Rect{-70, -70, 1070, 70});
    die = std::get_if<db::DimensionList>(s.cell("top").properties().find("FIXED_BBOX"));
    REQUIRE(die != nullptr);
    CHECK(die->values == std::vector<std::int32_t>{0, 0, 20, 20});
  }
}

}  // namespace
}  // namespace def_read_test
