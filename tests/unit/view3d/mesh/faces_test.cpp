// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Face generation of the layout-layer mesh (M10 task 6.2;
// openspec/changes/m10-3d-view/design.md D6 "Geometry"): top and bottom
// rectangles, side faces on boundary and clip segments, split-tile
// triangles with diagonal faces, closed cut faces, zero-thickness layers,
// and the closure and volume properties.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <random>
#include <vector>

#include "hefesto/db/transform.hpp"
#include "mesh_fixture.hpp"

namespace db = hefesto::db;
namespace view3d = hefesto::view3d;
using db::Rect;
using view3d::test::Seg;

namespace {

constexpr std::uint8_t kLeft = view3d::side_flag::outward_left;
constexpr std::uint8_t kClip = view3d::side_flag::clip_edge;
constexpr std::uint8_t kDiag = view3d::side_flag::diagonal;

// The eight Manhattan orientations (a, b, d, e).
constexpr std::array<std::array<int, 4>, 8> kOrientations = {{{1, 0, 0, 1},
                                                              {0, -1, 1, 0},
                                                              {-1, 0, 0, -1},
                                                              {0, 1, -1, 0},
                                                              {-1, 0, 0, 1},
                                                              {1, 0, 0, -1},
                                                              {0, 1, 1, 0},
                                                              {0, -1, -1, 0}}};

}  // namespace

TEST_CASE("a metal1 tile is a prism with four side faces", "[cap:view-3d][req:Rendered geometry]") {
  // Scenario "Metal prism in layout-layer mode": metal1 (0,0)-(10,4),
  // h = 2.0, t = 0.5, s_z = 25 spans z -62.5..-50. The mesh stores x and y;
  // the shader places z from the layer's h and t.
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  view3d::test::paint(root, t, Rect{0, 0, 10, 4}, t.metal1);
  const auto stack = view3d::test::stack_of({view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F)});
  const auto mesh = view3d::test::build(stack, root, db::expand(Rect{0, 0, 10, 4}, 1), false);
  REQUIRE(mesh.has_value());
  CHECK(view3d::test::rects_of(*mesh, 0) == std::vector<Seg>{{0, 0, 10, 4, 0}});
  CHECK(view3d::test::sides_of(*mesh, 0) ==
        std::vector<Seg>{{0, 0, 0, 4, kLeft}, {0, 0, 10, 0, 0}, {0, 4, 10, 4, kLeft}, {10, 0, 10, 4, 0}});
  const view3d::StackLayer& m1 = stack.layers()[0];
  const float s_z = 25.0F;
  const float z_top = -m1.height * s_z;
  CHECK(z_top == -50.0F);
  CHECK(z_top - (m1.thickness * s_z) == -62.5F);
}

TEST_CASE("a zero-thickness layer has top faces and no side faces", "[cap:view-3d][req:Rendered geometry]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  view3d::test::paint(root, t, Rect{0, 0, 10, 4}, t.metal1);
  view3d::test::paint(root, t, Rect{0, 0, 3, 3}, t.ndiff);
  const auto stack = view3d::test::stack_of(
      {view3d::test::layer("metal1", t.metal1, 2.0F, 0.0F), view3d::test::layer("ndiff", t.ndiff, 0.0F, 0.0F)});
  const auto mesh = view3d::test::build(stack, root, Rect{0, 0, 5, 5});
  REQUIRE(mesh.has_value());
  CHECK(view3d::test::rects_of(*mesh, 0) == std::vector<Seg>{{0, 0, 5, 4, 0}});
  CHECK(view3d::test::rects_of(*mesh, 1) == std::vector<Seg>{{0, 0, 3, 3, 0}});
  CHECK(view3d::test::sides_of(*mesh, 0).empty());
  CHECK(view3d::test::sides_of(*mesh, 1).empty());
}

