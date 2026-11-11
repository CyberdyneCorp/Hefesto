// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cell namespace, instances, arrays, bounding boxes and flattening
// (layout-hierarchy).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "db_fixture.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
using db::Rect;

namespace {

constexpr std::size_t kPlanes = 10;

db::CellDef* make(db::CellLibrary& lib, const char* name) {
  auto c = lib.create(name, kPlanes);
  REQUIRE(c.ok());
  return c.value();
}

db::CellUse* add_use(db::CellDef& parent, db::CellDef& child, db::PlaceOptions o = {}) {
  auto u = db::place(parent, child, o);
  REQUIRE(u.ok());
  return u.value();
}

db::Label label(const char* text, Rect r, bool port = false) {
  db::Label l;
  l.text = text;
  l.rect = r;
  l.port_directions = port ? db::kPortNorth : 0;
  return l;
}

std::vector<std::string> label_texts(const db::CellDef& d) {
  std::vector<std::string> out;
  for (const auto& l : d.labels()) {
    out.push_back(l.text);
  }
  return out;
}

}  // namespace

#define HI "[cap:layout-hierarchy]"

TEST_CASE(".mag suffix is stripped and new cells are empty", HI "[req:Cell definitions and the cell namespace]") {
  db::CellLibrary lib;
  auto* inv = make(lib, "inv.mag");
  CHECK(inv->name() == "inv");
  CHECK(lib.find("inv") == inv);
  CHECK(inv->bbox() == Rect{0, 0, 1, 1});
  CHECK(inv->extended_bbox() == Rect{0, 0, 1, 1});
  CHECK(inv->flags() == db::CellFlags{});
  CHECK(make(lib, "")->name() == "(UNNAMED)");
  const auto dup = lib.create("inv", kPlanes);
  REQUIRE_FALSE(dup.ok());
  CHECK(dup.error().code == db::codes::kCellExists);
}

TEST_CASE("the cell namespace iterates in Magic's hash order", HI "[req:Cell definitions and the cell namespace]") {
  db::CellLibrary lib;
  hefesto::util::MagicHashTable<int> reference(64);
  for (const char* n : {"inv", "nand2", "top", "a", "b", "lib/x", "sky130_fd_sc_hd__inv_1"}) {
    (void)make(lib, n);
    reference.insert(n, 0);
  }
  std::vector<std::string> order;
  lib.for_each([&](db::CellDef& d) { order.push_back(d.name()); });
  CHECK(order == reference.keys());
}

TEST_CASE("placing marks the parent modified", HI "[req:Cell instances]") {
  db::CellLibrary lib;
  auto* top = make(lib, "top");
  auto* inv = make(lib, "inv");
  auto* use = add_use(*top, *inv);
  CHECK(top->flags().modified);
  CHECK(use->transform == db::kIdentity);
  CHECK(use->array == db::ArrayInfo{});
  CHECK_FALSE(use->locked);
  CHECK(inv->instances() == std::vector<db::CellUse*>{use});
  const auto self = db::place(*inv, *top, {});
  REQUIRE_FALSE(self.ok());
  CHECK(self.error().code == db::codes::kUseRecursive);
}

TEST_CASE("sequential default identifiers and gap reuse", HI "[req:Instance identifier assignment]") {
  db::CellLibrary lib;
  auto* top = make(lib, "top");
  auto* inv = make(lib, "inv");
  CHECK(add_use(*top, *inv)->id == "inv_0");
  db::CellUse* second = add_use(*top, *inv);
  CHECK(second->id == "inv_1");
  CHECK(add_use(*top, *inv)->id == "inv_2");
  REQUIRE(db::remove_use(*second).ok());
  CHECK(add_use(*top, *inv)->id == "inv_1");
  db::PlaceOptions o;
  o.id = "inv_0";
  const auto taken = db::place(*top, *inv, o);
  REQUIRE_FALSE(taken.ok());
  CHECK(taken.error().code == db::codes::kUseIdTaken);
}

TEST_CASE("library path stripped from base", HI "[req:Instance identifier assignment]") {
  db::CellLibrary lib;
  auto* top = make(lib, "top");
  auto* nand = make(lib, "lib/nand2");
  CHECK(add_use(*top, *nand)->id == "nand2_0");
}

