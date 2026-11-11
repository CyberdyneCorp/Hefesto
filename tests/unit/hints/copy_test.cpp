// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// mask-hints "Copying, flattening and transforming hints": flatten into a
// new cell (DBFlatCopyMaskHints), flatten in place (DBCellCopyMaskHints),
// the DRC interaction copy, grid rescale and origin translation
// (MISC-MH-07 at unit level).

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <memory>
#include <string>
#include <vector>

#include "db_fixture.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/hints/hints.hpp"
#include "hefesto/tech/commands.hpp"

namespace db = hefesto::db;
namespace hints = hefesto::hints;
namespace tech = hefesto::tech;
using db::Rect;
using Rects = std::vector<Rect>;

#define COPY "[cap:mask-hints][req:Copying, flattening and transforming hints]"

namespace {

struct Layout {
  explicit Layout(const db::test::MtTech& mt) : tech(mt.tech), m1(mt.m1) {}

  std::shared_ptr<const tech::Technology> tech;
  tech::TileType m1;
  db::CellLibrary lib;

  db::CellDef* cell(const char* name, const Rect& paint) {
    db::CellDef* def = lib.create(name, tech->plane_count()).value();
    def->flags().available = true;
    REQUIRE(db::paint(*def, *tech, paint, m1).ok());
    db::recompute_bbox(*def);
    return def;
  }

  static db::CellUse* place(db::CellDef& parent, db::CellDef& child, const db::Transform& t,
                            const db::ArrayInfo& array = {}) {
    db::PlaceOptions o;
    o.transform = t;
    o.array = array;
    auto u = db::place(parent, child, o);
    REQUIRE(u.ok());
    db::recompute_bbox(parent);
    return u.value();
  }
};

void set_hint(db::CellDef& def, const char* name, const hints::HintRegion& region) {
  db::set_property(def, hints::property_key(name), db::PropertyValue{region});
}

Rects strips(const db::CellDef& def, const char* name) {
  const hints::HintRegion* region = hints::find_hint(def, name);
  return region != nullptr ? region->strips() : Rects{{-1, -1, -1, -1}};
}

}  // namespace

TEST_CASE("flatten in place copies a rotated instance's hints", COPY) {
  HEFESTO_REQUIRE_MT(mt);
  Layout l(mt);
  db::CellDef* child = l.cell("C", Rect{0, 0, 10, 20});
  db::CellDef* parent = l.cell("P", Rect{200, 200, 210, 210});
  set_hint(*child, "SRAM", hints::HintRegion{Rect{0, 0, 10, 20}});
  db::CellUse* use = Layout::place(*parent, *child, db::Transform{0, -1, 100, 1, 0, 0});
  REQUIRE(db::flatten_in_place(*use, *l.tech).ok());
  CHECK(strips(*parent, "SRAM") == Rects{{80, 0, 100, 10}});
}

TEST_CASE("flatten in place copies every array element's hints and unions them", COPY) {
  HEFESTO_REQUIRE_MT(mt);
  Layout l(mt);
  db::CellDef* child = l.cell("C", Rect{0, 0, 2, 2});
  db::CellDef* parent = l.cell("P", Rect{0, 0, 1, 1});
  set_hint(*child, "A", hints::HintRegion{Rect{0, 0, 2, 2}});
  set_hint(*parent, "A", hints::HintRegion{Rect{-10, 0, -5, 2}});
  db::CellUse* use = Layout::place(*parent, *child, db::kIdentity, db::ArrayInfo{0, 2, 0, 0, 10, 0});
  REQUIRE(db::flatten_in_place(*use, *l.tech).ok());
  CHECK(strips(*parent, "A") == Rects{{-10, 0, -5, 2}, {0, 0, 2, 2}, {10, 0, 12, 2}, {20, 0, 22, 2}});
}

