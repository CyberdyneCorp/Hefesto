// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Connectivity searches (capability layout-connectivity), on the `mt`
// technology of tests/golden/MODEL/CASES.md.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "../select/select_fixture.hpp"
#include "hefesto/connect/search.hpp"
#include "hefesto/connect/tree.hpp"
#include "hefesto/db/array_overlap.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/use_index.hpp"

namespace connect = hefesto::connect;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
using db::Rect;
using hefesto::select::test::MtTech;
using hefesto::select::test::paint;

namespace {

/// Collects a net in a scratch cell (one plane per technology plane).
struct Net final : connect::NetVisitor {
  db::CellDef cell;
  const tech::Technology* tech;
  std::vector<connect::NetLabel> labels;
  std::vector<std::string> paths;
  int tiles = 0;

  explicit Net(const tech::Technology& t) : cell("net", t.plane_count()), tech(&t) {}

  bool on_tile(const connect::NetTile& tile) override {
    ++tiles;
    paths.push_back(tile.use_path);
    REQUIRE(cell.plane(tile.plane).paint(tile.tile.area, db::paint_row(*tech, tile.plane, tile.tile.type)).ok());
    return true;
  }
  bool on_label(const connect::NetLabel& label) override {
    labels.push_back(label);
    return true;
  }
  [[nodiscard]] std::vector<Rect> rects(tech::PlaneId plane, tech::TileType type) const {
    return hefesto::select::test::rects_of(cell, plane, type);
  }
};

connect::SearchStart start_at(const MtTech& mt, db::Point p) {
  connect::SearchStart s;
  s.area = Rect{p.x, p.y, p.x + 1, p.y + 1};
  s.types = mt.tech->connects_to(mt.m1);
  s.types.set(mt.m1);
  return s;
}

connect::SearchOptions compat(bool on) {
  connect::SearchOptions o;
  o.magic_compat = on;
  return o;
}

}  // namespace

TEST_CASE("a stacked contact connects to both metals", "[cap:layout-connectivity][req:Connect table]") {
  HEFESTO_SELECT_MT(mt);
  const auto stacked = mt.tech->find_type("via1+via2");
  REQUIRE(stacked.ok());
  const tech::TypeMask& c = mt.tech->connects_to(*stacked.id);
  for (const tech::TileType t : {mt.via1, mt.via2, mt.m1, mt.m2, mt.m3}) {
    INFO(mt.tech->type_name(t));
    CHECK(c.test(t));
  }
  CHECK(c.test(*stacked.id));  // every type connects to itself
  CHECK_FALSE(mt.tech->connects_to(tech::kSpace).test(mt.m1));
}

TEST_CASE("corner touch does not connect", "[cap:layout-connectivity][req:Connectivity within one cell]") {
  HEFESTO_SELECT_MT(mt);
  db::CellDef def("top", mt.tech->plane_count());
  paint(def, mt, {0, 0, 10, 10}, mt.m1);
  paint(def, mt, {10, 10, 20, 20}, mt.m1);
  for (const bool on : {true, false}) {
    Net flat(*mt.tech);
    REQUIRE(connect::search_flat(def, *mt.tech, start_at(mt, {1, 1}), compat(on), flat).ok());
    CHECK(flat.rects(mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 10, 10}});
    Net tree(*mt.tech);
    REQUIRE(connect::search_hierarchical(def, *mt.tech, start_at(mt, {1, 1}), compat(on), tree).ok());
    CHECK(tree.rects(mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 10, 10}});
  }
}