TEST_CASE("identifier lookup ignores array subscripts", HI "[req:Instance identifier assignment]") {
  db::CellLibrary lib;
  auto* top = make(lib, "top");
  auto* inv = make(lib, "inv");
  auto* u = add_use(*top, *inv);
  CHECK(db::find_use(*top, "inv_0[1,2]") == u);
  CHECK(db::find_use(*top, "inv_0[3]") == u);
  CHECK(db::find_use(*top, "nope") == nullptr);
  top->flags() = db::CellFlags{};
  REQUIRE(db::rename_use(*u, "inv_0").ok());
  CHECK_FALSE(top->flags().modified);
  REQUIRE(db::rename_use(*u, "x").ok());
  CHECK(top->flags().modified);
  CHECK_FALSE(top->flags().needs_new_timestamp);
}

TEST_CASE("read uniquification of instance ids", HI "[req:Instance identifier assignment]") {
  db::CellLibrary lib;
  auto* top = make(lib, "top");
  auto* inv = make(lib, "lib/inv");
  auto* a = add_use(*top, *inv);
  auto* b = add_use(*top, *inv);
  auto* c = add_use(*top, *inv);
  a->id = "dup";
  b->id = "dup";
  c->id.clear();
  // DBGenerateUniqueIds walks the uses newest first (BPlane in-box order):
  // the later "dup" keeps its id, the earlier one is renamed after c.
  CHECK(db::make_use_ids_unique(*top, true) == 2);
  CHECK(b->id == "dup");
  CHECK(c->id == "lib/inv_0");  // Q-MODEL-8: full child name in compat
  CHECK(a->id == "lib/inv_1");
  a->id.clear();
  CHECK(db::make_use_ids_unique(*top, false) == 1);
  CHECK(a->id == "inv_0");
}

TEST_CASE("array element placement", HI "[req:Arrays]") {
  db::CellLibrary lib;
  auto* top = make(lib, "top");
  auto* child = make(lib, "child");
  child->set_bboxes({0, 0, 10, 10}, {0, 0, 10, 10});
  db::PlaceOptions o;
  o.transform = db::translation(100, 0);
  o.array = db::ArrayInfo{0, 3, 0, 1, 20, 15};
  auto* u = add_use(*top, *child, o);
  CHECK(db::apply(db::element_transform(*u, 2, 1), db::Point{0, 0}) == db::Point{140, 15});
  u->array = db::ArrayInfo{3, 0, 0, 0, 20, 0};
  u->transform = db::kIdentity;
  CHECK(db::apply(db::element_transform(*u, 3, 0), db::Point{0, 0}) == db::Point{0, 0});
  CHECK(db::apply(db::element_transform(*u, 0, 0), db::Point{0, 0}) == db::Point{60, 0});
}

TEST_CASE("arrays from root-coordinate parameters", HI "[req:Arrays]") {
  db::CellLibrary lib;
  auto* top = make(lib, "top");
  auto* child = make(lib, "child");
  db::PlaceOptions o;
  o.transform = db::Transform{0, 1, 0, -1, 0, 0};
  auto* u = add_use(*top, *child, o);
  db::make_array(*u, db::invert(u->transform), db::ArrayInfo{0, 3, 0, 1, 20, 15});
  // M.a == 0 swaps the index ranges; separations go through M.
  CHECK(u->array == db::ArrayInfo{0, 1, 0, 3, -15, 20});
}

TEST_CASE("array element names", HI "[req:Array element naming]") {
  db::CellUse u;
  u.id = "inv_0";
  CHECK(db::array_element_name(u, 0, 0) == "inv_0");
  u.array = db::ArrayInfo{0, 3, 0, 1, 1, 1};
  CHECK(db::array_element_name(u, 2, 1) == "inv_0[1,2]");
  u.array = db::ArrayInfo{0, 3, 0, 0, 1, 1};
  CHECK(db::array_element_name(u, 2, 0) == "inv_0[2]");
  u.array = db::ArrayInfo{0, 0, 0, 3, 1, 1};
  CHECK(db::array_element_name(u, 0, 2) == "inv_0[2]");
}