TEST_CASE("flatten in place copies the hints of every flattened level", COPY) {
  // Regression (Q-MISC-14, fix in both modes): DBFlattenInPlace copies only
  // the instance's own hints (DBCellCopyMaskHints) and loses those of the
  // flattened grandchildren, whose paint it keeps.
  const bool compat = GENERATE(true, false);
  HEFESTO_REQUIRE_MT(mt);
  Layout l(mt);
  db::CellDef* leaf = l.cell("L", Rect{0, 0, 2, 2});
  db::CellDef* mid = l.cell("M", Rect{0, 0, 4, 4});
  db::CellDef* top = l.cell("T", Rect{0, 0, 1, 1});
  set_hint(*leaf, "DEEP", hints::HintRegion{Rect{0, 0, 2, 2}});
  set_hint(*leaf, "OUT", hints::HintRegion{Rect{-50, 0, -40, 2}});  // outside every bbox: copied whole
  set_hint(*mid, "MID", hints::HintRegion{Rect{0, 0, 4, 4}});
  Layout::place(*mid, *leaf, db::translation(1, 0), db::ArrayInfo{0, 1, 0, 0, 3, 0});
  db::CellUse* use = Layout::place(*top, *mid, db::translation(10, 0));
  db::FlattenOptions options;
  options.magic_compat = compat;
  REQUIRE(db::flatten_in_place(*use, *l.tech, options).ok());
  CHECK(strips(*top, "MID") == Rects{{10, 0, 14, 4}});
  CHECK(strips(*top, "DEEP") == Rects{{11, 0, 13, 2}, {14, 0, 16, 2}});
  CHECK(strips(*top, "OUT") == Rects{{-39, 0, -26, 2}});
}

TEST_CASE("flatten in place leaves the hints of kept instances in their cells", COPY) {
  // Q-MISC-14: only the levels that are flattened contribute; a kept
  // (not descended) instance is copied as an instance with its own hints.
  const bool compat = GENERATE(true, false);
  HEFESTO_REQUIRE_MT(mt);
  Layout l(mt);
  db::CellDef* leaf = l.cell("L", Rect{0, 0, 2, 2});
  db::CellDef* vendor = l.cell("V", Rect{0, 0, 2, 2});
  db::CellDef* mid = l.cell("M", Rect{0, 0, 4, 4});
  db::CellDef* top = l.cell("T", Rect{0, 0, 1, 1});
  set_hint(*leaf, "DEEP", hints::HintRegion{Rect{0, 0, 2, 2}});
  set_hint(*vendor, "KEPT", hints::HintRegion{Rect{0, 0, 2, 2}});
  db::set_property(*vendor, "GDS_FILE", db::PropertyValue{std::string{"v.gds"}});  // sets vendor_gds
  Layout::place(*vendor, *leaf, db::kIdentity);
  Layout::place(*mid, *vendor, db::translation(0, 10));
  Layout::place(*mid, *leaf, db::translation(2, 0));
  db::CellUse* use = Layout::place(*top, *mid, db::translation(10, 0));
  db::FlattenOptions options;
  options.descend = db::Descend::no_vendor;
  options.magic_compat = compat;
  REQUIRE(db::flatten_in_place(*use, *l.tech, options).ok());
  CHECK(strips(*top, "DEEP") == Rects{{12, 0, 14, 2}});
  CHECK(hints::find_hint(*top, "KEPT") == nullptr);
  CHECK(strips(*vendor, "KEPT") == Rects{{0, 0, 2, 2}});
}

TEST_CASE("flatten into a new cell copies the descendants' hints clipped to the source", COPY) {
  HEFESTO_REQUIRE_MT(mt);
  Layout l(mt);
  db::CellDef* leaf = l.cell("L", Rect{0, 0, 10, 10});
  db::CellDef* mid = l.cell("M", Rect{0, 0, 10, 10});
  db::CellDef* top = l.cell("T", Rect{0, 0, 100, 100});
  set_hint(*top, "OWN", hints::HintRegion{Rect{0, 0, 5, 5}});           // the source's own hints are not copied
  set_hint(*mid, "SRAM", hints::HintRegion{Rect{0, 0, 10, 10}});
  set_hint(*leaf, "SRAM", hints::HintRegion{Rect{0, 0, 10, 10}});
  set_hint(*leaf, "WIDE", hints::HintRegion{Rect{-500, 0, 500, 10}});  // clipped to the source bbox
  Layout::place(*mid, *leaf, db::translation(10, 0));
  Layout::place(*top, *mid, db::translation(50, 50));
  auto flat = db::flatten_to_new(l.lib, *top, "flat", *l.tech);
  REQUIRE(flat.ok());
  CHECK(hints::find_hint(*flat.value(), "OWN") == nullptr);
  CHECK(strips(*flat.value(), "SRAM") == Rects{{50, 50, 70, 60}});
  CHECK(strips(*flat.value(), "WIDE") == Rects{{0, 50, 100, 60}});
}

