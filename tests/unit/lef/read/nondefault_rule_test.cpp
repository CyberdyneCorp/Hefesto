// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Unit tests of the LEF NONDEFAULTRULE section (capability lef-read;
// openspec/changes/archive/2026-11-22-m6-lef-def-oasis tasks.md 5.3).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

#include "read_fixture.hpp"

using namespace lefread_test;

namespace {

constexpr float kFiveNm = 0.005F;

lef::NonDefaultRuleLayer rule_layer(std::string name, lef::RuleTarget target, db::Coord width = 0,
                                    db::Coord spacing = 0, db::Coord extension = 0) {
  return lef::NonDefaultRuleLayer{std::move(name), target, width, spacing, extension};
}

using Layers = std::vector<lef::NonDefaultRuleLayer>;

void define_met1(LefRun& run) {
  run.layers.define({"met1"}, lef::LayerEntry{lef::LayerClass::route, type_of("metal1"), 0, 0, std::nullopt});
}

}  // namespace

TEST_CASE("double-width rule", "[cap:lef-read][req:LEF NONDEFAULTRULE section]") {
  LefRun run{true, kFiveNm};
  define_met1(run);
  run.read("NONDEFAULTRULE DW LAYER met1 WIDTH 0.28 ; SPACING 0.28 ; END met1 END DW\n");
  const auto& rule = run.rule("DW");
  CHECK(rule.name == "DW");
  CHECK(rule.layers == Layers{rule_layer("met1", lef::RuleTarget::layer, 56, 56, 0)});
  CHECK(run.texts().empty());
}

TEST_CASE("HARDSPACING swallows the next statement", "[cap:lef-read][req:LEF NONDEFAULTRULE section]") {
  constexpr std::string_view text = "NONDEFAULTRULE R HARDSPACING ; LAYER met1 WIDTH 0.2 ; END met1 END R\n";
  SECTION("magic-compat ON") {
    LefRun run{true, kFiveNm};
    define_met1(run);
    run.read(text);
    const auto& rule = run.rule("R");
    CHECK(rule.hard_spacing);
    CHECK(rule.layers.empty());
    // `END met1` ended the rule early, so `END R` is out of context.
    CHECK(run.texts() == Strings{"LEF read, Line 1 (Message): Unknown keyword \";\" in NONDEFAULTRULES definition; "
                                 "ignoring.",
                                 "LEF read, Line 1 (Error): END statement out of context."});
  }
  SECTION("magic-compat OFF") {
    LefRun run{false, kFiveNm};
    define_met1(run);
    run.read(text);
    CHECK(run.rule("R").layers == Layers{rule_layer("met1", lef::RuleTarget::layer, 40)});
    CHECK(run.texts().empty());
  }
}

TEST_CASE("USEVIA and USEVIARULE add a rule and leave their semicolon", "[cap:lef-read][req:LEF NONDEFAULTRULE section]") {
  constexpr std::string_view text =
      "NONDEFAULTRULE R USEVIA V1 ; USEVIARULE GEN ; LAYER met1 WIDTH 0.2 ; END met1 END R\n";
  SECTION("magic-compat ON") {
    LefRun run{true, kFiveNm};
    define_met1(run);
    run.vias.insert("V1", lef::ViaDefinition{});
    run.read(text);
    // USEVIA leaves its `;`, which is read as an unknown keyword whose
    // statement skip swallows `USEVIARULE GEN ;`.
    CHECK(run.rule("R").layers ==
          Layers{rule_layer("met1", lef::RuleTarget::layer, 40), rule_layer("V1", lef::RuleTarget::via)});
    CHECK(run.texts() == Strings{"LEF read, Line 1 (Message): Unknown keyword \";\" in NONDEFAULTRULES definition; "
                                 "ignoring."});
  }
  SECTION("magic-compat OFF") {
    LefRun run{false, kFiveNm};
    define_met1(run);
    run.vias.insert("V1", lef::ViaDefinition{});
    run.read(text);
    CHECK(run.rule("R").layers == Layers{rule_layer("met1", lef::RuleTarget::layer, 40),
                                         rule_layer("GEN", lef::RuleTarget::none),
                                         rule_layer("V1", lef::RuleTarget::via)});
    CHECK(run.layers.reserved == Strings{"GEN"});
    CHECK(run.texts().empty());
  }
}

