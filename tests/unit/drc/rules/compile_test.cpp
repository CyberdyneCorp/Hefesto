// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-rule-language: the primitive rules each keyword compiles into
// (tasks.md 4.4, 4.5), the spec scenarios as tests on the compiled table,
// with both magic-compat modes for Q-DRC-3 and the lint warnings of
// Q-DRC-3/18/19.
//
// rules.tech: planes well, active, metal1, cut, metal2; ndc (ndiff/metal1)
// and m1c (metal1/metal2) are contacts; via is a plain type on `cut`.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

#include "rules_fixture.hpp"

namespace drc = hefesto::drc;
namespace tech = hefesto::tech;
namespace f = hefesto::drc::flags;
using drc::test::compile;
using drc::test::is_rule;

namespace {

bool any_of(const std::vector<drc::Rule>& rules, const auto& pred) {
  return std::any_of(rules.begin(), rules.end(), pred);
}

}  // namespace

TEST_CASE("width compiles a forward rule and a split-tile reverse rule", "[cap:drc-rule-language][req:width compilation]") {
  const auto c = compile(R"(width poly 2 "w")");
  const auto poly = c.mask("poly");
  REQUIRE(c.at("space", "poly").size() == 1);
  CHECK(is_rule(c.at("space", "poly")[0], f::kForward, 2, 2, poly, poly));
  REQUIRE(c.at("poly", "space").size() == 1);
  CHECK(is_rule(c.at("poly", "space")[0], f::kReverse | f::kSplitTile, 2, 2, poly, poly));
  CHECK(c.at("space", "poly")[0].edge_plane == c.plane("active"));
  CHECK(c.at("space", "poly")[0].check_plane == c.plane("active"));
  // Every edge pair (~S, S) on the plane gets the rule.
  CHECK(c.at("ndiff", "poly").size() == 1);
  // `angles` puts SPLITTILE on the forward rule too.
  const auto a = compile(R"(width poly 3 angles "w")");
  CHECK(a.at("space", "poly")[0].flags == (f::kForward | f::kSplitTile));
  CHECK(a.why(a.at("space", "poly")[0]) == "w");
}

TEST_CASE("width types must share a plane", "[cap:drc-rule-language][req:width compilation]") {
  const auto c = compile(R"(width poly,m1 2 "w")");
  CHECK(c.rules->all_rules().empty());
  CHECK(c.said("All layers for \"width\" must be on same plane"));
}

TEST_CASE("touching_ok spacing of one set", "[cap:drc-rule-language][req:spacing compilation]") {
  const auto c = compile(R"(spacing m1,m1c m1,m1c 3 touching_ok "s")");
  const auto ok = c.plane_types("metal1") & ~c.mask("m1,m1c");
  const auto corner = c.plane_types("metal1") & ~c.mask("m1,m1c");
  for (const char* t : {"m1", "m1c"}) {
    const auto list = c.at(t, "space");
    REQUIRE(list.size() == 1);
    CHECK(is_rule(list[0], f::kForward, 3, 3, ok, corner));
    CHECK(list[0].check_plane == c.plane("metal1"));
  }
  CHECK_FALSE(any_of(c.at("space", "m1"), [](const drc::Rule& r) { return r.reverse(); }));
}

TEST_CASE("touching_ok spacing of two sets checks both directions", "[cap:drc-rule-language][req:spacing compilation]") {
  const auto c = compile(R"(spacing poly ndiff 2 touching_ok "s")");
  const auto r = c.plane_types("active") & ~c.mask("poly,ndiff");  // R1 = R2 on the plane
  const auto fwd = c.at("poly", "space");
  REQUIRE(fwd.size() == 1);
  CHECK(is_rule(fwd[0], f::kForward | f::kBothCorners, 2, 2, c.plane_types("active") & ~c.mask("ndiff"), r));
  const auto rev = c.at("space", "poly");
  REQUIRE(rev.size() == 1);
  CHECK(rev[0].flags == (f::kReverse | f::kBothCorners));
  const auto other = c.at("ndiff", "space");
  REQUIRE(other.size() == 1);
  CHECK(other[0].ok == (c.plane_types("active") & ~c.mask("poly")));
}

