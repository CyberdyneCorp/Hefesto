// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pick resolution and the 3D -> 2D link (M10 task 9.3; openspec/changes/
// m10-3d-view/design.md D10): a pick id gives its layer, tile type (a
// (type, style) pair mapped back, Q-MISC-77), source tile and instance
// path; selecting it selects the edit cell's material, or the subcell use
// holding it.

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "hefesto/db/transform.hpp"
#include "hefesto/select/selection.hpp"
#include "pick_fixture.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace view3d = hefesto::view3d;
using db::Rect;
using view3d::test::layer_of;
using view3d::test::rect_id;
using view3d::test::Session;

namespace {

/// The rectangles of `type` on `plane` of `def` (any split tile excluded).
std::vector<Rect> rects_of(const db::CellDef& def, tech::PlaneId plane, tech::TileType type) {
  std::vector<Rect> out;
  def.plane(plane).enumerate(db::whole_plane(), tech::TypeMask::of(type), [&](const db::TileView& t) {
    out.push_back(t.area);
    return true;
  });
  return out;
}

tech::PlaneId home(const Session& s, tech::TileType type) { return *s.t.tech->home_plane(type); }

}  // namespace

TEST_CASE("a pick in the edit cell resolves to its type and source tile",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, Rect{0, 0, 10, 4}, s.t.metal1);
  s.paint(top, Rect{2, -6, 6, 10}, s.t.poly);
  const view3d::MeshModel mesh = s.mesh(top);
  const auto id = rect_id(mesh, layer_of(s.t, s.t.metal1), Rect{0, 0, 10, 4});
  REQUIRE(id.has_value());

  const auto bare = view3d::resolve(mesh, *id);
  REQUIRE(bare.has_value());
  CHECK(bare->layer == layer_of(s.t, s.t.metal1));
  CHECK(bare->root_rect == Rect{0, 0, 10, 4});
  CHECK_FALSE(bare->type.has_value());

  const auto target = view3d::resolve(mesh, s.context(top), *id);
  REQUIRE(target.has_value());
  CHECK(target->type == s.t.metal1);
  CHECK(target->use_path.empty());
  CHECK(target->cell_rect == Rect{0, 0, 10, 4});
  CHECK(target->material == Rect{0, 0, 10, 4});
  CHECK(target->cell == &top);
  CHECK_FALSE(target->triangle);
  CHECK_FALSE(view3d::resolve(mesh, s.context(top), view3d::kNoPick).has_value());
}

TEST_CASE("a pick under a subcell resolves to the instance path and the cell-local tile",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Session s;
  db::CellDef& leaf = s.cell("leaf");
  s.paint(leaf, Rect{0, 0, 4, 10}, s.t.poly);
  db::CellDef& top = s.cell("top");
  s.paint(top, Rect{0, 0, 10, 10}, s.t.metal1);
  s.place(top, leaf, "u1", 100, 0);
  const view3d::MeshModel mesh = s.mesh(top);
  const auto id = rect_id(mesh, layer_of(s.t, s.t.poly), Rect{100, 0, 104, 10});
  REQUIRE(id.has_value());
  const auto target = view3d::resolve(mesh, s.context(top), *id);
  REQUIRE(target.has_value());
  CHECK(target->type == s.t.poly);
  CHECK(target->use_path == std::vector<std::string>{"u1"});
  CHECK(target->cell_rect == Rect{0, 0, 4, 10});
  CHECK(target->material == Rect{100, 0, 104, 10});
}

TEST_CASE("a side face resolves to the tile it bounds",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, Rect{0, 0, 10, 4}, s.t.metal1);
  const view3d::MeshModel mesh = s.mesh(top);
  REQUIRE(mesh.chunks.size() == 1);
  const view3d::Chunk& chunk = mesh.chunks[0];
  REQUIRE(chunk.sides.size() == 4);
  for (std::size_t i = 0; i < chunk.sides.size(); ++i) {
    const auto target = view3d::resolve(mesh, s.context(top), view3d::encode_pick(mesh, {0, view3d::PickKind::side, i}));
    REQUIRE(target.has_value());
    CHECK(target->type == s.t.metal1);
    CHECK(target->material == Rect{0, 0, 10, 4});
    CHECK((target->root_rect.x0 == target->root_rect.x1 || target->root_rect.y0 == target->root_rect.y1));
  }
}

TEST_CASE("a type drawn in two styles maps both of its layers back to the type (Q-MISC-77)",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Session s;
  std::vector<view3d::StackLayer> layers(s.stack.layers().begin(), s.stack.layers().end());
  view3d::StackLayer second = layers[layer_of(s.t, s.t.metal1)];
  second.style = 7;
  layers.push_back(second);
  s.stack = view3d::LayerStack{view3d::Mode::layout_layer, layers, true, 0.01};
  db::CellDef& top = s.cell("top");
  s.paint(top, Rect{0, 0, 10, 4}, s.t.metal1);
  const view3d::MeshModel mesh = s.mesh(top);
  const auto first = rect_id(mesh, layer_of(s.t, s.t.metal1), Rect{0, 0, 10, 4});
  const auto other = rect_id(mesh, static_cast<view3d::LayerId>(layers.size() - 1), Rect{0, 0, 10, 4});
  REQUIRE(first.has_value());
  REQUIRE(other.has_value());
  CHECK(view3d::resolve(mesh, s.context(top), *first)->type == s.t.metal1);
  CHECK(view3d::resolve(mesh, s.context(top), *other)->type == s.t.metal1);
}

