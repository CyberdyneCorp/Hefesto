// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// def-read: PINS, pin geometry and pin port labels (tasks.md 7.4;
// Q-LEFDEF-64, 65, 66 per mode; Q-LEFDEF-67, 68, 89 fixed), including the
// DEF pin POLYGON test LEFVIA left open (Q-LEFDEF-66).
//
// Owner: DEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "def_fixture.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/lef/via/polygon.hpp"

namespace def_read_test {
namespace {

std::string pins(std::string_view body, int count = 1) {
  return "DESIGN top ;\nPINS " + std::to_string(count) + " ;\n" + std::string{body} + "\nEND PINS\nEND DESIGN\n";
}

constexpr std::uint8_t kAll = db::kPortNorth | db::kPortEast | db::kPortSouth | db::kPortWest;

// --- PINS section grammar -----------------------------------------------------------

TEST_CASE("multi-port pin count", "[cap:def-read][req:PINS section grammar]") {
  Session s;
  REQUIRE(s.read(pins("- vdd + NET vdd + PORT + LAYER met1 ( 0 0 ) ( 10 10 ) + FIXED ( 0 0 ) N "
                      "+ PORT + LAYER met1 ( 0 0 ) ( 10 10 ) + FIXED ( 100 0 ) N ;",
                      2))
              .ok());
  CHECK(s.said("  Processed 2 pins total."));
  CHECK_FALSE(s.said("does not match the number declared (2)."));
  CHECK(s.covers("top", "metal1", db::Rect{100, 0, 110, 10}));
}

TEST_CASE("unknown pin properties, classes and uses", "[cap:def-read][req:PINS section grammar]") {
  Session s;
  REQUIRE(s.read(pins("- a + SPECIAL + DIRECTION OUTPUT + USE BOGUS + DIRECTION WRONG + FOO 3 "
                      "+ LAYER met1 ( 0 0 ) ( 10 10 ) + PLACED ( 0 0 ) N ;"))
              .ok());
  CHECK(s.said("Unknown pin use \"BOGUS\""));
  CHECK(s.said("Unknown pin class \"WRONG\""));
  CHECK(s.said("Unknown pin property \"FOO\" in PINS definition; ignoring."));
  REQUIRE(s.label("top", "a") != nullptr);
  CHECK(s.label("top", "a")->port_class == db::PortClass::output);
}

TEST_CASE("a malformed pin RECT skips the pin (Q-LEFDEF-68)", "[cap:def-read][req:PINS section grammar]") {
  Session s;
  REQUIRE(s.read(pins("- a + LAYER met1 MASK 1 ( 0 0 ) ( 10 10 ) + PLACED ( 0 0 ) N ;\n"
                      "- b + LAYER met1 ( 0 0 ) ( 10 10 ) + PLACED ( 50 0 ) N ;",
                      2))
              .ok());
  CHECK(s.said("Bad port geometry: RECT requires 4 values."));
  CHECK(s.label("top", "a") == nullptr);
  CHECK(s.covers("top", "metal1", db::Rect{50, 0, 60, 10}));
  CHECK(s.said("  Processed 2 pins total."));
}

// --- Pin geometry placement ---------------------------------------------------------

TEST_CASE("pin with orientation", "[cap:def-read][req:Pin geometry placement]") {
  Session s;
  REQUIRE(s.read(pins("- a + NET a + DIRECTION INPUT + USE SIGNAL + LAYER met2 ( -70 0 ) ( 70 140 ) "
                      "+ PLACED ( 100 100 ) S ;"))
              .ok());
  CHECK(s.painted("top", "metal2") == std::vector<db::Rect>{db::Rect{30, -40, 170, 100}});
}

TEST_CASE("placement before layer", "[cap:def-read][req:Pin geometry placement]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    REQUIRE(s.read(pins("- b + PLACED ( 500 0 ) N + LAYER met1 ( 0 0 ) ( 20 20 ) ;")).ok());
    CHECK(s.painted("top", "metal1") == std::vector<db::Rect>{db::Rect{500, 0, 520, 20}});
    REQUIRE(s.label("top", "b") != nullptr);
    CHECK(s.label("top", "b")->rect == db::Rect{500, 0, 520, 20});
  }
}

TEST_CASE("second layer after the placement per mode (Q-LEFDEF-65)", "[cap:def-read][req:Pin geometry placement]") {
  const std::string text = pins(
      "- b + PLACED ( 500 0 ) N + LAYER met1 ( 0 0 ) ( 20 20 ) + LAYER met2 ( 0 0 ) ( 20 20 ) ;\n"
      "- c + PLACED ( 900 0 ) N + LAYER met1 ( 0 0 ) ( 10 10 ) ;",
      2);
  SECTION("ON: painted with the next pin") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.painted("top", "metal2") == std::vector<db::Rect>{db::Rect{900, 0, 920, 20}});
    const auto& labels = s.cell("top").labels();
    CHECK(std::count_if(labels.begin(), labels.end(), [](const db::Label& l) { return l.text == "c"; }) == 2);
  }
  SECTION("OFF: painted with its own port") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK(s.painted("top", "metal2") == std::vector<db::Rect>{db::Rect{500, 0, 520, 20}});
    const db::Label* b = nullptr;
    for (const db::Label& l : s.cell("top").labels()) {
      if (l.type == s.type("metal2")) {
        b = &l;
      }
    }
    REQUIRE(b != nullptr);
    CHECK(b->text == "b");
  }
}