TEST_CASE("touching_illegal spacing of two sets", "[cap:drc-rule-language][req:spacing compilation]") {
  const auto c = compile(R"(spacing poly ndiff 1 touching_illegal "pd")");
  const auto ok1 = c.plane_types("active") & ~c.mask("ndiff");
  const auto corner1 = c.plane_types("active") & ~c.mask("poly");
  for (const char* b : {"space", "ndiff"}) {
    const auto list = c.at("poly", b);
    REQUIRE_FALSE(list.empty());
    CHECK(is_rule(list[0], f::kForward, 1, 1, ok1, corner1));
  }
  const auto ok2 = c.plane_types("active") & ~c.mask("poly");
  for (const char* b : {"space", "poly"}) {
    const auto list = c.at("ndiff", b);
    REQUIRE_FALSE(list.empty());
    CHECK(list[0].ok == ok2);
    CHECK(list[0].flags == f::kForward);
  }
  CHECK(c.at("space", "poly").empty());  // no reverse rules
}

TEST_CASE("touching_illegal across planes adds the coincidence rule last", "[cap:drc-rule-language][req:spacing compilation]") {
  const auto c = compile(R"(spacing via m2 2 touching_illegal "vm")");
  // Edge (space, via) on the cut plane: the forward spacing rule of the m2
  // side does not exist here; the coincidence rule is appended at the end.
  const auto list = c.at("space", "via");
  REQUIRE_FALSE(list.empty());
  const auto& last = list.back();
  CHECK(last.dist == 1);
  CHECK(last.cdist == 2);
  CHECK(last.ok == (c.plane_types("metal2") & ~c.mask("m2")));
  CHECK(last.corner.empty());
  CHECK(last.check_plane == c.plane("metal2"));
  CHECK(last.edge_plane == c.plane("cut"));
  // The via side checks m2 on the metal2 plane.
  const auto fwd = c.at("via", "space");
  REQUIRE_FALSE(fwd.empty());
  CHECK(fwd[0].check_plane == c.plane("metal2"));
}

TEST_CASE("manhattan_dist spacing is touching_illegal with MANHATTAN and no DERIVED flags",
          "[cap:drc-rule-language][req:spacing compilation]") {
  const auto c = compile(R"(spacing m1c m1c 2 manhattan_dist "m")");
  const auto list = c.at("m1c", "space");
  REQUIRE_FALSE(list.empty());
  CHECK(list[0].flags == (f::kForward | f::kManhattan));
}

TEST_CASE("DERIVED flags mark contact cuts", "[cap:drc-rule-language][req:spacing compilation]") {
  // m1c in a set without metal1 stands for its cut: DERIVED_EDGE at an m1c
  // edge, DERIVED_TARGET when checking towards m1c.
  const auto c = compile(R"(spacing m1c m1c 2 touching_illegal "cut")");
  const auto list = c.at("m1c", "space");
  REQUIRE_FALSE(list.empty());
  CHECK(list[0].flags == (f::kForward | f::kDerivedEdge | f::kDerivedTarget));
  // With metal1 in the set, m1c is a metal1 edge.
  const auto metal = compile(R"(spacing m1,m1c m1,m1c 2 touching_illegal "m")");
  CHECK(metal.at("m1c", "space")[0].flags == f::kForward);
}

TEST_CASE("surround_ok spacing becomes a trigger pair", "[cap:drc-rule-language][req:spacing compilation]") {
  const auto c = compile(R"(spacing ndiff nwell 3 surround_ok "s")");
  const auto list = c.at("ndiff", "space");
  REQUIRE(list.size() >= 2);
  CHECK(list[0].flags == (f::kForward | f::kTrigger));
  CHECK(list[0].dist == 1);
  CHECK(list[0].ok == c.mask("nwell"));
  CHECK(list[1].flags == (f::kForward | f::kBothCorners));
  CHECK(list[1].dist == 3);
  const auto rev = c.at("space", "ndiff");
  REQUIRE(rev.size() >= 2);
  CHECK(rev[0].flags == (f::kReverse | f::kTrigger));
  CHECK(rev[1].flags == (f::kReverse | f::kBothCorners));
  CHECK(c.at("nwell", "space").empty());  // no S2-side rules
  // Same plane: warned and compiled as touching_ok.
  const auto same = compile(R"(spacing poly ndiff 2 surround_ok "s")");
  CHECK(same.said("\"surround_ok\" used when spacing rule types are in the same plane.  Did you mean "
                  "\"touching_ok\"?"));
  CHECK(same.messages[0].code == "DRC-TECH-002");
  CHECK(same.at("poly", "space")[0].flags == (f::kForward | f::kBothCorners));
}

