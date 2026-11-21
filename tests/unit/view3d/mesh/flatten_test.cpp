// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Flatten and clip of the layout-layer mesh source (M10 task 6.1;
// openspec/changes/m10-3d-view/design.md D6; Q-MISC-70, fix in both modes).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>

#include "hefesto/db/transform.hpp"
#include "mesh_fixture.hpp"

namespace db = hefesto::db;
namespace view3d = hefesto::view3d;
using db::Rect;
using view3d::test::Seg;

namespace {

constexpr std::uint8_t kLeft = view3d::side_flag::outward_left;
constexpr std::uint8_t kClip = view3d::side_flag::clip_edge;

}  // namespace

TEST_CASE("a translated subcell under a cut box is clipped exactly to it in root coordinates",
          "[cap:view-3d][req:Rendered geometry]") {
  const auto t = view3d::test::build_tech();
  db::CellLibrary lib;
  db::CellDef& root = *lib.create("root", t.tech->plane_count()).value();
  db::CellDef& child = *lib.create("child", t.tech->plane_count()).value();
  view3d::test::paint(child, t, Rect{0, 0, 100, 10}, t.metal1);
  view3d::test::place(root, child, db::translation(1000, 0), "u1");
  const auto stack = view3d::test::stack_of({view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F)});

  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    const view3d::LayerStack s{view3d::Mode::layout_layer, {stack.layers().begin(), stack.layers().end()}, compat,
                               0.01};
    // The cut box covers part of the instance (x 1020..1050 of 1000..1100,
    // y 2..8 of 0..10): every side face is a closed cut face.
    const Rect cut{1020, 2, 1050, 8};
    const auto mesh = view3d::test::build(s, root, cut);
    REQUIRE(mesh.has_value());
    CHECK(view3d::test::rects_of(*mesh, 0) == std::vector<Seg>{{1020, 2, 1050, 8, 0}});
    CHECK(view3d::test::sides_of(*mesh, 0) == std::vector<Seg>{{1020, 2, 1020, 8, kClip | kLeft},
                                                               {1020, 2, 1050, 2, kClip},
                                                               {1020, 8, 1050, 8, kClip | kLeft},
                                                               {1050, 2, 1050, 8, kClip}});
    CHECK(view3d::test::open_edge(*mesh, 0).empty());
  }
}

TEST_CASE("a rotated subcell is clipped after its transform", "[cap:view-3d][req:Rendered geometry]") {
  const auto t = view3d::test::build_tech();
  db::CellLibrary lib;
  db::CellDef& root = *lib.create("root", t.tech->plane_count()).value();
  db::CellDef& child = *lib.create("child", t.tech->plane_count()).value();
  view3d::test::paint(child, t, Rect{0, 0, 100, 10}, t.metal1);
  // 90 degrees counter-clockwise, then up by 500: the shape is x -10..0, y 500..600.
  view3d::test::place(root, child, db::Transform{0, -1, 0, 1, 0, 500}, "u1");
  const auto stack = view3d::test::stack_of({view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F)});
  const auto mesh = view3d::test::build(stack, root, Rect{-20, 550, 20, 700});
  REQUIRE(mesh.has_value());
  CHECK(view3d::test::rects_of(*mesh, 0) == std::vector<Seg>{{-10, 550, 0, 600, 0}});
  // Left, right and top are the shape's own sides; the bottom is the cut.
  CHECK(view3d::test::sides_of(*mesh, 0) == std::vector<Seg>{{-10, 550, -10, 600, kLeft},
                                                             {-10, 550, 0, 550, kClip},
                                                             {-10, 600, 0, 600, kLeft},
                                                             {0, 550, 0, 600, 0}});
}