TEST_CASE("MINCUTS and PROPERTY ignore two tokens", "[cap:lef-read][req:LEF NONDEFAULTRULE section]") {
  constexpr std::string_view text =
      "NONDEFAULTRULE R LAYER met1 WIDTH 0.2 ; END met1 MINCUTS via1 2 ; PROPERTY p 1 ; LAYER met1 SPACING 0.1 ; "
      "END met1 END R\n";
  SECTION("magic-compat ON") {
    LefRun run{true, kFiveNm};
    define_met1(run);
    run.read(text);
    // MINCUTS' `;` swallows PROPERTY; PROPERTY never runs, the second LAYER does.
    CHECK(run.rule("R").layers == Layers{rule_layer("met1", lef::RuleTarget::layer, 0, 20),
                                         rule_layer("met1", lef::RuleTarget::layer, 40)});
    CHECK(run.texts().size() == 1);
  }
  SECTION("magic-compat OFF") {
    LefRun run{false, kFiveNm};
    define_met1(run);
    run.read(text);
    CHECK(run.rule("R").layers == Layers{rule_layer("met1", lef::RuleTarget::layer, 0, 20),
                                         rule_layer("met1", lef::RuleTarget::layer, 40)});
    CHECK(run.texts().empty());
  }
}

TEST_CASE("a known VIA's body", "[cap:lef-read][req:LEF NONDEFAULTRULE section]") {
  constexpr std::string_view text =
      "NONDEFAULTRULE R VIA V1 WIDTH 0.1 ; END V1 LAYER met1 WIDTH 0.2 ; END met1 END R\n";
  SECTION("magic-compat ON") {
    LefRun run{true, kFiveNm};
    define_met1(run);
    run.vias.insert("V1", lef::ViaDefinition{});
    run.read(text);
    // The body is read as rule statements: WIDTH sets the via rule's width,
    // `END V1` ends the rule, and the LAYER after it is a top-level section.
    CHECK(run.rule("R").layers == Layers{rule_layer("V1", lef::RuleTarget::via, 20)});
    CHECK(run.layers.at("met1").route.width == 40);
    CHECK(run.texts() == Strings{"LEF read, Line 1 (Message): WIDTH specified without layer.",
                                 "LEF read, Line 1 (Error): END statement out of context."});
  }
  SECTION("magic-compat OFF") {
    LefRun run{false, kFiveNm};
    define_met1(run);
    run.vias.insert("V1", lef::ViaDefinition{});
    run.read(text);
    CHECK(run.rule("R").layers ==
          Layers{rule_layer("met1", lef::RuleTarget::layer, 40), rule_layer("V1", lef::RuleTarget::via)});
    CHECK_FALSE(run.layers.at("met1").route.width);
    CHECK(run.texts().empty());
  }
}

TEST_CASE("a new VIA is read as a definition", "[cap:lef-read][req:LEF NONDEFAULTRULE section]") {
  // The VIA definition reader is LEFVIA's (tasks.md 4.1); until then the body
  // is skipped to its END and the rule names the via.
  LefRun run{true, kFiveNm};
  define_met1(run);
  run.read("NONDEFAULTRULE R VIA NEWV LAYER met1 ; RECT -0.1 -0.1 0.1 0.1 ; END NEWV END R\n");
  CHECK(run.rule("R").layers == Layers{rule_layer("NEWV", lef::RuleTarget::via)});
  CHECK(run.texts().empty());
}

TEST_CASE("wire extension is in half units", "[cap:lef-read][req:LEF NONDEFAULTRULE section]") {
  LefRun run{true, kFiveNm};
  define_met1(run);
  run.read("NONDEFAULTRULE R LAYER met1 WIREEXTENSION 0.1 ; DIAGWIDTH 0.3 ; END met1 END R\n");
  CHECK(run.rule("R").layers == Layers{rule_layer("met1", lef::RuleTarget::layer, 0, 0, 40)});
  CHECK(run.texts().empty());
}