TEST_CASE("corner_ok spacing", "[cap:drc-rule-language][req:spacing compilation]") {
  const auto c = compile(R"(spacing poly ndiff 1 corner_ok nfet "pd")");
  const auto x = ~c.mask("poly,nfet");
  const auto fwd = c.at("poly", "space");
  REQUIRE(fwd.size() == 1);
  CHECK(is_rule(fwd[0], f::kForward | f::kBothCorners, 1, 1, ~c.mask("ndiff"), x));
  CHECK(is_rule(c.at("space", "poly")[0], f::kReverse | f::kBothCorners, 1, 1, ~c.mask("ndiff"), x));
  CHECK(c.at("poly", "nfet").empty());
  const auto bad = compile(R"(spacing poly m1 1 corner_ok nfet "pd")");
  CHECK(bad.said("Spacing check with \"corner_ok\" must have all types in one plane."));
}

TEST_CASE("spacing messages", "[cap:drc-rule-language][req:spacing compilation]") {
  CHECK(compile(R"(spacing poly ndiff 1 sideways "x")")
            .said("Badly formed drc spacing line:  need \"touching_ok\", \"touching_illegal\", or \"surround_ok\"."));
  CHECK(compile(R"(spacing poly ndiff 1 touching_ok nfet "x")").said("Unknown argument in spacing line."));
  CHECK(compile(R"(spacing poly m1 1 touching_ok "x")")
            .said("Spacing check with \"touching_ok\" must have all types in one plane.  Possibly you want "
                  "\"surround_ok\"?"));
  CHECK(compile(R"(spacing ndc,m2 poly 1 touching_illegal "x")")
            .said("Types in first list must either be in one plane or else types must not share planes."));
}

TEST_CASE("multiplane spacing compiles the first plane pair with magic-compat ON",
          "[cap:drc-rule-language][req:spacing compilation]") {
  const char* rule = R"(spacing poly,m1 m2 2 touching_illegal "mp")";
  const auto on = compile(rule, true);
  CHECK_FALSE(on.at("poly", "space").empty());  // (active, metal2)
  CHECK(on.at("m1", "space").empty());          // (metal1, metal2) dropped
  REQUIRE(on.count_code("DRC-LINT-001") == 1);
  const auto lint = std::find_if(on.messages.begin(), on.messages.end(),
                                 [](const auto& d) { return d.code == "DRC-LINT-001"; });
  CHECK(lint->native_only);
  CHECK(lint->text.find("(metal1, metal2)") != std::string::npos);
}

TEST_CASE("multiplane spacing compiles every plane pair with magic-compat OFF",
          "[cap:drc-rule-language][req:spacing compilation]") {
  const auto off = compile(R"(spacing poly,m1 m2 2 touching_illegal "mp")", false);
  CHECK_FALSE(off.at("poly", "space").empty());
  REQUIRE_FALSE(off.at("m1", "space").empty());
  CHECK(off.at("m1", "space")[0].check_plane == off.plane("metal2"));
  CHECK(off.count_code("DRC-LINT-001") == 1);
}

TEST_CASE("widespacing compiles trigger pairs", "[cap:drc-rule-language][req:widespacing compilation]") {
  const auto c = compile(R"(widespacing m1 10 m1 4 touching_ok "wide")");
  const auto m1 = c.mask("m1");
  const auto list = c.at("m1", "space");
  REQUIRE(list.size() == 2);
  CHECK(is_rule(list[0], f::kReverse | f::kMaxWidth | f::kTrigger | f::kBends, 10, 4, m1, m1));
  CHECK(list[1].flags == (f::kForward | f::kBothCorners));
  CHECK(list[1].dist == 4);
  CHECK(list[1].cdist == 4);
  const auto rev = c.at("space", "m1");
  REQUIRE(rev.size() == 2);
  CHECK(rev[0].flags == (f::kForward | f::kMaxWidth | f::kTrigger | f::kBends));
  CHECK(rev[1].flags == (f::kReverse | f::kBothCorners));
  CHECK(c.rules->halo().value == 10);
}