TEST_CASE("bounding box of paint and labels", HI "[req:Bounding box and extended bounding box]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = lib.create("a", mt.tech->plane_count()).value();
  REQUIRE(db::paint(*d, *mt.tech, {0, 0, 10, 10}, mt.m1).ok());
  REQUIRE(db::put_label(*d, label("p", {20, 20, 20, 20})).ok());
  db::recompute_bbox(*d);
  CHECK(d->bbox() == Rect{0, 0, 20, 20});
  CHECK(d->extended_bbox() == Rect{0, 0, 20, 20});
  auto* only = lib.create("only", mt.tech->plane_count()).value();
  REQUIRE(db::put_label(*only, label("p", {5, 5, 5, 5})).ok());
  db::recompute_bbox(*only);
  CHECK(only->bbox() == Rect{5, 5, 6, 6});
  // Magic leaves the extended box at the degenerate origin box here.
  CHECK(only->extended_bbox() == Rect{0, 0, 1, 1});
  auto* empty = lib.create("empty", mt.tech->plane_count()).value();
  db::recompute_bbox(*empty);
  CHECK(empty->bbox() == Rect{0, 0, 1, 1});
}

TEST_CASE("arrayed use box and propagation to parents", HI "[req:Bounding box and extended bounding box]") {
  db::CellLibrary lib;
  auto* top = make(lib, "top");
  auto* child = make(lib, "child");
  REQUIRE(db::put_label(*child, label("a", {0, 0, 10, 10})).ok());
  db::recompute_bbox(*child);
  db::PlaceOptions o;
  o.array = db::ArrayInfo{0, 3, 0, 1, 20, 15};
  auto* u = add_use(*top, *child, o);
  CHECK(u->bbox == Rect{0, 0, 70, 25});
  u->array.xsep = -20;
  db::compute_use_bbox(*u);
  CHECK(u->bbox == Rect{-60, 0, 10, 25});
  db::recompute_bbox(*top);
  CHECK(top->bbox() == Rect{-60, 0, 10, 25});
  // A change in the child repositions the use and propagates upward.
  REQUIRE(db::put_label(*child, label("b", {0, 0, 12, 10})).ok());
  db::recompute_bbox(*child);
  CHECK(top->bbox() == Rect{-60, 0, 12, 25});
  CHECK(top->flags().child_bboxes_changed);
}

TEST_CASE("a parent keeps its box when the child was shown in a window before its placement",
          HI "[req:Bounding box and extended bounding box]") {
  // Regression (golden DRC-ENG-1, CELL-TS-02): dbReComputeBboxFunc's last
  // recursion is guarded by the parent of the last cd_parents use; a window's
  // root use, never freed and older than the placement, ends the list, so
  // the parent's box is not recomputed with magic-compat ON.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    db::CellLibrary lib;
    auto* top = make(lib, "top");
    auto* child = make(lib, "child");
    REQUIRE(db::put_label(*child, label("a", {0, 0, 10, 10})).ok());
    db::recompute_bbox(*child, compat);
    child->note_window_root();  // `load child` before the placement
    auto* u = add_use(*top, *child, {});
    db::recompute_bbox(*top, compat);
    REQUIRE(top->bbox() == Rect{0, 0, 10, 10});
    REQUIRE(db::put_label(*child, label("b", {0, 0, 20, 10})).ok());
    db::recompute_bbox(*child, compat);
    CHECK(u->bbox == Rect{0, 0, 20, 10});  // the use always follows its child
    CHECK(top->bbox() == (compat ? Rect{0, 0, 10, 10} : Rect{0, 0, 20, 10}));
  }
}

TEST_CASE("a parent follows its child when the placement is older than any window load",
          HI "[req:Bounding box and extended bounding box]") {
  // Q-MODEL-32 (compat): only a window load older than every placement
  // leaves the parent stale; here both modes recompute it.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    db::CellLibrary lib;
    auto* top = make(lib, "top");
    auto* child = make(lib, "child");
    REQUIRE(db::put_label(*child, label("a", {0, 0, 10, 10})).ok());
    db::recompute_bbox(*child, compat);
    (void)add_use(*top, *child, {});
    child->note_window_root();  // shown in a window after the placement
    REQUIRE(db::put_label(*child, label("b", {0, 0, 20, 10})).ok());
    db::recompute_bbox(*child, compat);
    CHECK(top->bbox() == Rect{0, 0, 20, 10});
  }
}

TEST_CASE("outline-font text boxes extend the extended box", HI "[req:Bounding box and extended bounding box]") {
  db::CellLibrary lib;
  auto* d = make(lib, "f");
  db::Label l = label("t", {0, 0, 2, 2});
  l.font = 0;
  l.text_bbox = Rect{-5, 0, 30, 4};
  REQUIRE(db::put_label(*d, l, false, 1).ok());  // one font loaded
  db::recompute_bbox(*d);
  CHECK(d->bbox() == Rect{0, 0, 2, 2});
  CHECK(d->extended_bbox() == Rect{-5, 0, 30, 4});
}

