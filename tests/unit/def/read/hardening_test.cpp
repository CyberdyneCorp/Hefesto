// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// def-read on hostile input (design D21, D23): every prefix of a DEF file
// that uses each section ends cleanly (Magic dereferences the NULL token
// at the end of the input in most of its loops), and values beyond the
// coordinate range or a zero scale paint nothing instead of casting an
// out-of-range float (undefined in Magic).
//
// Owner: DEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "def_fixture.hpp"

namespace def_read_test {
namespace {

constexpr std::string_view kRich =
    "VERSION 5.8 ;\nUNITS DISTANCE MICRONS 1 ;\nDESIGN top ;\nDIEAREA ( 0 0 ) ( 100 0 ) ( 100 100 ) ;\n"
    "VIAS 2 ;\n- V1 + RECT met1 ( -1 -1 ) ( 1 1 ) + RECT via1 ( 0 0 ) ( 1 1 ) + ORIGIN 1 1 ;\n"
    "- V2 + CUTSIZE 1 1 + LAYERS met1 via1 met2 + CUTSPACING 1 1 + ENCLOSURE 1 1 1 1 + ROWCOL 2 2 "
    "+ POLYGON met1 ( 0 0 ) ( 2 0 ) ( 2 2 ) + PATTERN 2_F ;\nEND VIAS\n"
    "NONDEFAULTRULES 1 ;\n- R + LAYER met1 WIDTH 2 SPACING 2 WIREEXT 1 ;\nEND NONDEFAULTRULES\n"
    "COMPONENTS 1 ;\n- u1 inv + PLACED ( 1 1 ) FS ;\nEND COMPONENTS\n"
    "PINS 1 ;\n- a + NET a + DIRECTION INPUT + PORT + LAYER met1 ( 0 0 ) ( 1 1 ) + POLYGON met2 ( 0 0 ) ( 1 0 ) "
    "( 1 1 ) + VIA V1 ( 0 0 ) + FIXED ( 5 5 ) N ;\nEND PINS\n"
    "SPECIALNETS 1 ;\n- vdd + ROUTED met1 2 + SHAPE STRIPE ( 0 0 ) ( 10 0 ) V2 NEW met2 0 ( 0 0 ) ( 0 10 ) "
    "+ FOO ;\nEND SPECIALNETS\n"
    "NETS 1 ;\n- n ( u1 A ) ( PIN a ) + ROUTED met1 ( 0 0 ) ( 10 * ) RECT ( 0 0 1 1 ) VIRTUAL ( 20 0 ) "
    "POLYGON ( 0 0 ) ( 1 0 ) ( 1 1 ) MASK 1 V1 N TAPERRULE R + NONDEFAULTRULE R ;\nEND NETS\n"
    "BLOCKAGES 1 ;\n- LAYER met1 RECT ( 0 0 ) ( 1 1 ) ;\nEND BLOCKAGES\n"
    "GROUPS 1 ; - g u1 ; END GROUPS\nEND DESIGN\n";

TEST_CASE("every prefix of a DEF file ends cleanly", "[cap:def-read][req:Diagnostics and error limits]") {
  for (const bool compat : {true, false}) {
    for (const bool annotate : {false, true}) {
      Session s{compat};
      s.options.annotate = annotate;
      db::CellDef* inv = s.editor.cells.create("inv", s.editor.technology->plane_count()).value();
      inv->flags().available = true;
      for (std::size_t n = 0; n <= kRich.size(); n += 2) {
        INFO("prefix " << n << " compat " << compat << " annotate " << annotate);
        CHECK(s.read(kRich.substr(0, n)).ok());
      }
    }
  }
}

TEST_CASE("values beyond the coordinate range paint nothing", "[cap:def-read][req:Unit scaling]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    REQUIRE(s.read("DESIGN top ;\nDIEAREA ( 0 0 ) ( 1e30 1e30 ) ;\nNETS 1 ;\n- n + ROUTED met1 ( 0 0 ) ( 1e30 0 ) "
                   "( 3e9 0 ) RECT ( 0 0 1e20 1 ) ;\nEND NETS\nSPECIALNETS 1 ;\n- v + ROUTED met2 1e30 ( 0 0 ) "
                   "( 10 0 ) ;\nEND SPECIALNETS\nEND DESIGN\n")
                .ok());
    CHECK(s.cell("top").properties().find("FIXED_BBOX") == nullptr);
  }
}

TEST_CASE("a zero or negative UNITS scale paints nothing", "[cap:def-read][req:Unit scaling]") {
  Session s{true};
  REQUIRE(s.read("UNITS DISTANCE MICRONS 0 ;\nDESIGN top ;\nNETS 1 ;\n- n + ROUTED met1 ( 0 0 ) ( 10 0 ) ;\n"
                 "END NETS\nVIAS 1 ;\n- V + CUTSIZE 1 1 + LAYERS met1 via1 met2 + CUTSPACING 0 0 "
                 "+ ENCLOSURE 0 0 0 0 ;\nEND VIAS\nEND DESIGN\n")
              .ok());
  CHECK(s.painted("top", "metal1").empty());
  Session n{false};
  REQUIRE(n.read("UNITS DISTANCE MICRONS -5 ;\nDESIGN top ;\nEND DESIGN\n").ok());
  CHECK(n.said("Invalid syntax for UNITS statement."));
}

}  // namespace
}  // namespace def_read_test
