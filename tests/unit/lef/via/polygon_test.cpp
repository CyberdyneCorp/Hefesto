// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Polygon painting shared by LEF POLYGON (Q-LEFDEF-23), DEF pin POLYGON
// (Q-LEFDEF-66 OFF) and DEF route POLYGON (Q-LEFDEF-70 OFF): one
// db::decompose_polygon, with port labels from the single decomposition
// (openspec/changes/m6-lef-def-oasis tasks.md 4.4).
//
// Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/lef/via/polygon.hpp"
#include "via_fixture.hpp"

namespace hefesto::lef {
namespace {

std::vector<db::TileView> tiles(const db::CellDef& cell, const tech::Technology& technology, tech::TileType type) {
  std::vector<db::TileView> out;
  const auto plane = technology.home_plane(type);
  REQUIRE(plane.has_value());
  (void)cell.plane(*plane).enumerate(db::whole_plane(), tech::TypeMask::of(type), [&](const db::TileView& t) {
    out.push_back(t);
    return true;
  });
  return out;
}

// An L: (0,0)-(30,10) plus (0,10)-(10,30), in LEF file order.
constexpr std::array<db::Point, 6> kEll{db::Point{0, 0}, {30, 0}, {30, 10}, {10, 10}, {10, 30}, {0, 30}};

TEST_CASE("LEF, DEF pin and DEF route polygons paint identical tiles",
          "[cap:lef-read][req:PORT and OBS geometry][cap:def-read][req:Pin geometry placement]"
          "[req:Other route elements]") {
  const auto technology = test::via_tech();
  const tech::TileType metal1 = test::type_of(*technology, "metal1");
  // The three readers call the one function; three cells, same vertices.
  std::array<db::CellDef, 3> cells{db::CellDef{"lef", technology->plane_count()},
                                   db::CellDef{"def_pin", technology->plane_count()},
                                   db::CellDef{"def_route", technology->plane_count()}};
  std::vector<std::vector<db::Rect>> rects;
  for (db::CellDef& cell : cells) {
    auto painted = paint_polygon_rects(cell, *technology, metal1, kEll);
    REQUIRE(painted.ok());
    rects.push_back(painted.value());
  }
  CHECK(rects[0] == rects[1]);
  CHECK(rects[1] == rects[2]);
  CHECK(tiles(cells[0], *technology, metal1) == tiles(cells[1], *technology, metal1));
  CHECK(tiles(cells[1], *technology, metal1) == tiles(cells[2], *technology, metal1));
  // The L is painted exactly, as two strips.
  CHECK(tiles(cells[0], *technology, metal1).size() == 2);
  CHECK(rects[0].size() == 2);
  CHECK(rects[0] == polygon_rects(kEll));
}

TEST_CASE("a diagonal polygon paints its triangles and returns its strips", "[cap:lef-read][req:PORT and OBS geometry]") {
  const auto technology = test::via_tech();
  const tech::TileType metal2 = test::type_of(*technology, "metal2");
  db::CellDef cell{"tri", technology->plane_count()};
  // A right triangle over a strip: (0,0)-(20,0)-(20,10)-(10,20)-(0,20).
  const std::array<db::Point, 5> pentagon{db::Point{0, 0}, {20, 0}, {20, 10}, {10, 20}, {0, 20}};
  auto painted = paint_polygon_rects(cell, *technology, metal2, pentagon);
  REQUIRE(painted.ok());
  bool split = false;
  for (const db::TileView& t : tiles(cell, *technology, metal2)) {
    split = split || t.split;
  }
  CHECK(split);
  CHECK_FALSE(painted.value().empty());
  CHECK(painted.value() == polygon_rects(pentagon));
}

TEST_CASE("polygon port labels come from the single decomposition", "[cap:lef-read][req:Port labels in creation mode]") {
  const auto technology = test::via_tech();
  const tech::TileType metal1 = test::type_of(*technology, "metal1");
  db::CellDef cell{"pin", technology->plane_count()};
  REQUIRE(paint_polygon(cell, *technology, metal1, kEll, PortLabel{"A", 3}).ok());
  const std::vector<db::Rect> rects = polygon_rects(kEll);
  REQUIRE(cell.labels().size() == rects.size());
  for (std::size_t i = 0; i < rects.size(); ++i) {
    const db::Label& label = cell.labels()[i];
    CHECK(label.text == "A");
    CHECK(label.rect == rects[i]);
    CHECK(label.type == metal1);
    CHECK(label.is_port());
    CHECK(label.sticky);
    CHECK(label.port_index == 3);
  }
  // A second polygon of the same pin takes the existing port's index.
  const std::array<db::Point, 4> square{db::Point{50, 0}, {60, 0}, {60, 10}, {50, 10}};
  REQUIRE(paint_polygon(cell, *technology, metal1, square, PortLabel{"A", 7}).ok());
  CHECK(cell.labels().back().port_index == 3);
  // Without a port nothing is labelled.
  REQUIRE(paint_polygon(cell, *technology, metal1, square).ok());
  CHECK(cell.labels().size() == rects.size() + 1);
}

TEST_CASE("a polygon of fewer than three vertices is refused", "[cap:lef-read][req:PORT and OBS geometry]") {
  const auto technology = test::via_tech();
  db::CellDef cell{"bad", technology->plane_count()};
  const std::array<db::Point, 2> line{db::Point{0, 0}, {10, 0}};
  CHECK_FALSE(paint_polygon(cell, *technology, test::type_of(*technology, "metal1"), line).ok());
}

}  // namespace
}  // namespace hefesto::lef