TEST_CASE("a net continues through a via to the other plane",
          "[cap:layout-connectivity][req:Connectivity within one cell]") {
  HEFESTO_SELECT_MT(mt);
  db::CellDef def("top", mt.tech->plane_count());
  paint(def, mt, {0, 0, 10, 10}, mt.m1);
  paint(def, mt, {4, 4, 30, 6}, mt.m2);
  paint(def, mt, {4, 4, 6, 6}, mt.via1);
  paint(def, mt, {40, 40, 50, 50}, mt.m2);  // another net
  const auto check = [&](const Net& net) {
    CHECK(hefesto::select::test::area_of(net.cell, mt.metal1, mt.m1) == 100 - 4);
    CHECK(net.rects(mt.metal1, mt.via1) == std::vector<Rect>{{4, 4, 6, 6}});
    CHECK(net.rects(mt.metal2, mt.via1) == std::vector<Rect>{{4, 4, 6, 6}});
    CHECK(net.rects(mt.metal2, mt.m2) == std::vector<Rect>{{6, 4, 30, 6}});
  };
  for (const bool on : {true, false}) {
    INFO("magic-compat " << on);
    Net flat(*mt.tech);
    REQUIRE(connect::search_flat(def, *mt.tech, start_at(mt, {1, 1}), compat(on), flat).ok());
    check(flat);
    Net tree(*mt.tech);
    REQUIRE(connect::search_hierarchical(def, *mt.tech, start_at(mt, {1, 1}), compat(on), tree).ok());
    check(tree);
  }
}

TEST_CASE("the flat search starts at the first tile under the start area and honours its bounds",
          "[cap:layout-connectivity][req:Connectivity within one cell]") {
  HEFESTO_SELECT_MT(mt);
  db::CellDef def("top", mt.tech->plane_count());
  paint(def, mt, {0, 0, 100, 10}, mt.m1);
  SECTION("no tile of the start types: the net is empty") {
    Net net(*mt.tech);
    REQUIRE(connect::search_flat(def, *mt.tech, start_at(mt, {50, 50}), {}, net).ok());
    CHECK(net.tiles == 0);
  }
  SECTION("tiles outside the bounds are neither reported nor traversed") {
    paint(def, mt, {200, 0, 300, 10}, mt.m1);
    paint(def, mt, {100, 0, 200, 2}, mt.m1);  // a thin bridge
    connect::SearchStart s = start_at(mt, {1, 1});
    s.bounds = Rect{0, 0, 150, 10};
    Net net(*mt.tech);
    REQUIRE(connect::search_flat(def, *mt.tech, s, {}, net).ok());
    for (const Rect& r : net.rects(mt.metal1, mt.m1)) {
      CHECK(db::overlap(r, s.bounds));
    }
    CHECK(net.rects(mt.metal1, mt.m1).back().x1 <= 200);
  }
}

TEST_CASE("a triangle connects only across its legs and its hypotenuse",
          "[cap:layout-connectivity][req:Connectivity within one cell]") {
  HEFESTO_SELECT_MT(mt);
  db::CellDef def("top", mt.tech->plane_count());
  // A lower-left metal1 triangle in (0,0)-(10,10) (`\` diagonal, left side).
  REQUIRE(db::paint_triangle(def, *mt.tech, {0, 0, 10, 10}, db::Diagonal::nw_se, false, mt.m1).ok());
  paint(def, mt, {10, 10, 20, 20}, mt.m1);  // at the corner opposite the right angle
  paint(def, mt, {-10, 0, 0, 10}, mt.m1);   // along the left leg
  struct Pieces final : connect::NetVisitor {
    std::vector<Rect> rects;
    bool on_tile(const connect::NetTile& t) override {
      rects.push_back(t.tile.area);
      return true;
    }
  } pieces;
  REQUIRE(connect::search_flat(def, *mt.tech, start_at(mt, {1, 1}), {}, pieces).ok());
  const auto has = [&](const Rect& r) { return std::find(pieces.rects.begin(), pieces.rects.end(), r) != pieces.rects.end(); };
  CHECK(has(Rect{0, 0, 10, 10}));    // the triangle
  CHECK(has(Rect{-10, 0, 0, 10}));   // the square on its leg
  CHECK_FALSE(has(Rect{10, 10, 20, 20}));  // not the square at the far corner
}

