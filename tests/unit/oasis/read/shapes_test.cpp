// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Geometry and TEXT elements read through oasis::read (tasks.md 11.2):
// layer mapping, shape repetitions, RECTANGLE, POLYGON, TRAPEZOID,
// CTRAPEZOID, CIRCLE (with the pinned vertices of Q-OASIS-31), PATH (per
// mode for Q-OASIS-33), TEXT, and the unsupported extension records. Each
// result is compared with the GDS-equivalent element read by gds::read.
// Owner: OASR.

#include <catch2/catch_test_macros.hpp>

#include <array>

#include "hefesto/oasis/read/circle.hpp"
#include "oasis_read_fixture.hpp"

using namespace oasis_read_test;

namespace {

/// The .mag of cell `c` read from `oas` and from `gds` in fresh sessions.
std::pair<std::string, std::string> both(Oas& oas, GdsFile& gds, std::string_view style = kStyle,
                                         bool magic_compat = true) {
  OasSession o(style, magic_compat);
  (void)o.read_oasis(oas.bytes());
  OasSession g(style, magic_compat);
  g.read(gds);
  return {o.mag("c"), g.mag("c")};
}

/// A GDS library with structure "c" built by `body`.
template <typename Body>
GdsFile gds_cell(Body body) {
  GdsFile f;
  f.library();
  f.begin("c");
  body(f);
  f.end().end_library();
  return f;
}

}  // namespace