TEST_CASE("values without a rule or a layer", "[cap:lef-read][req:LEF NONDEFAULTRULE section]") {
  LefRun run{true, kFiveNm};
  run.read(
      "NONDEFAULTRULE R\n"
      "SPACING 0.1 ;\n"
      "LAYER nosuch WIDTH 0.1 ; WIREEXTENSION 0.1 ; END nosuch\n"
      "DIAGWIDTH 0.1 ;\n"
      "FOO 1 ;\n"
      "END R\n");
  CHECK(run.rule("R").layers == Layers{rule_layer("nosuch", lef::RuleTarget::none)});
  CHECK(run.layers.reserved == Strings{"nosuch"});
  // Magic reports two of these as DEF messages.
  CHECK(run.texts() == Strings{"DEF read, Line 2 (Message): SPACING specified without layer.",
                               "LEF read, Line 2 (Message): No rule for non-default spacing.",
                               "LEF read, Line 3 (Message): No layer for non-default width.",
                               "LEF read, Line 3 (Message): No layer for non-default extension.",
                               "LEF read, Line 4 (Message): Layer value specified without layer.",
                               "LEF read, Line 5 (Message): Unknown keyword \"FOO\" in NONDEFAULTRULES definition; "
                               "ignoring."});
}

TEST_CASE("duplicate rule names are renamed", "[cap:lef-read][req:LEF NONDEFAULTRULE section]") {
  // Q-LEFDEF-9: Magic retries R_0 forever once R and R_0 exist; Hefesto
  // goes on to R_1.
  LefRun run{true, kFiveNm};
  define_met1(run);
  run.read(
      "NONDEFAULTRULE R LAYER met1 WIDTH 0.1 ; END met1 END R\n"
      "NONDEFAULTRULE R LAYER met1 WIDTH 0.2 ; END met1 END R\n"
      "NONDEFAULTRULE R LAYER met1 WIDTH 0.3 ; END met1 END R\n");
  CHECK(run.rule("R").layers == Layers{rule_layer("met1", lef::RuleTarget::layer, 20)});
  CHECK(run.rule("R_0").layers == Layers{rule_layer("met1", lef::RuleTarget::layer, 40)});
  CHECK(run.rule("R_1").layers == Layers{rule_layer("met1", lef::RuleTarget::layer, 60)});
  CHECK(run.rule("R_1").name == "R_1");
  CHECK(run.texts() == Strings{"LEF read, Line 2 (Warning): Nondefault rule \"R\" was already defined.",
                               "LEF read, Line 2 (Warning): Renaming this rule \"R_0\"",
                               "LEF read, Line 3 (Warning): Nondefault rule \"R\" was already defined.",
                               "LEF read, Line 3 (Warning): Renaming this rule \"R_1\""});
}

TEST_CASE("rules persist across reads", "[cap:lef-read][req:LEF NONDEFAULTRULE section]") {
  LefRun run{true, kFiveNm};
  define_met1(run);
  run.read("NONDEFAULTRULE R LAYER met1 WIDTH 0.1 ; END met1 END R\n");
  run.read("NONDEFAULTRULE R LAYER met1 WIDTH 0.2 ; END met1 END R\n");
  CHECK(run.rules.size() == 2);
  CHECK(run.rule("R_0").layers.front().width == 40);
}

// Regression: a LEF VIA's LAYER that names a key a NONDEFAULTRULE left with
// no record fell back to the magic type. Magic's LefReadLayer resolves no
// type for it, so the via gets no geometry from that layer, with the
// unknown-layer errors (Q-LEFDEF-151, compat: OFF treats it as absent).
TEST_CASE("a VIA LAYER that names a rule's unknown layer gets no type with magic-compat ON",
          "[cap:lef-read][req:LEF layer name resolution]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    LefRun run{on, kFiveNm};
    define_met1(run);
    run.read("NONDEFAULTRULE R LAYER poly WIDTH 0.2 ; END poly END R\n"
             "VIA V2 DEFAULT LAYER poly ; RECT -0.1 -0.1 0.1 0.1 ; END V2\nEND LIBRARY\n");
    const auto* via = run.vias.find("V2");
    REQUIRE(via != nullptr);
    REQUIRE(via->value.has_value());
    CHECK((via->value->type == std::nullopt) == on);
    const auto texts = run.texts();
    for (const auto& t : texts) {
      UNSCOPED_INFO(t);
    }
    const bool error = std::any_of(texts.begin(), texts.end(), [](const std::string& t) {
      return t.find("Don't know how to parse layer \"poly\"") != std::string::npos;
    });
    CHECK(error == on);
  }
}