TEST_CASE("abutting tiles of one type have no inner side faces", "[cap:view-3d][req:Rendered geometry]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  // An L: two tiles in the canonical decomposition, one outline.
  view3d::test::paint(root, t, Rect{0, 0, 10, 4}, t.metal1);
  view3d::test::paint(root, t, Rect{0, 4, 4, 10}, t.metal1);
  // poly beside ndiff on the same plane: each keeps the shared side.
  view3d::test::paint(root, t, Rect{20, 0, 30, 10}, t.ndiff);
  view3d::test::paint(root, t, Rect{30, 0, 40, 10}, t.poly);
  const auto stack = view3d::test::stack_of(
      {view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F), view3d::test::layer("ndiff", t.ndiff, 0.0F, 0.1F),
       view3d::test::layer("poly", t.poly, 0.1F, 0.2F)});
  const auto mesh = view3d::test::build(stack, root, Rect{-1, -1, 41, 11}, false);
  REQUIRE(mesh.has_value());
  CHECK(view3d::test::side_length(*mesh, 0) == 40.0);
  CHECK(view3d::test::side_length(*mesh, 1) == 40.0);
  CHECK(view3d::test::side_length(*mesh, 2) == 40.0);
  for (view3d::LayerId l = 0; l < 3; ++l) {
    CHECK(view3d::test::open_edge(*mesh, l).empty());
  }
}

TEST_CASE("a split tile renders as a triangle with a diagonal side face", "[cap:view-3d][req:Rendered geometry]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  // The upper-left triangle of (0,0)-(10,10) (`/` diagonal, left side).
  REQUIRE(db::paint_triangle(root, *t.tech, Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, false, t.metal1).ok());
  db::recompute_bbox(root);
  const auto stack = view3d::test::stack_of({view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F)});

  SECTION("whole") {
    const auto mesh = view3d::test::build(stack, root, Rect{-1, -1, 11, 11}, false);
    REQUIRE(mesh.has_value());
    CHECK(view3d::test::rects_of(*mesh, 0).empty());
    CHECK(view3d::test::face_area(*mesh, 0) == 50.0);
    CHECK(view3d::test::sides_of(*mesh, 0) ==
          std::vector<Seg>{{0, 0, 0, 10, kLeft}, {0, 0, 10, 10, kDiag}, {0, 10, 10, 10, kLeft}});
    CHECK(view3d::test::open_edge(*mesh, 0).empty());
  }
  SECTION("cut by the cut box: the exact polygon, a closed cut face") {
    const auto mesh = view3d::test::build(stack, root, Rect{-5, -5, 10, 5});
    REQUIRE(mesh.has_value());
    CHECK(view3d::test::face_area(*mesh, 0) == 12.5);
    CHECK(view3d::test::sides_of(*mesh, 0) ==
          std::vector<Seg>{{0, 0, 0, 5, kLeft}, {0, 0, 5, 5, kDiag}, {0, 5, 5, 5, kClip | kLeft}});
    CHECK(view3d::test::open_edge(*mesh, 0).empty());
  }
}

TEST_CASE("a split tile's right triangle takes its left type's height: flat over space",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  // Regression (PR #77 review): the right triangle took its own layer's h
  // and t. Magic draws it with the left type's (ExtGetZAxis), here space:
  // h = t = 0, so a top face and no side faces.
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  REQUIRE(db::paint_triangle(root, *t.tech, Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, true, t.metal1).ok());
  db::recompute_bbox(root);
  const auto stack = view3d::test::stack_of({view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F)});
  const auto mesh = view3d::test::build(stack, root, Rect{-1, -1, 11, 11}, false);
  REQUIRE(mesh.has_value());
  CHECK(view3d::test::face_area(*mesh, 0) == 50.0);
  CHECK(view3d::test::sides_of(*mesh, 0).empty());
  for (const view3d::Chunk& c : mesh->chunks) {
    for (const view3d::Triangle& tri : c.triangles) {
      CHECK(tri.height == 0.0F);
      CHECK(tri.thickness == 0.0F);
    }
  }
}

