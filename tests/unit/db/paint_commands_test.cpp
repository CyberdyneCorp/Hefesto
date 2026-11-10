// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cmd-paint-erase: layer lists and the paint commands through the
// dispatcher, on the `mt` technology (metal1 + metal2 = via1).

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "edit_session.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/layer_list.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/polygon.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
using db::Point;
using db::Rect;
using db::test::EditSession;
using db::test::rects_of;

#define CAP "[cap:cmd-paint-erase]"

namespace {

struct Paint : EditSession {
  const db::test::MtTech& mt;
  db::CellDef& top;

  explicit Paint(const db::test::MtTech& m) : EditSession(m), mt(m), top(*editor.edit_cell) { ok("units internal"); }

  void put(const Rect& r, tech::TileType t) { REQUIRE(db::paint(top, *mt.tech, r, t).ok()); }
  void put(db::CellDef& def, const Rect& r, tech::TileType t) { REQUIRE(db::paint(def, *mt.tech, r, t).ok()); }

  void label(const std::string& text, const Rect& r, tech::TileType t) {
    db::Label l;
    l.text = text;
    l.rect = r;
    l.type = t;
    REQUIRE(db::put_label(top, l).ok());
  }

  [[nodiscard]] bool has_label(const std::string& text) const {
    for (const db::Label& l : top.labels()) {
      if (l.text == text) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] tech::TileType at(tech::PlaneId p, Point pt) const { return top.plane(p).type_at(pt); }
};

}  // namespace

TEST_CASE("layer list modes, specials and errors", CAP "[req:Layer list syntax]") {
  HEFESTO_REQUIRE_MT(mt);
  const tech::Technology& t = *mt.tech;
  const auto all_but = db::parse_layer_list("*,-metal2", t);
  REQUIRE(all_but.ok());
  CHECK(all_but.value().types.test(mt.poly));
  CHECK(all_but.value().types.test(mt.m1));
  CHECK(all_but.value().types.test(mt.via1));
  CHECK_FALSE(all_but.value().types.test(mt.m2));
  CHECK_FALSE(all_but.value().types.test(db::kLabelsPseudoType));
  CHECK_FALSE(all_but.value().types.test(tech::kErrorP));
  CHECK(db::parse_layer_list("-poly,metal1", t).value().types.empty());
  const auto connect = db::parse_layer_list("metal1,connect", t);
  REQUIRE(connect.ok());
  CHECK(connect.value().types.test(mt.m1));
  CHECK(connect.value().types.test(mt.via1));
  CHECK_FALSE(connect.value().types.test(mt.m3));
  const auto specials = db::parse_layer_list("labels subcell errors", t);
  REQUIRE(specials.ok());
  CHECK(specials.value().labels);
  CHECK(specials.value().subcells);
  CHECK(specials.value().errors);
  const auto bad = db::parse_layer_list("foo", t);
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().text.starts_with("Unrecognized layer: foo\n"));
  CHECK(bad.error().text.find("    connect") != std::string::npos);
  CHECK(db::is_bad_layer_name(bad.error()));
  // Regression (CMDEDIT-PAINT-02): the type names are Magic's TxPrintf part
  // and are not in the error, which lists only the special names.
  CHECK(bad.error().text == "Unrecognized layer: foo\n    $\n    *\n    errors\n    labels\n    subcell\n    connect");
  CHECK_FALSE(db::parse_layer_list("$", t).ok());  // no pointer
  db::LayerListContext under;
  under.under_pointer = tech::TypeMask::of(mt.poly) | tech::TypeMask::of(tech::kErrorP);
  CHECK(db::parse_layer_list("$", t, under).value().types == tech::TypeMask::of(mt.poly));
}

TEST_CASE("paint changes mark the cell, fix labels and clear the selection",
          CAP "[req:Common effects of edit-cell paint changes]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.editor.selection.set_root(&s.top, mt.tech->plane_count());
  REQUIRE(db::paint(*s.editor.selection.def(), *mt.tech, Rect{0, 0, 1, 1}, mt.m1).ok());
  s.top.flags().modified = false;
  s.ok("box values 0 0 10 20");
  s.ok("paint metal1");
  CHECK(s.top.flags().modified);
  CHECK(s.editor.selection.empty());
  CHECK(s.top.bbox() == Rect{0, 0, 10, 20});
}

