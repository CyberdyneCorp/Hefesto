// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Capability tech-contacts-and-stacking: contact lines, residue checks,
// stackable, stacked types, finalization, residues and contact lookup.

#include <catch2/catch_test_macros.hpp>

#include "tech_fixture.hpp"

namespace tech = hefesto::tech;
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

const std::string kTinyContacts =
    "    pc   poly  metal1\n    ndc  ndiff metal1\n    m2c  metal1 metal2\n    stackable pc m2c pm12c\n";

std::string contacts(std::string_view lines) {
  return with(tiny(), kTinyContacts, lines);
}

}  // namespace

TEST_CASE("contact declaration lines", "[cap:tech-contacts-and-stacking][req:Contact declaration line]") {
  const auto t = tiny_tech();
  const auto* pc = t->contact(15);
  REQUIRE(pc != nullptr);
  CHECK(pc->residues == mask_of({10, 13}));
  CHECK(pc->planes == planes_of({kActive, kMetal1}));
  CHECK_FALSE(pc->stacked);

  auto run = load(contacts("    pc poly metal1\n    contact ndc ndiff metal1\n    device m2c metal1 metal2\n"));
  REQUIRE(run.ok());
  CHECK(run.t().contact(16)->residues == mask_of({11, 13}));
  CHECK(run.t().is_contact(17));

  run = load(contacts("    pc poly metal1\n    nosuch poly metal1\n"));
  CHECK(run.says("Unrecognized layer (type) name \"nosuch\""));
  CHECK_FALSE(run.ok());
  run = load(contacts("    pc\n"));
  CHECK(run.says("Contact type polycontact missing a residue on its home plane"));
}

TEST_CASE("contact keywords after prefix resolution and redeclaration (Q-TECH-17)",
          "[cap:tech-contacts-and-stacking][req:Contact declaration line]") {
  // A type named "contactx" makes the keyword `contact` resolve as a type name in magic-compat mode.
  std::string text = with(tiny(), "    -well   pwell,pw\n", "    -well   pwell,pw\n    metal1 contactx\n");
  text = with(text, kTinyContacts, "    contact ndc ndiff metal1\n");
  CHECK_FALSE(load(text).ok());   // "contact" -> contactx, "ndc" is a contact itself? no: residues ndc,ndiff...
  CHECK(load(text, false).ok());  // magic-compat OFF: the exact keyword wins
  // Redeclaring merges in magic-compat mode and is an error with it OFF.
  const std::string twice = contacts("    pc poly metal1\n    pc poly metal1\n");
  CHECK(load(twice).ok());
  CHECK_FALSE(load(twice, false).ok());
}

TEST_CASE("residue validation", "[cap:tech-contacts-and-stacking][req:Residue validation]") {
  auto run = load(contacts("    pc poly ndiff\n"));
  CHECK(run.says("Contact residues (ndiffusion) must be on different planes"));
  CHECK_FALSE(run.ok());
  run = load(contacts("    m2c poly metal2\n"));
  CHECK(run.says("Contact type m2contact missing a residue on its home plane"));
  run = load(contacts("    pc poly metal1\n    m2c pc metal2\n"));
  CHECK(run.says("Residue type polycontact is a contact itself"));
  run = load(contacts("    pc poly space\n"));
  CHECK(run.says("Residue type space doesn't have a home plane"));
  // Identical residues: accepted with a message (Q-TECH-22).
  std::string twin = with(tiny(), "    -well   pwell,pw\n", "    -well   pwell,pw\n    active pcx\n");
  run = load(with(twin, kTinyContacts, "    pc poly metal1\n    pcx poly metal1\n"));
  CHECK(run.says("Contact residues for pcx identical to those for polycontact"));
  CHECK(run.ok());
}