TEST_CASE("widespacing with a run length", "[cap:drc-rule-language][req:widespacing compilation]") {
  const auto c = compile(R"(widespacing m1 10 20 m1 4 touching_ok "wide")");
  const auto list = c.at("m1", "space");
  REQUIRE(list.size() == 2);
  CHECK(list[0].cdist == 20);
  CHECK(list[1].flags == (f::kForward | f::kRunLength | f::kBothCorners));
  CHECK(list[1].cdist == 20);
  CHECK(c.at("space", "m1")[1].flags == (f::kReverse | f::kRunLength | f::kBothCorners));
  CHECK(compile(R"(widespacing ndiff 10 nwell 4 surround_ok "w")").said("Widespacing checks cannot use \"surround_ok\"."));
}

TEST_CASE("edge4way compiles both directions with both corners", "[cap:drc-rule-language][req:edge and edge4way compilation]") {
  const auto c = compile(R"(edge4way nfet ndiff 2 ndiff,ndc ndiff 2 "overhang")");
  REQUIRE(c.at("nfet", "ndiff").size() == 1);
  CHECK(is_rule(c.at("nfet", "ndiff")[0], f::kForward | f::kBothCorners, 2, 2, c.mask("ndiff,ndc"), c.mask("ndiff")));
  REQUIRE(c.at("ndiff", "nfet").size() == 1);
  CHECK(is_rule(c.at("ndiff", "nfet")[0], f::kReverse | f::kBothCorners, 2, 2, c.mask("ndiff,ndc"), c.mask("ndiff")));
  // `edge` is forward only; manhattan_dist sets MANHATTAN; halo max(d, c).
  const auto e = compile(R"(edge nfet ndiff 2 ndiff ndiff 5 manhattan_dist "e")");
  CHECK(e.at("nfet", "ndiff")[0].flags == (f::kForward | f::kManhattan));
  CHECK(e.at("ndiff", "nfet").empty());
  CHECK(e.rules->halo().value == 5);
}

TEST_CASE("edge rule plane rules and messages", "[cap:drc-rule-language][req:edge and edge4way compilation]") {
  CHECK(compile(R"(edge poly m1 2 m1 m1 2 "e")").said("All edges in edge rule must lie in shared planes."));
  CHECK(compile(R"(edge poly ndiff 2 poly m1 2 "e")").said("Corner types aren't in same plane as edges."));
  CHECK(compile(R"(edge poly ndiff 2 poly,m1 ndiff 2 "e")").said("All OK types must lie in one plane."));
  const auto w = compile(R"(edge poly poly,ndiff 2 ndiff ndiff 2 "e")");
  CHECK(w.said("Warning:  types1 and types2 have nonempty intersection."));
  // OK = space takes the edge plane, with a message unless a plane is given.
  const auto s = compile(R"(edge poly ndiff 2 space ndiff 2 "e")");
  CHECK(s.said("OK types \"space\" in more than one plane.\n\tAssuming same plane (active) as edge."));
  CHECK(s.at("poly", "ndiff")[0].check_plane == s.plane("active"));
  const auto p = compile(R"(edge poly ndiff 2 space ndiff 2 "e" active)");
  CHECK(p.messages.empty());
  CHECK(compile(R"(edge poly ndiff 2 ndiff ndiff 2 "e" metal1)").said("Ignoring bad plane argument."));
  // OK types on another plane: edge on the pair's plane, check on OK's.
  const auto x = compile(R"(edge via space 1 m2 0 0 "v")");
  REQUIRE_FALSE(x.at("via", "space").empty());
  CHECK(x.at("via", "space")[0].edge_plane == x.plane("cut"));
  CHECK(x.at("via", "space")[0].check_plane == x.plane("metal2"));
}