TEST_CASE("paint paints each type over the box", CAP "[req:Paint command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.ok("box values 0 0 10 20");
  s.ok("paint poly,metal1");
  CHECK(rects_of(s.top, mt.active, mt.poly) == std::vector<Rect>{{0, 0, 10, 20}});
  CHECK(rects_of(s.top, mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 10, 20}});
  const auto labels = s.run("paint labels");
  REQUIRE_FALSE(labels.ok());
  CHECK(labels.diagnostics.back().text == "Label layer cannot be painted.  Use the \"label\" command");  // two spaces (CMDEDIT-PAINT-02)
  CHECK_FALSE(s.run("paint subcell").ok());
  const auto foo = s.run("paint foo");
  CHECK_FALSE(foo.ok());
  // Regression (CMDEDIT-PAINT-02): "Layer names are:" goes to standard output.
  CHECK(EditSession::texts(foo).find("Layer names are:\n    ndiff\n") != std::string::npos);
  CHECK(s.run("paint a b").diagnostics.back().text == "Usage: paint <layers> | cursor | pick x y");
}

TEST_CASE("paint goes into the edit cell in its coordinates", CAP "[req:Paint command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  db::CellDef* child = s.cell("child");
  s.editor.edit_root = &s.top;
  s.editor.edit_cell = child;
  s.editor.edit_transform = db::translation(100, 0);
  s.ok("box values 100 0 110 10");
  s.ok("paint metal1");
  CHECK(rects_of(*child, mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 10, 10}});
  CHECK(rects_of(s.top, mt.metal1, mt.m1).empty());
}

TEST_CASE("paint and erase by example", CAP "[req:Paint or erase by example (cursor and pick)]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.put(Rect{5, 5, 6, 6}, mt.m1);
  s.put(Rect{5, 5, 6, 6}, mt.poly);
  s.ok("box values 20 0 30 10");
  s.ok("paint pick 5 5");
  CHECK(s.at(mt.metal1, Point{25, 5}) == mt.m1);
  CHECK(s.at(mt.active, Point{25, 5}) == mt.poly);
  s.ok("paint pick 50 50");  // empty example: erase visible, active paint
  CHECK(s.at(mt.metal1, Point{25, 5}) == tech::kSpace);
  CHECK(s.at(mt.active, Point{25, 5}) == tech::kSpace);
  s.ok("box values 0 0 10 10");
  s.ok("erase pick 5 5");
  CHECK(s.at(mt.metal1, Point{5, 5}) == tech::kSpace);
  s.editor.command_point.reset();
  CHECK_FALSE(s.run("paint cursor").ok());
}

TEST_CASE("erase removes paint, labels and subcells", CAP "[req:Erase command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.put(Rect{0, 0, 10, 10}, mt.m1);
  s.put(Rect{0, 0, 10, 10}, mt.poly);
  s.label("A", Rect{2, 2, 2, 2}, mt.m1);
  s.ok("box values 0 0 10 10");
  s.ok("erase");
  CHECK(rects_of(s.top, mt.metal1, mt.m1).empty());
  CHECK(rects_of(s.top, mt.active, mt.poly).empty());
  CHECK_FALSE(s.has_label("A"));
  CHECK(s.run("erase a b").diagnostics.back().text == "Usage: erase [<layers> | cursor | pick x y]");
}

TEST_CASE("erase of one layer keeps labels on surviving paint", CAP "[req:Erase command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.put(Rect{-10, 0, 10, 10}, mt.m1);
  s.put(Rect{0, 0, 10, 10}, mt.poly);
  s.label("B", Rect{2, 2, 2, 2}, mt.m1);
  s.ok("box values 0 0 10 10");
  s.ok("erase poly");
  CHECK(s.has_label("B"));
  s.ok("erase space");  // `space` means `*,labels`
  CHECK_FALSE(s.has_label("B"));
}

TEST_CASE("erase subcell deletes the overlapping uses", CAP "[req:Erase command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  db::CellDef* leaf = s.cell("leaf");
  s.put(*leaf, Rect{0, 0, 4, 4}, mt.m1);
  db::recompute_bbox(*leaf);
  for (const db::Coord x : {0, 6, 100}) {
    db::PlaceOptions po;
    po.transform = db::translation(x, 0);
    REQUIRE(db::place(s.top, *leaf, po).ok());
  }
  s.ok("box values 0 0 10 10");
  s.ok("erase subcell");
  REQUIRE(s.top.uses().size() == 1);
  CHECK(s.top.uses().front()->bbox.x0 == 100);
}