TEST_CASE("hint flattening copies every flattened level with a restricting mode", COPY) {
  // Regression (Q-MISC-15): Magic's DBTreeSrCells with a restricting mode
  // copies a kept instance and searches a descended one without copying
  // it, so the flattened levels' hints are lost. Hefesto copies every
  // descended element in both modes; the kept instance's own hints are
  // copied as well only under magic-compat (tier 2).
  const bool compat = GENERATE(true, false);
  HEFESTO_REQUIRE_MT(mt);
  Layout l(mt);
  db::CellDef* leaf = l.cell("L", Rect{0, 0, 10, 10});
  db::CellDef* vendor = l.cell("V", Rect{0, 0, 10, 10});
  db::CellDef* plain = l.cell("N", Rect{0, 0, 10, 10});
  db::CellDef* top = l.cell("T", Rect{0, 0, 100, 100});
  set_hint(*vendor, "V", hints::HintRegion{Rect{0, 0, 1, 1}});
  db::set_property(*vendor, "GDS_FILE", db::PropertyValue{std::string{"v.gds"}});  // sets vendor_gds
  set_hint(*plain, "N", hints::HintRegion{Rect{0, 0, 1, 1}});
  set_hint(*leaf, "L", hints::HintRegion{Rect{0, 0, 1, 1}});
  Layout::place(*vendor, *leaf, db::kIdentity);
  Layout::place(*plain, *leaf, db::kIdentity);
  Layout::place(*top, *vendor, db::translation(20, 0));
  Layout::place(*top, *plain, db::translation(40, 0));
  db::CellDef* dest = l.lib.create("dest", l.tech->plane_count()).value();
  hints::flat_copy_hints(*top, top->bbox(), db::kIdentity, *dest, db::Descend::no_vendor, compat);
  CHECK(strips(*dest, "N") == Rects{{40, 0, 41, 1}});
  CHECK(strips(*dest, "L") == Rects{{40, 0, 41, 1}});  // not the copy inside the kept V
  if (compat) {
    CHECK(strips(*dest, "V") == Rects{{20, 0, 21, 1}});
  } else {
    CHECK(hints::find_hint(*dest, "V") == nullptr);
  }
  db::CellDef* all = l.lib.create("all", l.tech->plane_count()).value();
  hints::flat_copy_hints(*top, top->bbox(), db::kIdentity, *all, db::Descend::all, compat);
  CHECK(strips(*all, "V") == Rects{{20, 0, 21, 1}});
  CHECK(strips(*all, "N") == Rects{{40, 0, 41, 1}});
  CHECK(strips(*all, "L") == Rects{{20, 0, 21, 1}, {40, 0, 41, 1}});
}

TEST_CASE("flatten -novendor into a new cell keeps the flattened levels' hints", COPY) {
  // Q-MISC-15 through flatten_to_new (the `flatten -novendor` path).
  const bool compat = GENERATE(true, false);
  HEFESTO_REQUIRE_MT(mt);
  Layout l(mt);
  db::CellDef* leaf = l.cell("L", Rect{0, 0, 10, 10});
  db::CellDef* vendor = l.cell("V", Rect{0, 0, 10, 10});
  db::CellDef* top = l.cell("T", Rect{0, 0, 100, 100});
  set_hint(*leaf, "L", hints::HintRegion{Rect{0, 0, 2, 2}});
  set_hint(*vendor, "V", hints::HintRegion{Rect{0, 0, 2, 2}});
  db::set_property(*vendor, "GDS_FILE", db::PropertyValue{std::string{"v.gds"}});
  Layout::place(*top, *leaf, db::translation(30, 0));
  Layout::place(*top, *vendor, db::translation(60, 0));
  db::FlattenOptions options;
  options.descend = db::Descend::no_vendor;
  options.magic_compat = compat;
  auto flat = db::flatten_to_new(l.lib, *top, "flat", *l.tech, options);
  REQUIRE(flat.ok());
  CHECK(strips(*flat.value(), "L") == Rects{{30, 0, 32, 2}});
  if (compat) {
    CHECK(strips(*flat.value(), "V") == Rects{{60, 0, 62, 2}});
  } else {
    CHECK(hints::find_hint(*flat.value(), "V") == nullptr);
  }
}

