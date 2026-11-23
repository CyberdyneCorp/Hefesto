// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// def-read: NETS and SPECIALNETS (tasks.md 7.5): the grammar (Q-LEFDEF-69,
// 74), layer and width selection (the Q-LEFDEF-1 width fix), wire segments,
// vias (Q-LEFDEF-63, 72), other route elements (Q-LEFDEF-70, 71, 73),
// painting and labels, and annotation (Q-LEFDEF-81), including the DEF
// route POLYGON test LEFVIA left open (Q-LEFDEF-70).
//
// Owner: DEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "def_fixture.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/lef/via/polygon.hpp"

namespace def_read_test {
namespace {

constexpr std::string_view kRules =
    "NONDEFAULTRULES 1 ;\n- W2 + LAYER met1 WIDTH 280 SPACING 280 + LAYER met2 WIDTH 300 ;\n"
                                    "END NONDEFAULTRULES\n";

// VA first: with magic-compat ON a RECT via after a generated one is
// generated too (Q-LEFDEF-60).
constexpr std::string_view kVias =
    "VIAS 2 ;\n- VA + RECT met1 ( 0 0 ) ( 100 50 ) ;\n- VG + CUTSIZE 100 100 + LAYERS met1 via1 met2 "
    "+ CUTSPACING 100 100 + ENCLOSURE 50 20 20 50 + ROWCOL 1 2 ;\nEND VIAS\n";

// One regular net with routing `route`, after `prefix` sections.
std::string net(std::string_view route, std::string_view prefix = {}) {
  return "DESIGN top ;\n" + std::string{prefix} + "NETS 1 ;\n- n1 + ROUTED " + std::string{route} +
         " ;\nEND NETS\nEND DESIGN\n";
}

std::string special(std::string_view body, int count = 1) {
  return "DESIGN top ;\nSPECIALNETS " + std::to_string(count) + " ;\n" + std::string{body} +
         "\nEND SPECIALNETS\nEND DESIGN\n";
}

std::vector<db::Rect> metal1(Session& s) {
  return s.painted("top", "metal1");
}

// --- NETS and SPECIALNETS grammar ---------------------------------------------------

TEST_CASE("routing then NONDEFAULTRULE per mode (Q-LEFDEF-74)", "[cap:def-read][req:NETS and SPECIALNETS grammar]") {
  const std::string text = "DESIGN top ;\n" + std::string{kRules} +
                           "NETS 1 ;\n- n1 ( u1 A ) + ROUTED met1 ( 0 0 ) ( 1000 0 ) + NONDEFAULTRULE W2 ;\n"
                           "END NETS\nEND DESIGN\n";
  SECTION("ON: the default width") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(metal1(s) == std::vector<db::Rect>{db::Rect{-70, -70, 1070, 70}});
  }
  SECTION("OFF: the rule's width") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK(metal1(s) == std::vector<db::Rect>{db::Rect{-140, -140, 1140, 140}});
  }
}

TEST_CASE("unknown special-net keyword per mode (Q-LEFDEF-69)", "[cap:def-read][req:NETS and SPECIALNETS grammar]") {
  const std::string text = special("- vdd + ROUTED met1 200 ( 0 0 ) ( 1000 0 ) + FOO bar ;\n"
                                   "- gnd + ROUTED met1 200 ( 0 500 ) ( 1000 500 ) ;",
                                   2);
  SECTION("ON: the next net is swallowed as routes of vdd") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Unknown keyword \"FOO\" in SPECIALNET definition; ignoring."));
    CHECK(s.said("Via name \"gnd\" unknown in route."));
    CHECK(s.covers("top", "metal1", db::Rect{0, 400, 1000, 600}));
    CHECK(s.said("Number of nets read (1) does not match the number declared (2)."));
  }
  SECTION("OFF: the next net is read") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Unknown keyword \"FOO\" in SPECIALNET definition; ignoring."));
    CHECK(s.covers("top", "metal1", db::Rect{0, 400, 1000, 600}));
    CHECK(s.said("  Processed 2 special nets total."));
  }
}

