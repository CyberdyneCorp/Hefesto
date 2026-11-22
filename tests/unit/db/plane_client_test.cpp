// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The tile client slot of db::Plane (openspec/changes/m7-extraction design
// D6, Open Question 1 (a); task 3.4): Magic's ti_client, run-scoped scratch
// of the extractor. It is not part of the plane's value: clone(), TileView
// comparison and undo ignore it, and writing it records nothing.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <map>
#include <vector>

#include "../undo/undo_fixture.hpp"
#include "db_fixture.hpp"
#include "hefesto/db/plane.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
using db::Rect;
using db::test::paint_row_of;

namespace {

constexpr tech::TileType kM1 = 20;
constexpr tech::TileType kM2 = 21;

void paint(db::Plane& p, const Rect& r, tech::TileType t = kM1) {
  REQUIRE(p.paint(r, paint_row_of(t)).ok());
}

/// Every tile of the plane, space included.
std::vector<db::TileView> all_tiles(const db::Plane& p) {
  return db::test::tiles(p, tech::TypeMask::all());
}

/// Gives every tile a distinct slot value (1, 2, ...).
void number_clients(const db::Plane& p) {
  std::uintptr_t n = 0;
  for (const auto& t : all_tiles(p)) {
    p.set_client(t, ++n);
  }
}

bool all_default(const db::Plane& p) {
  for (const auto& t : all_tiles(p)) {
    if (p.client(t) != db::kClientDefault) {
      return false;
    }
  }
  return true;
}

}  // namespace

#define GEO "[cap:layout-geometry-model][req:Geometry data structure is informative]"

TEST_CASE("the client default is Magic's CLIENTDEFAULT", GEO) {
  if constexpr (sizeof(std::uintptr_t) == 8) {
    CHECK(db::kClientDefault == std::uintptr_t{0xC000000000000004ULL});  // -((1 << 62) - 4)
  } else {
    CHECK(db::kClientDefault == std::uintptr_t{0xC0000004UL});
  }
}

TEST_CASE("new tiles start at the client default and keep what is set", GEO) {
  db::Plane p;
  CHECK(all_default(p));
  paint(p, {0, 0, 10, 10});
  paint(p, {20, 0, 30, 10}, kM2);
  CHECK(all_default(p));  // the tiles the paints cut start at the default
  number_clients(p);
  std::uintptr_t n = 0;
  for (const auto& t : all_tiles(p)) {
    CHECK(p.client(t) == ++n);
  }
}

TEST_CASE("a view without a tile reads the default and writes nothing", GEO) {
  const db::Plane p;
  const db::TileView detached;
  p.set_client(detached, 5);
  CHECK(p.client(detached) == db::kClientDefault);
  CHECK(all_default(p));
}

TEST_CASE("both sides of a split tile share one slot", GEO) {
  db::Plane p;
  REQUIRE(p.paint_triangle({0, 0, 10, 10}, db::Diagonal::ne_sw, false, paint_row_of(kM1)).ok());
  std::vector<db::TileView> sides;
  p.enumerate({0, 0, 10, 10}, tech::TypeMask::all(), [&](const db::TileView& t) {
    if (t.split) {
      sides.push_back(t);
    }
    return true;
  });
  REQUIRE(sides.size() == 2);
  p.set_client(sides[0], 42);
  CHECK(p.client(sides[1]) == 42);
}

TEST_CASE("clone copies the geometry and not the slots", GEO) {
  db::Plane p;
  paint(p, {0, 0, 10, 10});
  paint(p, {5, 5, 20, 20}, kM2);
  REQUIRE(p.paint_triangle({30, 0, 40, 10}, db::Diagonal::nw_se, true, paint_row_of(kM1)).ok());
  number_clients(p);
  const db::Plane copy = p.clone();
  CHECK(all_default(copy));
  // TileView's operator== compares values only: the slot is not one.
  CHECK(all_tiles(copy) == all_tiles(p));
  CHECK(copy.tile_count() == p.tile_count());
  // The original keeps its slots.
  CHECK_FALSE(all_default(p));
}

