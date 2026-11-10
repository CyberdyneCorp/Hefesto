// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-rule-language: rule ordering, illegal-edge pruning, covered-rule
// elimination (Q-DRC-5, both modes), scale factor and units with grid
// rescaling (Q-DRC-1, both modes), halo and step size (Q-DRC-2, both
// modes) (tasks.md 4.6, 4.7).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "rules_fixture.hpp"

namespace drc = hefesto::drc;
namespace tech = hefesto::tech;
namespace f = hefesto::drc::flags;
using drc::test::compile;

namespace {

/// rules.tech with its cifoutput section replaced by `cifoutput`.
std::shared_ptr<const tech::Technology> tech_with_cif(const std::string& cifoutput, const std::string& drc_body) {
  std::string text = drc::test::read_text(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "rules.tech");
  const auto start = text.find("cifoutput\n");
  const auto end = text.find("cifinput\n");
  text = text.substr(0, start) + "cifoutput\n" + cifoutput + "\nend\n\n" + text.substr(end);
  text += "\ndrc\n" + drc_body + "\nend\n\nextract\nend\n";
  return drc::test::load_tech_text(text);
}

}  // namespace

TEST_CASE("rules are ordered by distance, a new rule before equal ones", "[cap:drc-rule-language][req:Rule table ordering]") {
  // magic-compat OFF keeps both rules (ON removes "B", see Q-DRC-5 below).
  const auto c = compile(R"(width m1 3 "A"
edge4way space m1 3 m1 m1 3 "B")", false);
  const auto list = c.at("space", "m1");
  REQUIRE(list.size() == 2);
  CHECK(c.why(list[0]) == "B");
  CHECK(c.why(list[1]) == "A");
  // Ascending distances regardless of the order of the lines.
  const auto d = compile(R"(width m1 5 "five"
edge space m1 2 m2 m1 2 "two")");
  const auto ordered = d.at("space", "m1");
  REQUIRE(ordered.size() == 2);
  CHECK(ordered[0].dist <= ordered[1].dist);
}

TEST_CASE("a trigger pair is never split by an inserted rule", "[cap:drc-rule-language][req:Rule table ordering]") {
  const auto c = compile(R"(widespacing m1 10 m1 4 touching_ok "wide"
edge m1 space 3 m1c m1 3 "three"
spacing m1 m1 5 touching_ok "five")", false);
  const auto list = c.at("m1", "space");
  for (std::size_t k = 0; k < list.size(); ++k) {
    if (list[k].has(f::kTrigger)) {
      REQUIRE(k + 1 < list.size());
      CHECK(c.why(list[k + 1]) == "wide");
      CHECK_FALSE(list[k + 1].has(f::kTrigger));
    }
  }
  // The pair is keyed by the triggered rule's distance (4): between 3 and 5.
  REQUIRE(list.size() == 4);
  CHECK(c.why(list[0]) == "three");
  CHECK(list[1].has(f::kTrigger));
  CHECK(c.why(list[3]) == "five");
}

TEST_CASE("the rectangle size rule ignores trigger pairs when inserted", "[cap:drc-rule-language][req:Rule table ordering]") {
  const auto c = compile(R"(rectangle m1c 2 even "r"
width m1c 3 "w")");
  const auto list = c.at("space", "m1c");
  REQUIRE(list.size() >= 2);
  CHECK(list.back().dist == 3);
}

TEST_CASE("an always-illegal edge keeps only its first violated rule", "[cap:drc-rule-language][req:Illegal-edge rule pruning]") {
  for (const bool compat : {true, false}) {
    const auto c = compile(R"(spacing poly ndiff 1 touching_illegal "A"
width ndiff 3 "B")", compat);
    const auto list = c.at("poly", "ndiff");
    REQUIRE(list.size() == 1);
    CHECK(c.why(list[0]) == "A");
    // Other pairs keep the width rule.
    CHECK(c.why(c.at("space", "ndiff").at(0)) == "B");
    CHECK(c.rules->details()->rules_optimized >= 1);
  }
}

