// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The 2D -> 3D highlight and net highlighting (M10 tasks 9.4, 9.5;
// openspec/changes/m10-3d-view/design.md D10): the instances over the
// selection's paint, and the instances of a net found through
// connect::search_flat or search_hierarchical, on an inverter.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <vector>

#include "hefesto/connect/search.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/select/selection.hpp"
#include "pick_fixture.hpp"

namespace connect = hefesto::connect;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace view3d = hefesto::view3d;
using db::Rect;
using view3d::test::layer_of;
using view3d::test::rect_id;
using view3d::test::Session;

namespace {

/// Selects the paint of `type` under `area` of `root` directly.
void select_paint(Session& s, db::CellDef& root, const Rect& area, tech::TileType type) {
  hefesto::select::Selection& sel = s.editor.selection;
  sel.set_root(&root, s.t.tech->plane_count());
  REQUIRE(db::copy_paint(root, area, db::kIdentity, *sel.def(), *s.t.tech, tech::TypeMask::of(type)).ok());
}

/// The ids of the instances of `layer` whose face lies within `area`
/// (rectangles inside it, sides on or inside its outline).
std::vector<view3d::PickId> ids_within(const view3d::MeshModel& mesh, view3d::LayerId layer, const Rect& area) {
  std::vector<view3d::PickId> out;
  for (const view3d::PickId id : view3d::test::all_ids(mesh)) {
    const auto t = view3d::resolve(mesh, id);
    const Rect& r = t->root_rect;
    if (t->layer == layer && r.x0 >= area.x0 && r.x1 <= area.x1 && r.y0 >= area.y0 && r.y1 <= area.y1) {
      out.push_back(id);
    }
  }
  return out;
}

/// The area of the rectangle instances in `ids`, per layer.
std::map<view3d::LayerId, std::int64_t> rect_area(const view3d::MeshModel& mesh, const view3d::HighlightSet& set) {
  std::map<view3d::LayerId, std::int64_t> out;
  for (const view3d::PickId id : set.ids) {
    const auto ref = view3d::decode_pick(mesh, id);
    if (ref->kind == view3d::PickKind::rect) {
      const auto t = view3d::resolve(mesh, id);
      out[t->layer] += t->root_rect.width() * t->root_rect.height();
    }
  }
  return out;
}

/// The net's tiles on their types' home planes, as area per stack layer.
struct NetArea final : connect::NetVisitor {
  const Session* s;
  std::map<view3d::LayerId, std::int64_t> area;

  explicit NetArea(const Session& session) : s(&session) {}

  bool on_tile(const connect::NetTile& t) override {
    if (s->t.tech->home_plane(t.tile.type) == t.plane) {
      area[layer_of(s->t, t.tile.type)] += t.tile.area.width() * t.tile.area.height();
    }
    return true;
  }
};

/// The inverter: ndiff and pdiff crossed by poly; GND, VDD and OUT metal1
/// with the source and drain contacts.
void draw_inverter(Session& s, db::CellDef& cell) {
  s.paint(cell, Rect{0, 0, 30, 10}, s.t.ndiff);
  s.paint(cell, Rect{0, 40, 30, 50}, s.t.pdiff);
  s.paint(cell, Rect{12, -5, 18, 55}, s.t.poly);
  s.paint(cell, Rect{0, -10, 10, 10}, s.t.metal1);  // GND
  s.paint(cell, Rect{0, 40, 10, 60}, s.t.metal1);   // VDD
  s.paint(cell, Rect{20, 0, 30, 50}, s.t.metal1);   // OUT
  s.paint(cell, Rect{2, 2, 8, 8}, s.t.ndc);
  s.paint(cell, Rect{22, 2, 28, 8}, s.t.ndc);
  s.paint(cell, Rect{2, 42, 8, 48}, s.t.pdc);
  s.paint(cell, Rect{22, 42, 28, 48}, s.t.pdc);
}

}  // namespace

TEST_CASE("selecting a rectangle in 2D highlights exactly the overlapping instances",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, Rect{0, 0, 10, 10}, s.t.metal1);
  s.paint(top, Rect{20, 0, 30, 10}, s.t.metal1);
  s.paint(top, Rect{0, 0, 10, 10}, s.t.poly);
  const view3d::MeshModel mesh = s.mesh(top);
  const view3d::LayerId m1 = layer_of(s.t, s.t.metal1);

  SECTION("the whole rectangle: its top face and its four sides") {
    select_paint(s, top, Rect{0, 0, 10, 10}, s.t.metal1);
    const auto set = view3d::from_selection(mesh, s.stack, s.editor.selection);
    const auto expected = ids_within(mesh, m1, Rect{0, 0, 10, 10});
    CHECK(expected.size() == 5);
    CHECK(set.ids == expected);
    // Without the stack every selected type matches every layer: poly too.
    const auto any = view3d::from_selection(mesh, s.editor.selection);
    CHECK(any.ids.size() == 10);
  }
  SECTION("half the rectangle: the face and the three sides the paint reaches") {
    select_paint(s, top, Rect{5, 0, 15, 10}, s.t.metal1);
    const auto set = view3d::from_selection(mesh, s.stack, s.editor.selection);
    std::vector<view3d::PickId> expected;
    for (const view3d::PickId id : ids_within(mesh, m1, Rect{0, 0, 10, 10})) {
      const auto t = view3d::resolve(mesh, id);
      if (t->root_rect.x1 > 5) {  // not the left side at x = 0
        expected.push_back(id);
      }
    }
    CHECK(expected.size() == 4);
    CHECK(set.ids == expected);
  }
  SECTION("nothing selected") {
    CHECK(view3d::from_selection(mesh, s.stack, s.editor.selection).empty());
  }
}

