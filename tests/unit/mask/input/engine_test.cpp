// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The read engine behind ReadSink (capability mask-input-styles; tasks.md
// 7.3): per-cell processing, the input operators, painting and unit
// conversion with grid subdivision, the mid-read rescale, labels, polygons,
// copyup, boundary and mask-hints.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/hints/hints.hpp"
#include "input_fixture.hpp"

namespace mask = hefesto::mask;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
using mask::Snap;
using mask::test::Session;

namespace {

/// Paints `rects` on the CIF layers named, as one cell `a`.
struct Shape {
  std::string layer;
  mask::ReadShape shape;
};

void read_cell(Session& s, mask::ReadEngine& e, const std::string& cell, const std::vector<Shape>& shapes) {
  REQUIRE(e.begin_cell(cell).ok());
  for (const Shape& sh : shapes) {
    const auto layer = e.style().find_cif_layer(sh.layer);
    REQUIRE(layer.has_value());
    REQUIRE(e.paint(*layer, sh.shape).ok());
  }
  REQUIRE(e.end_cell().ok());
  (void)s;
}

void read_one(Session& s, const std::vector<Shape>& shapes, mask::ReadEngine::Options options = {}) {
  auto e = s.engine(options);
  read_cell(s, e, "a", shapes);
  REQUIRE(e.finish().ok());
}

/// The bounding boxes of the split tiles of `type` in `cell`.
std::vector<db::Rect> split_areas(const Session& s, const std::string& cell, const std::string& type) {
  const db::CellDef* def = s.editor.cells.find(cell);
  REQUIRE(def != nullptr);
  const auto t = s.type(type);
  tech::TypeMask mask_of;
  mask_of.set(t);
  std::vector<db::Rect> out;
  def->plane(*s.editor.technology->home_plane(t)).enumerate(db::whole_plane(), mask_of, [&](const db::TileView& v) {
    if (v.split) {
      out.push_back(v.area);
    }
    return true;
  });
  return out;
}

bool has_split(const Session& s, const std::string& cell, const std::string& type) {
  return !split_areas(s, cell, type).empty();
}

std::vector<std::string> codes(Session& s) {
  std::vector<std::string> out;
  for (const auto& d : s.messages.take()) {
    out.push_back(d.code);
  }
  return out;
}

bool contains(const std::vector<std::string>& v, const std::string& x) {
  return std::find(v.begin(), v.end(), x) != v.end();
}

using Rects = std::vector<db::Rect>;

Rects sorted(Rects r) {
  std::sort(r.begin(), r.end(), [](const db::Rect& a, const db::Rect& b) {
    return std::tie(a.y0, a.x0, a.y1, a.x1) < std::tie(b.y0, b.x0, b.y1, b.x1);
  });
  return r;
}

}  // namespace

TEST_CASE("a later rule overrides earlier paint", "[cap:mask-input-styles][req:Per-cell layer processing order]") {
  Session s("style a\nscalefactor 100\nlayer ndiff CSN\nand CAA\nlayer nfet CPG\nand CAA\nand CSN");
  read_one(s, {{"CAA", db::Rect{0, 0, 1000, 400}}, {"CSN", db::Rect{0, 0, 1000, 400}},
               {"CPG", db::Rect{400, 0, 600, 400}}});
  CHECK(sorted(s.painted("a", "ndiff")) == Rects{{0, 0, 4, 4}, {6, 0, 10, 4}});
  CHECK(s.painted("a", "nfet") == Rects{{4, 0, 6, 4}});
}

TEST_CASE("a templayer replaces its input layer for later rules",
          "[cap:mask-input-styles][req:Layer and templayer rules]") {
  Session s("style a\nscalefactor 1\ntemplayer CAA CAA\nshrink 10\nlayer ndiff CAA");
  read_one(s, {{"CAA", db::Rect{0, 0, 100, 100}}});
  CHECK(s.painted("a", "ndiff") == Rects{{10, 10, 90, 90}});
}