TEST_CASE("special-net properties, SHAPE and STYLE", "[cap:def-read][req:NETS and SPECIALNETS grammar]") {
  Session s;
  REQUIRE(s.read(special("- vdd + USE POWER + SHAPE STRIPE + ROUTED met1 200 + SHAPE STRIPE ( 0 0 ) ( 1000 0 ) "
                         "+ STYLE 1 + USE POWER + PROPERTY a b + FIXEDBUMP ;\n"
                         "- vss + ROUTED met2 200 + SHAPE BOGUS ( 0 0 ) ( 10 0 ) ;\n"
                         "- vcc + ROUTED met2 200 + STYLE x ( 0 0 ) ( 10 0 ) ;",
                         3))
              .ok());
  CHECK(s.said("Unknown net property \"SHAPE\" in NET definition; ignoring."));
  CHECK(s.said("Unknown SHAPE \"BOGUS\" in SPECIALNET definition; ignoring."));
  CHECK(s.said("Net style \"x\" in SPECIALNET definition is not a number; ignoring."));
  CHECK(s.covers("top", "metal1", db::Rect{0, -100, 1000, 100}));
}

TEST_CASE("unknown net properties and rules", "[cap:def-read][req:NETS and SPECIALNETS grammar]") {
  Session s;
  REQUIRE(s.read("DESIGN top ;\nNETS 2 ;\n- n1 ( PIN a ) ( u1 Z ) + USE SIGNAL + BOGUS + NONDEFAULTRULE XX "
                 "+ ROUTED met1 ( 0 0 ) ( 10 0 ) + SOURCE NETLIST ;\n- n2 + VPIN p LAYER met1 ( 0 0 ) ( 1 1 ) ;\n"
                 "END NETS\nEND DESIGN\n")
              .ok());
  CHECK(s.said("Unknown net property \"BOGUS\" in NET definition; ignoring."));
  CHECK(s.said("Unknown nondefault rule \"XX\""));
  CHECK(s.said("  Processed 2 nets total."));
}

// --- Route layer resolution and width selection -------------------------------------

TEST_CASE("special net width", "[cap:def-read][req:Route layer resolution and width selection]") {
  Session s;
  REQUIRE(s.read(special("- vdd + ROUTED met2 300 ( 0 0 ) ( 10 0 ) NEW met1 200 ( 0 0 ) ( 1000 0 ) ;")).ok());
  CHECK(metal1(s) == std::vector<db::Rect>{db::Rect{0, -100, 1000, 100}});
}

TEST_CASE("zero special width", "[cap:def-read][req:Route layer resolution and width selection]") {
  Session s;
  REQUIRE(s.read(special("- vdd + ROUTED met1 0 ( 0 0 ) ( 0 500 ) ;")).ok());
  CHECK(metal1(s) == std::vector<db::Rect>{db::Rect{-70, 0, 70, 500}});
}

TEST_CASE("unfilled route width (Q-LEFDEF-1, both modes)",
          "[cap:def-read][req:Route layer resolution and width selection]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    const lef::LayerRecord* met1 = s.editor.lefdef.layers.find("met1");
    REQUIRE(met1 != nullptr);
    if (compat) {
      CHECK(met1->width == -1);  // Magic's sentinel survives on a 1:1 grid
    }
    REQUIRE(s.read(net("met1 ( 0 0 ) ( 1000 0 )")).ok());
    CHECK(metal1(s) == std::vector<db::Rect>{db::Rect{-70, -70, 1070, 70}});
  }
}

TEST_CASE("layer names fall back to magic types; unknown layers are reported",
          "[cap:def-read][req:Route layer resolution and width selection]") {
  Session s;
  REQUIRE(s.read(net("METAL2 ( 0 0 ) ( 100 0 ) NEW bogus ( 0 0 ) ( 10 0 )")).ok());
  CHECK(s.said("Unknown layer type \"bogus\" for NEW route"));
  // No LEF record: DEFAULT_WIDTH * DBLambda[1] / DBLambda[0] = 3.
  CHECK(s.painted("top", "metal2") == std::vector<db::Rect>{db::Rect{-2, -2, 101, 1}});
}

TEST_CASE("TAPERRULE and TAPER", "[cap:def-read][req:Route layer resolution and width selection]") {
  Session s;
  REQUIRE(s.read(net("met1 ( 0 0 ) TAPERRULE W2 ( 1000 0 ) TAPER ( 1000 1000 ) TAPERRULE NOPE", kRules)).ok());
  CHECK(s.covers("top", "metal1", db::Rect{-70, -140, 1140, 140}));
  CHECK(s.covers("top", "metal1", db::Rect{930, 140, 1070, 1070}));
  CHECK(s.said("Unknown nondefault rule \"NOPE\""));
}