TEST_CASE("both triangles of a split tile use the left type's h and t, and its thickness decides the side faces",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  // Regression (PR #77 review): side faces were decided per layer.
  const auto t = view3d::test::build_tech();
  const auto build_split = [&](float ndiff_t, float poly_t) {
    db::CellDef root("root", t.tech->plane_count());
    view3d::test::paint(root, t, Rect{0, 0, 10, 10}, t.ndiff);
    REQUIRE(db::paint_triangle(root, *t.tech, Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, true, t.poly).ok());
    db::recompute_bbox(root);
    std::vector<view3d::DrawHeight> heights(hefesto::tech::kMaxTileTypes);
    heights[t.ndiff] = view3d::DrawHeight{0.3F, ndiff_t};
    heights[t.poly] = view3d::DrawHeight{0.1F, poly_t};
    const view3d::LayerStack stack{{view3d::test::layer("ndiff", t.ndiff, 0.3F, ndiff_t),
                                    view3d::test::layer("poly", t.poly, 0.1F, poly_t)},
                                   std::move(heights), true, 0.01};
    auto mesh = view3d::test::build(stack, root, Rect{-1, -1, 11, 11}, false);
    REQUIRE(mesh.has_value());
    return *mesh;
  };
  const auto poly_triangles = [](const view3d::MeshModel& m) {
    std::vector<view3d::Triangle> out;
    for (const view3d::Chunk& c : m.chunks) {
      for (const view3d::Triangle& tri : c.triangles) {
        if (tri.layer == 1) {
          out.push_back(tri);
        }
      }
    }
    return out;
  };
  SECTION("left type of zero thickness: the poly triangle sits at ndiff's height with no side faces") {
    const view3d::MeshModel m = build_split(0.0F, 0.2F);
    const auto tris = poly_triangles(m);
    REQUIRE_FALSE(tris.empty());
    for (const view3d::Triangle& tri : tris) {
      CHECK(tri.height == 0.3F);
      CHECK(tri.thickness == 0.0F);
    }
    CHECK(view3d::test::sides_of(m, 1).empty());
  }
  SECTION("poly of zero thickness: the poly triangle still gets side faces, with ndiff's h and t") {
    const view3d::MeshModel m = build_split(0.25F, 0.0F);
    for (const view3d::Triangle& tri : poly_triangles(m)) {
      CHECK(tri.height == 0.3F);
      CHECK(tri.thickness == 0.25F);
    }
    const auto sides = view3d::test::sides_of(m, 1);
    REQUIRE_FALSE(sides.empty());
    for (const view3d::Chunk& c : m.chunks) {
      for (const view3d::SideInstance& side : c.sides) {
        if (side.layer == 1) {
          CHECK((side.flags & view3d::side_flag::own_height) != 0);
          CHECK(side.height == 0.3F);
          CHECK(side.thickness == 0.25F);
        }
      }
    }
    CHECK(view3d::test::open_edge(m, 1).empty());
  }
}

// Q-MISC-79 (J3): a split tile whose left half is space.
// magic-compat ON keeps Magic (the left type's h and t: flat, no side
// faces); OFF draws each triangle with its own type's h, t and side faces.
namespace {

view3d::LayerStack split_stack(const view3d::test::MeshTech& t, bool magic_compat) {
  std::vector<view3d::DrawHeight> heights(hefesto::tech::kMaxTileTypes);
  heights[t.ndiff] = view3d::DrawHeight{0.3F, 0.25F};
  heights[t.poly] = view3d::DrawHeight{0.1F, 0.2F};
  heights[t.metal1] = view3d::DrawHeight{2.0F, 0.5F};
  return view3d::LayerStack{{view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F),
                             view3d::test::layer("ndiff", t.ndiff, 0.3F, 0.25F),
                             view3d::test::layer("poly", t.poly, 0.1F, 0.2F)},
                            std::move(heights), magic_compat, 0.01};
}