TEST_CASE("flat search cross-plane step: magic-compat ON takes Magic's single tile",
          "[cap:layout-connectivity][req:Flat search cross-plane quirk]") {
  HEFESTO_SELECT_MT(mt);
  db::CellDef def("top", mt.tech->plane_count());
  paint(def, mt, {0, 0, 8, 8}, mt.m1);
  paint(def, mt, {5, 5, 8, 8}, mt.via1);
  paint(def, mt, {8, 8, 12, 12}, mt.m2);  // touches the via only at its corner
  Net on(*mt.tech);
  REQUIRE(connect::search_flat(def, *mt.tech, start_at(mt, {1, 1}), compat(true), on).ok());
  // Magic's DBSrPaintArea over the via grown by one unit meets the metal2
  // tile first (its strip lies above the via's), so the net continues there
  // and never visits the via's own image on metal2.
  CHECK(on.rects(mt.metal2, mt.m2) == std::vector<Rect>{{8, 8, 12, 12}});
  CHECK(on.rects(mt.metal2, mt.via1).empty());

  Net off(*mt.tech);
  REQUIRE(connect::search_flat(def, *mt.tech, start_at(mt, {1, 1}), compat(false), off).ok());
  CHECK(off.rects(mt.metal2, mt.m2).empty());
  CHECK(off.rects(mt.metal2, mt.via1) == std::vector<Rect>{{5, 5, 8, 8}});
  CHECK(off.rects(mt.metal1, mt.via1) == std::vector<Rect>{{5, 5, 8, 8}});
  CHECK(hefesto::select::test::area_of(off.cell, mt.metal1, mt.m1) == 64 - 9);
}

TEST_CASE("flat search modes agree on a DRC-clean layout",
          "[cap:layout-connectivity][req:Flat search cross-plane quirk]") {
  HEFESTO_SELECT_MT(mt);
  db::CellDef def("top", mt.tech->plane_count());
  paint(def, mt, {0, 0, 20, 10}, mt.m1);
  paint(def, mt, {10, 0, 40, 10}, mt.m2);
  paint(def, mt, {12, 2, 18, 8}, mt.via1);
  paint(def, mt, {30, 0, 40, 30}, mt.m2);
  paint(def, mt, {60, 0, 70, 10}, mt.m2);  // spaced away
  Net on(*mt.tech);
  Net off(*mt.tech);
  REQUIRE(connect::search_flat(def, *mt.tech, start_at(mt, {1, 1}), compat(true), on).ok());
  REQUIRE(connect::search_flat(def, *mt.tech, start_at(mt, {1, 1}), compat(false), off).ok());
  for (const tech::PlaneId p : {mt.metal1, mt.metal2}) {
    for (const tech::TileType t : {mt.m1, mt.m2, mt.via1}) {
      CHECK(on.rects(p, t) == off.rects(p, t));
    }
  }
  CHECK(hefesto::select::test::area_of(on.cell, mt.metal2, mt.m2) == 300 - 36 + 200);
}

TEST_CASE("hierarchical net through a subcell", "[cap:layout-connectivity][req:Hierarchical connectivity]") {
  HEFESTO_SELECT_MT(mt);
  db::CellLibrary lib;
  db::CellDef& parent = *lib.create("parent", mt.tech->plane_count()).value();
  db::CellDef& child = *lib.create("child", mt.tech->plane_count()).value();
  paint(parent, mt, {0, 0, 10, 2}, mt.m1);
  paint(child, mt, {0, 0, 10, 2}, mt.m1);
  db::PlaceOptions o;
  o.transform = db::translation(10, 0);
  REQUIRE(db::place(parent, child, o).ok());
  db::recompute_bbox(parent);
  Net net(*mt.tech);
  REQUIRE(connect::search_hierarchical(parent, *mt.tech, start_at(mt, {1, 1}), {}, net).ok());
  CHECK(net.rects(mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 20, 2}});
  CHECK(std::count(net.paths.begin(), net.paths.end(), std::string{"child_0"}) >= 1);

  SECTION("an unexpanded instance is not searched") {
    connect::SearchOptions opt;
    opt.descend = [](const db::CellUse&) { return false; };
    Net shallow(*mt.tech);
    REQUIRE(connect::search_hierarchical(parent, *mt.tech, start_at(mt, {1, 1}), opt, shallow).ok());
    CHECK(shallow.rects(mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 10, 2}});
  }
}

