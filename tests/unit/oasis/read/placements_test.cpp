// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PLACEMENT elements read through oasis::read (tasks.md 11.3): transforms
// (Q-OASIS-12), magnified copies, repetitions as arrays or separate uses
// (Q-OASIS-10), instance properties, and the post-order reading and
// flattening options, compared with the GDS-equivalent SREF/AREF where one
// exists. Owner: OASR.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/db/property.hpp"
#include "hefesto/db/transform.hpp"
#include "oasis_read_fixture.hpp"

using namespace oasis_read_test;

namespace {

void unit_cell(Oas& f, std::string_view name) {
  f.cell(name).add(rect(68, 20, 0, 0, 10, 10));
}

void unit_structure(GdsFile& f, std::string_view name) {
  f.begin(name).boundary(68, 20, {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}}).end();
}

const db::CellUse& only_use(const OasSession& s, std::string_view parent) {
  const auto u = uses(s.cell(parent));
  REQUIRE(u.size() == 1);
  return *u.front();
}

}  // namespace

TEST_CASE("placement transforms", "[cap:oasis-read][req:Placement transforms]") {
  SECTION("rotated and flipped placement") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("top").add(place("A", 100, 0, 90.0, true));
    (void)s.read_oasis(f.bytes());
    CHECK(db::apply(only_use(s, "top").transform, db::Point{1, 2}) == db::Point{102, 1});
  }
  SECTION("1.5x placement makes a magnified copy") {
    OasSession s;
    Oas f;
    unit_cell(f, "C");
    f.cell("top").add(place("C", 0, 0, 0.0, false, 1.5));
    (void)s.read_oasis(f.bytes());
    REQUIRE(s.has_cell("C_mag1p5"));
    CHECK(s.painted("C_mag1p5", "metal1") == std::vector<db::Rect>{{0, 0, 15, 15}});
    const auto* source = std::get_if<std::string>(s.cell("C_mag1p5").properties().find("GDS_MAG_SOURCE"));
    REQUIRE(source != nullptr);
    CHECK(*source == "C");
    CHECK(only_use(s, "top").def->name() == "C_mag1p5");
  }
  SECTION("a non-quarter-turn angle is rounded with the gds-read error") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("top").add(place("A", 0, 0, 30.0));
    (void)s.read_oasis(f.bytes());
    CHECK(s.said("Non-Manhattan angle (30) in transform"));
    CHECK(only_use(s, "top").transform == db::kIdentity);
  }
  SECTION("every transform reads as the GDS SREF") {
    for (const double angle : {0.0, 90.0, 180.0, 270.0}) {
      for (const bool flip : {false, true}) {
        Oas f;
        unit_cell(f, "A");
        f.cell("top").add(place("A", 7, -3, angle, flip, 2.0));
        OasSession o;
        (void)o.read_oasis(f.bytes());
        GdsFile g;
        g.library();
        unit_structure(g, "A");
        g.begin("top").sref("A", {7, -3}, RefSpec{flip ? 0x8000 : 0, 2.0, angle, {}}).end().end_library();
        OasSession r;
        r.read(g);
        INFO(angle << " " << flip);
        CHECK(o.mag("top") == r.mag("top"));
      }
    }
  }
}