TEST_CASE("selecting a picked edit-cell tile selects its material in 2D",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.editor.edit_cell = &top;
  s.paint(top, Rect{0, 0, 10, 4}, s.t.metal1);
  s.paint(top, Rect{20, 0, 30, 4}, s.t.metal1);
  s.paint(top, Rect{2, -6, 6, 10}, s.t.poly);
  const view3d::MeshModel mesh = s.mesh(top);
  const auto id = rect_id(mesh, layer_of(s.t, s.t.metal1), Rect{0, 0, 10, 4});
  REQUIRE(id.has_value());
  const auto target = view3d::resolve(mesh, s.context(top), *id);
  REQUIRE(target.has_value());

  const auto link = view3d::link_to_2d(s.editor, *target);
  REQUIRE(link.has_value());
  CHECK(link->root == &top);
  CHECK(link->area == Rect{0, 0, 10, 4});
  CHECK_FALSE(link->use);
  const hefesto::select::Selection& sel = s.editor.selection;
  CHECK(sel.root() == &top);
  CHECK(rects_of(*sel.def(), home(s, s.t.metal1), s.t.metal1) == std::vector<Rect>{{0, 0, 10, 4}});
  CHECK(rects_of(*sel.def(), home(s, s.t.poly), s.t.poly).empty());
  CHECK(sel.def()->uses().empty());
  CHECK(sel.area_types == tech::TypeMask::of(s.t.metal1));
  REQUIRE(sel.provenance().size() == 1);
  CHECK(sel.provenance()[0].source == &top);
  CHECK(sel.provenance()[0].use_path.empty());

  // to_selection replaces the selection, as a new `select area` does.
  const auto second = rect_id(mesh, layer_of(s.t, s.t.metal1), Rect{20, 0, 30, 4});
  REQUIRE(second.has_value());
  CHECK(view3d::to_selection(s.editor, *view3d::resolve(mesh, s.context(top), *second)).ok());
  CHECK(rects_of(*sel.def(), home(s, s.t.metal1), s.t.metal1) == std::vector<Rect>{{20, 0, 30, 4}});
}

TEST_CASE("selecting a picked tile under a subcell selects the subcell use",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Session s;
  db::CellDef& leaf = s.cell("leaf");
  s.paint(leaf, Rect{0, 0, 4, 10}, s.t.poly);
  db::CellDef& top = s.cell("top");
  s.editor.edit_cell = &top;
  s.paint(top, Rect{0, 0, 10, 10}, s.t.metal1);
  const db::CellUse& use = s.place(top, leaf, "u1", 100, 0);
  const view3d::MeshModel mesh = s.mesh(top);
  const auto id = rect_id(mesh, layer_of(s.t, s.t.poly), Rect{100, 0, 104, 10});
  REQUIRE(id.has_value());
  const auto target = view3d::resolve(mesh, s.context(top), *id);
  REQUIRE(target.has_value());

  const auto link = view3d::link_to_2d(s.editor, *target);
  REQUIRE(link.has_value());
  CHECK(link->use);
  CHECK(link->area == use.bbox);
  const hefesto::select::Selection& sel = s.editor.selection;
  CHECK(sel.root() == &top);
  REQUIRE(sel.def()->uses().size() == 1);
  const db::CellUse& selected = *sel.def()->uses().front();
  CHECK(selected.def == &leaf);
  CHECK(selected.transform == db::translation(100, 0));
  CHECK(rects_of(*sel.def(), home(s, s.t.poly), s.t.poly).empty());
  CHECK(rects_of(*sel.def(), home(s, s.t.metal1), s.t.metal1).empty());
}

TEST_CASE("a target whose tile is gone, or whose cell was deleted, selects nothing",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, Rect{0, 0, 10, 4}, s.t.metal1);
  const view3d::MeshModel mesh = s.mesh(top);
  const auto id = rect_id(mesh, layer_of(s.t, s.t.metal1), Rect{0, 0, 10, 4});
  REQUIRE(id.has_value());
  REQUIRE(db::erase_type(top, *s.t.tech, Rect{0, 0, 10, 4}, s.t.metal1).ok());
  const auto stale = view3d::resolve(mesh, s.context(top), *id);
  REQUIRE(stale.has_value());
  CHECK(stale->material.is_null());
  CHECK_FALSE(view3d::link_to_2d(s.editor, *stale).has_value());
  CHECK(s.editor.selection.root() == nullptr);

  view3d::PickTarget foreign = *stale;
  foreign.material = Rect{0, 0, 10, 4};
  db::CellDef orphan("orphan", s.t.tech->plane_count());
  foreign.cell = &orphan;
  CHECK_FALSE(view3d::link_to_2d(s.editor, foreign).has_value());
  CHECK(view3d::to_selection(s.editor, foreign).ok());
  CHECK(s.editor.selection.root() == nullptr);
}