TEST_CASE("hierarchical net through equivalent ports", "[cap:layout-connectivity][req:Hierarchical connectivity]") {
  HEFESTO_SELECT_MT(mt);
  db::CellLibrary lib;
  db::CellDef& parent = *lib.create("parent", mt.tech->plane_count()).value();
  db::CellDef& pad = *lib.create("pad", mt.tech->plane_count()).value();
  paint(pad, mt, {0, 0, 5, 5}, mt.m1);
  paint(pad, mt, {50, 0, 55, 5}, mt.m1);
  for (const Rect& r : {Rect{0, 0, 5, 5}, Rect{50, 0, 55, 5}}) {
    db::Label l;
    l.text = "VDD";
    l.type = mt.m1;
    l.rect = r;
    l.port_directions = db::kPortNorth;
    l.port_index = 1;
    REQUIRE(db::put_label(pad, l).ok());
  }
  db::recompute_bbox(pad);
  REQUIRE(db::place(parent, pad, {}).ok());
  db::recompute_bbox(parent);
  connect::SearchOptions opt;
  opt.labels = connect::LabelPolicy::full;
  Net net(*mt.tech);
  REQUIRE(connect::search_hierarchical(parent, *mt.tech, start_at(mt, {1, 1}), opt, net).ok());
  CHECK(net.rects(mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 5, 5}, {50, 0, 55, 5}});
  REQUIRE(net.labels.size() == 2);
  CHECK(net.labels[0].text == "pad_0/VDD");  // subcell labels get their path

  SECTION("simple labels keep their own text") {
    opt.labels = connect::LabelPolicy::simple;
    Net simple(*mt.tech);
    REQUIRE(connect::search_hierarchical(parent, *mt.tech, start_at(mt, {1, 1}), opt, simple).ok());
    REQUIRE_FALSE(simple.labels.empty());
    CHECK(simple.labels[0].text == "VDD");
  }
  SECTION("without labels, ports do not join the net") {
    opt.labels = connect::LabelPolicy::none;
    Net bare(*mt.tech);
    REQUIRE(connect::search_hierarchical(parent, *mt.tech, start_at(mt, {1, 1}), opt, bare).ok());
    CHECK(bare.rects(mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 5, 5}});
  }
}

TEST_CASE("the hierarchical search ignores diagonal neighbours", "[cap:layout-connectivity][req:Hierarchical connectivity]") {
  HEFESTO_SELECT_MT(mt);
  db::CellDef def("top", mt.tech->plane_count());
  paint(def, mt, {0, 0, 10, 10}, mt.m1);
  paint(def, mt, {10, 10, 20, 20}, mt.m1);
  paint(def, mt, {0, 10, 5, 20}, mt.m1);  // abuts along y = 10 for 5 units
  Net net(*mt.tech);
  REQUIRE(connect::search_hierarchical(def, *mt.tech, start_at(mt, {1, 1}), {}, net).ok());
  CHECK(hefesto::select::test::area_of(net.cell, mt.metal1, mt.m1) == 150);
}

TEST_CASE("an edit changes the next search", "[cap:layout-connectivity][req:Connectivity is computed on demand]") {
  HEFESTO_SELECT_MT(mt);
  db::CellDef def("top", mt.tech->plane_count());
  paint(def, mt, {0, 0, 10, 10}, mt.m1);
  paint(def, mt, {20, 0, 30, 10}, mt.m1);
  paint(def, mt, {10, 0, 20, 10}, mt.m1);
  Net joined(*mt.tech);
  REQUIRE(connect::search_flat(def, *mt.tech, start_at(mt, {1, 1}), {}, joined).ok());
  CHECK(joined.rects(mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 30, 10}});
  REQUIRE(db::erase(def, *mt.tech, {10, 0, 20, 10}, tech::TypeMask::of(mt.m1)).ok());
  Net split(*mt.tech);
  REQUIRE(connect::search_flat(def, *mt.tech, start_at(mt, {1, 1}), {}, split).ok());
  CHECK(split.rects(mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 10, 10}});
}