TEST_CASE("interaction flattening copies only the instances overlapping the area", COPY) {
  // DRCsubcell.c / DRCarray.c: DBFlatCopyMaskHints over the interaction area.
  HEFESTO_REQUIRE_MT(mt);
  Layout l(mt);
  db::CellDef* child = l.cell("C", Rect{0, 0, 10, 10});
  db::CellDef* top = l.cell("T", Rect{0, 0, 1, 1});
  set_hint(*child, "SRAM", hints::HintRegion{Rect{0, 0, 10, 10}});
  Layout::place(*top, *child, db::kIdentity, db::ArrayInfo{0, 3, 0, 0, 20, 0});
  db::CellDef* scratch = l.lib.create("scratch", l.tech->plane_count()).value();
  hints::flat_copy_hints(*top, Rect{25, 0, 45, 5}, db::kIdentity, *scratch);
  CHECK(strips(*scratch, "SRAM") == Rects{{25, 0, 30, 5}, {40, 0, 45, 5}});
}

TEST_CASE("copied hints create the destination property even when clipped away", COPY) {
  // dbCopyMaskHintsFunc creates the parent's plane before painting into it.
  HEFESTO_REQUIRE_MT(mt);
  Layout l(mt);
  db::CellDef* src = l.cell("S", Rect{0, 0, 1, 1});
  db::CellDef* dst = l.cell("D", Rect{0, 0, 1, 1});
  set_hint(*src, "A", hints::HintRegion{Rect{0, 0, 2, 2}});
  set_hint(*dst, "B", hints::HintRegion{Rect{0, 0, 2, 2}});
  set_hint(*src, "B", hints::HintRegion{Rect{50, 50, 60, 60}});
  db::set_property(*src, "NOT_A_HINT", db::PropertyValue{std::string{"x"}});
  hints::copy_hints(*src, *dst, db::kIdentity, Rect{100, 100, 200, 200});
  REQUIRE(hints::find_hint(*dst, "A") != nullptr);
  CHECK(hints::find_hint(*dst, "A")->empty());
  CHECK(strips(*dst, "B") == Rects{{0, 0, 2, 2}});  // an existing hint is kept
  CHECK(dst->properties().find("NOT_A_HINT") == nullptr);
}

TEST_CASE("grid rescale scales every hint like paint", COPY) {
  HEFESTO_REQUIRE_MT(mt);
  hefesto::editor::EditorState editor;
  editor.technology = mt.tech;
  db::CellDef* c = editor.cells.create("c", editor.technology->plane_count()).value();
  c->flags().available = true;
  set_hint(*c, "A", hints::HintRegion{Rect{0, 0, 5, 5}});
  set_hint(*c, "B", hints::HintRegion{Rect{-3, -3, 3, 3}});
  (void)tech::scale_layout(editor, 2, 1);
  CHECK(strips(*c, "A") == Rects{{0, 0, 10, 10}});
  (void)tech::scale_layout(editor, 1, 4);  // rounds down, as DBScaleValue
  CHECK(strips(*c, "A") == Rects{{0, 0, 2, 2}});
  CHECK(strips(*c, "B") == Rects{{-2, -2, 1, 1}});
}

TEST_CASE("an origin change translates every hint", COPY) {
  db::CellDef c{"c", 8};
  set_hint(c, "A", hints::HintRegion{Rect{0, 0, 5, 5}});
  set_hint(c, "B", hints::HintRegion{Rect{10, 10, 20, 20}});
  db::set_property(c, "OTHER", db::PropertyValue{std::string{"0 0 5 5"}});
  hints::translate_hints(c, -5, 3);
  CHECK(strips(c, "A") == Rects{{-5, 3, 0, 8}});
  CHECK(strips(c, "B") == Rects{{5, 13, 15, 23}});
  CHECK(*c.properties().find("OTHER") == db::PropertyValue{std::string{"0 0 5 5"}});
}