TEST_CASE("layer and datatype mapping", "[cap:oasis-read][req:Layer and datatype mapping]") {
  constexpr std::string_view kWild = "style test\nscalefactor 1\nlayer m1 M1\ncalma M1 68 20\nlayer m2 M1PIN\n"
                                     "calma M1PIN 68 *\nlayer poly ANY\ncalma ANY * 7\n";
  SECTION("datatype wildcard precedence") {
    OasSession s(kWild);
    Oas f;
    f.cell("c").add(rect(68, 20, 0, 0, 10, 10)).add(polygon(68, 16, 20, 0, {{10, 0}, {10, 10}, {0, 10}}));
    (void)s.read_oasis(f.bytes());
    CHECK(s.painted("c", "m1") == std::vector<db::Rect>{{0, 0, 10, 10}});
    CHECK(s.painted("c", "m2") == std::vector<db::Rect>{{20, 0, 30, 10}});
  }
  SECTION("a layer number above 255, and above INT2, matches only a wildcard") {
    OasSession s(kWild);
    Oas f;
    f.cell("c").add(rect(1000, 7, 0, 0, 10, 10)).add(rect(70000, 7, 20, 0, 10, 10));
    (void)s.read_oasis(f.bytes());
    CHECK(s.painted("c", "poly") == std::vector<db::Rect>{{0, 0, 10, 10}, {20, 0, 30, 10}});
  }
  SECTION("a large layer number with no wildcard is discarded with the box message") {
    OasSession s;
    Oas f;
    f.cell("c").add(rect(1000, 0, 0, 0, 10, 10)).add(polygon(1001, 0, 0, 0, {{10, 0}, {10, 10}, {0, 10}}));
    f.add(path(1002, 0, 5, 0, 0, 0, 0, {{100, 0}})).add(text(1003, 0, 0, 0, "L"));
    (void)s.read_oasis(f.bytes());
    CHECK(s.said("Unknown layer/datatype in box, layer=1000 type=0"));
    CHECK(s.said("Unknown layer/datatype in boundary, layer=1001 type=0"));
    CHECK(s.said("Unknown layer/datatype in path, layer=1002 type=0"));
    CHECK(s.said("Label on unknown layer/datatype, layer=1003 type=0"));
    CHECK(s.painted("c", "metal1").empty());
  }
  SECTION("LAYERNAME records do not affect the mapping (Q-OASIS-38)") {
    OasSession s;
    Oas f;
    f.layer_name("metal2", 68, 20);
    f.cell("c").add(rect(68, 20, 0, 0, 10, 10));
    (void)s.read_oasis(f.bytes());
    CHECK(s.painted("c", "metal1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  }
}

TEST_CASE("shape repetitions", "[cap:oasis-read][req:Shape repetitions]") {
  SECTION("a rectangle array reads as one boundary per displacement, in order") {
    Oas f;
    f.cell("c").add(with(rect(68, 20, 0, 0, 10, 10), grid(2, 3, {20, 0})));
    GdsFile g = gds_cell([](GdsFile& b) {
      for (const std::int32_t x : {0, 20, 40}) {
        b.boundary(68, 20, {{x, 0}, {x + 10, 0}, {x + 10, 10}, {x, 10}, {x, 0}});
      }
    });
    const auto [oas, gds] = both(f, g);
    CHECK(oas == gds);
  }
  SECTION("every instance counts toward the flatten limit") {
    for (const std::uint64_t n : {12U, 9U}) {
      OasSession s;
      s.oasis_options.stream_read.flatten = true;
      s.oasis_options.stream_read.flatten_limit = 10;
      Oas f;
      f.cell("leaf").add(with(rect(68, 20, 0, 0, 10, 10), grid(2, n, {20, 0})));
      f.cell("top").add(place("leaf", 0, 0));
      (void)s.read_oasis(f.bytes());
      INFO(n << " instances");
      CHECK(uses(s.cell("top")).size() == (n >= 10 ? 1U : 0U));
    }
  }
  SECTION("coincident instances are all painted") {
    Oas f;
    f.cell("c").add(with(rect(68, 20, 0, 0, 10, 10), list(4, {{0, 0}, {0, 0}})));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.painted("c", "metal1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  }
}

TEST_CASE("RECTANGLE elements", "[cap:oasis-read][req:RECTANGLE elements]") {
  SECTION("a pin rectangle gives the port label its rectangle") {
    Oas f;
    f.cell("c").add(text(68, 5, 5, 5, "IN")).add(rect(68, 16, 0, 0, 10, 10));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    const db::Label* l = label(s.cell("c"), "IN");
    REQUIRE(l != nullptr);
    CHECK(l->rect == db::Rect{0, 0, 10, 10});
    CHECK(l->port_index == 1);
    Oas again;
    again.cell("c").add(text(68, 5, 5, 5, "IN")).add(rect(68, 16, 0, 0, 10, 10));
    GdsFile g = gds_cell([](GdsFile& b) {
      b.text(68, 5, {5, 5}, "IN");
      b.boundary(68, 16, {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}});
    });
    const auto [oas, gds] = both(again, g);
    CHECK(oas == gds);
  }
  SECTION("a zero-width rectangle paints nothing") {
    Oas f;
    f.cell("c").add(rect(68, 20, 0, 0, 0, 10)).add(rect(68, 20, 20, 0, 10, 10));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.painted("c", "metal1") == std::vector<db::Rect>{{20, 0, 30, 10}});
  }
}

TEST_CASE("POLYGON, TRAPEZOID and CTRAPEZOID elements",
          "[cap:oasis-read][req:POLYGON, TRAPEZOID and CTRAPEZOID elements]") {
  SECTION("45-degree triangle") {
    Oas f;
    f.cell("c").add(polygon(68, 20, 0, 0, {{10, 0}, {10, 10}}));
    GdsFile g = gds_cell([](GdsFile& b) { b.boundary(68, 20, {{0, 0}, {10, 0}, {10, 10}, {0, 0}}); });
    const auto [oas, gds] = both(f, g);
    CHECK(oas == gds);
    OasSession s;
    Oas again;
    again.cell("c").add(polygon(68, 20, 0, 0, {{10, 0}, {10, 10}}));
    (void)s.read_oasis(again.bytes());
    CHECK(s.area("c", "metal1") == 50.0);
    CHECK_FALSE(s.said("Boundary is not closed."));
  }
  SECTION("CTRAPEZOID triangle") {
    oasis::Element e;
    e.kind = oasis::ElementKind::ctrapezoid;
    e.layer = 68;
    e.datatype = 20;
    e.ctrapezoid_type = 16;
    e.size = {10, 0};
    Oas f;
    f.cell("c").add(e);
    GdsFile g = gds_cell([](GdsFile& b) { b.boundary(68, 20, {{0, 0}, {0, 10}, {10, 0}, {0, 0}}); });
    const auto [oas, gds] = both(f, g);
    CHECK(oas == gds);
  }
  SECTION("TRAPEZOID") {
    oasis::Element e;
    e.kind = oasis::ElementKind::trapezoid;
    e.layer = 68;
    e.datatype = 20;
    e.position = {0, 0};
    e.size = {40, 10};
    e.delta_a = 10;
    e.delta_b = -10;
    Oas f;
    f.cell("c").add(e);
    GdsFile g = gds_cell([](GdsFile& b) { b.boundary(68, 20, {{10, 10}, {30, 10}, {40, 0}, {0, 0}, {10, 10}}); });
    const auto [oas, gds] = both(f, g);
    CHECK(oas == gds);
  }
  SECTION("a polygon over 8191 points is fractured (D1 of 2026-11-19; see also limits_test.cpp)") {
    std::vector<oasis::Point64> rest;
    for (std::int64_t i = 1; i <= 4200; ++i) {
      rest.push_back({2 * i - 1, i % 2 == 1 ? 10 : 0});
      rest.push_back({2 * i, i % 2 == 1 ? 10 : 0});
    }
    rest.push_back({8400, -5});
    rest.push_back({0, -5});
    Oas f;
    f.cell("c").add(polygon(68, 20, 0, 0, rest)).add(rect(68, 20, 0, 100, 10, 10));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.said("polygon with 8404 points fractured into"));
    CHECK_FALSE(s.said("dropped"));
    // The band under the teeth is painted whole, and so is the rectangle.
    CHECK(s.extent("c", "metal1") == db::Rect{0, -5, 8400, 110});
    CHECK(s.area("c", "metal1") >= 8400.0 * 5 + 100);
  }
}

TEST_CASE("CIRCLE elements", "[cap:oasis-read][req:CIRCLE elements]") {
  SECTION("small circle") {
    const std::vector<oasis::Point64> expected{{10, 0},  {7, 7},    {0, 10}, {-7, 7},
                                               {-10, 0}, {-7, -7}, {0, -10}, {7, -7}};
    CHECK(oasis::circle_vertices(10, 8) == expected);
  }
  SECTION("default circle vertices are fixed") {
    const std::vector<oasis::Point64> first{{1000, 0}, {995, 98},  {981, 195}, {957, 290}, {924, 383}, {882, 471},
                                            {831, 556}, {773, 634}, {707, 707}, {634, 773}, {556, 831}, {471, 882},
                                            {383, 924}, {290, 957}, {195, 981}, {98, 995},  {0, 1000}};
    const std::vector<oasis::Point64> v = oasis::circle_vertices(1000, 64);
    REQUIRE(v.size() == 64);
    for (std::size_t k = 0; k <= 16; ++k) {
      INFO("k = " << k);
      CHECK(v[k] == first[k]);
      CHECK(v[32 - k] == oasis::Point64{-first[k].x, first[k].y});
      CHECK(v[(32 + k) % 64] == oasis::Point64{-first[k].x, -first[k].y});
      CHECK(v[(64 - k) % 64] == oasis::Point64{first[k].x, -first[k].y});
    }
  }
  SECTION("the unit vectors are exact at the quarter turns") {
    for (const std::uint32_t n : {8U, 12U, 64U, 4096U}) {
      CHECK(oasis::unit_vector(0, n).cos == 1.0);
      CHECK(oasis::unit_vector(0, n).sin == 0.0);
      CHECK(oasis::unit_vector(n / 4, n).cos == 0.0);
      CHECK(oasis::unit_vector(n / 4, n).sin == 1.0);
      CHECK(oasis::unit_vector(n / 2, n).cos == -1.0);
    }
  }
  SECTION("the circle reads as the BOUNDARY of its vertices, with circlepoints") {
    Oas f;
    f.cell("c").add(circle(68, 20, 100, 100, 10));
    OasSession o;
    o.oasis_options.settings.circlepoints = 8;
    (void)o.read_oasis(f.bytes());
    OasSession g;
    GdsFile gf = gds_cell([](GdsFile& b) {
      b.boundary(68, 20, {{110, 100}, {107, 107}, {100, 110}, {93, 107}, {90, 100}, {93, 93}, {100, 90}, {107, 93},
                          {110, 100}});
    });
    g.read(gf);
    CHECK(o.mag("c") == g.mag("c"));
  }
  SECTION("a radius of 0 gives no geometry and no message") {
    Oas f;
    f.cell("c").add(circle(68, 20, 0, 0, 0));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.painted("c", "metal1").empty());
    CHECK(s.count("Error while reading cell") == 0);
  }
}

TEST_CASE("PATH elements", "[cap:oasis-read][req:PATH elements]") {
  SECTION("half-width extended path: PATHTYPE 2") {
    Oas f;
    f.cell("c").add(path(68, 20, 5, 5, 5, 0, 0, {{100, 0}}));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.extent("c", "metal1") == db::Rect{-5, -5, 105, 5});
    Oas again;
    again.cell("c").add(path(68, 20, 5, 5, 5, 0, 0, {{100, 0}}));
    GdsFile g = gds_cell([](GdsFile& b) { b.path(68, 20, PathSpec{2, 10, {}, {}}, {{0, 0}, {100, 0}}); });
    const auto [oas, gds] = both(again, g);
    CHECK(oas == gds);
  }
  SECTION("mixed extensions: PATHTYPE 4") {
    Oas f;
    f.cell("c").add(path(68, 20, 5, 5, 0, 0, 0, {{100, 0}}));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.extent("c", "metal1") == db::Rect{-5, -5, 100, 5});
  }
  SECTION("flush ends: PATHTYPE 0") {
    Oas f;
    f.cell("c").add(path(68, 20, 5, 0, 0, 0, 0, {{100, 0}}));
    GdsFile g = gds_cell([](GdsFile& b) { b.path(68, 20, PathSpec{0, 10, {}, {}}, {{0, 0}, {100, 0}}); });
    const auto [oas, gds] = both(f, g);
    CHECK(oas == gds);
  }
  SECTION("negative extensions follow gds-read in each mode (Q-OASIS-33)") {
    for (const bool magic_compat : {true, false}) {
      INFO("magic-compat " << magic_compat);
      Oas f;
      f.cell("c").add(path(68, 20, 5, -3, 0, 0, 0, {{100, 0}}));
      OasSession s(kStyle, magic_compat);
      (void)s.read_oasis(f.bytes());
      CHECK(s.extent("c", "metal1").x0 == (magic_compat ? 0 : 3));
      Oas again;
      again.cell("c").add(path(68, 20, 5, -3, 0, 0, 0, {{100, 0}}));
      GdsFile g = gds_cell([](GdsFile& b) { b.path(68, 20, PathSpec{4, 10, -3, 0}, {{0, 0}, {100, 0}}); });
      const auto [oas, gds] = both(again, g, kStyle, magic_compat);
      CHECK(oas == gds);
    }
  }
  SECTION("a half-width of 0 drops the path") {
    Oas f;
    f.cell("c").add(path(68, 20, 0, 0, 0, 0, 0, {{100, 0}})).add(rect(68, 20, 0, 50, 10, 10));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.painted("c", "metal1") == std::vector<db::Rect>{{0, 50, 10, 60}});
  }
}