TEST_CASE("and-not removes overlap", "[cap:mask-input-styles][req:Input operators or, and, and-not]") {
  Session s("style a\nscalefactor 50\nlayer poly CPG\nand-not CAA\nlayer m1 X\nor CPG,CAA");
  read_one(s, {{"CPG", db::Rect{0, 0, 100, 100}}, {"CAA", db::Rect{50, 0, 150, 100}}});
  CHECK(s.painted("a", "poly") == Rects{{0, 0, 1, 2}});
  CHECK(s.painted("a", "m1") == Rects{{0, 0, 3, 2}});
}

TEST_CASE("grow a contact cut to its Magic size", "[cap:mask-input-styles][req:Input operators grow and shrink]") {
  Session s("style a\nscalefactor 100\nlayer ndc CCA\ngrow 100\nand CAA");
  read_one(s, {{"CCA", db::Rect{100, 100, 300, 300}}, {"CAA", db::Rect{0, 0, 400, 400}}});
  CHECK(s.painted("a", "ndc") == Rects{{0, 0, 4, 4}});
}

TEST_CASE("input grow follows the output style's grow-euclidean with magic-compat ON (Q-MASK-306)",
          "[cap:mask-input-styles][req:Input operators grow and shrink]") {
  const char* input = "style a\nscalefactor 1\nlayer m1 A\ngrow 2";
  const char* output = "style out\nscalefactor 1\noptions grow-euclidean\nlayer CMF m1";
  const mask::Polygon triangle{{0, 0}, {20, 20}, {20, 0}};
  Session on(input, true, output);
  read_one(on, {{"A", triangle}});
  CHECK(split_areas(on, "a", "m1") == std::vector<db::Rect>{{-2, 1, 19, 22}});  // Euclidean
  CHECK(contains(codes(on), "MASK-INPUT-014"));
  Session off(input, false, output);
  read_one(off, {{"A", triangle}});
  CHECK(split_areas(off, "a", "m1") == std::vector<db::Rect>{{-2, 2, 18, 22}});  // square
  CHECK(contains(codes(off), "MASK-INPUT-014"));
}

TEST_CASE("grow-grid on positive coordinates (Q-MASK-334)", "[cap:mask-input-styles][req:Input operator grow-grid]") {
  const char* input = "style a\nscalefactor 1\nlayer m1 A\ngrow-grid 5";
  Session on(input, true);
  read_one(on, {{"A", db::Rect{7, 3, 12, 8}}});
  CHECK(on.painted("a", "m1") == Rects{{5, 0, 14, 11}});
  Session off(input, false);
  read_one(off, {{"A", db::Rect{7, 3, 12, 8}}});
  CHECK(off.painted("a", "m1") == Rects{{5, 0, 15, 10}});
}

TEST_CASE("not-square removes isolated squares", "[cap:mask-input-styles][req:Input operator not-square]") {
  Session s("style a\nscalefactor 1\nlayer m1 A\nnot-square");
  read_one(s, {{"A", db::Rect{0, 0, 20, 20}}, {"A", db::Rect{100, 0, 140, 20}}});
  CHECK(s.painted("a", "m1") == Rects{{100, 0, 140, 20}});
}

TEST_CASE("not-square drops triangles with magic-compat ON and keeps them OFF (Q-MASK-330)",
          "[cap:mask-input-styles][req:Input operator not-square]") {
  const char* input = "style a\nscalefactor 1\nlayer m1 A\nnot-square";
  const mask::Polygon triangle{{0, 0}, {10, 10}, {10, 0}};
  Session on(input, true);
  read_one(on, {{"A", triangle}});
  CHECK(on.painted("a", "m1").empty());
  Session off(input, false);
  read_one(off, {{"A", triangle}});
  CHECK(has_split(off, "a", "m1"));
}

