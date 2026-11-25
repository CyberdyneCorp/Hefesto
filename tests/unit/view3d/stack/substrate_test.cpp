// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The board's substrate slab (view3d/stack/substrate.hpp; Q-MISC-84/85,
// openspec/changes/m10-3d-view tasks.md 16.11): its x/y extent, its top at
// the lowest drawn layer's bottom and its thickness. Owner: SECTION.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

#include "../mesh/mesh_fixture.hpp"
#include "hefesto/view3d/stack/substrate.hpp"

namespace db = hefesto::db;
namespace view3d = hefesto::view3d;
using Catch::Matchers::WithinAbs;
using db::Rect;

namespace {

view3d::LayerStack stack_of(const view3d::test::MeshTech& t, bool magic_compat, float poly_h = 0.3F) {
  return view3d::LayerStack{view3d::Mode::layout_layer,
                            {view3d::test::layer("ndiff", t.ndiff, 0.0F, 0.2F),
                             view3d::test::layer("poly", t.poly, poly_h, 0.2F),
                             view3d::test::layer("metal1", t.metal1, 1.0F, 0.5F)},
                            magic_compat,
                            0.01};
}

}  // namespace

TEST_CASE("substrate slab spans the mesh region under the lowest drawn layer, the same in both modes",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const auto t = view3d::test::build_tech();
  for (const bool magic_compat : {true, false}) {
    INFO("magic-compat " << (magic_compat ? "ON" : "OFF"));
    const auto stack = stack_of(t, magic_compat);
    db::CellDef cell("c", t.tech->plane_count());
    view3d::test::paint(cell, t, Rect{0, 0, 30, 60}, t.ndiff);
    view3d::test::paint(cell, t, Rect{10, 20, 80, 40}, t.metal1);
    const auto mesh = view3d::test::build(stack, cell, Rect{-1, -1, 81, 61}, false);
    REQUIRE(mesh.has_value());
    const auto slab = view3d::substrate_slab(*mesh, stack);
    REQUIRE(slab.has_value());
    CHECK(slab->area == mesh->region);
    // The drawn stack runs from ndiff's bottom (0) to metal1's top (1.5).
    CHECK_THAT(static_cast<double>(slab->thickness), WithinAbs(static_cast<double>(view3d::kSubstrateFraction * 1.5F), 1e-6));
    CHECK_THAT(static_cast<double>(slab->height + slab->thickness), WithinAbs(0.0, 1e-6));
    CHECK(slab->color == view3d::kSubstrateColor);
  }
}

TEST_CASE("substrate slab follows the layers the mesh draws, not every layer of the stack",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  // Poly alone (h 0.3, t 0.2): the slab's top is poly's bottom; ndiff,
  // lower in the stack but not in the mesh, does not move it.
  const auto t = view3d::test::build_tech();
  const auto stack = stack_of(t, true);
  db::CellDef cell("c", t.tech->plane_count());
  view3d::test::paint(cell, t, Rect{0, 0, 10, 10}, t.poly);
  const auto mesh = view3d::test::build(stack, cell, Rect{0, 0, 10, 10}, true);
  REQUIRE(mesh.has_value());
  const auto slab = view3d::substrate_slab(*mesh, stack);
  REQUIRE(slab.has_value());
  CHECK_THAT(static_cast<double>(slab->thickness), WithinAbs(static_cast<double>(view3d::kSubstrateFraction * 0.2F), 1e-6));
  CHECK_THAT(static_cast<double>(slab->height + slab->thickness), WithinAbs(0.3, 1e-6));
}

TEST_CASE("substrate slab takes the model's layer heights and a split tile's own height",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const auto t = view3d::test::build_tech();
  const auto stack = stack_of(t, true);
  view3d::MeshModel mesh;
  mesh.region = Rect{0, 0, 100, 50};
  mesh.layer_heights = {{0.0F, 0.2F}, {0.3F, 0.2F}, {2.0F, 1.0F}};  // metal1 drawn at 2..3 (e.g. spanned)
  view3d::Chunk c;
  c.area = mesh.region;
  c.rects = {view3d::RectInstance{0, 0, 10, 10, 2}};
  view3d::Triangle tri;
  tri.x[1] = 10.0F;
  tri.y[2] = 10.0F;
  tri.layer = 1;
  tri.height = -1.0F;  // its own (left type's) h and t
  tri.thickness = 0.5F;
  c.triangles = {tri};
  mesh.chunks = {c};
  const auto slab = view3d::substrate_slab(mesh, stack);
  REQUIRE(slab.has_value());
  CHECK_THAT(static_cast<double>(slab->thickness), WithinAbs(static_cast<double>(view3d::kSubstrateFraction * 4.0F), 1e-6));  // -1 .. 3
  CHECK_THAT(static_cast<double>(slab->height + slab->thickness), WithinAbs(-1.0, 1e-6));
  CHECK(slab->area == mesh.region);
}

TEST_CASE("no substrate slab for an empty mesh or a flat stack",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const auto t = view3d::test::build_tech();
  const auto stack = stack_of(t, true);
  CHECK_FALSE(view3d::substrate_slab(view3d::MeshModel{}, stack).has_value());
  const view3d::LayerStack flat{view3d::Mode::layout_layer,
                                {view3d::test::layer("ndiff", t.ndiff, 0.0F, 0.0F)}, true, 0.01};
  db::CellDef cell("c", t.tech->plane_count());
  view3d::test::paint(cell, t, Rect{0, 0, 10, 10}, t.ndiff);
  const auto mesh = view3d::test::build(flat, cell, Rect{0, 0, 10, 10}, true);
  REQUIRE(mesh.has_value());
  REQUIRE_FALSE(mesh->empty());
  CHECK_FALSE(view3d::substrate_slab(*mesh, flat).has_value());
}