namespace {

// `mt` with the stackable alias `v12` for via1+via2 (golden CMDEDIT-PAINT-09).
std::shared_ptr<const tech::Technology> mt_with_v12() {
  tech::TechnologyBuilder b;
  b.set_name("mtv");
  bool ok = b.add_plane("metal1").ok() && b.add_plane("metal2").ok() && b.add_plane("metal3").ok();
  ok = ok && b.add_type("metal1", "metal1").ok() && b.add_type("metal2", "metal2").ok() &&
       b.add_type("metal3", "metal3").ok() && b.add_type("metal1", "via1").ok() && b.add_type("metal2", "via2").ok();
  const std::vector<std::string_view> v1{"metal1", "metal2"};
  const std::vector<std::string_view> v2{"metal2", "metal3"};
  const std::vector<std::string_view> stack{"via1", "via2", "v12"};
  ok = ok && b.add_contact("via1", v1).ok() && b.add_contact("via2", v2).ok() && b.add_stackable(stack).ok();
  if (!ok) {
    return nullptr;
  }
  auto t = b.finalize();
  return t.ok() ? t.value() : nullptr;
}

}  // namespace

TEST_CASE("named stacked contacts follow magic-compat", CAP "[req:Erase command]") {
  HEFESTO_REQUIRE_MT(mt);
  const auto tech = mt_with_v12();
  if (tech == nullptr || !tech->find_type("v12").ok()) {
    SKIP("the tech contract cannot build a stackable alias");
  }
  const tech::PlaneId metal2 = tech->find_plane("metal2").id.value();
  for (const bool compat : {true, false}) {
    Paint s(mt);
    s.editor.technology = tech;
    s.compat(compat);
    s.ok("box values 0 0 10 10");
    s.ok("paint v12");
    const auto painted = s.top.plane(metal2).type_at(Point{5, 5});
    CHECK(painted != tech::kSpace);
    s.ok("erase v12");
    const bool left = s.top.plane(metal2).type_at(Point{5, 5}) == painted;
    CHECK(left == compat);
  }
}

namespace {

// The contacts of the golden `tiny` technology: pc and ndc on the active
// plane and the stacked type pm12c = pc+m2c.
std::shared_ptr<const tech::Technology> tiny_contacts() {
  tech::TechnologyBuilder b;
  b.set_name("tinyc");
  bool ok = b.add_plane("active").ok() && b.add_plane("metal1").ok() && b.add_plane("metal2").ok();
  ok = ok && b.add_type("active", "poly").ok() && b.add_type("active", "ndiff").ok() &&
       b.add_type("metal1", "metal1").ok() && b.add_type("metal2", "metal2").ok() && b.add_type("active", "pc").ok() &&
       b.add_type("active", "ndc").ok() && b.add_type("metal1", "m2c").ok();
  const std::vector<std::string_view> pc{"poly", "metal1"};
  const std::vector<std::string_view> ndc{"ndiff", "metal1"};
  const std::vector<std::string_view> m2c{"metal1", "metal2"};
  const std::vector<std::string_view> stack{"pc", "m2c", "pm12c"};
  ok = ok && b.add_contact("pc", pc).ok() && b.add_contact("ndc", ndc).ok() && b.add_contact("m2c", m2c).ok() &&
       b.add_stackable(stack).ok();
  if (!ok) {
    return nullptr;
  }
  auto t = b.finalize();
  return t.ok() ? t.value() : nullptr;
}

}  // namespace

// Regression (CMDEDIT-PAINT-02): a stacked type in the layer list adds its
// residues to the set, and the set is painted once in type order, so pc is not
// painted again after m2c.
TEST_CASE("paint decomposes stacked types before painting in type order", CAP "[req:Paint command]") {
  HEFESTO_REQUIRE_MT(mt);
  const auto tech = tiny_contacts();
  if (tech == nullptr || !tech->find_type("pm12c").ok()) {
    SKIP("the tech contract cannot build a stackable alias");
  }
  const tech::PlaneId active = tech->find_plane("active").id.value();
  Paint s(mt);
  s.editor.technology = tech;
  s.ok("box values 0 0 10 10");
  s.ok("paint pc,ndc,pm12c");
  // pc, ndc, then m2c: m2c on metal1 breaks ndc down to ndiff, as in Magic.
  // Painting pc again for pm12c would leave pc.
  CHECK(s.top.plane(active).type_at(Point{5, 5}) == *tech->find_type("ndiff").id);
}