// --- Route wire segment geometry ----------------------------------------------------

TEST_CASE("wire extensions", "[cap:def-read][req:Route wire segment geometry]") {
  Session s;
  REQUIRE(s.read(net("met1 ( 0 0 0 ) ( 1000 0 )")).ok());
  CHECK(metal1(s) == std::vector<db::Rect>{db::Rect{0, -70, 1070, 70}});
  Session r;
  REQUIRE(r.read(net("met1 ( 0 1000 0 ) ( 0 0 )")).ok());
  CHECK(metal1(r) == std::vector<db::Rect>{db::Rect{-70, -70, 70, 1000}});
}

TEST_CASE("odd special width rounds asymmetrically", "[cap:def-read][req:Route wire segment geometry]") {
  Session s;
  REQUIRE(s.read(special("- vdd + ROUTED met1 3 ( 0 0 ) ( 10 0 ) ;")).ok());
  CHECK(metal1(s) == std::vector<db::Rect>{db::Rect{0, -2, 10, 1}});
}

TEST_CASE("wildcard coordinates", "[cap:def-read][req:Route wire segment geometry]") {
  Session s;
  REQUIRE(s.read(net("met1 ( 0 0 ) ( 1000 * ) ( * 500 )")).ok());
  CHECK(s.covers("top", "metal1", db::Rect{-70, -70, 1070, 70}));
  CHECK(s.covers("top", "metal1", db::Rect{930, -70, 1070, 570}));
  CHECK(s.extent("top", "metal1") == db::Rect{-70, -70, 1070, 570});
}

TEST_CASE("bad points, wildcards and non-Manhattan segments", "[cap:def-read][req:Route wire segment geometry]") {
  Session s;
  REQUIRE(s.read(net("met1 ( * 0 ) ( 0 0 ) ( 5 x ) ( 10 10 ) ( 10 20 q )")).ok());
  CHECK(s.said("No reference point for \"*\" wildcard"));
  CHECK(s.said("Cannot parse Y coordinate."));
  CHECK(s.said("Can't deal with nonmanhattan geometry in route."));
  CHECK(s.said("Can't parse route extension value."));
}

TEST_CASE("a repeated point is a square", "[cap:def-read][req:Route wire segment geometry]") {
  Session s;
  REQUIRE(s.read(net("met1 ( 100 100 ) ( 100 100 )")).ok());
  CHECK(metal1(s) == std::vector<db::Rect>{db::Rect{30, 30, 170, 170}});
}

// --- Route via placement ------------------------------------------------------------

TEST_CASE("via between legs", "[cap:def-read][req:Route via placement]") {
  Session s;
  REQUIRE(s.read(net("met1 ( 0 500 ) ( 1000 500 ) VG ( 1000 2000 )", kVias)).ok());
  // The cuts split the metal1 wire and both enclosures.
  CHECK(s.covers("top", "metal1", db::Rect{-70, 430, 850, 570}));
  CHECK(s.covers("top", "metal1", db::Rect{1150, 430, 1200, 570}));
  CHECK(s.extent("top", "metal1") == db::Rect{-70, 430, 1200, 570});
  CHECK(s.covers("top", "metal2", db::Rect{830, 400, 1170, 450}));
  CHECK(s.covers("top", "metal2", db::Rect{830, 550, 1170, 600}));
  CHECK(s.covers("top", "m2c", db::Rect{850, 450, 950, 550}));
  CHECK(s.covers("top", "m2c", db::Rect{1050, 450, 1150, 550}));
  // The route goes on on metal2.
  CHECK(s.covers("top", "metal2", db::Rect{930, 600, 1070, 2070}));
}

TEST_CASE("route continuing on the top layer of a generated via (Q-LEFDEF-63)",
          "[cap:def-read][req:Route via placement]") {
  const std::string text = net("met2 ( 0 500 ) ( 1000 500 ) VG ( 1000 2000 )", kVias);
  SECTION("ON: metal2") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.covers("top", "metal2", db::Rect{930, 600, 1070, 2070}));
    CHECK_FALSE(s.covers("top", "metal1", db::Rect{930, 600, 1070, 2070}));
  }
  SECTION("OFF: metal1") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK(s.covers("top", "metal1", db::Rect{930, 600, 1070, 2070}));
    CHECK_FALSE(s.covers("top", "metal2", db::Rect{930, 700, 1070, 2070}));
  }
}