TEST_CASE("without a cut box an instance beside the root's paint has no missing or spurious faces",
          "[cap:view-3d][req:Rendered geometry]") {
  // The decision table's example (Q-MISC-70): root (0,0)-(2000,100) with an
  // instance at x+1000. Magic clips the instance against the window area in
  // its own coordinates, dropping its top faces and adding side faces.
  const auto t = view3d::test::build_tech();
  db::CellLibrary lib;
  db::CellDef& root = *lib.create("root", t.tech->plane_count()).value();
  db::CellDef& child = *lib.create("child", t.tech->plane_count()).value();
  view3d::test::paint(root, t, Rect{0, 0, 1000, 100}, t.metal1);
  view3d::test::paint(child, t, Rect{0, 0, 1000, 100}, t.metal1);
  view3d::test::place(root, child, db::translation(1000, 0), "u1");
  const auto stack = view3d::test::stack_of({view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F)});
  // The window area (about the bbox fitted to a square window) grown by 1.
  const Rect window = db::expand(Rect{0, -950, 2000, 1050}, 1);
  const auto mesh = view3d::test::build(stack, root, window, false);
  REQUIRE(mesh.has_value());
  CHECK(view3d::test::face_area(*mesh, 0) == 2000.0 * 100.0);
  CHECK(view3d::test::sides_of(*mesh, 0) == std::vector<Seg>{{0, 0, 0, 100, kLeft},
                                                             {0, 0, 2000, 0, 0},
                                                             {0, 100, 2000, 100, kLeft},
                                                             {2000, 0, 2000, 100, 0}});
  CHECK(view3d::test::open_edge(*mesh, 0).empty());
}

TEST_CASE("a box over one transistor meshes only the geometry inside it",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const auto t = view3d::test::build_tech();
  db::CellLibrary lib;
  db::CellDef& root = *lib.create("root", t.tech->plane_count()).value();
  db::CellDef& fet = *lib.create("fet", t.tech->plane_count()).value();
  view3d::test::paint(fet, t, Rect{0, 5, 20, 15}, t.ndiff);
  view3d::test::paint(fet, t, Rect{8, 0, 12, 20}, t.poly);
  view3d::test::place(root, fet, db::kIdentity, "m1");
  view3d::test::place(root, fet, db::translation(100, 0), "m2");
  const auto stack = view3d::test::stack_of(
      {view3d::test::layer("ndiff", t.ndiff, 0.0F, 0.1F), view3d::test::layer("poly", t.poly, 0.1F, 0.2F)});
  // Halo 0 for the layout-layer source: the box is the region.
  const Rect box{0, 0, 20, 20};
  const auto mesh = view3d::test::build(stack, root, box);
  REQUIRE(mesh.has_value());
  CHECK(mesh->region == box);
  CHECK(view3d::test::face_area(*mesh, 0) == 160.0);
  CHECK(view3d::test::face_area(*mesh, 1) == 80.0);
  for (view3d::LayerId l = 0; l < 2; ++l) {
    for (const Seg& r : view3d::test::rects_of(*mesh, l)) {
      CHECK((r.x0 >= 0 && r.x1 <= 20 && r.y0 >= 0 && r.y1 <= 20));
    }
    for (const Seg& s : view3d::test::sides_of(*mesh, l)) {
      CHECK((s.x0 >= 0 && s.x1 <= 20 && s.y0 >= 0 && s.y1 <= 20));
    }
    CHECK(view3d::test::open_edge(*mesh, l).empty());
  }
  // The poly ends on the box's top and bottom edges: closed cut faces.
  const auto poly_sides = view3d::test::sides_of(*mesh, 1);
  CHECK(std::count_if(poly_sides.begin(), poly_sides.end(),
                      [](const Seg& s) { return (s.flags & kClip) != 0; }) == 2);
}

TEST_CASE("a type in two display styles is flattened once and meshed per (type, style) pair",
          "[cap:view-3d][req:Rendered geometry]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  view3d::test::paint(root, t, Rect{0, 0, 10, 10}, t.metal1);
  const auto stack = view3d::test::stack_of({view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F, 20),
                                             view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F, 21)});
  const auto mesh = view3d::test::build(stack, root, Rect{-1, -1, 11, 11}, false);
  REQUIRE(mesh.has_value());
  CHECK(view3d::test::rects_of(*mesh, 0) == view3d::test::rects_of(*mesh, 1));
  CHECK(view3d::test::rects_of(*mesh, 0).size() == 1);
  CHECK(view3d::test::sides_of(*mesh, 1).size() == 4);
}

TEST_CASE("a cancelled flatten returns no model", "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  view3d::test::paint(root, t, Rect{0, 0, 10, 10}, t.metal1);
  const auto stack = view3d::test::stack_of({view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F)});
  std::atomic<bool> cancel{true};
  view3d::BuildContext ctx;
  ctx.progress.cancel = &cancel;
  CHECK_FALSE(view3d::build_from_layout(stack, root, view3d::ClipArea{Rect{0, 0, 10, 10}, true, 0}, ctx).has_value());
}