// A metal1 right triangle over space, and a poly right triangle over ndiff.
void paint_splits(db::CellDef& root, const view3d::test::MeshTech& t) {
  REQUIRE(db::paint_triangle(root, *t.tech, Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, true, t.metal1).ok());
  view3d::test::paint(root, t, Rect{20, 0, 30, 10}, t.ndiff);
  REQUIRE(db::paint_triangle(root, *t.tech, Rect{20, 0, 30, 10}, db::Diagonal::ne_sw, true, t.poly).ok());
  db::recompute_bbox(root);
}

std::vector<view3d::Triangle> layer_triangles(const view3d::MeshModel& m, view3d::LayerId layer) {
  std::vector<view3d::Triangle> out;
  for (const view3d::Chunk& c : m.chunks) {
    for (const view3d::Triangle& tri : c.triangles) {
      if (tri.layer == layer) {
        out.push_back(tri);
      }
    }
  }
  return out;
}

}  // namespace

TEST_CASE("J3 magic-compat ON: a right triangle over space is flat with no side faces",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  paint_splits(root, t);
  const auto stack = split_stack(t, true);
  const auto mesh = view3d::test::build(stack, root, Rect{-1, -1, 31, 11}, false);
  REQUIRE(mesh.has_value());
  const auto metal = layer_triangles(*mesh, 0);
  REQUIRE_FALSE(metal.empty());
  for (const view3d::Triangle& tri : metal) {
    CHECK(tri.height == 0.0F);
    CHECK(tri.thickness == 0.0F);
  }
  CHECK(view3d::test::sides_of(*mesh, 0).empty());
  for (const view3d::Triangle& tri : layer_triangles(*mesh, 2)) {
    CHECK(tri.height == 0.3F);  // ndiff, the left type
    CHECK(tri.thickness == 0.25F);
  }
}

TEST_CASE("J3 magic-compat OFF: each triangle takes its own type's height, thickness and side faces",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  const auto t = view3d::test::build_tech();
  db::CellDef root("root", t.tech->plane_count());
  paint_splits(root, t);
  const auto stack = split_stack(t, false);
  const auto mesh = view3d::test::build(stack, root, Rect{-1, -1, 31, 11}, false);
  REQUIRE(mesh.has_value());
  const auto metal = layer_triangles(*mesh, 0);
  REQUIRE_FALSE(metal.empty());
  for (const view3d::Triangle& tri : metal) {
    CHECK(tri.height == 2.0F);
    CHECK(tri.thickness == 0.5F);
  }
  for (const view3d::Triangle& tri : layer_triangles(*mesh, 2)) {
    CHECK(tri.height == 0.1F);
    CHECK(tri.thickness == 0.2F);
  }
  for (const view3d::LayerId l : {view3d::LayerId{0}, view3d::LayerId{2}}) {
    INFO("layer " << l);
    const auto sides = view3d::test::sides_of(*mesh, l);
    REQUIRE_FALSE(sides.empty());
    for (const Seg& s : sides) {
      CHECK((s.flags & view3d::side_flag::own_height) == 0);
    }
    CHECK(view3d::test::open_edge(*mesh, l).empty());
  }
  CHECK(view3d::test::face_area(*mesh, 0) == 50.0);
}