TEST_CASE("pin POLYGON per mode (Q-LEFDEF-66)", "[cap:def-read][req:Pin geometry placement]") {
  const std::string text = pins("- p + POLYGON met1 ( 0 0 ) ( 20 0 ) ( 20 10 ) ( 10 10 ) ( 10 20 ) ( 0 20 ) "
                                "+ PLACED ( 100 0 ) N ;");
  SECTION("ON: an unknown property") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Unknown pin property \"POLYGON\" in PINS definition; ignoring."));
    CHECK(s.painted("top", "metal1").empty());
  }
  SECTION("OFF: painted over the L shape and labelled, as lef::paint_polygon decomposes it") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK_FALSE(s.said("ignoring."));
    const std::vector<db::Point> vertices{{100, 0}, {120, 0}, {120, 10}, {110, 10}, {110, 20}, {100, 20}};
    CHECK(s.covers("top", "metal1", db::Rect{100, 0, 120, 10}));
    CHECK(s.covers("top", "metal1", db::Rect{100, 10, 110, 20}));
    CHECK_FALSE(s.covers("top", "metal1", db::Rect{110, 10, 120, 20}));
    std::vector<db::Rect> labelled;
    for (const db::Label& l : s.cell("top").labels()) {
      CHECK(l.text == "p");
      labelled.push_back(l.rect);
    }
    CHECK(labelled == lef::polygon_rects(vertices));
  }
}

TEST_CASE("pin VIA with magic-compat OFF", "[cap:def-read][req:Pin geometry placement]") {
  Session s{false};
  REQUIRE(s.read("DESIGN top ;\nVIAS 1 ;\n- V1 + RECT met1 ( -10 -10 ) ( 10 10 ) + RECT met2 ( -20 -20 ) ( 20 20 ) ;\n"
                 "END VIAS\n" +
                 pins("- v + VIA V1 ( 5 0 ) + PLACED ( 100 100 ) N ;").substr(13))
              .ok());
  CHECK(s.covers("top", "metal1", db::Rect{95, 90, 115, 110}));
  CHECK(s.covers("top", "metal2", db::Rect{85, 80, 125, 120}));
  CHECK(s.label("top", "v") != nullptr);
}

TEST_CASE("unknown pin layer skips its rectangle (Q-LEFDEF-67)", "[cap:def-read][req:Pin geometry placement]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    REQUIRE(s.read(pins("- q + LAYER met9 ( 0 0 ) ( 10 10 ) + PLACED ( 0 0 ) N ;\n"
                        "- r + LAYER met1 ( 0 0 ) ( 10 10 ) + PLACED ( 50 0 ) N ;",
                        2))
                .ok());
    CHECK(s.said("Don't know how to parse layer \"met9\""));
    CHECK(s.said("Pin specified without layer, was not placed."));
    CHECK(s.label("top", "q") == nullptr);
    CHECK(s.painted("top", "metal1") == std::vector<db::Rect>{db::Rect{50, 0, 60, 10}});
  }
}