TEST_CASE("well geometry copied up to the parent", "[cap:mask-input-styles][req:Input operator copyup]") {
  Session s("style a\nscalefactor 100\ntemplayer NW_UP NWELL\ncopyup NWELL\nlayer nwell NWELL\nlayer m1 CMF");
  auto e = s.engine();
  read_cell(s, e, "A", {{"NWELL", db::Rect{0, 0, 100, 100}}, {"CMF", db::Rect{0, 0, 100, 100}}});
  REQUIRE(e.saved_planes(*s.editor.cells.find("A")) != nullptr);
  REQUIRE(e.begin_cell("TOP").ok());
  REQUIRE(e.call("A", db::translation(1000, 0), db::ArrayInfo{}).ok());
  REQUIRE(e.end_cell().ok());
  REQUIRE(e.finish().ok());
  CHECK(s.painted("A", "nwell") == Rects{{0, 0, 1, 1}});
  CHECK(s.painted("TOP", "nwell") == Rects{{10, 0, 11, 1}});
  const db::CellDef* top = s.editor.cells.find("TOP");
  REQUIRE(top->uses().size() == 1);
  CHECK(top->uses()[0]->transform == db::translation(10, 0));
}

TEST_CASE("copyup into an array visits every element", "[cap:mask-input-styles][req:Input operator copyup]") {
  Session s("style a\nscalefactor 1\ntemplayer UP NWELL\ncopyup NWELL\nlayer nwell NWELL");
  auto e = s.engine();
  read_cell(s, e, "A", {{"NWELL", db::Rect{0, 0, 10, 10}}});
  REQUIRE(e.begin_cell("TOP").ok());
  REQUIRE(e.call("A", db::kIdentity, db::ArrayInfo{0, 1, 0, 0, 20, 0}).ok());
  REQUIRE(e.end_cell().ok());
  REQUIRE(e.finish().ok());
  CHECK(sorted(s.painted("TOP", "nwell")) == Rects{{0, 0, 10, 10}, {20, 0, 30, 10}});
}

TEST_CASE("CIF calls do not copy up with magic-compat ON (Q-MASK-309)",
          "[cap:mask-input-styles][req:Input operator copyup]") {
  Session s("style a\nscalefactor 1\ntemplayer UP NWELL\ncopyup NWELL\nlayer nwell NWELL");
  mask::ReadEngine::Options cif;
  cif.copyup_on_call = false;  // what the CIF reader passes with magic-compat ON
  auto e = s.engine(cif);
  read_cell(s, e, "A", {{"NWELL", db::Rect{0, 0, 10, 10}}});
  REQUIRE(e.begin_cell("TOP").ok());
  REQUIRE(e.call("A", db::kIdentity, db::ArrayInfo{}).ok());
  REQUIRE(e.end_cell().ok());
  REQUIRE(e.finish().ok());
  CHECK(s.painted("TOP", "nwell").empty());
  CHECK(s.editor.cells.find("TOP")->uses().size() == 1);
}

TEST_CASE("a GDS boundary layer sets FIXED_BBOX", "[cap:mask-input-styles][req:Input operator boundary]") {
  Session s("style a\nscalefactor 100\ntemplayer BND BOUNDARY\nboundary");
  read_one(s, {{"BOUNDARY", db::Rect{0, 0, 500, 300}}});
  const db::CellDef* a = s.editor.cells.find("a");
  CHECK(a->flags().fixed_bbox);
  const auto* value = a->properties().find("FIXED_BBOX");
  REQUIRE(value != nullptr);
  CHECK(std::get<db::DimensionList>(*value).values == std::vector<std::int32_t>{0, 0, 5, 3});
  // A different boundary on a re-read warns.
  read_one(s, {{"BOUNDARY", db::Rect{0, 0, 600, 300}}});
  CHECK(contains(codes(s), "MASK-INPUT-013"));
}

TEST_CASE("boundary in a layer rule ORs FIXED_BBOX with magic-compat ON only (Q-MASK-307)",
          "[cap:mask-input-styles][req:Input operator boundary]") {
  for (const bool magic : {true, false}) {
    Session s("style a\nscalefactor 1\nlayer m1 A\nboundary", magic, "style out\nscalefactor 1\nlayer CMF m1");
    auto created = s.editor.cells.create("a", s.editor.technology->plane_count());
    REQUIRE(created.ok());
    db::set_property(*created.value(), "FIXED_BBOX", db::DimensionList{{0, 0, 2, 2}});
    read_one(s, {});
    if (magic) {
      CHECK(s.painted("a", "m1") == Rects{{0, 0, 2, 2}});
    } else {
      CHECK(s.painted("a", "m1").empty());
    }
  }
}