TEST_CASE("extend compiles same-plane and multi-plane rules", "[cap:drc-rule-language][req:extend compilation]") {
  const auto c = compile(R"(extend nfet poly 3 "gate length")");
  const auto m = c.mask("nfet,poly");
  REQUIRE(c.at("poly", "nfet").size() == 1);
  CHECK(is_rule(c.at("poly", "nfet")[0], f::kForward, 3, 0, m, tech::TypeMask{}));
  CHECK(is_rule(c.at("nfet", "poly")[0], f::kReverse, 3, 0, m, tech::TypeMask{}));
  const auto exact = compile(R"(extend nfet poly 3 exact_width "gate")");
  const auto list = exact.at("poly", "nfet");
  REQUIRE(list.size() == 2);
  CHECK(any_of(list, [&](const drc::Rule& r) { return r.flags == (f::kForward | f::kOutside) && r.ok == ~exact.mask("nfet"); }));
  const auto excl = compile(R"(extend nfet poly 3 exclusive "gate")");
  CHECK(excl.at("poly", "nfet")[0].ok == excl.mask("nfet"));
  // Multi-plane: a trigger on S2's plane, checked on S1's plane.
  const auto mp = compile(R"(extend m2 via 2 "m2 over via")");
  const auto tr = mp.at("via", "space");
  REQUIRE(tr.size() == 2);
  CHECK(tr[0].flags == (f::kForward | f::kTrigger));
  CHECK(tr[0].dist == 1);
  CHECK(tr[0].ok == ~mp.mask("m2"));
  CHECK(tr[1].flags == f::kForward);
  CHECK(tr[1].check_plane == mp.plane("metal2"));
}

TEST_CASE("overhang compiles both-corner rules", "[cap:drc-rule-language][req:overhang compilation]") {
  const auto c = compile(R"(overhang poly nfet 2 "endcap")");
  const auto m = c.mask("nfet,poly");
  for (const char* b : {"space", "poly"}) {
    REQUIRE(c.at("nfet", b).size() == 1);
    CHECK(is_rule(c.at("nfet", b)[0], f::kForward | f::kBothCorners, 2, 2, m, m));
    REQUIRE(c.at(b, "nfet").size() == 1);
    CHECK(is_rule(c.at(b, "nfet")[0], f::kReverse | f::kBothCorners, 2, 2, m, m));
  }
  CHECK(compile(R"(overhang poly poly,nfet 2 "x")").said("Warning:  inside and outside types have nonempty intersection."));
  // Different planes: trigger pairs on S2's plane checked on S1's.
  const auto mp = compile(R"(overhang m2 via 1 "m2 over via")");
  const auto list = mp.at("m2", "space");
  REQUIRE(list.size() == 2);
  CHECK(list[0].flags == (f::kForward | f::kTrigger));
  CHECK(is_rule(list[1], f::kForward, 1, 1, mp.mask("via"), mp.mask("via")));
  CHECK(list[1].check_plane == mp.plane("cut"));
}

TEST_CASE("via surround, absence illegal, different planes", "[cap:drc-rule-language][req:surround compilation]") {
  const auto c = compile(R"(surround via m2 1 absence_illegal "m2 surround")");
  const auto base = c.at("space", "m2");
  REQUIRE(base.size() == 1);
  CHECK(is_rule(base[0], f::kForward | f::kBothCorners | f::kDerivedTarget, 1, 1, ~c.mask("via"), c.mask("m2")));
  CHECK(base[0].check_plane == c.plane("cut"));
  CHECK(base[0].edge_plane == c.plane("metal2"));
  CHECK(c.at("m2", "space")[0].flags == (f::kReverse | f::kBothCorners | f::kDerivedTarget));
  const auto presence = c.at("via", "space");
  REQUIRE(presence.size() == 1);
  CHECK(is_rule(presence[0], f::kForward | f::kBothCorners, 1, 1, c.mask("m2"), ~c.mask("via")));
  CHECK(presence[0].check_plane == c.plane("metal2"));
  // absence_ok has no presence rule.
  CHECK(compile(R"(surround via m2 1 absence_ok "x")").at("via", "space").empty());
}

