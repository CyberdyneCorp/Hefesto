// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// build_from_planes: the mask-layer source's faces from generated planes
// (view-3d "Rendered geometry", mask-layer clause; design D6, D15; task
// 10.1). Owner: MASKSRC.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/mask/gen/context.hpp"
#include "hefesto/mask/gen/generate.hpp"
#include "hefesto/mask/plane/plane.hpp"
#include "view3d/mesh/internal.hpp"
#include "mesh_fixture.hpp"

namespace view3d = hefesto::view3d;
namespace mask = hefesto::mask;
namespace db = hefesto::db;
using view3d::test::Seg;

namespace {

constexpr std::uint8_t kLeft = view3d::side_flag::outward_left;
constexpr std::uint8_t kClip = view3d::side_flag::clip_edge;

// A mask layer as build_mask_stack makes it.
view3d::StackLayer mask_layer(const std::string& name, std::size_t index, float h, float t, bool render = true) {
  view3d::StackLayer l;
  l.name = name;
  l.mask_layer = index;
  l.style = render ? 72 : view3d::kPaleHighlightStyle;
  l.height = h;
  l.thickness = t;
  l.source = render ? view3d::ValueSource::render : view3d::ValueSource::none;
  return l;
}

// MET1 (render 1.376 0.36) and a layer without `render`, 0.01 µm per unit.
view3d::LayerStack two_layers(bool magic_compat) {
  return view3d::LayerStack{view3d::Mode::mask_layer,
                            {mask_layer("MET1", 0, 1.376F, 0.36F), mask_layer("NWELL", 1, 0.0F, 0.0F, false)},
                            magic_compat, 0.01};
}

struct Planes {
  mask::GenerationContext gen;
  mask::LayerPlanes planes{gen.pool, 2};

  Planes() {
    planes.planes[0] = &gen.pool.acquire();
    planes.planes[1] = &gen.pool.acquire();
  }
  void paint(std::size_t layer, const db::Rect& r) { REQUIRE(planes.planes[layer]->paint(r).ok()); }
};

std::optional<view3d::MeshModel> build(const view3d::LayerStack& stack, const mask::LayerPlanes& planes,
                                       const db::Rect& clip) {
  view3d::BuildContext ctx;
  return view3d::build_from_planes(stack, planes, view3d::ClipArea{clip, false, 0}, ctx);
}

}  // namespace

TEST_CASE("mask-layer prisms: each generated rectangle is a prism with side faces on its outline",
          "[cap:view-3d][req:Rendered geometry]") {
  Planes p;
  p.paint(0, db::Rect{0, 0, 100, 40});
  p.paint(1, db::Rect{-20, -20, 200, 80});
  const view3d::LayerStack stack = two_layers(true);
  const auto mesh = build(stack, p.planes, db::Rect{-50, -50, 300, 300});
  REQUIRE(mesh.has_value());
  CHECK(mesh->mode == view3d::Mode::mask_layer);
  CHECK(view3d::test::rects_of(*mesh, 0) == std::vector<Seg>{{0, 0, 100, 40, 0}});
  CHECK(view3d::test::sides_of(*mesh, 0) == std::vector<Seg>{{0, 0, 0, 40, kLeft},
                                                             {0, 0, 100, 0, 0},
                                                             {0, 40, 100, 40, kLeft},
                                                             {100, 0, 100, 40, 0}});
  // A layer without `render` is drawn flat in the pale-highlight style: a
  // top face and no side faces (W3Dmain.c:287), whatever overlaps it.
  CHECK(view3d::test::rects_of(*mesh, 1) == std::vector<Seg>{{-20, -20, 200, 80, 0}});
  CHECK(view3d::test::sides_of(*mesh, 1).empty());
  CHECK(stack.layers()[1].style == view3d::kPaleHighlightStyle);
  CHECK(mesh->layer_heights[1] == view3d::DrawHeight{});
}

TEST_CASE("mask-layer prisms take the render values as stored with magic-compat ON (Q-MISC-72)",
          "[cap:view-3d][req:Rendered geometry]") {
  Planes p;
  p.paint(0, db::Rect{0, 0, 100, 40});
  const auto mesh = build(two_layers(true), p.planes, db::Rect{-1, -1, 101, 41});
  REQUIRE(mesh.has_value());
  CHECK(mesh->layer_heights[0] == view3d::DrawHeight{1.376F, 0.36F});
}

