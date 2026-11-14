// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// BOX, BOUNDARY and PATH elements read through gds::read (tasks.md 9.3):
// layer mapping, polygon decomposition, path outlines and extensions, path
// centerline properties and the automatic read-unit rescale, in both
// magic-compat settings where a decision applies (Q-MASK-113..116, 118,
// 119). Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/db/property.hpp"
#include "read_fixture.hpp"

using namespace gds_read_test;

namespace {

GdsFile one_structure(const std::function<void(GdsFile&)>& body) {
  GdsFile f;
  f.library().begin("top");
  body(f);
  f.end().end_library();
  return f;
}

std::vector<std::int32_t> dimensions(const db::CellDef& def, std::string_view name) {
  const db::PropertyValue* value = def.properties().find(name);
  REQUIRE(value != nullptr);
  const auto* dims = std::get_if<db::DimensionList>(value);
  REQUIRE(dims != nullptr);
  return dims->values;
}

bool has_split(const Session& s, std::string_view cell, std::string_view type_name) {
  const db::CellDef& def = s.cell(cell);
  tech::TypeMask m;
  m.set(s.type(type_name));
  bool split = false;
  def.plane(*s.editor.technology->home_plane(s.type(type_name))).enumerate(db::whole_plane(), m, [&](const db::TileView& v) {
    split = split || v.split;
    return true;
  });
  return split;
}

}  // namespace

TEST_CASE("BOX painted as its bounding box", "[cap:gds-read][req:BOX elements]") {
  Session s;
  s.read(one_structure([](GdsFile& f) { f.box(68, 20, {{0, 0}, {0, 5}, {8, 5}, {8, 0}, {0, 0}}); }));
  CHECK(s.painted("top", "m1") == std::vector<db::Rect>{{0, 0, 8, 5}});

  SECTION("a box without 5 points is dropped") {
    Session t;
    t.read(one_structure([](GdsFile& f) { f.box(68, 20, {{0, 0}, {0, 5}, {8, 5}, {8, 0}}); }));
    CHECK(t.said("Box doesn't have 5 points."));
    CHECK(t.painted("top", "m1").empty());
  }
}