TEST_CASE("surround presence rule on a shared plane", "[cap:drc-rule-language][req:surround compilation]") {
  const auto c = compile(R"(surround ndc ndiff 1 absence_illegal "s")");
  const auto x = ~c.mask("ndc,ndiff");
  const auto list = c.at("ndc", "space");
  REQUIRE_FALSE(list.empty());
  CHECK(any_of(list, [&](const drc::Rule& r) {
    return is_rule(r, f::kForward | f::kBothCorners, 1, 1, tech::TypeMask{}, x);
  }));
}

TEST_CASE("exact_width surround adds the maximum rule on shared planes", "[cap:drc-rule-language][req:surround compilation]") {
  const auto c = compile(R"(surround m1c m2 1 exact_width "exact")");
  const auto list = c.at("m1c", "m2");
  REQUIRE_FALSE(list.empty());
  CHECK(any_of(list, [&](const drc::Rule& r) {
    return r.flags == (f::kForward | f::kBothCorners | f::kOutside) && r.ok == ~c.mask("m2");
  }));
  CHECK(c.count_code("DRC-LINT-002") == 0);
}

TEST_CASE("cross-plane exact_width gets only absence rules and a lint warning in both modes",
          "[cap:drc-rule-language][req:surround compilation]") {
  for (const bool compat : {true, false}) {
    const auto c = compile(R"(surround ndiff nwell 1 exact_width "x")", compat);
    CHECK(c.count_code("DRC-LINT-002") == 1);
    CHECK_FALSE(any_of(c.rules->all_rules().empty() ? std::vector<drc::Rule>{} : c.at("ndiff", "space"),
                       [](const drc::Rule& r) { return r.has(f::kOutside); }));
    CHECK_FALSE(c.at("ndiff", "space").empty());  // the absence_illegal rule
  }
}

TEST_CASE("directional surround", "[cap:drc-rule-language][req:surround compilation]") {
  const auto c = compile(R"(surround m1c m1 1 2 directional "dir")");
  const auto list = c.at("m1c", "space");
  REQUIRE(list.size() == 2);
  CHECK(is_rule(list[0], f::kForward | f::kTrigger, 2, 0, c.mask("m1"), tech::TypeMask{}));
  CHECK(is_rule(list[1], f::kReverse | f::kBothCorners, 1, 1, c.mask("m1c,m1"), tech::TypeMask::all()));
  const auto rev = c.at("space", "m1c");
  REQUIRE(rev.size() == 2);
  CHECK(rev[0].flags == (f::kReverse | f::kTrigger));
  CHECK(rev[1].flags == (f::kForward | f::kBothCorners));
  CHECK(compile(R"(surround m1c m1 1 2x directional "dir")").said("Incorrect arguments in directional \"surround\" rule"));
}

TEST_CASE("rect_only compiles both-corner rules", "[cap:drc-rule-language][req:rect_only and rectangle compilation]") {
  const auto c = compile(R"(rect_only m1c "contact must be rectangular")");
  REQUIRE(c.at("m1c", "space").size() == 1);
  CHECK(is_rule(c.at("m1c", "space")[0], f::kForward | f::kBothCorners, 1, 1, ~c.mask("m1c"),
                c.plane_types("metal1")));
  CHECK(c.at("space", "m1c")[0].flags == (f::kReverse | f::kBothCorners));
  CHECK(c.rules->halo().value == 1);
}

