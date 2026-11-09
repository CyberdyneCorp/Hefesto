// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Capability tech-compose-and-connect: paint/erase/write tables, defaults,
// compose rules, explicit rules, contact rules, finalization steps,
// affected planes, contact locking and connectivity.

#include <catch2/catch_test_macros.hpp>

#include "tech_fixture.hpp"

namespace tech = hefesto::tech;
using tech::TileType;
using tech::test::load;
using tech::test::mask_of;
using tech::test::planes_of;
using tech::test::tiny;
using tech::test::tiny_tech;
using tech::test::with;

namespace {

constexpr tech::PlaneId kActive = 7;
constexpr tech::PlaneId kMetal1 = 8;
constexpr tech::PlaneId kMetal2 = 9;

// Types of the annotated example.
constexpr TileType kNwell = 9;
constexpr TileType kPoly = 10;
constexpr TileType kNdiff = 11;
constexpr TileType kNfet = 12;
constexpr TileType kM1 = 13;
constexpr TileType kM2 = 14;
constexpr TileType kPc = 15;
constexpr TileType kNdc = 16;
constexpr TileType kM2c = 17;
constexpr TileType kPwell = 18;
constexpr TileType kPcM2c = 19;

TileType P(const tech::Technology& t, tech::PlaneId p, TileType h, TileType a) {
  return t.paint_result(p, h, a);
}
TileType E(const tech::Technology& t, tech::PlaneId p, TileType h, TileType a) {
  return t.erase_result(p, h, a);
}

// A scmos-like technology with pdiff/ndiff, wells and contacts pdc/ndc.
std::string scmos_like(std::string_view compose) {
  return tech::test::skeleton("well\nactive\nmetal1,m1\n",
                              "well nwell\nwell pwell\nactive pdiff\nactive ndiff\nmetal1 metal1,m1\n"
                              "active pdc\nactive ndc\n",
                              "pdc pdiff metal1\nndc ndiff metal1\n", compose);
}

}  // namespace

TEST_CASE("write table ignores composition", "[cap:tech-compose-and-connect][req:Paint, erase and write table model]") {
  const auto t = tiny_tech();
  CHECK(t->write_result(kPoly, kNdiff) == kNdiff);
  CHECK(P(*t, 30, kPoly, kNdiff) == kPoly);  // planes beyond K are identity
}

TEST_CASE("default paint and erase rules", "[cap:tech-compose-and-connect][req:Default paint and erase rules]") {
  const auto run = load(with(tiny(), "    compose nfet poly ndiff\n", ""));
  REQUIRE(run.ok());
  const auto& t = run.t();
  CHECK(P(t, kActive, kPoly, kNdiff) == kNdiff);
  CHECK(P(t, kMetal1, kM1, kPoly) == kM1);
  CHECK(E(t, kActive, kPoly, kPoly) == 0);
  CHECK(E(t, kActive, kPoly, kNdiff) == kPoly);
  CHECK(P(t, kActive, 0, kPoly) == kPoly);
  CHECK(P(t, tech::kPlaneDrcError, tech::kErrorP, tech::kErrorS) == tech::kErrorPS);
  CHECK(E(t, tech::kPlaneDrcError, tech::kErrorPS, tech::kErrorS) == tech::kErrorP);
  CHECK(P(t, tech::kPlaneDrcCheck, tech::kCheckSubcell, tech::kCheckPaint) == tech::kCheckPaint);
  CHECK(P(t, tech::kPlaneDrcCheck, tech::kCheckPaint, tech::kCheckSubcell) == tech::kCheckPaint);
}