TEST_CASE("tree searches visit descended cells in root coordinates", "[cap:layout-connectivity][req:Hierarchical connectivity]") {
  HEFESTO_SELECT_MT(mt);
  db::CellLibrary lib;
  db::CellDef& parent = *lib.create("parent", mt.tech->plane_count()).value();
  db::CellDef& child = *lib.create("child", mt.tech->plane_count()).value();
  paint(child, mt, {0, 0, 4, 4}, mt.m2);
  db::PlaceOptions o;
  o.id = "c";
  o.transform = db::translation(100, 0);
  o.array = db::ArrayInfo{0, 1, 0, 0, 10, 0};
  REQUIRE(db::place(parent, child, o).ok());
  db::recompute_bbox(parent);
  std::vector<std::pair<std::string, Rect>> seen;
  (void)connect::search_tree_tiles(parent, db::whole_plane(), tech::TypeMask::of(mt.m2), {}, true,
                                   [&](const connect::TreeTile& t) {
                                     seen.emplace_back(*t.path, db::apply(t.to_root, t.tile.area));
                                     return true;
                                   });
  REQUIRE(seen.size() == 2);
  CHECK(seen[0] == std::pair<std::string, Rect>{"c[0]", {100, 0, 104, 4}});
  CHECK(seen[1] == std::pair<std::string, Rect>{"c[1]", {110, 0, 114, 4}});
  int cells = 0;
  (void)connect::search_tree_cells(parent, db::whole_plane(), [](const db::CellUse&) { return false; }, true,
                                   [&](const connect::TreeCell& c) {
                                     ++cells;
                                     CHECK(c.use->id == "c");
                                     return true;
                                   });
  CHECK(cells == 1);
}

TEST_CASE("tree searches enumerate uses in the use index order of the session's compat",
          "[cap:layout-connectivity][req:Hierarchical connectivity]") {
  // Twelve uses binned by a partial-area search: magic-compat ON follows
  // the bin walk (DBCellSrArea over the BPlane), OFF the newest-first order.
  HEFESTO_SELECT_MT(mt);
  db::CellLibrary lib;
  db::CellDef& parent = *lib.create("parent", mt.tech->plane_count()).value();
  db::CellDef& child = *lib.create("child", mt.tech->plane_count()).value();
  paint(child, mt, {0, 0, 4, 4}, mt.m2);
  for (int i = 0; i < 12; ++i) {
    db::PlaceOptions o;
    o.id = "u" + std::to_string(i + 1);
    o.transform = db::translation(10 * i, 10 * (i % 3));
    REQUIRE(db::place(parent, child, o).ok());
  }
  db::recompute_bbox(parent);
  (void)db::uses_overlapping(parent, db::Rect{0, 0, 30, 30}, [](db::CellUse&) { return true; });
  REQUIRE(parent.use_index().state_for_test().binned);
  std::vector<std::string> newest_first;
  for (int i = 12; i >= 1; --i) {
    newest_first.push_back("u" + std::to_string(i));
  }
  for (const bool on : {true, false}) {
    std::vector<std::string> expected;
    for (const db::CellUse* u : db::uses_in_search_order(parent, hefesto::compat::CompatSettings{on, true})) {
      expected.push_back(u->id);
    }
    CHECK((expected == newest_first) == !on);
    std::vector<std::string> tiles;
    (void)connect::search_tree_tiles(parent, db::whole_plane(), tech::TypeMask::of(mt.m2), {}, on,
                                     [&](const connect::TreeTile& t) {
                                       tiles.push_back(*t.path);
                                       return true;
                                     });
    CHECK(tiles == expected);
    std::vector<std::string> cells;
    (void)connect::search_tree_cells(parent, db::whole_plane(), [](const db::CellUse&) { return false; }, on,
                                     [&](const connect::TreeCell& c) {
                                       cells.push_back(c.use->id);
                                       return true;
                                     });
    CHECK(cells == expected);
  }
  std::vector<std::string> off;
  (void)connect::search_tree_defs(parent, db::whole_plane(), {}, false,
                                  [&](const db::CellDef&, const db::Transform&, const std::string& path) {
                                    if (!path.empty()) {
                                      off.push_back(path);
                                    }
                                    return true;
                                  });
  CHECK(off == newest_first);
}

