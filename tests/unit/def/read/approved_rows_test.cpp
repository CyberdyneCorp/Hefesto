// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// def-read behaviours recorded as approved rows on 2026-11-19 (user
// decision D2), each tested in both magic-compat modes:
// - Q-LEFDEF-129: annotation erases a pin's old labels only on the
//   placement path (ON) / on every painted rectangle (OFF);
// - Q-LEFDEF-131: a route layer naming a via record takes the via's
//   contact-area lower-left x as its default width (Magic's union);
// - Q-LEFDEF-130: values Magic converts with undefined behaviour are
//   treated as unparsed and geometry is clipped to the plane;
// - Q-LEFDEF-132: a gzip-compressed DEF file is decompressed.
//
// Owner: DEFR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "def_fixture.hpp"
#include "hefesto/gds/stream/io.hpp"

namespace def_read_test {
namespace {

std::string pins(std::string_view body) {
  return "DESIGN top ;\nPINS 1 ;\n" + std::string{body} + "\nEND PINS\nEND DESIGN\n";
}

std::size_t labels_named(Session& s, std::string_view text) {
  std::size_t n = 0;
  for (const db::Label& l : s.cell("top").labels()) {
    n += l.text == text ? 1 : 0;
  }
  return n;
}

TEST_CASE("annotation erases old pin labels on the placement path only with magic-compat ON (Q-LEFDEF-129)",
          "[cap:def-read][req:Pin port labels]") {
  // The placement comes before the LAYER: the rectangle is painted when the
  // LAYER arrives, not by the placement statement.
  const std::string waiting = pins("- a + PLACED ( 0 0 ) N + LAYER met1 ( 0 0 ) ( 10 10 ) ;");
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s{on};
    REQUIRE(s.read(waiting).ok());
    REQUIRE(labels_named(s, "a") == 1);
    s.options.annotate = true;
    REQUIRE(s.read(waiting).ok());
    CHECK(labels_named(s, "a") == (on ? 2U : 1U));
    // On the placement path both modes erase the old label.
    Session p{on};
    const std::string placed = pins("- a + LAYER met1 ( 0 0 ) ( 10 10 ) + PLACED ( 0 0 ) N ;");
    REQUIRE(p.read(placed).ok());
    p.options.annotate = true;
    REQUIRE(p.read(placed).ok());
    CHECK(labels_named(p, "a") == 1U);
  }
}

TEST_CASE("a route on a via record takes the via's lower-left x as its default width (Q-LEFDEF-131)",
          "[cap:def-read][req:Route layer resolution and width selection]") {
  // VP's contact area is (20, 0)-(60, 40) in half units, so Magic's
  // info.route.width, which shares the storage of info.via.area.r_xbot,
  // reads 20: the wire is 20 wide with a 10 extension.
  const std::string text =
      "DESIGN top ;\nVIAS 1 ;\n- VP + RECT met1 ( 10 0 ) ( 30 20 ) ;\nEND VIAS\n"
      "NETS 1 ;\n- n + ROUTED VP ( 0 100 ) ( 200 100 ) ;\nEND NETS\nEND DESIGN\n";
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s{on};
    REQUIRE(s.read(text).ok());
    CHECK(s.covers("top", "metal1", db::Rect{-10, 90, 210, 110}));
    CHECK_FALSE(s.covers("top", "metal1", db::Rect{-11, 90, -10, 110}));
  }
}

TEST_CASE("values that do not fit an internal coordinate are unparsed in both modes (Q-LEFDEF-130)",
          "[cap:def-read][req:Unit scaling]") {
  const std::string text =
      "DESIGN top ;\nNETS 2 ;\n- a + ROUTED met1 ( 0 0 ) ( 1e30 0 ) ;\n"
      "- b + ROUTED met1 ( 0 100 ) ( 2000000000 100 ) ;\nEND NETS\nEND DESIGN\n";
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s{on};
    REQUIRE(s.read(text).ok());
    for (const db::Rect& r : s.painted("top", "metal1")) {
      CHECK(r.x0 >= db::kPlaneMin);
      CHECK(r.x1 <= db::kPlaneMax);
    }
  }
}

TEST_CASE("a gzip-compressed DEF file is read decompressed (Q-LEFDEF-132)", "[cap:def-read][req:DEF file lookup]") {
  const std::string text = "DESIGN top ;\nPINS 1 ;\n- a + LAYER met1 ( 0 0 ) ( 10 10 ) + PLACED ( 0 0 ) N ;\n"
                           "END PINS\nEND DESIGN\n";
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s{on};
    auto sink = hefesto::gds::open_sink(s.services.view().files, "/t/top.def.gz", 6);
    REQUIRE(sink.ok());
    std::vector<std::byte> data(text.size());
    std::transform(text.begin(), text.end(), data.begin(), [](char c) { return static_cast<std::byte>(c); });
    REQUIRE(sink.value()->write(data).ok());
    REQUIRE(sink.value()->finish().ok());
    s.editor.command_point = cmd::PointerState{};
    REQUIRE(s.run("defr read /t/top.def.gz").ok());
    CHECK(s.label("top", "a") != nullptr);
  }
}

}  // namespace
}  // namespace def_read_test