TEST_CASE("compose line syntax", "[cap:tech-compose-and-connect][req:compose section line syntax]") {
  auto run = load(with(tiny(), "    compose nfet poly ndiff\n", "    Comp nfet poly ndiff\n"));
  REQUIRE(run.ok());
  CHECK(P(run.t(), kActive, kPoly, kNdiff) == kNfet);
  run = load(with(tiny(), "    compose nfet poly ndiff\n", "    \"\" nfet poly ndiff\n"));
  CHECK(run.says("Ambiguous rule type .  Must be one of:"));
  CHECK(load(with(tiny(), "    compose nfet poly ndiff\n", "    d nfet poly ndiff\n")).ok());
  run = load(with(tiny(), "    compose nfet poly ndiff\n", "    x nfet poly ndiff\n"));
  CHECK(run.says("Unknown rule type x"));
  CHECK_FALSE(run.ok());
  run = load(with(tiny(), "    compose nfet poly ndiff\n", "    compose nfet poly\n"));
  CHECK(run.says("Line must contain at least ruletype, result + pair"));
  run = load(with(tiny(), "    compose nfet poly ndiff\n", "    compose nfet poly ndiff m1\n"));
  CHECK(run.says("Types on RHS of rule must be in pairs"));
}

TEST_CASE("compose and decompose for non-contact results",
          "[cap:tech-compose-and-connect][req:compose and decompose rules for non-contact results]") {
  const auto t = tiny_tech();
  CHECK(P(*t, kActive, kPoly, kNdiff) == kNfet);
  CHECK(P(*t, kActive, kNdiff, kPoly) == kNfet);
  CHECK(P(*t, kActive, kNfet, kPoly) == kNfet);
  CHECK(E(*t, kActive, kNfet, kPoly) == kNdiff);
  CHECK(E(*t, kActive, kNfet, kNdiff) == kPoly);

  const auto run = load(with(tiny(), "    compose nfet poly ndiff\n", "    decompose nfet poly ndiff\n"));
  REQUIRE(run.ok());
  CHECK(P(run.t(), kActive, kPoly, kNdiff) == kNdiff);
  CHECK(E(run.t(), kActive, kNfet, kPoly) == kNdiff);
  CHECK(load(with(tiny(), "    compose nfet poly ndiff\n", "    compose nfet pc ndiff\n"))
            .says("Can't have contact layers on RHS of non-contact rule"));
  // Components on different planes: Magic writes on r's plane; magic-compat OFF rejects (Q-TECH-21).
  const std::string mixed = with(tiny(), "    compose nfet poly ndiff\n", "    compose nfet poly nwell\n");
  CHECK(load(mixed).ok());
  CHECK_FALSE(load(mixed, false).ok());
}

TEST_CASE("explicit paint and erase rules", "[cap:tech-compose-and-connect][req:Explicit paint and erase rules]") {
  auto run = load(scmos_like("paint pdiff pwell ndiff\n"));
  REQUIRE(run.ok());
  const auto& t = run.t();
  const TileType pdiff = tech::test::type_of(t, "pdiff");
  const TileType ndiff = tech::test::type_of(t, "ndiff");
  const TileType pwell = tech::test::type_of(t, "pwell");
  CHECK(P(t, 7, pdiff, pwell) == ndiff);

  run = load(scmos_like("paint pdc/active pwell ndc/active\npaint pdc/m1 pwell ndc/m1\n"));
  REQUIRE(run.ok());
  const TileType pdc = tech::test::type_of(run.t(), "pdc");
  const TileType ndc = tech::test::type_of(run.t(), "ndc");
  // Spec scenario "Contact image swap per plane" expects ndc on both planes, but the spec's own
  // algorithm (and Magic's dbTechAddPaintErase) drops the /plane qualifier of h, so the second
  // rule's step 5 writes space on active (Pm = Planes(pdc) minus {metal1}). Recorded as a spec
  // inconsistency; the port follows the algorithm.
  CHECK(P(run.t(), 7, pdc, pwell) == 0);
  CHECK(P(run.t(), 8, pdc, pwell) == ndc);
  // The scmos form `paint pdc pwell ndc` swaps the contact on both planes.
  run = load(scmos_like("paint pdc pwell ndc\n"));
  REQUIRE(run.ok());
  CHECK(P(run.t(), 7, pdc, pwell) == ndc);
  CHECK(P(run.t(), 8, pdc, pwell) == ndc);

  run = load(scmos_like("paint space nwell nwell\n"));
  CHECK(run.says("Must specify plane in paint table for painting space"));
  CHECK_FALSE(run.ok());
  CHECK(load(scmos_like("paint space nwell nwell well\n")).ok());
}