TEST_CASE("regression: tree searches over part of a descended cell search its use index",
          "[cap:gds-write][req:Structure ordering]") {
  // DBTreeSrTiles, DBTreeSrLabels and DBTreeSrCells search the cell plane
  // of every cell they descend into with DBCellSrArea (database/
  // DBcellsrch.c:283, :858, :1151-1153), and a partial search of 10 or more
  // unbinned uses builds the bins. The tree searches took each cell's uses
  // in the index's order without searching it, so after `select area` or
  // `what` over part of a child the writers still saw its unbinned index.
  HEFESTO_SELECT_MT(mt);
  for (const bool on : {true, false}) {
    for (int search = 0; search < 4; ++search) {
      CAPTURE(on, search);
      db::CellLibrary lib;
      db::CellDef& top = *lib.create("top", mt.tech->plane_count()).value();
      db::CellDef& mid = *lib.create("mid", mt.tech->plane_count()).value();
      db::CellDef& leaf = *lib.create("leaf", mt.tech->plane_count()).value();
      paint(leaf, mt, {0, 0, 4, 4}, mt.m2);
      db::recompute_bbox(leaf);
      for (int i = 0; i < 12; ++i) {
        db::PlaceOptions o;
        o.transform = db::translation(10 * i, 0);
        REQUIRE(db::place(mid, leaf, o).ok());
      }
      db::recompute_bbox(mid);
      REQUIRE(db::place(top, mid, db::PlaceOptions{}).ok());
      db::recompute_bbox(top);
      REQUIRE_FALSE(mid.use_index().state_for_test().binned);
      const Rect area{0, 0, 15, 4};
      const connect::Descend all;
      if (search == 0) {
        (void)connect::search_tree_tiles(top, area, tech::TypeMask::of(mt.m2), all, on,
                                         [](const connect::TreeTile&) { return true; });
      } else if (search == 1) {
        (void)connect::search_tree_labels(top, area, tech::TypeMask::all(), all, on,
                                          [](const connect::TreeLabel&) { return true; });
      } else if (search == 2) {
        (void)connect::search_tree_cells(top, area, [](const db::CellUse& u) { return u.def->name() == "mid"; }, on,
                                         [](const connect::TreeCell&) { return true; });
      } else {
        (void)connect::search_tree_defs(top, area, all, on,
                                        [](const db::CellDef&, const db::Transform&, const std::string&) { return true; });
      }
      CHECK(mid.use_index().state_for_test().binned);
    }
  }
}

TEST_CASE("regression: a hierarchical search ends when the net copy fractures a triangle",
          "[cap:layout-connectivity][req:Hierarchical connectivity]") {
  // A metal1 polygon reduced from the fuzz_lef_reader timeout input: the
  // net copy painted the triangle as smaller triangles that leave slivers
  // of it uncovered, so it was found unprocessed and queued again forever.
  HEFESTO_SELECT_MT(mt);
  db::CellDef def("top", mt.tech->plane_count());
  for (const Rect& r : {Rect{122, 180, 1734, 184}, Rect{122, 177, 1476, 180}, Rect{261, 176, 1476, 177},
                        Rect{314, 168, 1476, 176}}) {
    paint(def, mt, r, mt.m1);
  }
  REQUIRE(db::paint_triangle(def, *mt.tech, {198, 148, 281, 168}, db::Diagonal::ne_sw, true, mt.m1).ok());
  paint(def, mt, {281, 167, 1476, 168}, mt.m1);
  struct Bounded final : connect::NetVisitor {
    long tiles = 0;
    bool triangle = false;
    bool on_tile(const connect::NetTile& t) override {
      triangle = triangle || (t.tile.split && t.tile.area == Rect{198, 148, 281, 168});
      return ++tiles < 100000;  // the old search reported tiles without end
    }
  } net;
  REQUIRE(connect::search_hierarchical(def, *mt.tech, start_at(mt, {200, 181}), {}, net).ok());
  CHECK(net.tiles < 1000);
  CHECK(net.triangle);
}

