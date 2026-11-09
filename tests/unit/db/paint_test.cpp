// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cell-level paint semantics with the golden test technology `mt`
// (layout-paint-semantics), built through the tech contract.

#include <catch2/catch_test_macros.hpp>

#include "db_fixture.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
using db::Rect;

namespace {

struct Typed {
  Rect r;
  tech::TileType t;
  friend bool operator==(const Typed&, const Typed&) = default;
};

std::vector<Typed> plane_tiles(const db::CellDef& def, tech::PlaneId p) {
  std::vector<Typed> out;
  for (const auto& t : db::test::tiles(def.plane(p))) {
    out.push_back(Typed{t.area, t.type});
  }
  return out;
}

db::CellDef* new_cell(db::CellLibrary& lib, const db::test::MtTech& mt, const char* name = "a") {
  auto c = lib.create(name, mt.tech->plane_count());
  REQUIRE(c.ok());
  return c.value();
}

void paint(db::CellDef& d, const db::test::MtTech& mt, const Rect& r, tech::TileType t) {
  REQUIRE(db::paint(d, *mt.tech, r, t).ok());
}

void erase(db::CellDef& d, const db::test::MtTech& mt, const Rect& r, tech::TileType t) {
  REQUIRE(db::erase(d, *mt.tech, r, tech::TypeMask::of(t)).ok());
}

}  // namespace

#define PS "[cap:layout-paint-semantics]"

TEST_CASE("painting over another type on the same plane replaces it", PS "[req:Result-table model of painting]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = new_cell(lib, mt);
  paint(*d, mt, {0, 0, 10, 10}, mt.ndiff);
  paint(*d, mt, {5, 0, 15, 10}, mt.pdiff);
  CHECK(plane_tiles(*d, mt.active) == std::vector<Typed>{{{0, 0, 5, 10}, mt.ndiff}, {{5, 0, 15, 10}, mt.pdiff}});
}

TEST_CASE("erasing a different type is a no-op", PS "[req:Result-table model of painting]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = new_cell(lib, mt);
  paint(*d, mt, {0, 0, 10, 10}, mt.m1);
  erase(*d, mt, {0, 0, 10, 10}, mt.m2);
  CHECK(plane_tiles(*d, mt.metal1) == std::vector<Typed>{{{0, 0, 10, 10}, mt.m1}});
}

TEST_CASE("painting present paint and erasing nothing still mark modified",
          PS "[req:Modification marking for paint and copy]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = new_cell(lib, mt);
  paint(*d, mt, {0, 0, 10, 10}, mt.m1);
  d->flags() = db::CellFlags{};
  paint(*d, mt, {0, 0, 10, 10}, mt.m1);
  CHECK(d->flags().modified);
  CHECK(d->flags().needs_new_timestamp);
  auto* e = new_cell(lib, mt, "empty");
  erase(*e, mt, {0, 0, 10, 10}, mt.m1);
  CHECK(e->flags().modified);
}

TEST_CASE("erase-mask erases every type of the mask", PS "[req:Paint operation]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = new_cell(lib, mt);
  paint(*d, mt, {0, 0, 10, 10}, mt.m1);
  paint(*d, mt, {0, 0, 10, 10}, mt.poly);
  tech::TypeMask mask = tech::TypeMask::of(mt.poly);
  mask.set(mt.m1);
  REQUIRE(db::erase(*d, *mt.tech, {0, 0, 5, 10}, mask).ok());
  CHECK(plane_tiles(*d, mt.metal1) == std::vector<Typed>{{{5, 0, 10, 10}, mt.m1}});
  CHECK(plane_tiles(*d, mt.active) == std::vector<Typed>{{{5, 0, 10, 10}, mt.poly}});
}

TEST_CASE("painting a via puts it on both metal planes", PS "[req:Default contact painting rules]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = new_cell(lib, mt);
  paint(*d, mt, {0, 0, 20, 10}, mt.m1);
  paint(*d, mt, {0, 0, 10, 10}, mt.via1);
  CHECK(plane_tiles(*d, mt.metal1) == std::vector<Typed>{{{0, 0, 10, 10}, mt.via1}, {{10, 0, 20, 10}, mt.m1}});
  CHECK(plane_tiles(*d, mt.metal2) == std::vector<Typed>{{{0, 0, 10, 10}, mt.via1}});
}

TEST_CASE("painting a residue over a contact is harmless", PS "[req:Default contact painting rules]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = new_cell(lib, mt);
  paint(*d, mt, {0, 0, 10, 10}, mt.via1);
  paint(*d, mt, {0, 0, 20, 10}, mt.m1);
  CHECK(plane_tiles(*d, mt.metal1) == std::vector<Typed>{{{0, 0, 10, 10}, mt.via1}, {{10, 0, 20, 10}, mt.m1}});
  CHECK(plane_tiles(*d, mt.metal2) == std::vector<Typed>{{{0, 0, 10, 10}, mt.via1}});
}

TEST_CASE("erasing a residue dissolves the contact", PS "[req:Default contact painting rules]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = new_cell(lib, mt);
  paint(*d, mt, {0, 0, 10, 10}, mt.via1);
  erase(*d, mt, {0, 0, 10, 10}, mt.m1);
  CHECK(plane_tiles(*d, mt.metal1).empty());
  CHECK(plane_tiles(*d, mt.metal2) == std::vector<Typed>{{{0, 0, 10, 10}, mt.m2}});
}