TEST_CASE("explicit erase rules mark the paint entry (Q-TECH-20)",
          "[cap:tech-compose-and-connect][req:Explicit paint and erase rules]") {
  // E[active](pdc, ndiff) set explicitly; the contact defaults overwrite it in magic-compat mode.
  const std::string text = scmos_like("erase pdc ndiff pdiff\n");
  const auto compat = load(text);
  const auto off = load(text, false);
  REQUIRE(compat.ok());
  REQUIRE(off.ok());
  const TileType pdc = tech::test::type_of(compat.t(), "pdc");
  const TileType ndiff = tech::test::type_of(compat.t(), "ndiff");
  const TileType pdiff = tech::test::type_of(compat.t(), "pdiff");
  CHECK(E(compat.t(), 7, pdc, ndiff) == pdc);  // step 3b: an unrelated type leaves the contact
  CHECK(E(off.t(), 7, pdc, ndiff) == pdiff);   // magic-compat OFF keeps the explicit rule
  // A rule on a type sharing no plane with the contact is never overwritten.
  const auto well = load(scmos_like("erase pdc pwell pdiff\n"));
  CHECK(E(well.t(), 7, pdc, tech::test::type_of(well.t(), "pwell")) == pdiff);
}

TEST_CASE("contact compose rules are saved and checked",
          "[cap:tech-compose-and-connect][req:Contact compose and decompose rules]") {
  auto run = load(with(tiny(), "    compose nfet poly ndiff\n", "    compose pc poly ndiff\n"));
  CHECK(run.says("Union of pair planes must = result planes"));
  CHECK_FALSE(run.ok());
  run = load(with(tiny(), "    compose nfet poly ndiff\n", "    compose pc ndc m1\n"));
  CHECK(run.says("Component planes are a superset of result planes") == false);
  run = load(with(tiny(), "    compose nfet poly ndiff\n", "    decompose pc m2c poly\n"));
  CHECK(run.says("Component planes are a superset of result planes"));
  run = load(with(tiny(), "    compose nfet poly ndiff\n", "    decompose pc pc ndc\n"));
  CHECK(run.says("Only one type in each pair may be a contact"));
}

TEST_CASE("finalization order", "[cap:tech-compose-and-connect][req:Compose finalization order]") {
  CHECK(P(*tiny_tech(), kMetal1, kPc, kM1) == kPc);
}

TEST_CASE("step 1 paints all images", "[cap:tech-compose-and-connect][req:Finalization step 1, paint all images]") {
  const auto t = tiny_tech();
  CHECK(P(*t, kMetal1, kM1, kPc) == kPc);
  CHECK(P(*t, kMetal1, 0, kPc) == kPc);
  CHECK(P(*t, kActive, kNdiff, kPc) == kPc);
}

TEST_CASE("step 2 residue-preserving paints", "[cap:tech-compose-and-connect][req:Finalization step 2, residue-preserving paints]") {
  const auto t = tiny_tech();
  CHECK(P(*t, kMetal1, kPc, kM1) == kPc);
  CHECK(P(*t, kActive, kPc, kM1) == kPc);
}