TEST_CASE("oriented via in a route (Q-LEFDEF-72)", "[cap:def-read][req:Route via placement]") {
  const std::string text = net("met1 ( 0 0 ) VA E", kVias);
  SECTION("ON: the orientation is a via name") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Via name \"E\" unknown in route."));
    CHECK(metal1(s) == std::vector<db::Rect>{db::Rect{0, 0, 100, 50}});
  }
  SECTION("OFF: the via turns") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK_FALSE(s.said("unknown in route."));
    CHECK(metal1(s) == std::vector<db::Rect>{db::Rect{0, -100, 50, 0}});
  }
}

TEST_CASE("MASK tokens per mode (Q-LEFDEF-72)", "[cap:def-read][req:Route via placement]") {
  const std::string text = net("met1 ( 0 0 ) MASK 2 ( 100 0 ) MASK 031 VA", kVias);
  SECTION("ON") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Via name \"MASK\" unknown in route."));
    CHECK(s.said("Via name \"031\" unknown in route."));
  }
  SECTION("OFF") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK_FALSE(s.said("unknown in route."));
    CHECK(s.covers("top", "metal1", db::Rect{-70, -70, 170, 70}));
  }
}

TEST_CASE("unknown and misplaced via names", "[cap:def-read][req:Route via placement]") {
  Session s;
  REQUIRE(s.read(net("met1 VA ( 0 0 ) NOSUCHVIA", kVias)).ok());
  CHECK(s.said("Route has via name \"VA\" but no points!"));
  CHECK(s.said("Via name \"NOSUCHVIA\" unknown in route."));
  CHECK(metal1(s).empty());
}

// --- Other route elements -----------------------------------------------------------

TEST_CASE("relative RECT", "[cap:def-read][req:Other route elements]") {
  Session s;
  REQUIRE(s.read(net("met2 ( 1000 500 ) RECT ( -50 -20 50 20 )")).ok());
  CHECK(s.painted("top", "metal2") == std::vector<db::Rect>{db::Rect{950, 480, 1050, 520}});
}

TEST_CASE("RECT at a negative odd reference per mode (Q-LEFDEF-73)", "[cap:def-read][req:Other route elements]") {
  const std::string text = net("met2 ( -1.5 0 ) RECT ( 0 0 10 10 )");
  SECTION("ON: truncation") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.painted("top", "metal2") == std::vector<db::Rect>{db::Rect{-1, 0, 9, 10}});
  }
  SECTION("OFF: floor") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK(s.painted("top", "metal2") == std::vector<db::Rect>{db::Rect{-2, 0, 8, 10}});
  }
}

TEST_CASE("RECT without a reference point or with a bad value (Q-LEFDEF-73)",
          "[cap:def-read][req:Other route elements]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    REQUIRE(s.read(net("met2 RECT ( 0 0 10 10 ) ( 0 0 ) RECT ( 0 x 10 10 ) RECT ( 0 0 10 10 ]")).ok());
    CHECK(s.said("No reference point for RECT"));
    CHECK(s.said("Cannot parse Y coordinate in RECT."));
    CHECK(s.said("Bad coordinates in RECT."));
    CHECK(s.painted("top", "metal2").empty());
  }
}

TEST_CASE("VIRTUAL point per mode (Q-LEFDEF-71)", "[cap:def-read][req:Other route elements]") {
  const std::string text = net("met1 ( 0 0 ) ( 1000 0 ) VIRTUAL ( 1000 500 ) ( 2000 500 )");
  SECTION("ON: wired") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.covers("top", "metal1", db::Rect{930, 70, 1070, 430}));
  }
  SECTION("OFF: reference point only") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK_FALSE(s.covers("top", "metal1", db::Rect{930, 100, 1070, 400}));
    CHECK(s.covers("top", "metal1", db::Rect{930, 430, 2070, 570}));
    CHECK_FALSE(s.said("nonmanhattan"));
  }
}