TEST_CASE("an incompatible residue breaks a diffusion contact", PS "[req:Default contact painting rules]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = new_cell(lib, mt);
  paint(*d, mt, {0, 0, 4, 4}, mt.pdc);
  paint(*d, mt, {0, 0, 4, 4}, mt.ndiff);
  CHECK(plane_tiles(*d, mt.active) == std::vector<Typed>{{{0, 0, 4, 4}, mt.ndiff}});
  CHECK(plane_tiles(*d, mt.metal1) == std::vector<Typed>{{{0, 0, 4, 4}, mt.m1}});
}

TEST_CASE("via2 over via1 makes the stacked type on the shared plane", PS "[req:Stacked contacts]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = new_cell(lib, mt);
  paint(*d, mt, {0, 0, 4, 4}, mt.via1);
  paint(*d, mt, {0, 0, 4, 4}, mt.via2);
  const auto stacked = plane_tiles(*d, mt.metal2);
  REQUIRE(stacked.size() == 1);
  const tech::ContactInfo* info = mt.tech->contact(stacked[0].t);
  REQUIRE(info != nullptr);
  CHECK(info->stacked);
  CHECK(plane_tiles(*d, mt.metal1) == std::vector<Typed>{{{0, 0, 4, 4}, mt.via1}});
  CHECK(plane_tiles(*d, mt.metal3) == std::vector<Typed>{{{0, 0, 4, 4}, mt.via2}});
  SECTION("erasing one component of the stack") {
    erase(*d, mt, {0, 0, 4, 4}, mt.via2);
    CHECK(plane_tiles(*d, mt.metal1) == std::vector<Typed>{{{0, 0, 4, 4}, mt.via1}});
    CHECK(plane_tiles(*d, mt.metal2) == std::vector<Typed>{{{0, 0, 4, 4}, mt.via1}});
    CHECK(plane_tiles(*d, mt.metal3).empty());
  }
}

TEST_CASE("compose rule for a non-contact", PS "[req:Technology compose rules at runtime]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = new_cell(lib, mt);
  paint(*d, mt, {0, 0, 10, 10}, mt.ndiff);
  paint(*d, mt, {4, -2, 6, 12}, mt.poly);
  CHECK(d->plane(mt.active).type_at({5, 5}) == mt.nfet);
  CHECK(d->plane(mt.active).type_at({5, -1}) == mt.poly);
  CHECK(d->plane(mt.active).type_at({5, 11}) == mt.poly);
  CHECK(d->plane(mt.active).type_at({1, 5}) == mt.ndiff);
  erase(*d, mt, {4, 0, 6, 10}, mt.poly);
  CHECK(d->plane(mt.active).type_at({5, 5}) == mt.ndiff);
  CHECK(plane_tiles(*d, mt.active) == std::vector<Typed>{{{4, 10, 6, 12}, mt.poly}, {{0, 0, 10, 10}, mt.ndiff},
                                                         {{4, -2, 6, 0}, mt.poly}});
}

TEST_CASE("copying a child with a contact into its parent", PS "[req:Copying paint between cells]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* child = new_cell(lib, mt, "child");
  auto* parent = new_cell(lib, mt, "parent");
  paint(*child, mt, {0, 0, 4, 4}, mt.via1);
  paint(*parent, mt, {100, 0, 120, 4}, mt.m2);
  REQUIRE(db::copy_paint(*child, {0, 0, 4, 4}, db::translation(100, 0), *parent, *mt.tech).ok());
  CHECK(plane_tiles(*parent, mt.metal1) == std::vector<Typed>{{{100, 0, 104, 4}, mt.via1}});
  CHECK(plane_tiles(*parent, mt.metal2) ==
        std::vector<Typed>{{{100, 0, 104, 4}, mt.via1}, {{104, 0, 120, 4}, mt.m2}});
  CHECK(parent->flags().modified);
}

TEST_CASE("painting a contact whose metal2 residue is locked", PS "[req:Active (locked) layers]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = new_cell(lib, mt);
  db::PaintOptions o;
  o.locked.set(mt.m2);
  REQUIRE(db::paint(*d, *mt.tech, {0, 0, 4, 4}, mt.via1, o).ok());
  CHECK(plane_tiles(*d, mt.metal1) == std::vector<Typed>{{{0, 0, 4, 4}, mt.m1}});
  CHECK(plane_tiles(*d, mt.metal2).empty());
  // A locked type itself is never painted.
  REQUIRE(db::paint(*d, *mt.tech, {10, 0, 14, 4}, mt.m2, o).ok());
  CHECK(plane_tiles(*d, mt.metal2).empty());
}

TEST_CASE("paint over a subcell leaves the child alone", PS "[req:Paint is local to the target cell]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* inv = new_cell(lib, mt, "inv");
  auto* top = new_cell(lib, mt, "top");
  paint(*inv, mt, {0, 0, 10, 10}, mt.m1);
  REQUIRE(db::place(*top, *inv, {}).ok());
  inv->flags() = db::CellFlags{};
  erase(*top, mt, {0, 0, 10, 10}, mt.m1);
  CHECK(plane_tiles(*inv, mt.metal1) == std::vector<Typed>{{{0, 0, 10, 10}, mt.m1}});
  CHECK_FALSE(inv->flags().modified);
}

TEST_CASE("triangles paint through the cell entry point", PS "[req:Non-Manhattan painting]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = new_cell(lib, mt);
  REQUIRE(db::paint_triangle(*d, *mt.tech, {0, 0, 10, 10}, db::Diagonal::ne_sw, false, mt.m1).ok());
  const auto t = db::test::tiles(d->plane(mt.metal1));
  REQUIRE(t.size() == 1);
  CHECK(t[0].split);
  CHECK(db::triangle_corner(t[0].diagonal, t[0].right_side) == db::TriangleCorner::nw);
}