TEST_CASE("step 3a painting over a contact", "[cap:tech-compose-and-connect][req:Finalization step 3a, painting over a contact]") {
  const auto t = tiny_tech();
  CHECK(P(*t, kMetal1, kPc, kM2c) == kPcM2c);
  CHECK(P(*t, kActive, kPc, kM2c) == kPc);
  CHECK(P(*t, kMetal1, kM2c, kPc) == kPcM2c);
  CHECK(P(*t, kMetal2, kM2c, kPc) == kM2c);
  // Q-TECH-23: a non-residue over a contact.
  CHECK(P(*t, kMetal1, kPc, kNdiff) == kM1);
  CHECK(P(*t, kActive, kPc, kNdiff) == kNdiff);
  CHECK(P(*t, kActive, kNdc, kPc) == kPc);
  CHECK(P(*t, kMetal1, kNdc, kPc) == kM1);
}

TEST_CASE("step 3a with explicit contacts of identical residues (Q-TECH-22)",
          "[cap:tech-compose-and-connect][req:Finalization step 3a, painting over a contact]") {
  // Two user contacts with residues {poly, metal1}: painting metal1 over the second gives the first
  // in magic-compat mode and keeps the image type with it OFF.
  std::string text = with(tiny(), "    -well   pwell,pw\n", "    -well   pwell,pw\n    active pcx\n");
  text = with(text, "    stackable pc m2c pm12c\n", "    pcx poly metal1\n    stackable pc m2c pm12c\n");
  const auto compat = load(text);
  const auto off = load(text, false);
  REQUIRE(compat.ok());
  REQUIRE(off.ok());
  const TileType pcx = tech::test::type_of(compat.t(), "pcx");
  CHECK(P(compat.t(), kActive, pcx, kPoly) == kPc);
  CHECK(P(off.t(), kActive, pcx, kPoly) == pcx);
}

TEST_CASE("step 3b erasing from a contact", "[cap:tech-compose-and-connect][req:Finalization step 3b, erasing from a contact]") {
  const auto t = tiny_tech();
  CHECK(E(*t, kMetal1, kPc, kM1) == 0);
  CHECK(E(*t, kActive, kPc, kM1) == kPoly);
  CHECK(E(*t, kActive, kPc, kPoly) == 0);
  CHECK(E(*t, kMetal1, kPc, kPoly) == kM1);
  CHECK(E(*t, kActive, kPc, kNdiff) == kPc);
  CHECK(E(*t, kMetal1, kPc, kNdiff) == kPc);
  CHECK(E(*t, kActive, kPc, kM2c) == kPc);
  CHECK(E(*t, kMetal1, kPc, kM2c) == kPc);
}

TEST_CASE("step 3c stacked types as applied types", "[cap:tech-compose-and-connect][req:Finalization step 3c, stacked types as applied types]") {
  const auto t = tiny_tech();
  CHECK(P(*t, kMetal1, 0, kPcM2c) == kPcM2c);
  CHECK(P(*t, kActive, 0, kPcM2c) == kPc);
  CHECK(P(*t, kMetal2, 0, kPcM2c) == kM2c);
  CHECK(E(*t, kMetal1, kPcM2c, kPcM2c) == 0);
}

TEST_CASE("step 4 saved contact rules", "[cap:tech-compose-and-connect][req:Finalization step 4, saved contact rules]") {
  const auto run = load(scmos_like("compose pdc pdiff m1\n"));
  REQUIRE(run.ok());
  const auto& t = run.t();
  const TileType pdc = tech::test::type_of(t, "pdc");
  const TileType pdiff = tech::test::type_of(t, "pdiff");
  const TileType m1 = tech::test::type_of(t, "m1");
  CHECK(P(t, 7, pdiff, m1) == pdc);
  CHECK(P(t, 7, pdc, pdiff) == pdc);
  CHECK(E(t, 7, pdc, pdiff) == pdiff);
  CHECK(E(t, 7, pdc, m1) == pdiff);
  // P[active](m1, pdiff) is not written by step 4 (active is not a plane of m1): default identity.
  CHECK(P(t, 7, m1, pdiff) == m1);
}