TEST_CASE("TEXT elements", "[cap:oasis-read][req:TEXT elements]") {
  SECTION("port labels from OASIS text") {
    Oas f;
    f.cell("c").add(text(68, 5, 5, 5, "A")).add(text(68, 5, 15, 5, "B"));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    const db::Label* a = label(s.cell("c"), "A");
    const db::Label* b = label(s.cell("c"), "B");
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    CHECK(a->port_index == 1);
    CHECK(b->port_index == 2);
    CHECK(a->sticky);
    CHECK(a->port_directions == (db::kPortNorth | db::kPortSouth | db::kPortEast | db::kPortWest));
  }
  SECTION("the label reads as the GDS TEXT with no presentation") {
    Oas f;
    f.cell("c").add(text(68, 20, 3, 4, "net1"));
    GdsFile g = gds_cell([](GdsFile& b) { b.text(68, 20, {3, 4}, "net1"); });
    const auto [oas, gds] = both(f, g);
    CHECK(oas == gds);
  }
  SECTION("a TEXTSTRING reference with no definition is an empty label") {
    Oas f(/*strict=*/true);
    f.cell("c").add(text(68, 20, 3, 4, ""));
    OasSession s;
    (void)s.read_oasis(f.bytes());
    CHECK(s.said("Ignoring empty string label at (3, 4)"));
  }
}

TEST_CASE("unsupported and extension content", "[cap:oasis-read][req:Unsupported and extension content]") {
  oasis::Element x;
  x.kind = oasis::ElementKind::xgeometry;
  x.layer = 68;
  x.datatype = 20;
  x.attribute = 1;
  x.payload = "opaque";
  Oas f;
  f.cell("c").add(x).add(x).add(rect(68, 20, 0, 0, 10, 10));
  f.cell("d").add(x);
  OasSession s;
  (void)s.read_oasis(f.bytes());
  CHECK(s.painted("c", "metal1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  CHECK(s.count("XGEOMETRY not supported: skipping.") == 2);  // once per cell
}
