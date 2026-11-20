// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef-tech-section: golden case LEFDEF-003's technology, Hefesto-only
// (user decision D4 of 2026-11-19; the first input, kept in
// tests/golden/LEFDEF/magic-crash/LEFDEF-003/). Its cifoutput style has `scalefactor 20
// nanometers`, `gridlimit 10` and `set-minimum-grid`. Magic normalises the
// scale factor to 2 centimicrons (CIFTechFinal -> CIFTechOutputScale(1, 1)
// divides scale factor and expander 10 by their common factor), then sets
// the internal grid to scaleFactor / gridLimit = 2 / 10 = 0 internal units
// per lambda (utils/tech.c:808-812, which divides centimicrons by the grid
// limit in nanometres) and scales every table by 0 / 1: CIFTechOutputScale
// (1, 0) makes the expander 0 and FindGCF(scaleFactor, 0) divides by zero
// (utils/fraction.c:60, SIGFPE). Hefesto takes a ratio below 1 as 1 in both
// modes (tech/loader.cpp apply_post_load), so the LEF table keeps its 1:1
// values and nothing crashes.
//
// Owner: LEFTECH (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "fixture.hpp"

namespace lef = hefesto::lef;
namespace tech = hefesto::tech;
using lef::test::Session;

TEST_CASE("regression: a minimum grid below one internal unit per lambda loads at 1:1 (LEFDEF-003)",
          "[cap:lef-tech-section][req:Filling defaults after grid scaling][req:Scaling the LEF table]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    s.file("/t/grid.tech",
           lef::test::read_text(std::filesystem::path{HEFESTO_SOURCE_DIR} /
                                       "tests/golden/LEFDEF/magic-crash/LEFDEF-003/lefdef.tech"));
    REQUIRE(s.run("tech load /t/grid.tech -noprompt").ok());
    CHECK(s.editor.grid == tech::GridScale{1, 1});
    for (const auto& d : s.messages) {
      CHECK_FALSE(d.text.starts_with("Scaled tech values by"));
    }
    // The table is the 1:1 one: metal1's DRC width 3 is not scaled.
    const lef::LayerRecord& met1 = s.record("met1");
    CHECK(met1.type == s.type("metal1"));
    if (on) {
      CHECK(met1.width == -1);  // the unfilled sentinel of a 1:1 load (Q-LEFDEF-1 ON)
    } else {
      CHECK(met1.width == 3);
    }
  }
}