TEST_CASE("split tiles of a transformed subcell keep their area and stay closed",
          "[cap:view-3d][req:Rendered geometry]") {
  const auto t = view3d::test::build_tech();
  db::CellLibrary lib;
  db::CellDef& root = *lib.create("root", t.tech->plane_count()).value();
  db::CellDef& child = *lib.create("child", t.tech->plane_count()).value();
  // The left triangle, so the tile's left type (whose h and t both
  // triangles take) is metal1 itself.
  REQUIRE(db::paint_triangle(child, *t.tech, Rect{0, 0, 20, 10}, db::Diagonal::nw_se, false, t.metal1).ok());
  view3d::test::paint(child, t, Rect{0, 10, 20, 14}, t.metal1);
  for (std::size_t o = 0; o < kOrientations.size(); ++o) {
    const auto& m = kOrientations[o];
    view3d::test::place(root, child, db::Transform{m[0], m[1], 100 * static_cast<db::Coord>(o), m[2], m[3], 0},
                        "u" + std::to_string(o));
  }
  const auto stack = view3d::test::stack_of({view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F)});
  SECTION("whole") {
    const auto mesh = view3d::test::build(stack, root, Rect{-100, -100, 900, 100}, false);
    REQUIRE(mesh.has_value());
    CHECK(view3d::test::face_area(*mesh, 0) == 8.0 * (100.0 + 80.0));
    CHECK(view3d::test::open_edge(*mesh, 0).empty());
  }
  SECTION("cut through every triangle") {
    const auto mesh = view3d::test::build(stack, root, Rect{-100, -7, 900, 3});
    REQUIRE(mesh.has_value());
    CHECK(view3d::test::open_edge(*mesh, 0).empty());
    CHECK(view3d::test::triangles_of(*mesh, 0).size() >= 8);
  }
}

TEST_CASE("every layer's mesh is closed and its volume is the sum of area times thickness",
          "[cap:view-3d][req:Rendered geometry]") {
  const auto t = view3d::test::build_tech();
  std::mt19937 rng(20261120U);
  const auto coord = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
  const auto stack = view3d::test::stack_of(
      {view3d::test::layer("metal1", t.metal1, 2.0F, 0.5F), view3d::test::layer("ndiff", t.ndiff, 0.0F, 0.25F)});
  for (int round = 0; round < 25; ++round) {
    INFO("round " << round);
    db::CellLibrary lib;
    db::CellDef& root = *lib.create("root", t.tech->plane_count()).value();
    db::CellDef& child = *lib.create("child", t.tech->plane_count()).value();
    std::vector<std::pair<Rect, hefesto::tech::TileType>> root_rects;  // in root coordinates
    const auto add = [&](db::CellDef& def, const db::Transform& tr, bool record) {
      for (int i = 0; i < 6; ++i) {
        const int x = coord(0, 30);
        const int y = coord(0, 30);
        const Rect r{x, y, x + coord(1, 12), y + coord(1, 12)};
        const auto type = coord(0, 1) == 0 ? t.metal1 : t.ndiff;
        view3d::test::paint(def, t, r, type);
        if (record) {
          root_rects.emplace_back(db::apply(tr, r), type);
        }
      }
    };
    add(root, db::kIdentity, true);
    const auto& m = kOrientations[static_cast<std::size_t>(coord(0, 7))];
    const db::Transform tr{m[0], m[1], coord(0, 40), m[2], m[3], coord(0, 40)};
    add(child, tr, true);
    view3d::test::place(root, child, tr, "u");
    const int cx = coord(-5, 20);
    const int cy = coord(-5, 20);
    const Rect clip{cx, cy, cx + coord(5, 45), cy + coord(5, 45)};
    const auto mesh = view3d::test::build(stack, root, clip);
    REQUIRE(mesh.has_value());
    for (view3d::LayerId l = 0; l < 2; ++l) {
      const auto type = l == 0 ? t.metal1 : t.ndiff;
      double expected = 0.0;  // unit squares of the clip covered by the type
      for (int y = clip.y0; y < clip.y1; ++y) {
        for (int x = clip.x0; x < clip.x1; ++x) {
          const bool covered = std::any_of(root_rects.begin(), root_rects.end(), [&](const auto& rt) {
            return rt.second == type && rt.first.x0 <= x && x < rt.first.x1 && rt.first.y0 <= y && y < rt.first.y1;
          });
          expected += covered ? 1.0 : 0.0;
        }
      }
      const auto thickness = static_cast<double>(stack.layers()[l].thickness);
      CHECK(view3d::test::face_area(*mesh, l) * thickness == expected * thickness);
      CHECK(view3d::test::open_edge(*mesh, l).empty());
    }
  }
}