TEST_CASE("saved contact rules do not survive into the next load (Q-TECH-19)",
          "[cap:tech-compose-and-connect][req:Contact compose and decompose rules]") {
  const auto first = load(scmos_like("compose pdc pdiff m1\n"));
  const auto second = load(scmos_like(""));
  REQUIRE(first.ok());
  REQUIRE(second.ok());
  const TileType pdiff = tech::test::type_of(second.t(), "pdiff");
  const TileType m1 = tech::test::type_of(second.t(), "m1");
  CHECK(P(second.t(), 7, pdiff, m1) != P(first.t(), 7, pdiff, m1));
}

TEST_CASE("affected-plane tables", "[cap:tech-compose-and-connect][req:Affected-plane tables]") {
  const auto t = tiny_tech();
  CHECK(t->paint_planes(kPc) == planes_of({kActive, kMetal1}));
  CHECK(t->paint_planes(kM1) == planes_of({kMetal1}));
  CHECK(t->paint_planes(0) == ~tech::plane_bit(0));
  CHECK(t->erase_planes(kPcM2c) == planes_of({kActive, kMetal1, kMetal2}));
}

TEST_CASE("contact locking", "[cap:tech-compose-and-connect][req:Contact locking]") {
  const auto run = load(with(tiny(), "    active  polycontact,pcontact,pc\n", "    -active  polycontact,pcontact,pc\n"));
  REQUIRE(run.ok());
  CHECK(E(run.t(), kMetal1, kPc, kM1) == kPc);
  CHECK(E(run.t(), kActive, kPc, kM1) == kPc);
  CHECK_FALSE(run.t().active_types().test(kPcM2c));  // a component is locked
  CHECK(E(run.t(), kMetal1, kPc, kPc) == 0);
  // Runtime lock/unlock restores the defaults.
  const auto locked = tech::lock_types(*tiny_tech(), mask_of({kPc}));
  CHECK(E(*locked, kMetal1, kPc, kM1) == kPc);
  CHECK_FALSE(locked->active_types().test(kPcM2c));
  const auto unlocked = tech::unlock_types(*locked, mask_of({kPc}));
  CHECK(E(*unlocked, kMetal1, kPc, kM1) == 0);
  CHECK(unlocked->active_types().test(kPcM2c));
}

TEST_CASE("connect section", "[cap:tech-compose-and-connect][req:connect section]") {
  const auto t = tiny_tech();
  CHECK(t->connects_to(kPoly) == mask_of({kPoly, kPc, kPcM2c}));
  CHECK_FALSE(t->connects_to(kPoly).test(kM1));
  CHECK(t->connects_to(kNfet) == mask_of({kNfet}));
  auto run = load(with(tiny(), "    m2,m2c       m2,m2c\n", "    m2,m2c,nosuch       m2,m2c\n"));
  CHECK(run.says("Unrecognized layer (type) name \"nosuch\""));
  CHECK(run.ok());  // unknown names do not reject the line
  CHECK(load(with(tiny(), "    m2,m2c       m2,m2c\n", "    m1\n")).says("Line must contain exactly 2 lists of types"));
}

TEST_CASE("connectivity finalization", "[cap:tech-compose-and-connect][req:Connectivity finalization]") {
  const auto t = tiny_tech();
  const auto conn = t->connects_to(kPcM2c);
  for (const TileType x : {kPoly, kM1, kM2, kPc, kM2c, kNdc}) {
    CHECK(conn.test(x));
  }
  CHECK(t->connects_to(kNdc).test(kPcM2c));  // symmetric
  CHECK(t->cross_planes(kPoly) == 0);
  CHECK(t->contact_planes(kPc) == planes_of({kActive, kMetal1}));
  CHECK(t->contact_planes(kPoly) == 0);
  CHECK_FALSE(t->not_connected(kPoly).test(kPc));
  CHECK(t->not_connected(kPoly).test(kM1));
  CHECK_FALSE(t->not_connected(kPc).test(kNdc));  // contacts sharing a residue (metal1)
  CHECK(t->not_connected(kNwell).test(kPwell));
}