TEST_CASE("pruning keeps rules of other edge planes and clears the kept trigger flag",
          "[cap:drc-rule-language][req:Illegal-edge rule pruning]") {
  // The forward trigger of a directional surround is always violated at
  // (m1c, ndc) on metal1: ndc is not m1. It is kept, without TRIGGER, and
  // the rule it triggered (same edge plane) is dropped.
  const auto c = compile(R"(surround m1c m1 1 2 directional "dir")");
  const auto list = c.at("m1c", "ndc");
  REQUIRE(list.size() == 1);
  CHECK(list[0].flags == f::kForward);
  CHECK(list[0].dist == 2);
  // A rule whose OK types are on another plane does not count when the
  // checked type is not on that plane: surround_ok's trigger checks nwell
  // on the well plane at (ndiff, poly), so nothing is pruned there.
  const auto s = compile(R"(spacing ndiff nwell 3 surround_ok "s")");
  CHECK(s.at("ndiff", "poly").size() == 2);
}

TEST_CASE("a rule covered by a later rule is removed", "[cap:drc-rule-language][req:Covered rule elimination]") {
  for (const bool compat : {true, false}) {
    const auto c = compile(R"(width m1 2 "small"
width m1 3 "big")", compat);
    const auto list = c.at("space", "m1");
    REQUIRE(list.size() == 1);
    CHECK(c.why(list[0]) == "big");
  }
}

TEST_CASE("a BOTHCORNERS rule is removed for a single-corner rule with magic-compat ON",
          "[cap:drc-rule-language][req:Covered rule elimination]") {
  const auto c = compile(R"(edge4way space m1 2 m1 m1 2 "A"
edge space m1 3 m1 m1 3 "B")", true);
  const auto list = c.at("space", "m1");
  REQUIRE(list.size() == 1);
  CHECK(c.why(list[0]) == "B");
}

TEST_CASE("a BOTHCORNERS rule is kept with magic-compat OFF", "[cap:drc-rule-language][req:Covered rule elimination]") {
  const auto c = compile(R"(edge4way space m1 2 m1 m1 2 "A"
edge space m1 3 m1 m1 3 "B")", false);
  const auto list = c.at("space", "m1");
  REQUIRE(list.size() == 2);
  CHECK(c.why(list[0]) == "A");
  // OFF also compares MANHATTAN (and the edge plane, RUNLENGTH, DERIVED).
  const auto m = compile(R"(edge space m1 2 m1 m1 2 "A"
edge space m1 3 m1 m1 3 manhattan_dist "B")", false);
  CHECK(m.at("space", "m1").size() == 2);
  const auto mon = compile(R"(edge space m1 2 m1 m1 2 "A"
edge space m1 3 m1 m1 3 manhattan_dist "B")", true);
  CHECK(mon.at("space", "m1").size() == 1);
}