TEST_CASE("placement repetitions", "[cap:oasis-read][req:Placement repetitions]") {
  SECTION("simple 3x2 array") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("top").add(with(place("A", 0, 0), grid(1, 3, {100, 0}, 2, {0, 200})));
    (void)s.read_oasis(f.bytes());
    const db::CellUse& u = only_use(s, "top");
    CHECK(u.array.xlo == 0);
    CHECK(u.array.xhi == 2);
    CHECK(u.array.ylo == 0);
    CHECK(u.array.yhi == 1);
    CHECK(u.array.xsep == 100);
    CHECK(u.array.ysep == 200);
    GdsFile g;
    g.library();
    unit_structure(g, "A");
    g.begin("top").aref("A", 3, 2, {{0, 0}, {300, 0}, {0, 400}}).end().end_library();
    OasSession r;
    r.read(g);
    CHECK(s.mag("top") == r.mag("top"));
  }
  SECTION("rotated array: rule 2") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("top").add(with(place("A", 0, 0, 90.0), grid(1, 2, {400, 0}, 3, {0, 100})));
    (void)s.read_oasis(f.bytes());
    const db::CellUse& u = only_use(s, "top");
    CHECK(u.array.xlo == 0);
    CHECK(u.array.xhi == 2);
    CHECK(u.array.ylo == 0);
    CHECK(u.array.yhi == 1);
    CHECK(u.array.xsep == 100);
    CHECK(u.array.ysep == -400);
  }
  SECTION("irregular repetition: separate uses") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("top").add(with(place("A", 0, 0), list(4, {{5, 0}, {12, 0}})));
    (void)s.read_oasis(f.bytes());
    std::vector<db::Coord> xs;
    for (const db::CellUse* u : uses(s.cell("top"))) {
      xs.push_back(u->transform.c);
    }
    std::sort(xs.begin(), xs.end());
    CHECK(xs == std::vector<db::Coord>{0, 5, 12});
  }
  SECTION("diagonal grid vectors: separate uses") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("top").add(with(place("A", 0, 0), grid(8, 2, {50, 50}, 2, {-50, 50})));
    (void)s.read_oasis(f.bytes());
    CHECK(uses(s.cell("top")).size() == 4);
  }
  SECTION("coincident displacements: the duplicate-use rule (Q-OASIS-10)") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("top").add(with(place("A", 0, 0), list(10, {{5, 0}, {5, 0}})));
    (void)s.read_oasis(f.bytes());
    CHECK(uses(s.cell("top")).size() == 2);
    CHECK(s.said("Warning: cell \"A\" placed on top of itself.  Ignoring the extra one."));
  }
  SECTION("a zero spacing gives a zero separation") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("top").add(with(place("A", 0, 0), grid(2, 3, {0, 0})));
    (void)s.read_oasis(f.bytes());
    const db::CellUse& u = only_use(s, "top");
    CHECK(u.array.xhi == 2);
    CHECK(u.array.xsep == 0);
  }
  SECTION("a dimension above 32767 keeps its index range") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("top").add(with(place("A", 0, 0), grid(2, 40000, {20, 0})));
    (void)s.read_oasis(f.bytes());
    const db::CellUse& u = only_use(s, "top");
    CHECK(u.array.xlo == 0);
    CHECK(u.array.xhi == 39999);
    CHECK(u.array.xsep == 20);
  }
}

TEST_CASE("instance properties", "[cap:oasis-read][req:Instance properties]") {
  SECTION("named instance with array limits") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("top").add(with(place("A", 0, 0), grid(2, 2, {20, 0})));
    f.property(gds_property(61, "x/y")).property(gds_property(99, "1_2_0_0"));
    (void)s.read_oasis(f.bytes());
    const db::CellUse& u = only_use(s, "top");
    CHECK(u.id == "x_y");
    CHECK(u.array.xlo == 1);
    CHECK(u.array.xhi == 2);
    CHECK(u.array.ylo == 0);
    CHECK(u.array.yhi == 0);
  }
  SECTION("a non-standard property is ignored and processing goes on") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("top").add(place("A", 0, 0));
    oasis::Property other;
    other.name = "vendor";
    other.values = {oasis::PropertyValue::of_unsigned(5)};
    f.property(other).property(gds_property(61, "inst1"));
    (void)s.read_oasis(f.bytes());
    CHECK(only_use(s, "top").id == "inst1");
  }
  SECTION("an unknown attribute ends the list with magic-compat ON, is skipped OFF") {
    for (const bool magic_compat : {true, false}) {
      OasSession s(kStyle, magic_compat);
      Oas f;
      unit_cell(f, "A");
      f.cell("top").add(place("A", 0, 0));
      f.property(gds_property(7, "x")).property(gds_property(61, "inst1"));
      (void)s.read_oasis(f.bytes());
      INFO("magic-compat " << magic_compat);
      CHECK((only_use(s, "top").id == "inst1") == !magic_compat);
    }
  }
  SECTION("separate uses share the identifier, made unique at the end of the cell") {
    OasSession s;
    Oas f;
    unit_cell(f, "A");
    f.cell("top").add(with(place("A", 0, 0), list(4, {{50, 0}})));
    f.property(gds_property(61, "u"));
    (void)s.read_oasis(f.bytes());
    const auto u = uses(s.cell("top"));
    REQUIRE(u.size() == 2);
    CHECK(u[0]->id != u[1]->id);
  }
  SECTION("properties of geometry and cells are discarded (Q-OASIS-37)") {
    OasSession s;
    Oas f;
    f.cell("A").property(gds_property(1, "cellprop")).add(rect(68, 20, 0, 0, 10, 10));
    f.property(gds_property(1, "shapeprop"));
    (void)s.read_oasis(f.bytes());
    CHECK(s.cell("A").properties().find("1") == nullptr);
    CHECK(s.mag("A").find("prop") == std::string::npos);
  }
}