TEST_CASE("BOUNDARY fills with the nonzero winding rule", "[cap:gds-read][req:BOUNDARY elements]") {
  SECTION("L-shaped boundary") {
    Session s;
    s.read(one_structure(
        [](GdsFile& f) { f.boundary(68, 20, {{0, 0}, {0, 20}, {10, 20}, {10, 10}, {20, 10}, {20, 0}, {0, 0}}); }));
    CHECK(s.area("top", "m1") == 300.0);
    CHECK(s.extent("top", "m1") == db::Rect{0, 0, 20, 20});
  }
  SECTION("a square traversed twice is painted once") {
    Session s;
    s.read(one_structure([](GdsFile& f) {
      f.boundary(68, 20, {{0, 0}, {0, 10}, {10, 10}, {10, 0}, {0, 0}, {0, 10}, {10, 10}, {10, 0}, {0, 0}});
    }));
    CHECK(s.area("top", "m1") == 100.0);
  }
  SECTION("an unclosed boundary is closed with an error") {
    Session s;
    s.read(one_structure([](GdsFile& f) { f.boundary(68, 20, {{0, 0}, {0, 10}, {10, 10}, {10, 0}}); }));
    CHECK(s.said("Boundary is not closed."));
    CHECK(s.painted("top", "m1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  }
  SECTION("a boundary on an unmapped layer is consumed silently") {
    Session s;
    s.read(one_structure([](GdsFile& f) { f.boundary(1, 1, {{0, 0}, {0, 10}, {10, 10}, {10, 0}, {0, 0}}); }));
    CHECK(s.painted("top", "m1").empty());
  }
}

TEST_CASE("non-Manhattan boundaries become split tiles", "[cap:gds-read][req:Non-Manhattan boundary decomposition]") {
  Session s;
  s.read(one_structure([](GdsFile& f) { f.boundary(68, 20, {{0, 0}, {10, 10}, {10, 0}, {0, 0}}); }));
  CHECK(s.painted("top", "m1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  CHECK(has_split(s, "top", "m1"));
  CHECK(s.area("top", "m1") == 50.0);

  SECTION("octagon without stair steps") {
    Session t;
    t.read(one_structure([](GdsFile& f) {
      f.boundary(68, 20, {{4, 0}, {8, 0}, {12, 4}, {12, 8}, {8, 12}, {4, 12}, {0, 8}, {0, 4}, {4, 0}});
    }));
    CHECK(t.area("top", "m1") == 144.0 - 4 * 8.0);
    CHECK(has_split(t, "top", "m1"));
  }
}

TEST_CASE("layer and datatype mapping", "[cap:gds-read][req:Layer and datatype mapping]") {
  Session s("style test\nscalefactor 1\nlayer m1 M1\ncalma M1 68 20\nlayer m2 M1PIN\ncalma M1PIN 68 *\n");
  s.read(one_structure([](GdsFile& f) {
    f.boundary(68, 20, {{0, 0}, {0, 10}, {10, 10}, {10, 0}, {0, 0}});
    f.boundary(68, 16, {{20, 0}, {20, 10}, {30, 10}, {30, 0}, {20, 0}});
    f.boundary(3, 3, {{0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}});
    f.box(3, 3, {{0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}});
    f.boundary(3, 3, {{0, 0}, {0, 2}, {2, 2}, {2, 0}, {0, 0}});
  }));
  CHECK(s.painted("top", "m1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  CHECK(s.painted("top", "m2") == std::vector<db::Rect>{{20, 0, 30, 10}});
  CHECK(s.count("Unknown layer/datatype in boundary, layer=3 type=3") == 1);
  CHECK(s.count("Unknown layer/datatype in box, layer=3 type=3") == 0);  // one report per pair and structure
}

TEST_CASE("PATH ends and widths", "[cap:gds-read][req:PATH elements]") {
  using Spec = GdsFile::PathSpec;
  const auto path = [](Spec spec, std::initializer_list<P> points, bool magic_compat = true) {
    auto s = std::make_unique<Session>(kStyle, magic_compat);
    s->read(one_structure([&](GdsFile& f) { f.path(68, 20, spec, points); }));
    return s;
  };
  SECTION("flush") {
    CHECK(path(Spec{0, 10, {}, {}}, {{0, 0}, {100, 0}})->extent("top", "m1") == db::Rect{0, -5, 100, 5});
  }
  SECTION("half-width extension") {
    CHECK(path(Spec{2, 10, {}, {}}, {{0, 0}, {100, 0}})->extent("top", "m1") == db::Rect{-5, -5, 105, 5});
  }
  SECTION("custom extensions") {
    CHECK(path(Spec{4, 10, 3, 7}, {{0, 0}, {100, 0}})->extent("top", "m1") == db::Rect{-3, -5, 107, 5});
  }
  SECTION("odd width") {
    // OFF: H = pi/2 gives offsets of exactly +-2.5, rounded away from zero.
    // ON: Magic's H = 1.5708 gives 4.99999999997 / 2, which rounds to 2
    // (design D22: Magic's float arithmetic, so the scenario's +-3 holds OFF).
    CHECK(path(Spec{0, 5, {}, {}}, {{0, 0}, {100, 0}}, false)->extent("top", "m1") == db::Rect{0, -3, 100, 3});
    CHECK(path(Spec{0, 5, {}, {}}, {{0, 0}, {100, 0}}, true)->extent("top", "m1") == db::Rect{0, -2, 100, 2});
  }
  SECTION("round path type, magic-compat ON") {
    const auto s = path(Spec{1, 10, {}, {}}, {{0, 0}, {100, 0}});
    CHECK(s->said("Warning: pathtype 1 unsupported (ignored)."));
    CHECK(s->extent("top", "m1") == db::Rect{0, -5, 100, 5});
  }
  SECTION("round path type, magic-compat OFF") {
    const auto s = path(Spec{1, 10, {}, {}}, {{0, 0}, {100, 0}}, false);
    CHECK(s->said("Q-MASK-113"));
    CHECK(s->extent("top", "m1") == db::Rect{-5, -5, 105, 5});
  }
  SECTION("negative width, magic-compat ON") {
    CHECK(path(Spec{0, -10, {}, {}}, {{0, 0}, {100, 0}})->painted("top", "m1").empty());
  }
  SECTION("negative width, magic-compat OFF") {
    const auto s = path(Spec{0, -10, {}, {}}, {{0, 0}, {100, 0}}, false);
    CHECK(s->said("Q-MASK-115"));
    CHECK(s->extent("top", "m1") == db::Rect{0, -5, 100, 5});
  }
  SECTION("zero width is dropped") {
    CHECK(path(Spec{0, 0, {}, {}}, {{0, 0}, {100, 0}})->painted("top", "m1").empty());
  }
  SECTION("unmapped layer") {
    Session s;
    s.read(one_structure([](GdsFile& f) { f.path(2, 2, Spec{0, 10, {}, {}}, {{0, 0}, {100, 0}}); }));
    CHECK(s.said("Unknown layer/datatype in path, layer=2 type=2"));
  }
}

TEST_CASE("path end extensions by magic-compat", "[cap:gds-read][req:PATH elements]") {
  using Spec = GdsFile::PathSpec;
  for (const bool magic_compat : {true, false}) {
    Session s(kStyle, magic_compat);
    s.options.settings.savepaths = true;
    s.read(one_structure([](GdsFile& f) { f.path(68, 20, Spec{4, 10, 0, 5}, {{0, 0}, {30, 40}}); }));
    const auto values = dimensions(s.cell("top"), "path");
    if (magic_compat) {
      CHECK(values == std::vector<std::int32_t>{0, 0, 35, 45});
    } else {
      CHECK(values == std::vector<std::int32_t>{0, 0, 33, 44});
    }
  }
}

TEST_CASE("path to polygon conversion", "[cap:gds-read][req:Path-to-polygon conversion]") {
  using Spec = GdsFile::PathSpec;
  SECTION("right-angle bend") {
    Session s;
    s.read(one_structure([](GdsFile& f) { f.path(68, 20, Spec{0, 10, {}, {}}, {{0, 0}, {100, 0}, {100, 100}}); }));
    CHECK(s.area("top", "m1") == 105.0 * 10 + 10.0 * 95);
    CHECK(s.extent("top", "m1") == db::Rect{0, -5, 105, 100});
  }
  SECTION("single-point square path") {
    Session s;
    s.read(one_structure([](GdsFile& f) { f.path(68, 20, Spec{2, 10, {}, {}}, {{50, 50}}); }));
    CHECK(s.painted("top", "m1") == std::vector<db::Rect>{{45, 45, 55, 55}});
  }
  SECTION("single-point flush path paints nothing") {
    Session s;
    s.read(one_structure([](GdsFile& f) { f.path(68, 20, Spec{0, 10, {}, {}}, {{50, 50}}); }));
    CHECK(s.painted("top", "m1").empty());
  }
  SECTION("45-degree segment") {
    Session s;
    s.read(one_structure([](GdsFile& f) { f.path(68, 20, Spec{0, 10, {}, {}}, {{0, 0}, {100, 100}}); }));
    CHECK(s.extent("top", "m1") == db::Rect{-4, -4, 104, 104});
    CHECK(s.area("top", "m1") == 1600.0);
    CHECK(has_split(s, "top", "m1"));
  }
}

TEST_CASE("path centerline properties", "[cap:gds-read][req:Path centerline properties]") {
  using Spec = GdsFile::PathSpec;
  Session s;
  s.options.settings.savepaths = true;
  s.read(one_structure([](GdsFile& f) {
    f.path(68, 20, Spec{0, 10, {}, {}}, {{0, 0}, {100, 0}});
    f.path(68, 20, Spec{0, 10, {}, {}}, {{0, 50}, {100, 50}});
  }));
  CHECK(dimensions(s.cell("top"), "path") == std::vector<std::int32_t>{0, 0, 100, 0});
  CHECK(dimensions(s.cell("top"), "path_0") == std::vector<std::int32_t>{0, 50, 100, 50});

  SECTION("internal units with magic-compat OFF") {
    for (const bool magic_compat : {true, false}) {
      Session t("style test\nscalefactor 10\nlayer m1 M1\ncalma M1 68 20\n", magic_compat);
      t.options.settings.savepaths = true;
      t.read(one_structure([](GdsFile& f) { f.path(68, 20, Spec{0, 20, {}, {}}, {{0, -30}, {100, -30}}); }));
      const auto values = dimensions(t.cell("top"), "path");
      if (magic_compat) {
        CHECK(values == std::vector<std::int32_t>{0, -30, 100, -30});
      } else {
        CHECK(values == std::vector<std::int32_t>{0, -3, 10, -3});
      }
    }
  }
}

TEST_CASE("off-grid coordinates rescale the read units", "[cap:gds-read][req:Coordinate conversion and automatic rescaling]") {
  Session s;
  GdsFile f;
  f.library("lib", 1e-9).begin("top");
  f.box(68, 20, {{0, 0}, {0, 10}, {15, 10}, {15, 0}, {0, 0}});
  f.end().end_library();
  s.read(f);
  CHECK(s.said("units rescaled by factor of 2 / 1"));
  CHECK(s.painted("top", "m1") == std::vector<db::Rect>{{0, 0, 3, 2}});
  const mask::InputStyle* style = s.editor.mask.input_styles.current_style();
  REQUIRE(style != nullptr);
  CHECK(style->multiplier == 2);  // the rescaled style is stored back, as Magic's global style
}