TEST_CASE("regression: tree searches over part of a large array try only the elements under the area",
          "[cap:layout-connectivity][req:Hierarchical connectivity]") {
  // A 1000 x 1000 array (10^6 elements). The tree searches tried every
  // element of an array use touching the area, so each queued area of a
  // hierarchical search cost 10^6 element transforms and a net crossing a
  // row of the array did not finish. DBTreeSrTiles prunes with
  // DBArrayOverlap; so do they now, in both modes.
  HEFESTO_SELECT_MT(mt);
  db::CellLibrary lib;
  db::CellDef& top = *lib.create("top", mt.tech->plane_count()).value();
  db::CellDef& leaf = *lib.create("leaf", mt.tech->plane_count()).value();
  paint(leaf, mt, {0, 0, 10, 10}, mt.m1);
  db::recompute_bbox(leaf);
  db::PlaceOptions o;
  o.id = "a";
  o.array = db::ArrayInfo{0, 999, 0, 999, 20, 20};
  REQUIRE(db::place(top, leaf, o).ok());
  paint(top, mt, {0, 4, 395, 6}, mt.m1);  // a wire across the first 20 elements of row 0
  db::recompute_bbox(top);
  // DBArrayOverlap of a point inside element [370,370] is that element alone.
  const db::Rect inside{20 * 370 + 2, 20 * 370 + 2, 20 * 370 + 3, 20 * 370 + 3};
  CHECK(db::array_overlap(*std::as_const(top).uses().front(), inside) == db::ElementRange{370, 370, 370, 370});
  for (const bool on : {true, false}) {
    CAPTURE(on);
    const auto begin = std::chrono::steady_clock::now();
    connect::detail::elements_tried() = 0;
    int visits = 0;
    std::set<std::string> elements;
    for (int i = 0; i < 200; ++i) {
      const db::Coord c = 20 * (i * 37 % 1000);
      (void)connect::search_tree_tiles(top, Rect{c + 2, c + 2, c + 3, c + 3}, tech::TypeMask::of(mt.m1), {}, on,
                                       [&](const connect::TreeTile& t) {
                                         ++visits;
                                         elements.insert(*t.path);
                                         return true;
                                       });
    }
    CHECK(visits == 200);
    CHECK(elements.size() == 200);
    // Pruned: a handful of elements per point search; unpruned: 10^6 each.
    CHECK(connect::detail::elements_tried() <= 4 * 200);
    connect::detail::elements_tried() = 0;
    CHECK(elements.count("a[370,370]") == 1);
    Net net(*mt.tech);
    REQUIRE(connect::search_hierarchical(top, *mt.tech, start_at(mt, {1, 5}), compat(on), net).ok());
    std::set<std::string> net_elements(net.paths.begin(), net.paths.end());
    net_elements.erase("");
    std::set<std::string> row;
    for (int x = 0; x < 20; ++x) {
      row.insert("a[0," + std::to_string(x) + "]");  // a[y,x]
    }
    CHECK(net_elements == row);
    // The net search queues areas along one row of 20 elements.
    CHECK(connect::detail::elements_tried() < 10000);
    // A backstop only (the counts above decide): pruned, milliseconds;
    // unpruned, about 2 x 10^8 element transforms.
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    CHECK(elapsed < std::chrono::seconds(30));
  }
}