TEST_CASE("route POLYGON per mode (Q-LEFDEF-70)", "[cap:def-read][req:Other route elements]") {
  const std::string text =
      net("met1 ( 0 0 ) POLYGON ( 0 0 ) ( 2000 0 ) ( 2000 1000 ) ( 1000 1000 ) ( 1000 2000 ) ( 0 2000 )");
  SECTION("ON: the later vertices are wire points") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Route has POLYGON entries, this is not handled!"));
    CHECK(s.covers("top", "metal1", db::Rect{-70, -70, 2070, 70}));        // ( 0 0 ) to ( 2000 0 )
    CHECK(s.covers("top", "metal1", db::Rect{1930, -70, 2070, 1070}));     // to ( 2000 1000 )
    CHECK_FALSE(s.covers("top", "metal1", db::Rect{100, 100, 900, 900}));  // no polygon
  }
  SECTION("OFF: one painted polygon, the same tiles as lef::paint_polygon") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Route has POLYGON entries, this is not handled!"));
    const std::vector<db::Point> vertices{{0, 0}, {2000, 0}, {2000, 1000}, {1000, 1000}, {1000, 2000}, {0, 2000}};
    auto made = s.editor.cells.create("expected", s.editor.technology->plane_count());
    REQUIRE(made.ok());
    REQUIRE(lef::paint_polygon(*made.value(), *s.editor.technology, s.type("metal1"), vertices).ok());
    CHECK(metal1(s) == s.painted("expected", "metal1"));
  }
}

// --- Route painting and net labels --------------------------------------------------

TEST_CASE("net label", "[cap:def-read][req:Route painting and net labels]") {
  Session s;
  s.options.labels = true;
  REQUIRE(s.read("DESIGN top ;\nNETS 1 ;\n- n1 ( u1 Y ) ( u2 A ) + ROUTED met1 ( 0 0 ) ( 1000 0 ) "
                 "+ ROUTED met2 ( 0 0 ) ( 0 10 ) ;\nEND NETS\nEND DESIGN\n")
              .ok());
  const auto& labels = s.cell("top").labels();
  REQUIRE(labels.size() == 2);
  CHECK(labels[0].text == "n1");
  CHECK(labels[0].type == s.type("metal1"));
  CHECK(labels[0].rect == db::Rect{500, 0, 500, 0});
  CHECK_FALSE(labels[0].sticky);
  CHECK(labels[0].port_directions == 0);
  CHECK(labels[1].type == s.type("metal2"));
}

TEST_CASE("special nets get one label", "[cap:def-read][req:Route painting and net labels]") {
  Session s;
  s.options.labels = true;
  REQUIRE(s.read(special("- vdd + ROUTED met1 200 ( 0 0 ) ( 1000 0 ) NEW met2 200 ( 0 0 ) ( 0 1000 ) "
                         "+ ROUTED met2 200 ( 0 0 ) ( 10 0 ) ;"))
              .ok());
  CHECK(s.cell("top").labels().size() == 1);
}

// --- Net annotation mode (Q-LEFDEF-81) ----------------------------------------------

struct Annotated {
  Session s;
  db::CellDef* top = nullptr;

  explicit Annotated(bool compat, bool edit_is_top) : s{compat} {
    auto& cells = s.editor.cells;
    db::CellDef* child = cells.create("child", s.editor.technology->plane_count()).value();
    child->flags().available = true;
    db::Label y;
    y.text = "Y";
    y.type = s.type("metal1");
    y.rect = db::Rect{10, 10, 20, 20};
    REQUIRE(db::put_label(*child, y).ok());
    top = cells.create("top", s.editor.technology->plane_count()).value();
    top->flags().available = true;
    REQUIRE(db::paint_type(*top, *s.editor.technology, db::Rect{0, 0, 100, 30}, s.type("metal1")).ok());
    db::PlaceOptions place;
    place.id = "u1";
    REQUIRE(db::place(*top, *child, place).ok());
    db::CellDef* other = cells.create("other", s.editor.technology->plane_count()).value();
    s.editor.edit_cell = edit_is_top ? top : other;
    s.options.annotate = true;
  }
};

constexpr std::string_view kAnnotated =
    "DESIGN top ;\nNETS 1 ;\n- n1 ( u1 Y ) + ROUTED met1 ( 0 15 ) ( 100 15 ) ;\nEND NETS\nEND DESIGN\n";