// --- Pin port labels ----------------------------------------------------------------

TEST_CASE("pin label attributes", "[cap:def-read][req:Pin port labels]") {
  Session s;
  REQUIRE(s.read(pins("- a + DIRECTION INOUT + USE POWER + LAYER met1 ( 0 0 ) ( 10 10 ) + PLACED ( 0 0 ) N ;\n"
                      "- b + DIRECTION FEEDTHRU + USE TIEOFF + LAYER met1 ( 0 0 ) ( 10 10 ) + PLACED ( 100 0 ) N ;",
                      2))
              .ok());
  const db::Label* a = s.label("top", "a");
  REQUIRE(a != nullptr);
  CHECK(a->sticky);
  CHECK(a->port_directions == kAll);
  CHECK(a->justification == db::kCenter);
  CHECK(a->port_class == db::PortClass::bidirectional);
  CHECK(a->port_use == db::PortUse::power);
  CHECK(a->port_index == 0);
  CHECK(a->font == -1);  // no rendering font loaded
  CHECK(a->size == 0);
  const db::Label* b = s.label("top", "b");
  REQUIRE(b != nullptr);
  CHECK(b->port_class == db::PortClass::feedthrough);
  CHECK(b->port_use == db::PortUse::tieoff);
  CHECK(b->port_index == 1);
}

TEST_CASE("direction of a pin without DIRECTION per mode (Q-LEFDEF-64)", "[cap:def-read][req:Pin port labels]") {
  const std::string text = pins(
      "- a + DIRECTION INPUT + USE CLOCK + LAYER met1 ( 0 0 ) ( 10 10 ) + PLACED ( 0 0 ) N ;\n"
      "- b + LAYER met1 ( 0 0 ) ( 10 10 ) + PLACED ( 100 0 ) N ;",
      2);
  SECTION("ON: carried over") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.label("top", "b")->port_class == db::PortClass::input);
    CHECK(s.label("top", "b")->port_use == db::PortUse::clock);
  }
  SECTION("OFF: reset") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK(s.label("top", "b")->port_class == db::PortClass::default_class);
    CHECK(s.label("top", "b")->port_use == db::PortUse::default_use);
  }
}

TEST_CASE("label size and rotation", "[cap:def-read][req:Pin port labels]") {
  Session s;
  s.add_fonts(1);
  REQUIRE(s.read(pins("- t + LAYER met2 ( 0 0 ) ( 140 1000 ) + PLACED ( 0 0 ) N ;\n"
                      "- q + LAYER met1 ( 0 0 ) ( 600 600 ) + PLACED ( 1000 0 ) N ;",
                      2))
              .ok());
  const db::Label* t = s.label("top", "t");
  REQUIRE(t != nullptr);
  CHECK(t->font == 0);
  CHECK(t->rotation == 90);
  CHECK(t->size == 1120);
  const db::Label* q = s.label("top", "q");
  REQUIRE(q != nullptr);
  CHECK(q->rotation == 0);
  CHECK(q->size == 4480);
}

TEST_CASE("label size without a default width ends (Q-LEFDEF-89)", "[cap:def-read][req:Pin port labels]") {
  Session s;
  s.add_fonts(1);
  REQUIRE(s.read(pins("- g + LAYER poly ( 0 0 ) ( 10 40 ) + PLACED ( 0 0 ) N ;")).ok());
  const db::Label* g = s.label("top", "g");
  REQUIRE(g != nullptr);
  CHECK(g->type == s.type("poly"));
  CHECK(g->rotation == 90);
  CHECK(g->size == 80);  // the (rotated) height 10, times 8
}

TEST_CASE("annotation replaces a matching label", "[cap:def-read][req:Pin port labels]") {
  Session s;
  REQUIRE(s.read(pins("- a + LAYER met1 ( 0 0 ) ( 10 10 ) + PLACED ( 0 0 ) N ;")).ok());
  REQUIRE(s.cell("top").labels().size() == 1);
  s.options.annotate = true;
  REQUIRE(s.read(pins("- a + LAYER met1 ( 0 0 ) ( 10 10 ) + PLACED ( 0 0 ) N ;")).ok());
  CHECK(s.cell("top").labels().size() == 1);
}

}  // namespace
}  // namespace def_read_test