TEST_CASE("a point box erases the labels it touches", CAP "[req:Erase command]") {
  HEFESTO_REQUIRE_MT(mt);
  for (const bool compat : {true, false}) {
    Paint s(mt);
    s.compat(compat);
    s.put(Rect{0, 0, 4, 4}, mt.m1);
    s.label("A", Rect{0, 0, 4, 4}, mt.m1);
    s.ok("box values 4 2 4 2");
    s.ok("erase labels");
    CHECK_FALSE(s.has_label("A"));
  }
}

TEST_CASE("fill extends paint entering the box", CAP "[req:Fill command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.put(Rect{2, -5, 4, 0}, mt.poly);
  s.put(Rect{6, 5, 8, 8}, mt.m1);
  s.ok("box values 0 0 10 20");
  s.ok("fill n metal1");
  CHECK(s.at(mt.active, Point{3, 10}) == tech::kSpace);
  s.ok("fill up");
  CHECK(s.at(mt.active, Point{3, 0}) == mt.poly);
  CHECK(s.at(mt.active, Point{3, 19}) == mt.poly);
  CHECK(rects_of(s.top, mt.metal1, mt.m1) == std::vector<Rect>{{6, 5, 8, 8}});
  const auto diag = s.run("fill ne");
  REQUIRE_FALSE(diag.ok());
  CHECK(diag.diagnostics.back().text.starts_with("\"ne\" is not a Manhattan direction or position."));
}

TEST_CASE("fill in each direction", CAP "[req:Fill command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.put(Rect{2, 20, 4, 25}, mt.m1);   // enters through the top: fill s
  s.put(Rect{-5, 2, 0, 4}, mt.m2);    // enters through the left: fill e
  s.put(Rect{10, 6, 15, 8}, mt.m3);   // enters through the right: fill w
  s.ok("box values 0 0 10 20");
  s.ok("fill s");
  s.ok("fill e");
  s.ok("fill w");
  CHECK(s.at(mt.metal1, Point{3, 0}) == mt.m1);
  CHECK(s.at(mt.metal2, Point{9, 3}) == mt.m2);
  CHECK(s.at(mt.metal3, Point{0, 7}) == mt.m3);
}

TEST_CASE("corner paints L-shaped wires", CAP "[req:Corner command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.put(Rect{4, -10, 8, 0}, mt.m1);
  s.ok("box values 0 0 20 20");
  s.ok("corner n e");
  CHECK(s.at(mt.metal1, Point{5, 0}) == mt.m1);
  CHECK(s.at(mt.metal1, Point{5, 15}) == mt.m1);
  CHECK(s.at(mt.metal1, Point{19, 13}) == mt.m1);
  CHECK(s.at(mt.metal1, Point{5, 17}) == tech::kSpace);
  CHECK(s.at(mt.metal1, Point{19, 11}) == tech::kSpace);
  const auto r = s.ok("corner n s");
  CHECK(EditSession::texts(r).find("Can't corner-fill n and then s.") != std::string::npos);
}

TEST_CASE("corner reports when the wires do not fit", CAP "[req:Corner command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.put(Rect{0, -10, 15, 0}, mt.m1);  // 15 wide wire in a 20 x 10 box
  s.ok("box values 0 0 20 10");
  const auto r = s.ok("corner n e");
  CHECK(EditSession::texts(r).find("There's not enough room in the box for all the wires.") != std::string::npos);
}

TEST_CASE("bevel corners paint into the edit cell", CAP "[req:Corner command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  db::CellDef* child = s.cell("child");
  s.put(Rect{104, -10, 108, 0}, mt.m1);
  s.editor.edit_root = &s.top;
  s.editor.edit_cell = child;
  s.editor.edit_transform = db::translation(100, 0);
  child->flags().modified = false;
  s.ok("box values 100 0 120 20");
  s.ok("corner n e metal1 bevel");
  CHECK(child->flags().modified);
  CHECK(child->plane(mt.metal1).type_at(Point{5, 1}) == mt.m1);    // first leg
  CHECK(child->plane(mt.metal1).type_at(Point{19, 13}) == mt.m1);  // second leg
  CHECK(s.top.plane(mt.metal1).type_at(Point{105, 5}) == tech::kSpace);
}