TEST_CASE("locking marks the parent modified (Q-MODEL-13)", HI "[req:Locked instances]") {
  db::CellLibrary lib;
  auto* top = make(lib, "top");
  auto* inv = make(lib, "inv");
  auto* u = add_use(*top, *inv);
  top->flags() = db::CellFlags{};
  db::set_use_locked(*u, true);
  CHECK(u->locked);
  CHECK(top->flags().modified);
}

TEST_CASE("rename refused for an existing name and for internal cells", HI "[req:Renaming a cell definition]") {
  db::CellLibrary lib;
  auto* a = make(lib, "a");
  auto* b = make(lib, "b");
  auto st = lib.rename(*a, "b");
  REQUIRE_FALSE(st.ok());
  CHECK(st.error().code == db::codes::kCellExists);
  CHECK(lib.find("a") == a);
  CHECK(lib.find("b") == b);
  b->flags().internal = true;
  CHECK(lib.rename(*b, "c").error().code == db::codes::kCellInternal);
}

TEST_CASE("rename marks parents and the cell modified, vendor needs force", HI "[req:Renaming a cell definition]") {
  db::CellLibrary lib;
  auto* top = make(lib, "top");
  auto* pad = make(lib, "pad");
  add_use(*top, *pad);
  top->flags() = db::CellFlags{};
  REQUIRE(lib.rename(*pad, "pad2").ok());
  CHECK(lib.find("pad") == nullptr);
  CHECK(lib.find("pad2") == pad);
  CHECK(top->flags().modified);
  CHECK(pad->flags().modified);  // Q-MODEL-15
  db::set_property(*pad, "GDS_FILE", db::PropertyValue{std::string{"x.gds"}});
  db::set_property(*pad, "GDS_START", db::PropertyValue{db::DoubleList{{12}}});
  CHECK(lib.rename(*pad, "pad3").error().code == db::codes::kCellVendor);
  REQUIRE(lib.rename(*pad, "pad3", true).ok());
  CHECK(pad->properties().find("GDS_FILE") == nullptr);
  CHECK(pad->properties().find("GDS_START") == nullptr);
  CHECK_FALSE(pad->flags().vendor_gds);
}

TEST_CASE("delete refused while instantiated", HI "[req:Deleting a cell definition]") {
  db::CellLibrary lib;
  auto* top = make(lib, "top");
  auto* inv = make(lib, "inv");
  auto* u = add_use(*top, *inv);
  const auto st = lib.remove(*inv);
  REQUIRE_FALSE(st.ok());
  CHECK(st.error().code == db::codes::kCellInUse);
  CHECK(st.error().text.find("inv_0") != std::string::npos);
  REQUIRE(db::remove_use(*u).ok());
  REQUIRE(lib.remove(*inv).ok());
  CHECK(lib.find("inv") == nullptr);
  CHECK(lib.size() == 1);
}

TEST_CASE("hierarchical label naming when flattening", HI "[req:Flattening into a new cell]") {
  const auto tech = tech::builtin_technology();
  db::CellLibrary lib;
  auto* top = lib.create("top", tech->plane_count()).value();
  auto* mid = lib.create("mid", tech->plane_count()).value();
  auto* leaf = lib.create("leaf", tech->plane_count()).value();
  REQUIRE(db::put_label(*leaf, label("A", {1, 1, 1, 1}, true)).ok());
  REQUIRE(db::put_label(*top, label("T", {0, 0, 0, 0}, true)).ok());
  db::PlaceOptions y;
  y.id = "y_1";
  add_use(*mid, *leaf, y);
  db::PlaceOptions x;
  x.id = "x_0";
  add_use(*top, *mid, x);
  auto flat = db::flatten_to_new(lib, *top, "flat", *tech);
  REQUIRE(flat.ok());
  CHECK(label_texts(*flat.value()) == std::vector<std::string>{"T", "x_0/y_1.A"});
  CHECK(flat.value()->labels()[0].is_port());       // top-level ports kept
  CHECK_FALSE(flat.value()->labels()[1].is_port());
  CHECK(flat.value()->uses().empty());
  CHECK(db::flatten_to_new(lib, *top, "flat", *tech).error().code == db::codes::kCellExists);
  db::FlattenOptions top_only;
  top_only.labels = db::FlattenLabels::top_only;
  CHECK(label_texts(*db::flatten_to_new(lib, *top, "flat2", *tech, top_only).value()) ==
        std::vector<std::string>{"T"});
}

