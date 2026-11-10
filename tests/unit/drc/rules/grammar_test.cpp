// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-rule-language: section grammar, styles and variants, run-time style
// selection, the why-text table, exceptions/exemptions and `option`
// (tasks.md 4.2, 4.3).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "rules_fixture.hpp"

namespace drc = hefesto::drc;
namespace tech = hefesto::tech;
using drc::test::compile;
using drc::test::rules_tech;

TEST_CASE("a unique keyword prefix selects the rule type", "[cap:drc-rule-language][req:DRC section grammar]") {
  const auto full = compile(R"(spacing poly poly 2 touching_ok "x")");
  const auto prefix = compile(R"(spac poly poly 2 touching_ok "x")");
  REQUIRE_FALSE(full.rules->all_rules().empty());
  CHECK(std::vector<drc::Rule>(prefix.rules->all_rules().begin(), prefix.rules->all_rules().end()) ==
        std::vector<drc::Rule>(full.rules->all_rules().begin(), full.rules->all_rules().end()));
  CHECK(prefix.messages.empty());
  // Keywords are case-insensitive; an exact match wins over prefixes.
  const auto upper = compile(R"(SPACING poly poly 2 touching_ok "x")");
  CHECK(upper.rules->all_rules().size() == full.rules->all_rules().size());
  const auto edge = compile(R"(edge poly space 2 space poly 2 "e")");
  CHECK_FALSE(edge.said("Bad DRC rule type"));
}

TEST_CASE("an ambiguous or unknown keyword is rejected with the rule list",
          "[cap:drc-rule-language][req:DRC section grammar]") {
  const auto c = compile(R"(wid poly 2 "x")");
  CHECK(c.rules->all_rules().empty());
  REQUIRE(c.messages.size() == 1);
  const auto& d = c.messages[0];
  CHECK(d.code == "DRC-TECH-001");
  CHECK(d.text ==
        "Bad DRC rule type \"wid\"\nValid rule types are:\nangles, edge, edge4way, exact_overlap, exception, "
        "exemption, extend, no_overlap, option, overhang, rect_only, spacing, stepsize, surround, width, "
        "widespacing, area, off_grid, maxwidth, cifstyle, cifwidth, cifspacing, cifarea, cifmaxwidth, rectangle.");
  // The message carries the line and the drc section (TechPrintLine).
  const auto* where = std::get_if<hefesto::diag::SourceLocation>(&d.location);
  REQUIRE(where != nullptr);
  CHECK(where->file == "/t/rules.tech");
  CHECK(where->line > 0);
  REQUIRE(d.related.has_value());
  CHECK(d.related->name == "drc");
  CHECK(compile(R"(frobnicate poly 2 "x")").said("Bad DRC rule type \"frobnicate\""));
}

TEST_CASE("a wrong token count prints the keyword usage", "[cap:drc-rule-language][req:DRC section grammar]") {
  const auto c = compile(R"(width poly "x")");
  CHECK(c.rules->all_rules().empty());
  REQUIRE(c.messages.size() == 1);
  CHECK(c.messages[0].text == "Rule type \"width\" usage: width layers width why");
  CHECK(compile(R"(spacing poly)").said("Rule type \"spacing\" usage: spacing layers1 layers2 separation [layers3] "
                                        "adjacency why"));
  // Integer parameters are read like atoi: "2x" is 2, "x" is 0.
  const auto lax = compile(R"(width poly 2x "w")");
  REQUIRE(lax.at("space", "poly").size() == 1);
  CHECK(lax.at("space", "poly")[0].dist == 2);
}

TEST_CASE("style variants restrict rules to matching styles", "[cap:drc-rule-language][req:DRC styles and variants]") {
  const char* section = R"(style s variant A,B
width m1 3 "w"
variant B
width m2 4 "w2"
variant *
width poly 5 "w3")";
  CHECK(drc::style_list(*rules_tech(section)).names == std::vector<std::string>{"sA", "sB"});
  const auto a = compile(section, true, "sA");
  CHECK(a.rules->style_name() == "sA");
  CHECK(a.at("space", "m1").size() == 1);
  CHECK(a.at("space", "m2").empty());
  CHECK(a.at("space", "poly").size() == 1);
  const auto b = compile(section, true, "sB");
  CHECK(b.at("space", "m1").size() == 1);
  CHECK(b.at("space", "m2").size() == 1);
  CHECK(b.at("space", "poly").size() == 1);
}