TEST_CASE("contact paints where the residues overlap", CAP "[req:Contact command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.put(Rect{0, 0, 6, 10}, mt.m1);
  s.put(Rect{3, 0, 10, 10}, mt.m2);
  s.ok("box values 0 0 10 10");
  s.ok("contact via1");
  CHECK(rects_of(s.top, mt.metal1, mt.via1) == std::vector<Rect>{{3, 0, 6, 10}});
  s.ok("contact erase via1");
  CHECK(rects_of(s.top, mt.metal1, mt.via1).empty());
  CHECK(s.at(mt.metal1, Point{4, 5}) == mt.m1);
  CHECK(s.at(mt.metal2, Point{4, 5}) == mt.m2);
  const auto r = s.run("contact metal1");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().text == "Error:  tile type \"metal1\" is not a contact.");  // two spaces (CMDEDIT-PAINT-11)
  CHECK(s.run("contact x via1").diagnostics.back().text == "Usage: contact erase <contact_type>");
}

TEST_CASE("polygon decomposes into rectangles and triangles", CAP "[req:Polygon command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.ok("polygon metal1 0 0 10 0 0 10");
  const auto tiles = db::test::tiles(s.top.plane(mt.metal1));
  REQUIRE(tiles.size() == 1);
  CHECK(tiles[0].split);
  CHECK(tiles[0].area == Rect{0, 0, 10, 10});
  CHECK(s.at(mt.metal1, Point{1, 1}) == mt.m1);
  CHECK(s.at(mt.metal1, Point{8, 8}) == tech::kSpace);
  CHECK(s.run("polygon metal1 0 0 10 0 0 10 5").diagnostics.back().text == "Unpaired coordinate value");
  CHECK_FALSE(s.run("polygon metal1 0 0 1 0").ok());
  s.ok("polygon metal2 0 0 20 0 20 10 10 20 0 10");
  CHECK(s.at(mt.metal2, Point{1, 1}) == mt.m2);
  CHECK(s.at(mt.metal2, Point{10, 15}) == mt.m2);
  CHECK(s.at(mt.metal2, Point{18, 18}) == tech::kSpace);
}

TEST_CASE("the shared decomposer handles self-intersecting paths", CAP "[req:Polygon command]") {
  const std::vector<Point> square{{0, 0}, {0, 10}, {10, 10}, {10, 0}};
  const auto sq = db::decompose_polygon(square);
  REQUIRE(sq.ok());
  REQUIRE(sq.value().size() == 1);
  CHECK(sq.value()[0] == db::PolygonPiece{Rect{0, 0, 10, 10}, std::nullopt});
  const std::vector<Point> bowtie{{0, 10}, {10, 0}, {10, 10}, {0, 0}};
  const auto bt = db::decompose_polygon(bowtie);
  REQUIRE(bt.ok());
  CHECK_FALSE(bt.value().empty());
  CHECK_FALSE(db::decompose_polygon(std::vector<Point>{{0, 0}, {1, 1}}).ok());
}

TEST_CASE("splitpaint paints the two triangles", CAP "[req:Splitpaint command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.ok("box values 0 0 10 10");
  s.ok("splitpaint ne metal1 metal2");
  CHECK(s.at(mt.metal1, Point{8, 8}) == mt.m1);
  CHECK(s.at(mt.metal1, Point{1, 1}) == tech::kSpace);
  CHECK(s.at(mt.metal2, Point{1, 1}) == mt.m2);
}

TEST_CASE("splitpaint with a Manhattan direction follows magic-compat", CAP "[req:Splitpaint command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint on(mt);
  on.ok("box values 0 0 10 10");
  const auto r = on.ok("splitpaint east metal1");
  CHECK(EditSession::texts(r).find("Direction \"east\" is not a corner; using ne") != std::string::npos);
  CHECK(on.at(mt.metal1, Point{8, 8}) == mt.m1);
  Paint off(mt);
  off.compat(false);
  off.ok("box values 0 0 10 10");
  const auto f = off.run("splitpaint east metal1");
  REQUIRE_FALSE(f.ok());
  CHECK(f.diagnostics.back().text == "Direction \"east\" is not a corner; use ne, nw, se or sw");
  CHECK(rects_of(off.top, mt.metal1, mt.m1).empty());
}