TEST_CASE("rectangle compiles rectangle-ness and size rules", "[cap:drc-rule-language][req:rect_only and rectangle compilation]") {
  const auto c = compile(R"(rectangle m1c 4 even "r")");
  CHECK(is_rule(c.at("m1c", "space")[0], f::kForward, 1, 1, ~c.mask("m1c"), tech::TypeMask::all()));
  const auto rev = c.at("space", "m1c");
  REQUIRE(rev.size() == 2);
  CHECK(is_rule(rev[0], f::kReverse, 1, 1, ~c.mask("m1c"), tech::TypeMask::all()));
  CHECK(is_rule(rev[1], f::kRectSize, 4, 0, c.mask("m1c"), tech::TypeMask{}));
  CHECK(c.rules->halo().value == 4);
  CHECK(compile(R"(rectangle m1c 4 any "r")").at("space", "m1c")[1].cdist == -1);
  CHECK(compile(R"(rectangle m1c 4 odd "r")").at("space", "m1c")[1].cdist == 1);
  CHECK(compile(R"(rectangle m1c x even "r")").said("bad maxwidth in rectangle rule"));
  CHECK(compile(R"(rectangle m1c 4 foo "r")").said("bad [even|odd|any] selection in rectangle rule"));
  CHECK(compile(R"(rectangle poly,m1 4 even "r")").said("Layers in rectangle rule must lie in a single plane."));
}

TEST_CASE("area compiles a forward area rule", "[cap:drc-rule-language][req:area, maxwidth and off_grid compilation]") {
  const auto c = compile(R"(area m1 4 2 "area")");
  REQUIRE(c.at("space", "m1").size() == 1);
  CHECK(is_rule(c.at("space", "m1")[0], f::kArea | f::kForward, 2, 4, c.mask("m1"), c.mask("m1")));
  CHECK(c.at("m1", "space").empty());
  CHECK(c.rules->halo().value == 2);
}

TEST_CASE("maxwidth bends options", "[cap:drc-rule-language][req:area, maxwidth and off_grid compilation]") {
  const auto c = compile(R"(maxwidth m1 100 "slot")");
  REQUIRE(c.at("space", "m1").size() == 1);
  CHECK(is_rule(c.at("space", "m1")[0], f::kMaxWidth | f::kBends, 100, 100, c.mask("m1"), c.mask("m1")));
  CHECK(compile(R"(maxwidth m1 0 "zero")").at("space", "m1")[0].flags == f::kMaxWidth);
  CHECK(compile(R"(maxwidth m1 10 bend_illegal "x")").at("space", "m1")[0].flags == f::kMaxWidth);
  CHECK(compile(R"(maxwidth m1 10 bend_ok "x")").at("space", "m1")[0].flags == (f::kMaxWidth | f::kBends));
  CHECK(compile(R"(maxwidth m1 10 both "x")").at("space", "m1")[0].flags == (f::kMaxWidth | f::kMaxWidthBoth));
  const auto bad = compile(R"(maxwidth m1 10 sideways "x")");
  CHECK(bad.said("unknown bend option sideways"));
  CHECK(bad.rules->all_rules().empty());
}

TEST_CASE("maxwidth exclude types", "[cap:drc-rule-language][req:area, maxwidth and off_grid compilation]") {
  const auto ok = compile(R"(maxwidth m1 10 bend_ok m2 "x")");
  const auto rule = ok.at("space", "m1").at(0);
  CHECK(rule.corner == ok.mask("m2"));
  CHECK(rule.check_plane == ok.plane("metal2"));
  CHECK(ok.count_code("DRC-LINT-003") == 0);
  const auto same = compile(R"(maxwidth m1 10 bend_ok m1c "x")");
  CHECK(same.said("Warning:  Exclude types for \"maxwidth\" are on the same plane and so cannot be checked."));
}

TEST_CASE("maxwidth exclude types with bend_illegal or both warn in both modes",
          "[cap:drc-rule-language][req:area, maxwidth and off_grid compilation]") {
  for (const bool compat : {true, false}) {
    for (const char* bends : {"bend_illegal", "both"}) {
      const auto c = compile(std::string{"maxwidth m1 10 "} + bends + R"( m2 "x")", compat);
      CHECK(c.count_code("DRC-LINT-003") == 1);
      // Replicated: the rule still stores the exclude types (Q-DRC-19).
      CHECK(c.at("space", "m1").at(0).corner == c.mask("m2"));
    }
  }
}