TEST_CASE("mask-layer prisms take the render values in micrometres with magic-compat OFF (Q-MISC-72)",
          "[cap:view-3d][req:Rendered geometry]") {
  Planes p;
  p.paint(0, db::Rect{0, 0, 100, 40});
  const view3d::LayerStack stack = two_layers(false);
  const auto mesh = build(stack, p.planes, db::Rect{-1, -1, 101, 41});
  REQUIRE(mesh.has_value());
  const view3d::DrawHeight h = mesh->layer_heights[0];
  CHECK(h.height == static_cast<float>(static_cast<double>(1.376F) / 0.01));
  CHECK(h.thickness == static_cast<float>(static_cast<double>(0.36F) / 0.01));
  // The physical height does not depend on the setting.
  CHECK(stack.physical_height_um(0) == two_layers(true).physical_height_um(0));
  CHECK_FALSE(stack.physical_height_um(1).has_value());
}

TEST_CASE("a clipped mask-layer split tile is cut along the triangle in both modes",
          "[cap:view-3d][req:Rendered geometry]") {
  // The plane holds what CIFGen leaves after clipping to (0,0)-(5,10) the
  // upper-left triangle of (0,0)-(10,10): the split tile (0,0)-(5,5) and
  // the rectangle (0,5)-(5,10). Magic draws the clipped planes with no clip
  // rectangle (W3Dmain.c:441), so the face on x = 5 covers y 5..10 only,
  // not the split tile's bounding rectangle (Q-MISC-83 is the layout-layer
  // cut). Before round 5 magic-compat ON added the face y 0..5.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Planes p;
    REQUIRE(p.planes.planes[0]->paint_triangle(db::Rect{0, 0, 5, 5}, db::Diagonal::ne_sw, false).ok());
    p.paint(0, db::Rect{0, 5, 5, 10});
    const auto mesh = build(two_layers(compat), p.planes, db::Rect{0, 0, 5, 10});
    REQUIRE(mesh.has_value());
    std::vector<Seg> at_x5;
    for (const Seg& s : view3d::test::sides_of(*mesh, 0)) {
      if (s.x0 == 5.0 && s.x1 == 5.0) {
        at_x5.push_back(s);
      }
    }
    CHECK(at_x5 == std::vector<Seg>{{5, 5, 5, 10, kClip}});
  }
}

TEST_CASE("a mask layer without a plane draws nothing and a cancelled build gives no model",
          "[cap:view-3d][req:Rendered geometry]") {
  mask::GenerationContext gen;
  mask::LayerPlanes planes{gen.pool, 2};  // nothing generated
  const auto empty = build(two_layers(true), planes, db::Rect{0, 0, 10, 10});
  REQUIRE(empty.has_value());
  CHECK(empty->chunks.empty());
  CHECK(empty->layer_heights.size() == 2);
  Planes p;
  p.paint(0, db::Rect{0, 0, 100, 40});
  std::atomic<bool> cancel{true};
  view3d::BuildContext ctx;
  ctx.progress.cancel = &cancel;
  const view3d::LayerStack stack = two_layers(true);
  CHECK_FALSE(view3d::build_from_planes(stack, p.planes, view3d::ClipArea{db::Rect{0, 0, 10, 10}, false, 0}, ctx));
}

TEST_CASE("mask-unit conversions of the incremental rebuild", "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  // A layout change marks the mask area within the halo of it.
  const std::vector<db::Rect> dirty{db::Rect{10, 10, 20, 20}};
  CHECK(view3d::detail::mask_dirty(dirty, 10, 3) == std::vector<db::Rect>{db::Rect{70, 70, 230, 230}});
  // A chunk area in mask units is regenerated over the internal area that
  // covers it (rounded outward), within the generation area.
  const db::Rect limit{-100, -100, 100, 100};
  CHECK(view3d::detail::internal_area(db::Rect{-15, 5, 21, 30}, 10, limit) == db::Rect{-2, 0, 3, 3});
  CHECK(view3d::detail::internal_area(db::Rect{-2000, 5, 21, 3000}, 10, limit) == db::Rect{-100, 0, 3, 100});
}