TEST_CASE("stackable statements", "[cap:tech-contacts-and-stacking][req:stackable statement]") {
  const auto t = tiny_tech();
  CHECK(t->type_name(19) == "pc+m2c");
  CHECK(*t->find_type("pm12c").id == 19);

  auto run = load(contacts("    pc poly metal1\n    ndc ndiff metal1\n    m2c metal1 metal2\n    stackable\n"));
  REQUIRE(run.ok());
  CHECK(run.t().type_count() == 21);
  CHECK(run.t().type_name(19) == "pc+m2c");
  CHECK(run.t().type_name(20) == "ndc+m2c");
  CHECK_FALSE(run.t().find_type("pc+ndc").ok());

  run = load(contacts("    pc poly metal1\n    ndc ndiff metal1\n    m2c metal1 metal2\n    stackable m2c\n"));
  REQUIRE(run.ok());
  CHECK(run.t().type_name(19) == "m2c+pc");
  CHECK(run.t().type_name(20) == "m2c+ndc");

  run = load(contacts("    pc poly metal1\n    ndc ndiff metal1\n    m2c metal1 metal2\n    stackable pc ndc x y\n"));
  CHECK(run.says("Contact types polycontact and ndcontact do not stack"));
  CHECK(run.says("Contact type x unknown or contact missing in stackable statement"));
  CHECK(run.ok());  // form 3 never rejects

  run = load(contacts("    pc poly metal1\n    ndc ndiff metal1\n    m2c metal1 metal2\n    stackable pc m2c a1 a2\n"));
  REQUIRE(run.ok());
  CHECK(*run.t().find_type("a1").id == 19);
  CHECK(*run.t().find_type("a2").id == 19);
  CHECK_FALSE(load(contacts("    pc poly metal1\n    stackable nosuch\n")).ok());
}

TEST_CASE("stacking eligibility", "[cap:tech-contacts-and-stacking][req:Stacking eligibility]") {
  // An explicit three-plane contact suppresses the stacked type.
  std::string text = with(tiny(), "    -well   pwell,pw\n", "    -well   pwell,pw\n    active pm12contact\n");
  text = with(text, kTinyContacts,
              "    pc poly metal1\n    m2c metal1 metal2\n    pm12contact poly metal1 metal2\n    stackable pc m2c\n");
  auto run = load(text);
  REQUIRE(run.ok());
  CHECK(run.t().type_count() == run.t().user_type_end());
  // Contacts that share two planes, or no residue, do not stack.
  run = load(contacts("    pc poly metal1\n    ndc ndiff metal1\n    stackable pc ndc\n"));
  CHECK(run.t().type_count() == 19);
}

TEST_CASE("stacked type properties", "[cap:tech-contacts-and-stacking][req:Stacked type properties]") {
  const auto t = tiny_tech();
  const auto* s = t->contact(19);
  REQUIRE(s != nullptr);
  CHECK(s->stacked);
  CHECK(s->residues == mask_of({15, 17}));
  CHECK(s->component_a == 15);
  CHECK(s->component_b == 17);
  CHECK(s->planes == planes_of({kActive, kMetal1, kMetal2}));
  CHECK(*t->home_plane(19) == kMetal1);
  CHECK(t->type_name(19, tech::NameForm::long_name) == "pc+m2c");

  const std::string via_first = with(tiny(), "metal1  m2contact,m2c,via", "metal1  m2contact,via,m2c");
  CHECK(load(via_first).t().type_name(19) == "pc+via");
}

TEST_CASE("contact finalization gives non-contacts themselves as residue",
          "[cap:tech-contacts-and-stacking][req:Contact finalization and image placement]") {
  const auto t = tiny_tech();
  CHECK(t->full_residues(13) == mask_of({13}));
  CHECK(t->full_residues(19) == mask_of({10, 13, 14}));
  CHECK(t->plane_types(kActive).test(15));
  CHECK(t->plane_types(kMetal1).test(15));
  CHECK(t->contacts().size() == 4);
}

TEST_CASE("residue on a plane", "[cap:tech-contacts-and-stacking][req:Residue on a plane]") {
  const auto t = tiny_tech();
  CHECK(t->residue_on(19, kMetal1) == 13);
  CHECK(t->residue_on(19, kMetal2) == 14);
  CHECK(t->residue_on(19, kActive) == 10);
  CHECK(t->residue_on(15, kMetal2) == 0);
  CHECK(t->residue_on(13, kMetal1) == 13);
}

TEST_CASE("contact lookup by planes", "[cap:tech-contacts-and-stacking][req:Contact lookup by planes]") {
  const auto t = tiny_tech();
  CHECK(t->contact_between(13, 14) == std::optional<tech::TileType>{17});
  CHECK(t->contact_between(10, 13) == std::optional<tech::TileType>{15});
  CHECK_FALSE(t->contact_between(9, 14).has_value());
}