TEST_CASE("post-order reading and flattening options",
          "[cap:oasis-read][req:Post-order reading and flattening options]") {
  SECTION("ordering on with a top-down file") {
    OasSession s;
    s.oasis_options.stream_read.ordering = true;
    Oas f;
    f.cell("TOP").add(place("A", 0, 0));
    unit_cell(f, "A");
    (void)s.read_oasis(f.bytes());
    CHECK(uses(s.cell("TOP")).size() == 1);
    CHECK_FALSE(s.said("already defined"));
    CHECK_FALSE(s.said("Rewinding"));
  }
  SECTION("look-ahead for an undefined cell never rewinds") {
    OasSession s;
    s.oasis_options.stream_read.flatten = true;
    const bool rewound_before = s.editor.mask.gds.rewound;
    Oas f;
    unit_cell(f, "B");
    f.cell("TOP").add(place("ghost", 0, 0));
    (void)s.read_oasis(f.bytes());
    CHECK_FALSE(s.said("Rewinding"));
    CHECK(s.said("Cell \"ghost\" is used but not defined in this file."));
    CHECK(s.cell("ghost").flags().dereference);
    CHECK(s.editor.mask.gds.rewound == rewound_before);
  }
  SECTION("default flatglob") {
    OasSession s;
    Oas f;
    f.cell("pcell_CDNS_7")
        .add(rect(68, 20, 0, 0, 10, 10))
        .add(rect(68, 20, 20, 0, 10, 10))
        .add(rect(68, 20, 40, 0, 10, 10));
    f.cell("TOP").add(place("pcell_CDNS_7", 100, 0));
    (void)s.read_oasis(f.bytes());
    CHECK(uses(s.cell("TOP")).empty());
    CHECK(s.painted("TOP", "metal1").size() == 3);
  }
  SECTION("flattened array copies per element") {
    OasSession s;
    s.oasis_options.stream_read.flatten = true;
    Oas f;
    unit_cell(f, "leaf");
    f.cell("TOP").add(with(place("leaf", 0, 0), grid(2, 3, {20, 0})));
    (void)s.read_oasis(f.bytes());
    CHECK(uses(s.cell("TOP")).empty());
    CHECK(s.painted("TOP", "metal1").size() == 3);
  }
  SECTION("separate flattened uses each paint, coincident ones included") {
    OasSession s;
    s.oasis_options.stream_read.flatten = true;
    Oas f;
    unit_cell(f, "leaf");
    f.cell("TOP").add(with(place("leaf", 0, 0), list(10, {{20, 0}, {20, 0}})));
    (void)s.read_oasis(f.bytes());
    CHECK(uses(s.cell("TOP")).empty());
    CHECK_FALSE(s.said("placed on top of itself"));
    CHECK(s.painted("TOP", "metal1") == std::vector<db::Rect>{{0, 0, 10, 10}, {20, 0, 30, 10}});
  }
}