TEST_CASE("no-subcircuit flatten keeps ported children as instances", HI "[req:Descend rules]") {
  const auto tech = tech::builtin_technology();
  db::CellLibrary lib;
  auto* top = lib.create("top", tech->plane_count()).value();
  auto* a = lib.create("a", tech->plane_count()).value();
  auto* b = lib.create("b", tech->plane_count()).value();
  REQUIRE(db::put_label(*a, label("P", {0, 0, 0, 0}, true)).ok());
  REQUIRE(db::put_label(*b, label("Q", {0, 0, 0, 0})).ok());
  add_use(*top, *a);
  add_use(*top, *b);
  db::FlattenOptions o;
  o.descend = db::Descend::no_subcircuit;
  auto flat = db::flatten_to_new(lib, *top, "flat", *tech, o);
  REQUIRE(flat.ok());
  REQUIRE(flat.value()->uses().size() == 1);
  CHECK(flat.value()->uses()[0]->def == a);
  CHECK(label_texts(*flat.value()) == std::vector<std::string>{"b_0.Q"});
}

TEST_CASE("in-place flatten of an arrayed use", HI "[req:Flattening an instance in place]") {
  const auto tech = tech::builtin_technology();
  db::CellLibrary lib;
  auto* top = lib.create("top", tech->plane_count()).value();
  auto* inv = lib.create("inv", tech->plane_count()).value();
  REQUIRE(db::put_label(*inv, label("out", {1, 1, 1, 1}, true)).ok());
  db::recompute_bbox(*inv);
  db::PlaceOptions o;
  o.array = db::ArrayInfo{0, 1, 0, 1, 10, 10};
  auto* u = add_use(*top, *inv, o);
  SECTION("magic-compat on: [x][y]") {
    REQUIRE(db::flatten_in_place(*u, *tech).ok());
    CHECK(label_texts(*top) ==
          std::vector<std::string>{"inv_0[0][0]/out", "inv_0[1][0]/out", "inv_0[0][1]/out", "inv_0[1][1]/out"});
    CHECK(top->uses().empty());
    for (const auto& l : top->labels()) {
      CHECK_FALSE(l.is_port());
    }
    CHECK(top->labels()[1].rect == Rect{11, 1, 11, 1});
  }
  SECTION("magic-compat off: [y][x] (Q-MODEL-7)") {
    db::FlattenOptions off;
    off.magic_compat = false;
    REQUIRE(db::flatten_in_place(*u, *tech, off).ok());
    CHECK(label_texts(*top)[1] == "inv_0[0][1]/out");
  }
}

TEST_CASE("flattening copies paint through the paint tables", HI "[req:Flattening into a new cell]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* child = lib.create("child", mt.tech->plane_count()).value();
  auto* top = lib.create("top", mt.tech->plane_count()).value();
  REQUIRE(db::paint(*child, *mt.tech, {0, 0, 4, 4}, mt.via1).ok());
  REQUIRE(db::paint(*top, *mt.tech, {100, 0, 120, 4}, mt.m2).ok());
  db::recompute_bbox(*child);
  db::PlaceOptions o;
  o.transform = db::translation(100, 0);
  add_use(*top, *child, o);
  auto flat = db::flatten_to_new(lib, *top, "flat", *mt.tech);
  REQUIRE(flat.ok());
  CHECK(db::test::rects(flat.value()->plane(mt.metal2)) == std::vector<Rect>{{100, 0, 104, 4}, {104, 0, 120, 4}});
  CHECK(flat.value()->plane(mt.metal2).type_at({101, 1}) == mt.via1);
  CHECK(flat.value()->bbox() == Rect{100, 0, 120, 4});
}

TEST_CASE("walking the hierarchy visits array elements with transforms", HI "[req:Arrays]") {
  db::CellLibrary lib;
  auto* top = make(lib, "top");
  auto* child = make(lib, "child");
  db::PlaceOptions o;
  o.array = db::ArrayInfo{0, 1, 0, 0, 10, 0};
  add_use(*top, *child, o);
  std::vector<db::Point> origins;
  db::walk_hierarchy(*top, [&](const db::CellUse&, const db::Transform& t, int) {
    origins.push_back(db::apply(t, db::Point{0, 0}));
    return true;
  });
  CHECK(origins == std::vector<db::Point>{{0, 0}, {10, 0}});
}