TEST_CASE("off_grid compiles both directions", "[cap:drc-rule-language][req:area, maxwidth and off_grid compilation]") {
  const auto c = compile(R"(off_grid m1 5 "grid")");
  CHECK(is_rule(c.at("space", "m1").at(0), f::kOffGrid | f::kForward, 5, 0, c.mask("m1"), c.mask("m1")));
  CHECK(c.at("m1", "space").at(0).flags == (f::kOffGrid | f::kReverse));
  // Types need not share a plane.
  const auto multi = compile(R"(off_grid poly,m1 5 "grid")");
  CHECK_FALSE(multi.at("space", "poly").empty());
  CHECK_FALSE(multi.at("space", "m1").empty());
  CHECK(multi.messages.empty());
}

TEST_CASE("angles 90 compiles ANGLES_90 rules", "[cap:drc-rule-language][req:angles compilation]") {
  const auto c = compile(R"(angles poly 90 "no diagonal poly")");
  REQUIRE(c.at("space", "poly").size() == 1);
  CHECK(is_rule(c.at("space", "poly")[0], f::kAngles90, 1, 1, c.mask("poly"), c.mask("poly")));
  CHECK(c.at("poly", "space").at(0).flags == (f::kAngles90 | f::kReverse));
  CHECK(c.at("ndiff", "poly").size() == 1);
  CHECK(c.rules->halo().value == 1);
}

TEST_CASE("angles 45 never reads past the token (Q-DRC-4)", "[cap:drc-rule-language][req:angles compilation]") {
  const auto both = compile(R"(angles poly 45 "x")");
  CHECK(both.at("space", "poly").at(0).flags == (f::kAngles45 | f::kAngles90));
  const auto only = compile(R"(angles poly 45-only "x")");
  CHECK(any_of(only.at("space", "poly"), [](const drc::Rule& r) { return r.flags == f::kAngles45; }));
  // The rect_only-style corner rules of the 45-only form.
  CHECK(any_of(only.at("poly", "space"), [&](const drc::Rule& r) {
    return r.flags == (f::kAngles45 | f::kForward | f::kBothCorners) && r.ok == ~only.mask("poly");
  }));
  CHECK(any_of(only.at("space", "poly"), [&](const drc::Rule& r) {
    return r.flags == (f::kAngles45 | f::kForward | f::kBothCorners) && r.ok == ~only.mask("space");
  }));
  // Without exactly one separator before "only", 45 means 45 and 90.
  CHECK(compile(R"(angles poly 45only "x")").at("space", "poly").at(0).flags == (f::kAngles45 | f::kAngles90));
  CHECK(compile(R"(angles poly 30 "x")").said("angles must be 45 or 90"));
}

TEST_CASE("no_overlap and exact_overlap statements", "[cap:drc-rule-language][req:Overlap rules and the DRC paint table]") {
  const auto c = compile(R"(no_overlap m1 poly
exact_overlap m1c,ndc)");
  for (std::size_t plane = 0; plane < c.tech->plane_count(); ++plane) {
    const auto p = static_cast<tech::PlaneId>(plane);
    CHECK(c.rules->paint_result(p, c.type("poly"), c.type("m1")) == tech::kErrorS);
    CHECK(c.rules->paint_result(p, c.type("m1"), c.type("poly")) == tech::kErrorS);
  }
  CHECK(c.rules->exact_overlap_types() == c.mask("m1c,ndc"));
  CHECK(c.rules->halo().value == 0);
}

TEST_CASE("the default DRC paint table marks illegal overlaps", "[cap:drc-rule-language][req:Overlap rules and the DRC paint table]") {
  const auto c = compile("");
  const auto active = c.plane("active");
  // Painting poly over ndiff gives nfet in the database: illegal between cells.
  CHECK(c.tech->paint_result(active, c.type("ndiff"), c.type("poly")) == c.type("nfet"));
  CHECK(c.rules->paint_result(active, c.type("ndiff"), c.type("poly")) == tech::kErrorS);
  // Space and error_s rows.
  CHECK(c.rules->paint_result(active, tech::kSpace, c.type("poly")) == c.type("poly"));
  CHECK(c.rules->paint_result(active, c.type("poly"), tech::kErrorS) == tech::kErrorS);
  CHECK(c.rules->paint_result(active, tech::kErrorS, c.type("poly")) == tech::kErrorS);
  // A type over itself is fine.
  CHECK(c.rules->paint_result(active, c.type("poly"), c.type("poly")) == c.type("poly"));
}
