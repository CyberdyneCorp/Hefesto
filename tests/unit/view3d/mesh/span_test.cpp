// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The J1 view option "Span connected layers" (M10 task 6.5; openspec/
// changes/m10-3d-view/design.md D10 "Contacts and vias"): by default a
// contact keeps its technology h and t; with the option it spans from the
// top of its lowest residue to the bottom of its highest.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include "mesh_fixture.hpp"
#include "view3d/mesh/internal.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace view3d = hefesto::view3d;
namespace detail = hefesto::view3d::detail;
using db::Rect;

namespace {

struct ViaTech {
  std::shared_ptr<const tech::Technology> tech;
  tech::TileType m1 = 0;
  tech::TileType m2 = 0;
  tech::TileType via = 0;
};

ViaTech build_via_tech() {
  tech::TechnologyBuilder b;
  b.set_name("span");
  REQUIRE(b.add_plane("metal1").ok());
  REQUIRE(b.add_plane("metal2").ok());
  ViaTech t;
  t.m1 = b.add_type("metal1", "m1").value();
  t.m2 = b.add_type("metal2", "m2").value();
  t.via = b.add_type("metal1", "via").value();
  const std::array<std::string_view, 2> residues{"m1", "m2"};
  REQUIRE(b.add_contact("via", residues).ok());
  REQUIRE(b.add_connect("m1,via", "m1,via").ok());
  REQUIRE(b.add_connect("m2,via", "m2,via").ok());
  auto fin = b.finalize();
  REQUIRE(fin.ok());
  t.tech = fin.value();
  return t;
}

// m1 from 1.0 to 1.5, m2 from m2_h to m2_h + 0.5, via (tech) 1.2 to 1.5.
view3d::LayerStack via_stack(const ViaTech& t, float m2_h) {
  std::vector<view3d::DrawHeight> heights(tech::kMaxTileTypes);
  heights[t.m1] = view3d::DrawHeight{1.0F, 0.5F};
  heights[t.m2] = view3d::DrawHeight{m2_h, 0.5F};
  heights[t.via] = view3d::DrawHeight{1.2F, 0.3F};
  return view3d::LayerStack{{view3d::test::layer("m1", t.m1, 1.0F, 0.5F), view3d::test::layer("m2", t.m2, m2_h, 0.5F),
                             view3d::test::layer("via", t.via, 1.2F, 0.3F)},
                            std::move(heights), true, 0.01};
}

std::optional<view3d::MeshModel> build_via(const ViaTech& t, const view3d::LayerStack& stack, bool span,
                                           bool with_tech = true) {
  db::CellDef root("root", t.tech->plane_count());
  REQUIRE(db::paint(root, *t.tech, Rect{0, 0, 10, 10}, t.m1).ok());
  REQUIRE(db::paint(root, *t.tech, Rect{0, 0, 10, 10}, t.m2).ok());
  REQUIRE(db::paint(root, *t.tech, Rect{2, 2, 6, 6}, t.via).ok());
  db::recompute_bbox(root);
  view3d::BuildContext ctx;
  ctx.span_connected_layers = span;
  ctx.technology = with_tech ? t.tech.get() : nullptr;
  return view3d::build_from_layout(stack, root, view3d::ClipArea{Rect{-1, -1, 11, 11}, false, 0}, ctx);
}

}  // namespace

TEST_CASE("by default a via keeps its technology height and thickness",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const ViaTech t = build_via_tech();
  const auto stack = via_stack(t, 3.0F);
  const auto mesh = build_via(t, stack, false);
  REQUIRE(mesh.has_value());
  REQUIRE(mesh->layer_heights.size() == 3);
  CHECK(mesh->layer_heights[0] == view3d::DrawHeight{1.0F, 0.5F});
  CHECK(mesh->layer_heights[1] == view3d::DrawHeight{3.0F, 0.5F});
  CHECK(mesh->layer_heights[2] == view3d::DrawHeight{1.2F, 0.3F});
  CHECK(view3d::test::face_area(*mesh, 2) == 16.0);
  // The option without a technology has no residues to span.
  const auto no_tech = build_via(t, stack, true, false);
  REQUIRE(no_tech.has_value());
  CHECK(no_tech->layer_heights[2] == view3d::DrawHeight{1.2F, 0.3F});
}

TEST_CASE("with the option a via spans from the lower layer's top to the upper layer's bottom",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const ViaTech t = build_via_tech();
  const auto stack = via_stack(t, 3.0F);
  const auto mesh = build_via(t, stack, true);
  REQUIRE(mesh.has_value());
  // From m1's top (1.0 + 0.5) to m2's bottom (3.0).
  CHECK(mesh->layer_heights[2] == view3d::DrawHeight{1.5F, 1.5F});
  CHECK(mesh->layer_heights[0] == view3d::DrawHeight{1.0F, 0.5F});
  CHECK(mesh->layer_heights[1] == view3d::DrawHeight{3.0F, 0.5F});
  // Same footprint, a closed prism.
  CHECK(view3d::test::face_area(*mesh, 2) == 16.0);
  CHECK(view3d::test::side_length(*mesh, 2) == 16.0);
  CHECK(view3d::test::open_edge(*mesh, 2).empty());
}

TEST_CASE("a contact whose residues leave no gap, or a type that is no contact, is not spanned",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  const ViaTech t = build_via_tech();
  // m2 starts below m1's top: nothing to span.
  const auto overlapping = via_stack(t, 1.2F);
  CHECK_FALSE(detail::spanned_height(overlapping, *t.tech, t.via).has_value());
  const auto mesh = build_via(t, overlapping, true);
  REQUIRE(mesh.has_value());
  CHECK(mesh->layer_heights[2] == view3d::DrawHeight{1.2F, 0.3F});
  const auto normal = via_stack(t, 3.0F);
  CHECK_FALSE(detail::spanned_height(normal, *t.tech, t.m1).has_value());
  CHECK(detail::spanned_height(normal, *t.tech, t.via) == view3d::DrawHeight{1.5F, 1.5F});
}