TEST_CASE("hint geometry stored as a property", "[cap:mask-input-styles][req:Input operator mask-hints]") {
  // With magic-compat ON a templayer's mask-hints also ORs the cell's existing hint, scaled by the output
  // style (Q-MASK-307); the output scale equals the input scale here, as the scenario assumes.
  Session s("style a\nscalefactor 100\ntemplayer HM POLYHINT\nmask-hints poly\ntemplayer HM2 POLYHINT2\n"
            "mask-hints poly",
            true, "style out\nscalefactor 100\nlayer CMF m1");
  SECTION("one rule") {
    read_one(s, {{"POLYHINT", db::Rect{200, 0, 400, 100}}});
    const auto* hint = hefesto::hints::find_hint(*s.editor.cells.find("a"), "poly");
    REQUIRE(hint != nullptr);
    CHECK(hint->strips() == Rects{{2, 0, 4, 1}});
  }
  SECTION("a repeated hint name unions (Q-MASK-308)") {
    read_one(s, {{"POLYHINT", db::Rect{200, 0, 400, 100}}, {"POLYHINT2", db::Rect{400, 0, 500, 100}}});
    const auto* hint = hefesto::hints::find_hint(*s.editor.cells.find("a"), "poly");
    REQUIRE(hint != nullptr);
    CHECK(hint->strips() == Rects{{2, 0, 5, 1}});
  }
}

TEST_CASE("repeated hint names union with magic-compat OFF, where mask-hints has no output meaning (Q-MASK-307)",
          "[cap:mask-input-styles][req:Input operator mask-hints]") {
  Session s("style a\nscalefactor 100\ntemplayer HM POLYHINT\nmask-hints poly\ntemplayer HM2 POLYHINT2\n"
            "mask-hints poly\nlayer m1 X\nmask-hints poly",
            false);
  read_one(s, {{"POLYHINT", db::Rect{200, 0, 400, 100}}, {"POLYHINT2", db::Rect{400, 0, 500, 100}}});
  const auto* hint = hefesto::hints::find_hint(*s.editor.cells.find("a"), "poly");
  REQUIRE(hint != nullptr);
  CHECK(hint->strips() == Rects{{2, 0, 5, 1}});
  CHECK(s.painted("a", "m1").empty());  // the layer rule's mask-hints contributes nothing
}

TEST_CASE("a tagged region selected by label", "[cap:mask-input-styles][req:Input operator tagged]") {
  Session t("style a\nscalefactor 1\nlayer m1\ntagged VDD CMF");
  auto e = t.engine();
  REQUIRE(e.begin_cell("a").ok());
  const auto cmf = e.style().find_cif_layer("CMF");
  REQUIRE(cmf.has_value());
  REQUIRE(e.paint(*cmf, db::Rect{0, 0, 10, 10}).ok());
  REQUIRE(e.paint(*cmf, db::Rect{20, 0, 30, 10}).ok());
  mask::ReadLabel vdd;
  vdd.text = "VDD";
  vdd.area = db::Rect{5, 5, 5, 5};
  REQUIRE(e.label(vdd).ok());
  REQUIRE(e.end_cell().ok());
  REQUIRE(e.finish().ok());
  CHECK(t.painted("a", "m1") == Rects{{0, 0, 10, 10}});
}