TEST_CASE("reset_clients resets every tile overlapping the area", GEO) {
  db::Plane p;
  paint(p, {0, 0, 10, 10});
  paint(p, {100, 100, 110, 110}, kM2);
  number_clients(p);
  constexpr std::uintptr_t kMark = 0xabcd;  // above every number_clients() value
  p.reset_clients({0, 0, 10, 10}, kMark);
  for (const auto& t : all_tiles(p)) {
    const bool overlaps = t.area.x0 < 10 && t.area.x1 > 0 && t.area.y0 < 10 && t.area.y1 > 0;
    INFO(t.area.x0 << "," << t.area.y0 << " " << t.area.x1 << "," << t.area.y1);
    CHECK((p.client(t) == kMark) == overlaps);
  }
  p.reset_clients({0, 0, 0, 10}, kMark + 1);  // no extent: nothing
  for (const auto& t : all_tiles(p)) {
    CHECK(p.client(t) != kMark + 1);
  }
  p.reset_clients(db::whole_plane());
  CHECK(all_default(p));
}

TEST_CASE("reset_clients over an area beyond the plane is clamped", GEO) {
  db::Plane p;
  paint(p, {0, 0, 10, 10});
  number_clients(p);
  p.reset_clients(Rect{db::kMinusInfinity, db::kMinusInfinity, db::kInfinity, db::kInfinity});
  CHECK(all_default(p));
  p.reset_clients(Rect{10, 10, 0, 0}, 3);  // inverted: nothing
  CHECK(all_default(p));
}

TEST_CASE("a paint keeps the slots of the tiles it does not touch", GEO) {
  db::Plane p;
  paint(p, {0, 0, 10, 10});
  paint(p, {100, 0, 110, 10}, kM2);
  db::TileView far;
  for (const auto& t : all_tiles(p)) {
    if (t.type == kM2) {
      far = t;
    }
  }
  p.set_client(far, 11);
  paint(p, {0, 0, 5, 5}, kM2);  // changes only tiles around (0,0)-(10,10)
  for (const auto& t : all_tiles(p)) {
    if (t.area == Rect{100, 0, 110, 10}) {
      CHECK(p.client(t) == 11);
    }
  }
}

namespace {

std::map<tech::PlaneId, std::vector<db::TileView>> geometry(const db::CellDef& def) {
  std::map<tech::PlaneId, std::vector<db::TileView>> out;
  for (std::size_t p = tech::kPlaneDrcError + 1; p < def.plane_count(); ++p) {
    const auto id = static_cast<tech::PlaneId>(p);
    out[id] = db::test::tiles(def.plane(id));
  }
  return out;
}

void number_cell_clients(const db::CellDef& def) {
  for (std::size_t p = tech::kPlaneDrcError + 1; p < def.plane_count(); ++p) {
    number_clients(def.plane(static_cast<tech::PlaneId>(p)));
  }
}

}  // namespace

TEST_CASE("undo ignores the slots and writing them records nothing",
          "[cap:layout-undo][req:Non-recorded operations]") {
  HEFESTO_REQUIRE_MT(mt);
  hefesto::undo::test::UndoSession s(mt);
  s.paint("metal1", 0, 0, 20, 10);
  const auto one = geometry(*s.top);
  number_cell_clients(*s.top);
  s.top->plane(mt.metal1).reset_clients(db::whole_plane(), 5);
  CHECK(s.editor.undo.redo(s.editor).steps == 0);  // nothing new to redo
  s.paint("metal1", 10, 0, 30, 20);
  const auto two = geometry(*s.top);
  number_cell_clients(*s.top);

  // One undo step goes back over the second paint only: the slot writes
  // made no steps of their own.
  auto undone = s.editor.undo.undo(s.editor);
  REQUIRE(undone.ok());
  CHECK(undone.value().steps == 1);
  CHECK(geometry(*s.top) == one);
  CHECK(s.editor.undo.redo(s.editor).steps == 1);
  CHECK(geometry(*s.top) == two);
  undone = s.editor.undo.undo(s.editor, 2);
  REQUIRE(undone.ok());
  CHECK(undone.value().steps == 2);
  CHECK(s.rects(mt.metal1).empty());
}