TEST_CASE("styles are declared in order and the first is the default",
          "[cap:drc-rule-language][req:DRC styles and variants]") {
  const char* section = R"(style fast
width poly 2 "fast poly"
style full
width poly 3 "full poly"
width m1 3 "full m1")";
  const auto t = rules_tech(section);
  CHECK(drc::style_list(*t).names == std::vector<std::string>{"fast", "full"});
  drc::test::FirstLoad first(t);
  CHECK(first.editor.drc.style == "fast");
  CHECK(first.editor.drc.styles.names == std::vector<std::string>{"fast", "full"});
  REQUIRE(first.editor.drc.rules->rules(10, 0).size() == 1);  // R[poly][space]
  CHECK(first.editor.drc.rules->why().text(first.editor.drc.rules->rules(0, 10)[0].why) == "fast poly");
  const auto full = compile(section, true, "full");
  CHECK(full.why(full.at("space", "poly")[0]) == "full poly");
  CHECK(full.at("space", "m1").size() == 1);
  // A name that starts with a declared one is not declared again (strncmp test).
  CHECK(drc::style_list(*rules_tech("style abc\nstyle ab\nstyle abcd")).names ==
        std::vector<std::string>{"abc", "abcd"});
}

TEST_CASE("rule lines before any style line make the implicit default style",
          "[cap:drc-rule-language][req:DRC styles and variants]") {
  const auto t = rules_tech(R"(width poly 2 "w")");
  CHECK(drc::style_list(*t).names == std::vector<std::string>{"default"});
  drc::test::FirstLoad first(t);
  CHECK(first.editor.drc.style == "default");
  CHECK(first.editor.drc.rules->rules(0, 10).size() == 1);
  // An empty section has the default style with no rules.
  const auto empty = rules_tech("");
  CHECK(drc::style_list(*empty).names == std::vector<std::string>{"default"});
  drc::test::FirstLoad none(empty);
  CHECK(none.editor.drc.rules->all_rules().empty());
  CHECK(none.editor.drc.rules->style_name() == "default");
}

TEST_CASE("a single style ignores variant restrictions", "[cap:drc-rule-language][req:DRC styles and variants]") {
  const auto c = compile(R"(style only
variant (x)
width poly 2 "w")");
  CHECK(c.at("space", "poly").size() == 1);
}

TEST_CASE("style selection by unique prefix", "[cap:drc-rule-language][req:Style selection at run time]") {
  const drc::DrcStyleList styles{{"fast", "full"}};
  const auto fa = drc::select_style(styles, "fa");
  REQUIRE(fa.ok());
  CHECK(fa.value() == "fast");
  const auto f = drc::select_style(styles, "f");
  REQUIRE_FALSE(f.ok());
  CHECK(f.error().code == "DRC-TECH-003");
  CHECK(f.error().text == "DRC style \"f\" is ambiguous.");
  const auto x = drc::select_style(styles, "x");
  REQUIRE_FALSE(x.ok());
  CHECK(x.error().text == "\"x\" is not one of the DRC styles Magic knows.");
  // Magic matches prefixes only: a full name that prefixes another is ambiguous.
  CHECK_FALSE(drc::select_style(drc::DrcStyleList{{"a", "ab"}}, "a").ok());
}

TEST_CASE("selecting a style reloads it under the current grid", "[cap:drc-rule-language][req:Style selection at run time]") {
  const char* section = R"(style fast
width poly 2 "w"
style full
width poly 3 "w")";
  const auto full = compile(section, true, "full", tech::GridScale{2, 1});
  REQUIRE(full.at("space", "poly").size() == 1);
  CHECK(full.at("space", "poly")[0].dist == 6);
  CHECK(full.rules->halo().value == 6);
  CHECK(full.rules->step_size() == 16 * 3 * 2);
}

TEST_CASE("why texts: predefined 1-5, then shared indices in registration order",
          "[cap:drc-rule-language][req:Why-text table]") {
  const auto c = compile(R"(width poly 2 "too narrow"
width m1 3 "too narrow"
width m2 4 "other")");
  const auto& why = c.rules->why();
  CHECK(why.text(1) == "This layer can't abut or partially overlap between array elements");
  CHECK(why.text(2) == "Can't overlap those layers");
  CHECK(why.text(3) == "This layer can't abut or partially overlap between subcells");
  CHECK(why.text(4) == "See error definition in the subcell");
  CHECK(why.text(5) == "This position does not align with the manufacturing grid");
  CHECK(c.at("space", "poly")[0].why == 6);
  CHECK(c.at("space", "m1")[0].why == 6);
  CHECK(c.at("space", "m2")[0].why == 7);
  // A why string is registered even when the rule then fails validation.
  const auto failed = compile(R"(width poly,m1 2 "never compiled"
width m2 4 "next")");
  CHECK(failed.rules->why().find("never compiled") == 6);
  CHECK(failed.at("space", "m2")[0].why == 7);
  // The directional surround form registers its presence word first (Magic).
  const auto dir = compile(R"(surround m1c m1 1 2 directional "dir")");
  CHECK(dir.rules->why().find("directional") == 6);
  CHECK(dir.rules->why().find("dir") == 7);
}