TEST_CASE("tagged with a read scale of 10 (Q-MASK-310)", "[cap:mask-input-styles][req:Input operator tagged]") {
  for (const bool magic : {true, false}) {
    Session s("style a\nscalefactor 10\nlayer m1\ntagged VDD CMF", magic);
    auto e = s.engine();
    REQUIRE(e.begin_cell("a").ok());
    const auto cmf = e.style().find_cif_layer("CMF");
    REQUIRE(e.paint(*cmf, db::Rect{0, 0, 100, 100}).ok());
    REQUIRE(e.paint(*cmf, db::Rect{200, 0, 300, 100}).ok());
    mask::ReadLabel vdd;
    vdd.text = "VDD";
    vdd.area = db::Rect{250, 50, 250, 50};  // (25, 5) in internal units
    REQUIRE(e.label(vdd).ok());
    REQUIRE(e.end_cell().ok());
    REQUIRE(e.finish().ok());
    CHECK(s.painted("a", "m1") == (magic ? Rects{{0, 0, 10, 10}} : Rects{{20, 0, 30, 10}}));
    CHECK(contains(codes(s), "MASK-INPUT-015"));
  }
}

TEST_CASE("painting rule results", "[cap:mask-input-styles][req:Painting rule results]") {
  SECTION("a contact on half lambda is expanded outward") {
    Session s("style a\nscalefactor 100\nlayer ndc CCA");
    read_one(s, {{"CCA", db::Rect{50, 50, 250, 250}}});
    CHECK(s.painted("a", "ndc") == Rects{{0, 0, 3, 3}});
    CHECK(s.editor.grid == hefesto::tech::GridScale{1, 1});
  }
  SECTION("an off-grid non-contact subdivides the grid") {
    Session s("style a\nscalefactor 100\nlayer m1 CMF");
    auto other = s.editor.cells.create("other", s.editor.technology->plane_count());
    REQUIRE(other.ok());
    other.value()->flags().available = true;
    REQUIRE(db::paint_type(*other.value(), *s.editor.technology, db::Rect{0, 0, 1, 1}, s.type("m1")).ok());
    auto e = s.engine();
    read_cell(s, e, "a", {{"CMF", db::Rect{0, 0, 150, 100}}});
    REQUIRE(e.finish().ok());
    const auto messages = s.messages.take();
    REQUIRE_FALSE(messages.empty());
    CHECK(messages.front().text == "Input off lambda grid by 1/2; grid redefined.");
    CHECK(s.painted("a", "m1") == Rects{{0, 0, 3, 2}});
    CHECK(s.painted("other", "m1") == Rects{{0, 0, 2, 2}});  // existing geometry doubled
    CHECK(e.style().scale_factor == 50);
    CHECK(s.editor.grid.internal == 2);
  }
  SECTION("a degenerate area is skipped") {
    Session s("style a\nscalefactor 100\ngridlimit 1000\nlayer m1 CMF");
    read_one(s, {{"CMF", db::Rect{0, 0, 20, 100}}});
    CHECK(s.painted("a", "m1").empty());
  }
}

TEST_CASE("conversion of read units to internal units",
          "[cap:mask-input-styles][req:Conversion of read units to internal units]") {
  SECTION("snapping rounds ties downward") {
    Session s("style a\nscalefactor 100\nlayer m1 A");
    auto e = s.engine();
    CHECK(e.scale_coord(50, Snap::any) == 0);
    CHECK(e.scale_coord(51, Snap::any) == 1);
    CHECK(e.scale_coord(-50, Snap::any) == -1);
    CHECK(e.scale_coord(-51, Snap::any) == -1);
    CHECK(e.scale_coord(300, Snap::exact) == 3);
    const auto messages = s.messages.take();
    REQUIRE(messages.size() == 1);  // once per read
    CHECK(messages[0].text == "Input off lambda grid by 1/2; snapped to grid.");
  }
  SECTION("the grid limit forces snapping") {
    Session s("style a\nscalefactor 100\ngridlimit 1000\nlayer m1 A");
    auto e = s.engine();
    CHECK(e.scale_coord(150, Snap::exact) == 1);
    CHECK(s.editor.grid.internal == 1);
  }
  SECTION("rescaling disallowed snaps") {
    Session s("style a\nscalefactor 100\nlayer m1 A");
    mask::ReadEngine::Options options;
    options.rescale_allowed = false;
    auto e = s.engine(options);
    CHECK(e.scale_coord(150, Snap::exact) == 1);
    CHECK(e.style().scale_factor == 100);
  }
  SECTION("half-up and half-down subdivide beyond the half grid") {
    Session s("style a\nscalefactor 100\nlayer m1 A");
    auto e = s.engine();
    CHECK(e.scale_coord(150, Snap::half_up) == 2);
    CHECK(e.scale_coord(150, Snap::half_down) == 1);
    CHECK(e.style().scale_factor == 100);
    CHECK(e.scale_coord(125, Snap::half_up) == 3);  // den 4: subdivided by 2, S = 50
    CHECK(e.style().scale_factor == 50);
    CHECK(s.editor.grid.internal == 2);
  }
}