TEST_CASE("spliterase erases one triangle", CAP "[req:Spliterase command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.put(Rect{0, 0, 10, 10}, mt.m1);
  s.ok("box values 0 0 10 10");
  s.ok("spliterase sw metal1");
  CHECK(s.at(mt.metal1, Point{8, 8}) == mt.m1);
  CHECK(s.at(mt.metal1, Point{1, 1}) == tech::kSpace);
}

TEST_CASE("drop paints into the cells owning the selected paint", CAP "[req:Drop command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  db::CellDef* child = s.cell("child");
  s.put(*child, Rect{0, 0, 10, 10}, mt.m2);
  db::recompute_bbox(*child);
  REQUIRE(db::place(s.top, *child, db::PlaceOptions{}).ok());
  s.editor.selection.set_root(&s.top, mt.tech->plane_count());
  REQUIRE(db::paint(*s.editor.selection.def(), *mt.tech, Rect{0, 0, 10, 10}, mt.m2).ok());
  s.ok("box values 0 0 10 10");
  s.ok("drop metal3");
  CHECK(rects_of(*child, mt.metal3, mt.m3) == std::vector<Rect>{{0, 0, 10, 10}});
  CHECK(rects_of(s.top, mt.metal3, mt.m3).empty());
  CHECK_FALSE(s.editor.selection.empty());  // drop keeps the selection
}

TEST_CASE("drop refuses a foreign selection and non-editable cells", CAP "[req:Drop command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  db::CellDef* child = s.cell("child");
  s.put(*child, Rect{0, 0, 10, 10}, mt.m2);
  db::recompute_bbox(*child);
  REQUIRE(db::place(s.top, *child, db::PlaceOptions{}).ok());
  db::CellDef* other = s.cell("other");
  s.editor.selection.set_root(other, mt.tech->plane_count());
  REQUIRE(db::paint(*s.editor.selection.def(), *mt.tech, Rect{0, 0, 10, 10}, mt.m2).ok());
  s.ok("box values 0 0 10 10");
  CHECK(s.run("drop metal3").diagnostics.back().text == "The selection does not match the edit cell.");
  s.editor.selection.set_root(&s.top, mt.tech->plane_count());
  REQUIRE(db::paint(*s.editor.selection.def(), *mt.tech, Rect{0, 0, 10, 10}, mt.m2).ok());
  child->flags().no_edit = true;
  CHECK(s.run("drop metal3").diagnostics.back().text == "Cannot drop into non-editable cell child");
  CHECK(rects_of(*child, mt.metal3, mt.m3).empty());
}

// Regression (CMDEDIT-PAINT-15): Magic's CmdDrop compares the edit cell
// itself with the selection root, so a child edited in place is refused.
TEST_CASE("drop refuses when the edit cell is a child of the selection root", CAP "[req:Drop command]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  db::CellDef* child = s.cell("child");
  s.put(*child, Rect{0, 0, 10, 10}, mt.m2);
  db::recompute_bbox(*child);
  auto use = db::place(s.top, *child, db::PlaceOptions{});
  REQUIRE(use.ok());
  s.editor.selection.set_root(&s.top, mt.tech->plane_count());
  REQUIRE(db::paint(*s.editor.selection.def(), *mt.tech, Rect{0, 0, 10, 10}, mt.m2).ok());
  s.ok("box values 0 0 10 10");
  s.editor.edit_root = &s.top;
  s.editor.edit_cell = child;
  CHECK(s.run("drop metal3").diagnostics.back().text == "The selection does not match the edit cell.");
  CHECK(rects_of(*child, mt.metal3, mt.m3).empty());
}

TEST_CASE("paint commands without an edit cell", CAP "[req:Paint commands in a non-editable context]") {
  HEFESTO_REQUIRE_MT(mt);
  Paint s(mt);
  s.ok("box values 0 0 10 10");
  s.editor.edit_cell = nullptr;
  CHECK(s.run("polygon metal1 0 0 1 0 0 1").diagnostics.back().text == "No cell being edited");
  CHECK(s.run("paint metal1").diagnostics.back().text == "The cell is not editable.");
  CHECK(s.run("erase metal1").diagnostics.back().text == "No cell def being edited!");
  CHECK(s.run("contact via1").diagnostics.back().text == "The cell in the window is not editable.");
}