TEST_CASE("triggered rules are removed with their trigger and never cover", "[cap:drc-rule-language][req:Covered rule elimination]") {
  // A triggered rule covered by a later plain rule goes with its trigger
  // (magic-compat ON: the plain rule lacks BOTHCORNERS, which ON ignores).
  const auto c = compile(R"(widespacing m1 10 m1 4 touching_ok "wide"
spacing m1 m1 5 touching_ok "plain")", true);
  const auto list = c.at("m1", "space");
  REQUIRE(list.size() == 1);
  CHECK(c.why(list[0]) == "plain");
  CHECK(compile(R"(widespacing m1 10 m1 4 touching_ok "wide"
spacing m1 m1 5 touching_ok "plain")", false).at("m1", "space").size() == 3);
  // A triggered pair never covers: two identical widespacing rules stay.
  CHECK(compile(R"(widespacing m1 10 m1 4 touching_ok "w1"
widespacing m1 10 m1 4 touching_ok "w2")").at("m1", "space").size() == 4);
  // A plain rule is not covered by a triggered one.
  const auto t = compile(R"(spacing m1 m1 2 touching_ok "plain"
widespacing m1 10 m1 4 touching_ok "wide")");
  CHECK(t.at("m1", "space").size() == 3);
}

TEST_CASE("distances are scaled with ceiling, maxwidth with floor", "[cap:drc-rule-language][req:Scale factor and distance units]") {
  const auto s = compile(R"(scalefactor 10
spacing m1 m1 25 touching_ok "s")");
  const auto list = s.at("m1", "space");
  REQUIRE(list.size() == 1);
  CHECK(list[0].dist == 3);
  CHECK(list[0].cdist == 3);
  CHECK(list[0].file_dist == 25);
  CHECK(s.rules->scale() == drc::ScaleFactor{10, 1});
  const auto m = compile(R"(scalefactor 10
maxwidth m1 25 bend_illegal "mw")");
  CHECK(m.at("space", "m1").at(0).dist == 2);
  CHECK(m.at("space", "m1").at(0).cdist == 3);
  const auto a = compile(R"(scalefactor 10
area m1 1050 20 "a")");
  CHECK(a.at("space", "m1").at(0).dist == 2);
  CHECK(a.at("space", "m1").at(0).cdist == 11);
  // N D: distances times D/N; non-positive values are not scaled.
  const auto nd = compile(R"(scalefactor 3 2
rectangle m1c 7 any "r")");
  const auto r = nd.at("space", "m1c");
  CHECK(r.back().dist == 5);  // ceil(7 * 2 / 3)
  CHECK(r.back().cdist == -1);
}

TEST_CASE("bad scale factors reset to 1", "[cap:drc-rule-language][req:Scale factor and distance units]") {
  const auto c = compile(R"(scalefactor 0
width m1 3 "w")");
  CHECK(c.said("Scale factor must be greater than 0."));
  CHECK(c.said("Setting scale factor to default value 1."));
  CHECK(c.at("space", "m1").at(0).dist == 3);
  // Parsed as floating point and truncated.
  CHECK(compile(R"(scalefactor 10.7
width m1 25 "w")").at("space", "m1").at(0).dist == 3);
  CHECK(compile("scalefactor 1 2 3").said("Wrong number of arguments in scalefactor statement."));
}

TEST_CASE("a finer internal grid scales the rules", "[cap:drc-rule-language][req:Scale factor and distance units]") {
  for (const bool compat : {true, false}) {
    const auto c = compile(R"(width poly 2 "w")", compat);
    const auto finer = drc::rescale(*c.rules, 1, 2, drc::test::compat_mode(compat));
    const auto list = finer->rules(c.type("space"), c.type("poly"));
    REQUIRE(list.size() == 1);
    CHECK(list[0].dist == 4);
    CHECK(list[0].cdist == 4);
    CHECK(finer->halo().value == 4);
    CHECK(finer->step_size() == 2 * c.rules->step_size());
    // The same through load_style with the grid scale.
    const auto loaded = compile(R"(width poly 2 "w")", compat, "", tech::GridScale{2, 1});
    CHECK(loaded.at("space", "poly").at(0).dist == 4);
  }
}

TEST_CASE("an area remainder is lost on rescale with magic-compat ON", "[cap:drc-rule-language][req:Scale factor and distance units]") {
  const auto c = compile(R"(scalefactor 10
area m1 1001 2 "a")", true);
  CHECK(c.at("space", "m1").at(0).cdist == 11);
  const auto finer = drc::rescale(*c.rules, 1, 2, drc::test::compat_mode(true));
  CHECK(finer->rules(0, c.type("m1")).front().cdist == 44);
  CHECK(finer->scale() == drc::ScaleFactor{5, 1});
}

TEST_CASE("an area is recomputed on rescale with magic-compat OFF", "[cap:drc-rule-language][req:Scale factor and distance units]") {
  const auto c = compile(R"(scalefactor 10
area m1 1001 2 "a")", false);
  const auto finer = drc::rescale(*c.rules, 1, 2, drc::test::compat_mode(false));
  CHECK(finer->rules(0, c.type("m1")).front().cdist == 41);
}

TEST_CASE("one-byte remainders wrap above 256 with magic-compat ON", "[cap:drc-rule-language][req:Scale factor and distance units]") {
  // N = 300: 1000 = 3 * 300 + 100 (stored); 556 = 1 * 300 + 256, stored as
  // 0: Magic loses the ceiling at load and the 256 on restore.
  const char* rules = R"(scalefactor 300
width m1 1000 "a"
width m2 556 "b")";
  const auto on = compile(rules, true);
  CHECK(on.at("space", "m1").at(0).dist == 4);
  CHECK(on.at("space", "m2").at(0).dist == 1);
  const auto r_on = drc::rescale(*on.rules, 1, 2, drc::test::compat_mode(true));
  CHECK(r_on->rules(0, on.type("m1")).front().dist == 7);  // restored 1000, ceil(1000 / 150)
  CHECK(r_on->rules(0, on.type("m2")).front().dist == 2);  // restored 1 * 300 = 300 -> 300 / 150
  const auto off = compile(rules, false);
  CHECK(off.at("space", "m2").at(0).dist == 2);  // the true ceiling
  const auto r_off = drc::rescale(*off.rules, 1, 2, drc::test::compat_mode(false));
  CHECK(r_off->rules(0, off.type("m1")).front().dist == 7);
  CHECK(r_off->rules(0, off.type("m2")).front().dist == 4);  // ceil(556 / 150)
  // Rescaling back restores the original scale.
  const auto back = drc::rescale(*r_off, 2, 1, drc::test::compat_mode(false));
  CHECK(back->rules(0, off.type("m2")).front().dist == 2);
}

TEST_CASE("halo and default step size", "[cap:drc-rule-language][req:Halo and step size]") {
  const auto c = compile(R"(widespacing m1 10 m1 3 touching_ok "w")");
  CHECK(c.rules->halo().value == 10);
  CHECK(c.rules->step_size() == 160);
  // A scalefactor scales the halo with ceiling.
  const auto s = compile(R"(scalefactor 10
width m1 25 "w")");
  CHECK(s.rules->halo().value == 3);
  CHECK(s.rules->halo().tech_contribution == 25);
  // No rule distance: 6400 / CIF scale of the current output style (100).
  const auto none = compile("exact_overlap m1c");
  CHECK(none.rules->halo().value == 0);
  CHECK(none.rules->step_size() == 64);
}

TEST_CASE("the stepsize statement", "[cap:drc-rule-language][req:Halo and step size]") {
  CHECK(compile(R"(stepsize 500
width m1 3 "w")").rules->step_size() == 500);
  const auto small = compile("stepsize 10");
  CHECK(small.rules->step_size() == 10);
  CHECK(small.said("Warning: abnormally small DRC step size (10)"));
  const auto bad = compile(R"(stepsize -5
width m1 3 "w")");
  CHECK(bad.said("Step size must be a positive integer."));
  CHECK(bad.rules->step_size() == 48);
  // Rescaling floors the step size.
  const auto odd = compile("stepsize 101");
  CHECK(drc::rescale(*odd.rules, 2, 1, drc::test::compat_mode(true))->step_size() == 50);
}

namespace {

const char* kCifTen = R"(style ten
    scalefactor 10
    layer MET1 metal1
    layer OTHER metal2)";

}  // namespace

TEST_CASE("a mask-layer halo with magic-compat ON", "[cap:drc-rule-language][req:Halo and step size]") {
  const auto c = drc::test::compile_tech(tech_with_cif(kCifTen, R"(scalefactor 10
cifstyle ten
cifwidth MET1 140 "w")"), true);
  CHECK(c.messages.empty());
  CHECK(c.rules->halo().value == 2);
  CHECK(c.rules->halo().mask_contribution == 14);
  REQUIRE(c.rules->details()->mask_layer_rules.size() == 1);
  CHECK(c.rules->details()->mask_layer_rules[0].distance == 140);
  CHECK(c.rules->details()->cif_bound);
}

TEST_CASE("a mask-layer halo with magic-compat OFF", "[cap:drc-rule-language][req:Halo and step size]") {
  const auto c = drc::test::compile_tech(tech_with_cif(kCifTen, R"(scalefactor 10
cifstyle ten
cifwidth MET1 140 "w")"), false);
  CHECK(c.rules->halo().value == 14);
  // Recomputed after a rescale: the tech part with D'/N', the mask part scaled.
  const auto finer = drc::rescale(*c.rules, 1, 2, drc::test::compat_mode(false));
  CHECK(finer->halo().value == 28);
}