TEST_CASE("exceptions and exemptions tag the following rules", "[cap:drc-rule-language][req:Exceptions and exemptions]") {
  const auto c = compile(R"(exemption SRAM
spacing m1 m1 3 touching_ok "s"
exemption none
width m1 2 "w"
exception RAM2
width poly 2 "p"
exemption RAM2
width m2 2 "m2")");
  REQUIRE(c.rules->exception_names().size() == 2);
  CHECK(c.rules->exception_names()[0] == "SRAM");
  CHECK(c.rules->exception_names()[1] == "RAM2");
  const auto spacing = c.at("m1", "space");
  REQUIRE_FALSE(spacing.empty());
  for (const auto& r : spacing) {
    if ((r.flags & drc::flags::kReverse) == 0 && r.dist == 3) {
      CHECK(r.exception == (0 | drc::kExemptionBit));
    }
  }
  CHECK(c.at("space", "m1").back().exception == drc::kNoException);
  CHECK(c.at("space", "poly")[0].exception == 1);
  CHECK(c.at("space", "m2")[0].exception == (1 | drc::kExemptionBit));
}

TEST_CASE("at most 126 exception names", "[cap:drc-rule-language][req:Exceptions and exemptions]") {
  std::string section;
  for (int i = 0; i < 128; ++i) {
    section += "exception X" + std::to_string(i) + "\n";
  }
  section += R"(width poly 2 "w")";
  const auto c = compile(section);
  CHECK(c.rules->exception_names().size() == 127);
  CHECK(c.count_code("DRC-TECH-001") == 1);
  CHECK(c.said("Error:  Too many rule exceptions!  Limit is 126."));
  CHECK(c.at("space", "poly")[0].exception == drc::kNoException);
}

TEST_CASE("the exception tag is reset at each style load", "[cap:drc-rule-language][req:Exceptions and exemptions]") {
  const char* section = R"(style a
exception X
width poly 2 "w"
style b
width poly 3 "w")";
  const auto b = compile(section, true, "b");
  CHECK(b.at("space", "poly")[0].exception == drc::kNoException);
  CHECK(b.rules->exception_names().empty());
}

TEST_CASE("option accepts wide-width-noninclusive only", "[cap:drc-rule-language][req:option statement]") {
  const auto bad = compile("option foo");
  REQUIRE(bad.messages.size() == 1);
  CHECK(bad.messages[0].text == "Unrecognized DRC option \"foo\" (ignored).");
  CHECK_FALSE(bad.rules->options().wide_width_noninclusive);
  const auto good = compile("option wide-width-noninclusive");
  CHECK(good.messages.empty());
  CHECK(good.rules->options().wide_width_noninclusive);
}

TEST_CASE("type-name errors are repeated by the plow rule reader in every style",
          "[cap:drc-rule-language][req:DRC section grammar]") {
  const char* section = R"(style a
width poly 2 "w"
style b
width nosuch 2 "x")";
  // Loading `b`: the drc reader and the plow reader both report the name.
  const auto b = compile(section, true, "b");
  std::size_t count = 0;
  for (const auto& d : b.messages) {
    if (d.text == "Unrecognized layer (type) name \"nosuch\"") {
      ++count;
    }
  }
  CHECK(count == 2);
  // Loading `a`: only the plow reader sees the line of style `b`.
  const auto a = compile(section, true, "a");
  count = 0;
  for (const auto& d : a.messages) {
    if (d.text == "Unrecognized layer (type) name \"nosuch\"") {
      ++count;
    }
  }
  CHECK(count == 1);
}

TEST_CASE("a negative rule distance fails the line", "[cap:drc-rule-language][req:DRC section grammar]") {
  const auto c = compile(R"(width poly -2 "w"
width m1 3 "m")");
  CHECK(c.rules->details()->line_failed);
  CHECK(c.rules->halo().value == 3);
}