TEST_CASE("a half-unit coordinate subdivides read units",
          "[cap:mask-input-styles][req:Mid-read subdivision of read units]") {
  // DS 1 1 2 with M = 1: N = 1, Dn = 2; the coordinate 3.
  const mask::ReadUnitValue v = mask::subdivide_read_unit(3, 1, 2, 100);
  CHECK(v.factor == 2);
  CHECK(v.value == 3);
  CHECK_FALSE(v.rounded);
  Session s("style a\nscalefactor 100\nlayer m1 A\ntemplayer T B\ngrow 3");
  auto e = s.engine();
  REQUIRE(e.begin_cell("a").ok());
  REQUIRE(e.paint(0, db::Rect{0, 0, 100, 100}).ok());
  e.rescale(v.factor, 1);
  CHECK(e.style().scale_factor == 200);
  CHECK(e.style().multiplier == 2);
  CHECK(e.style().rules[1].ops[1].distance == 6);
  REQUIRE(e.end_cell().ok());
  REQUIRE(e.finish().ok());
  CHECK(s.painted("a", "m1") == Rects{{0, 0, 1, 1}});  // (0,0)-(200,200) RU at S = 200
  CHECK(contains(codes(s), "MASK-INPUT-012"));
}

TEST_CASE("read-unit values beyond the rescale limit are rounded",
          "[cap:mask-input-styles][req:Mid-read subdivision of read units]") {
  CHECK(mask::subdivide_read_unit(3, 1, 2, 1).value == 2);
  CHECK(mask::subdivide_read_unit(-3, 1, 2, 1).value == -1);
  CHECK(mask::subdivide_read_unit(3, 1, 2, 1).rounded);
  CHECK(mask::subdivide_read_unit(4, 1, 2, 1).value == 2);  // divisible: never rounded
  CHECK(mask::subdivide_read_unit(7, 60, 3, 100).rounded);   // 60 * 3 > 100
  CHECK(mask::subdivide_read_unit(7, 30, 3, 100).factor == 3);
}

TEST_CASE("label placement during read", "[cap:mask-input-styles][req:Label placement semantics during read]") {
  Session s("style a\nscalefactor 100\nlayer m1 CMF\nlabels CMF\nlabels CMFPIN port\nlabels CMFTXT text\n"
            "ignore CX\ntemplayer ID\nlabels CELLNAME cellid");
  auto e = s.engine();
  REQUIRE(e.begin_cell("a").ok());
  const auto layer = [&](const char* n) { return e.style().find_cif_layer(n); };
  REQUIRE(e.paint(*layer("CMF"), db::Rect{0, 0, 1000, 1000}).ok());
  mask::ReadLabel l;
  l.text = "FOO";
  l.area = db::Rect{120, 120, 120, 120};
  l.cif_layer = layer("CX");
  REQUIRE(e.label(l).ok());  // dropped
  l.cif_layer = layer("CMF");
  REQUIRE(e.label(l).ok());
  l.text = "A";
  l.cif_layer = layer("CMFPIN");
  REQUIRE(e.label(l).ok());
  l.text = "B";
  REQUIRE(e.label(l).ok());
  l.text = "A";
  REQUIRE(e.label(l).ok());  // same text, same port index
  l.text = "T";
  l.cif_layer = layer("CMFTXT");
  REQUIRE(e.label(l).ok());
  REQUIRE(e.end_cell().ok());
  REQUIRE(e.finish().ok());
  const db::CellDef* a = s.editor.cells.find("a");
  REQUIRE(a->labels().size() == 5);
  const auto& labels = a->labels();
  CHECK(labels[0].text == "FOO");
  CHECK(labels[0].rect == db::Rect{1, 1, 1, 1});
  CHECK_FALSE(labels[0].sticky);
  CHECK(labels[1].is_port());
  CHECK(labels[1].sticky);
  CHECK(labels[1].port_index == 0);
  CHECK(labels[2].port_index == 1);
  CHECK(labels[3].port_index == 0);
  CHECK(labels[4].sticky);
  CHECK_FALSE(labels[4].is_port());
}