TEST_CASE("net highlighting on an inverter equals the net's tiles from connect::search_flat",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s;
    db::CellDef& inv = s.cell("inv");
    draw_inverter(s, inv);
    const view3d::MeshModel mesh = s.mesh(inv);
    const auto out_id = view3d::test::rect_id_at(mesh, layer_of(s.t, s.t.metal1), 25, 25);
    REQUIRE(out_id.has_value());
    const auto seed = view3d::resolve(mesh, s.context(inv), *out_id);
    REQUIRE(seed.has_value());

    view3d::NetOptions flat;
    flat.scope = view3d::NetScope::flat;
    flat.magic_compat = compat;
    flat.stack = &s.stack;
    const auto set = view3d::net_highlight(mesh, s.editor, *seed, flat);

    NetArea net(s);
    connect::SearchStart start;
    start.area = seed->material;
    start.types = tech::TypeMask::of(s.t.metal1);
    connect::SearchOptions o;
    o.magic_compat = compat;
    REQUIRE(connect::search_flat(inv, *s.t.tech, start, o, net).ok());
    CHECK(rect_area(mesh, set) == net.area);
    CHECK(net.area.count(layer_of(s.t, s.t.pdc)) == 1);
    if (!compat) {
      CHECK(net.area.count(layer_of(s.t, s.t.ndiff)) == 1);  // the full search reaches the drains
    }

    // The rails are another net, and the hierarchical search agrees here.
    const auto gnd = view3d::test::rect_id_at(mesh, layer_of(s.t, s.t.metal1), 5, -5);
    REQUIRE(gnd.has_value());
    CHECK_FALSE(std::binary_search(set.ids.begin(), set.ids.end(), *gnd));
    view3d::NetOptions tree = flat;
    tree.scope = view3d::NetScope::hierarchical;
    CHECK(view3d::net_highlight(mesh, s.editor, *seed, tree).ids == set.ids);

    // "Isolate selected net" dims or hides every other instance.
    const auto others = view3d::isolate_others(mesh, set);
    CHECK(others.ids.size() + set.ids.size() == view3d::test::all_ids(mesh).size());
    CHECK(std::binary_search(others.ids.begin(), others.ids.end(), *gnd));
  }
}

TEST_CASE("a net seeded in a subcell is found by the hierarchical search only",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Session s;
  db::CellDef& inv = s.cell("inv");
  draw_inverter(s, inv);
  db::CellDef& top = s.cell("top");
  s.place(top, inv, "x1", 100, 0);
  s.paint(top, Rect{125, 50, 135, 70}, s.t.metal1);  // continues OUT in the parent
  const view3d::MeshModel mesh = s.mesh(top);
  const auto out_id = view3d::test::rect_id_at(mesh, layer_of(s.t, s.t.metal1), 125, 25);
  REQUIRE(out_id.has_value());
  const auto seed = view3d::resolve(mesh, s.context(top), *out_id);
  REQUIRE(seed.has_value());
  REQUIRE(seed->use_path == std::vector<std::string>{"x1"});

  view3d::NetOptions flat;
  flat.scope = view3d::NetScope::flat;
  flat.stack = &s.stack;
  CHECK(view3d::net_highlight(mesh, s.editor, *seed, flat).empty());

  const auto set = view3d::net_highlight(mesh, s.editor, *seed, view3d::NetOptions{view3d::NetScope::hierarchical, true, &s.stack});
  const auto parent_wire = view3d::test::rect_id_at(mesh, layer_of(s.t, s.t.metal1), 130, 65);
  REQUIRE(parent_wire.has_value());
  CHECK(std::binary_search(set.ids.begin(), set.ids.end(), *parent_wire));
  const auto drain = view3d::test::rect_id_at(mesh, layer_of(s.t, s.t.ndc), 125, 5);
  REQUIRE(drain.has_value());
  CHECK(std::binary_search(set.ids.begin(), set.ids.end(), *drain));
  const auto source = view3d::test::rect_id_at(mesh, layer_of(s.t, s.t.ndc), 105, 5);
  REQUIRE(source.has_value());
  CHECK_FALSE(std::binary_search(set.ids.begin(), set.ids.end(), *source));
}