TEST_CASE("annotate a net through a terminal", "[cap:def-read][req:Net annotation mode]") {
  for (const bool compat : {true, false}) {
    Annotated a{compat, true};
    REQUIRE(a.s.read(kAnnotated).ok());
    const db::Label* l = a.s.label("top", "n1");
    REQUIRE(l != nullptr);
    CHECK(l->rect == db::Rect{50, 15, 50, 15});
    CHECK(l->type == a.s.type("metal1"));
    CHECK(a.s.painted("top", "metal1") == std::vector<db::Rect>{db::Rect{0, 0, 100, 30}});
    CHECK_FALSE(a.s.said("did not land on any existing net geometry."));
  }
}

TEST_CASE("regression: annotation reads an unread subcell to find its terminal",
          "[cap:def-read][req:Net annotation mode]") {
  // The terminal lookup read the child's labels without reading the child
  // (`load top` leaves subcells unread until they are needed), so the
  // terminal of LEFDEF-232's u1 was never found. Magic's CmdFindNetProc
  // reads it (DBTreeFindUse, "Cell leaf read from path ...").
  for (const bool compat : {true, false}) {
    Annotated a{compat, true};
    db::CellDef& child = a.s.cell("child");
    child.labels().clear();
    child.flags().available = false;
    std::vector<std::string> asked;
    a.s.options.read_master = [&](db::CellDef& def) {
      asked.push_back(def.name());
      db::Label y;
      y.text = "Y";
      y.type = a.s.type("metal1");
      y.rect = db::Rect{10, 10, 20, 20};
      def.flags().available = true;
      return db::put_label(def, y).ok();
    };
    REQUIRE(a.s.read(kAnnotated).ok());
    CHECK(asked == std::vector<std::string>{"child"});
    const db::Label* l = a.s.label("top", "n1");
    REQUIRE(l != nullptr);
    CHECK(l->rect == db::Rect{50, 15, 50, 15});
    CHECK_FALSE(a.s.said("did not land on any existing net geometry."));
  }
}

TEST_CASE("annotation when the edit cell is not the design cell (Q-LEFDEF-81)",
          "[cap:def-read][req:Net annotation mode]") {
  SECTION("ON: the terminal is not found") {
    Annotated a{true, false};
    REQUIRE(a.s.read(kAnnotated).ok());
    CHECK(a.s.said("Warning:  Label \"n1\" did not land on any existing net geometry."));
    CHECK(a.s.label("top", "n1") != nullptr);
  }
  SECTION("OFF: found in the design cell") {
    Annotated a{false, false};
    REQUIRE(a.s.read(kAnnotated).ok());
    CHECK_FALSE(a.s.said("did not land on any existing net geometry."));
    REQUIRE(a.s.label("top", "n1") != nullptr);
    CHECK(a.s.label("top", "n1")->rect == db::Rect{50, 15, 50, 15});
  }
}

TEST_CASE("annotation labels the first route rectangle on paint", "[cap:def-read][req:Net annotation mode]") {
  Annotated a{true, true};
  REQUIRE(a.s.read("DESIGN top ;\nNETS 1 ;\n- n2 ( PIN p ) + ROUTED met2 ( 0 15 ) ( 100 15 ) "
                   "NEW met1 ( 10 15 ) ( 90 15 ) ;\n"
                   "END NETS\nEND DESIGN\n")
              .ok());
  // ( PIN p ): no terminal lookup and no label from the route either.
  CHECK(a.s.label("top", "n2") == nullptr);
  REQUIRE(a.s.read("DESIGN top ;\nSPECIALNETS 1 ;\n- n3 + ROUTED met2 2 ( 20 15 ) ( 80 15 ) "
                   "NEW met1 2 ( 20 15 ) ( 80 15 ) ;\nEND SPECIALNETS\nEND DESIGN\n")
              .ok());
  const db::Label* l = a.s.label("top", "n3");
  REQUIRE(l != nullptr);
  CHECK(l->type == a.s.type("metal1"));
  CHECK(l->rect == db::Rect{50, 15, 50, 15});
  CHECK_FALSE(a.s.said("did not land on any existing net geometry."));
  CHECK(a.s.painted("top", "metal2").empty());
}

}  // namespace
}  // namespace def_read_test