TEST_CASE("a cell-id label renames the cell", "[cap:mask-input-styles][req:Label placement semantics during read]") {
  Session s("style a\nscalefactor 1\nlayer m1 CMF\nlabels ID cellid");
  auto e = s.engine();
  REQUIRE(e.begin_cell("a").ok());
  mask::ReadLabel l;
  l.text = "renamed";
  l.cif_layer = e.style().find_cif_layer("ID");
  REQUIRE(e.label(l).ok());
  REQUIRE(e.end_cell().ok());
  REQUIRE(e.finish().ok());
  CHECK(s.editor.cells.find("renamed") != nullptr);
  CHECK(s.editor.cells.find("a") == nullptr);
}

TEST_CASE("polygon to region conversion", "[cap:mask-input-styles][req:Polygon to region conversion]") {
  SECTION("a Manhattan L-shape") {
    Session s("style a\nscalefactor 1\nlayer m1 P");
    read_one(s, {{"P", mask::Polygon{{0, 0}, {0, 20}, {10, 20}, {10, 10}, {20, 10}, {20, 0}}}});
    CHECK(sorted(s.painted("a", "m1")) == Rects{{0, 0, 20, 10}, {0, 10, 10, 20}});
  }
  SECTION("a right triangle becomes a split tile") {
    Session s("style a\nscalefactor 1\nlayer m1 P");
    read_one(s, {{"P", mask::Polygon{{0, 0}, {10, 10}, {10, 0}}}});
    CHECK(s.painted("a", "m1") == Rects{{0, 0, 10, 10}});
    CHECK(has_split(s, "a", "m1"));
  }
  SECTION("a self-overlapping polygon fills with nonzero winding") {
    Session s("style a\nscalefactor 1\nlayer m1 P");
    read_one(s, {{"P", mask::Polygon{{0, 0}, {0, 10}, {10, 10}, {10, 0}, {0, 0}, {0, 10}, {10, 10}, {10, 0}}}});
    CHECK(s.painted("a", "m1") == Rects{{0, 0, 10, 10}});
  }
}

TEST_CASE("the engine top level, properties and misuse", "[cap:mask-input-styles][req:Per-cell layer processing order]") {
  Session s("style a\nscalefactor 1\nlayer m1 A");
  auto top = s.editor.cells.create("edit", s.editor.technology->plane_count());
  REQUIRE(top.ok());
  mask::ReadEngine::Options options;
  options.top = top.value();
  auto e = s.engine(options);
  CHECK_FALSE(e.end_cell().ok());
  REQUIRE(e.paint(0, db::Rect{0, 0, 5, 5}).ok());  // top-level geometry
  REQUIRE(e.paint(7, db::Rect{0, 0, 5, 5}).ok());  // an unknown layer is dropped
  REQUIRE(e.begin_cell("c").ok());
  CHECK_FALSE(e.begin_cell("d").ok());
  REQUIRE(e.property("KEY", "value").ok());
  REQUIRE(e.end_cell().ok());
  REQUIRE(e.finish().ok());
  CHECK(s.painted("edit", "m1") == Rects{{0, 0, 5, 5}});
  const auto* value = s.editor.cells.find("c")->properties().find("KEY");
  REQUIRE(value != nullptr);
  CHECK(std::get<std::string>(*value) == "value");
}
