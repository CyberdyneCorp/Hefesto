// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-rule-semantics "Exceptions and exemptions at check time": rules
// scoped by the MASKHINTS_<name> hint region of the checked cell, with the
// inside test of hints::inside in both magic-compat modes (Q-DRC-11 with it
// ON, Q-MISC-13 with it OFF), evaluated before the rule's flags.

#include "check_fixture.hpp"

using namespace hefesto;
using namespace hefesto::drc::test;
using drc::RuleSetBuilder;

#define EXCEPTIONS "[cap:drc-rule-semantics][req:Exceptions and exemptions at check time]"

namespace {

/// `exception X` (or `exemption X`) + `width m1 3 "w"` + `exception none`.
std::shared_ptr<const drc::RuleSet> scoped_width(bool exemption) {
  const CheckTech& t = check_tech();
  RuleSetBuilder b;
  const std::uint8_t tag = exemption ? drc::kExemptionBit : std::uint8_t{0};
  add_width(b, mask_of({t.m1}), t.metal1, 3, b.why("w"), tag);
  b.exception_names({"X"});
  return b.build();
}

}  // namespace

TEST_CASE("exempted spacing inside SRAM (spec scenario)", EXCEPTIONS) {
  const CheckTech& t = check_tech();
  RuleSetBuilder b;
  add_spacing(b, mask_of({t.m1}), t.metal1, 3, b.why("s"), 0, drc::kExemptionBit);
  b.exception_names({"SRAM"});
  const auto rules = b.build();
  for (const bool compat : {true, false}) {
    Layout l;
    l.hint("SRAM", {Rect{0, 0, 100, 100}});
    l.paint(t.m1, Rect{10, 10, 20, 20});
    l.paint(t.m1, Rect{21, 10, 30, 20});
    CHECK(run_check(l, *rules, Options{.magic_compat = compat}).empty());
    // The same shapes outside the hint area report the spacing error.
    l.paint(t.m1, Rect{200, 10, 210, 20});
    l.paint(t.m1, Rect{211, 10, 220, 20});
    CHECK(run_check(l, *rules, Options{.magic_compat = compat}) == std::vector<Rect>{Rect{211, 10, 213, 20}});
  }
}

TEST_CASE("a cell without the hint property is outside every hint", EXCEPTIONS) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 20, 2});
  for (const bool compat : {true, false}) {
    CHECK(run_check(l, *scoped_width(false), Options{.magic_compat = compat}).empty());  // exception: skipped
    CHECK_FALSE(run_check(l, *scoped_width(true), Options{.magic_compat = compat}).empty());  // exemption: applied
  }
}

TEST_CASE("horizontal edge depends on the hint tiling with magic-compat ON (spec scenario)", EXCEPTIONS) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 20, 2});
  l.hint("X", {Rect{5, -5, 15, 5}});
  CHECK(run_check(l, *scoped_width(false), Options{.magic_compat = true}) == std::vector<Rect>{Rect{0, 2, 20, 3}});
  l.hint("X", {Rect{5, -5, 15, 5}, Rect{-100, 0, -90, 10}});
  CHECK(run_check(l, *scoped_width(false), Options{.magic_compat = true}).empty());
}

TEST_CASE("horizontal edge independent of the hint tiling with magic-compat OFF (spec scenario)", EXCEPTIONS) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 20, 2});
  l.hint("X", {Rect{5, -5, 15, 5}});
  CHECK(run_check(l, *scoped_width(false), Options{.magic_compat = false}) == std::vector<Rect>{Rect{0, 2, 20, 3}});
  l.hint("X", {Rect{5, -5, 15, 5}, Rect{-100, 0, -90, 10}});
  CHECK(run_check(l, *scoped_width(false), Options{.magic_compat = false}) == std::vector<Rect>{Rect{0, 2, 20, 3}});
}

TEST_CASE("vertical edge on a hint boundary (spec scenario)", EXCEPTIONS) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 2, 20});
  l.hint("X", {Rect{-10, -10, 0, 30}});
  CHECK(run_check(l, *scoped_width(false), Options{.magic_compat = true}).empty());
  CHECK(run_check(l, *scoped_width(false), Options{.magic_compat = false}) == std::vector<Rect>{Rect{2, 0, 3, 20}});
}

TEST_CASE("the inside test gates nonstandard rules too", EXCEPTIONS) {
  const CheckTech& t = check_tech();
  RuleSetBuilder b;
  add_pairs(b, on_plane(t.metal1) & ~mask_of({t.m1}), mask_of({t.m1}),
            [&] {
              drc::Rule r = make_rule(4, 100, mask_of({t.m1}), mask_of({t.m1}), drc::flags::kArea | drc::flags::kForward,
                                 t.metal1, t.metal1, b.why("area"));
              r.exception = 0;  // exception X
              return r;
            }());
  b.exception_names({"X"});
  const auto rules = b.build();
  for (const bool compat : {true, false}) {
    Layout l;
    l.paint(t.m1, Rect{0, 0, 4, 4});
    CHECK(run_check(l, *rules, Options{.magic_compat = compat}).empty());
    l.hint("X", {Rect{-10, -10, 10, 10}});
    CHECK(run_check(l, *rules, Options{.magic_compat = compat}) == std::vector<Rect>{Rect{0, 0, 4, 4}});
  }
}
