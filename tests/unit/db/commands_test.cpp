// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The MODEL command subset through the dispatcher, scripted like the
// inputs of tests/golden/MODEL/CASES.md (with the `mt` technology).

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "db_fixture.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/cell/commands.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/select/commands.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
using db::Rect;

namespace {

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  explicit Session(const db::test::MtTech& mt) {
    REQUIRE(db::register_db_commands(registry));
    REQUIRE(hefesto::cell::register_cell_commands(registry));  // getcell, flatten (they may read files)
    REQUIRE(hefesto::select::register_select_commands(registry));  // array (moved to select in M2)
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    editor.technology = mt.tech;
    editor.edit_cell = cell("top");
  }

  db::CellDef* cell(const std::string& name) {
    db::CellDef* def = editor.cells.create(name, editor.technology->plane_count()).value();
    def->flags().available = true;  // in memory: nothing to read from a file
    return def;
  }

  cmd::CommandResult run(const std::string& line) {
    cmd::Invocation inv;
    std::size_t pos = 0;
    while (pos < line.size()) {
      const std::size_t end = line.find(' ', pos);
      const std::string tok = line.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
      if (!tok.empty()) {
        (inv.name.empty() ? inv.name : inv.args.emplace_back()) = tok;
      }
      pos = end == std::string::npos ? line.size() : end + 1;
    }
    return dispatcher->execute(inv);
  }

  void ok(const std::string& line) {
    const auto r = run(line);
    INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
  }
};

std::string error_text(const cmd::CommandResult& r) {
  return r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text;
}

std::string code(const cmd::CommandResult& r) {
  return r.diagnostics.empty() ? std::string{} : r.diagnostics.back().code;
}

}  // namespace

#define CMD "[cap:layout-geometry-model][req:Canonical maximal-horizontal-strip decomposition]"

TEST_CASE("scripted paint builds the canonical decomposition (MODEL-G-001/003)", CMD) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.ok("snap internal");
  s.ok("box 0 0 10 10");
  s.ok("paint metal1");
  s.ok("box 10 0 20 10");
  s.ok("paint metal1");
  CHECK(db::test::rects(s.editor.edit_cell->plane(mt.metal1)) == std::vector<Rect>{{0, 0, 20, 10}});
  CHECK(s.editor.edit_cell->bbox() == Rect{0, 0, 20, 10});
  const auto report = s.run("box");  // CmdBox's report is printed, not returned
  CHECK(report.value.to_string().empty());
  REQUIRE_FALSE(report.diagnostics.empty());
  CHECK(report.diagnostics.front().text.starts_with("Root cell box:\n"));
  CHECK(s.run("box values").value.to_string() == "10 0 20 10");
  s.ok("erase metal1");
  CHECK(db::test::rects(s.editor.edit_cell->plane(mt.metal1)) == std::vector<Rect>{{0, 0, 10, 10}});
}

TEST_CASE("box and snap units", "[cap:layout-geometry-model][req:Integer database coordinate space]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  CHECK(s.run("snap").value.to_string() == "lambda");
  s.editor.grid = tech::GridScale{2, 1};  // 2 internal units = 1 lambda
  s.ok("box 0 0 5 5");
  CHECK(*s.editor.box == Rect{0, 0, 10, 10});
  s.ok("box 1i 1i 3i 3i");
  CHECK(*s.editor.box == Rect{1, 1, 3, 3});
  CHECK(s.editor.box_root == s.editor.edit_cell);  // the box belongs to the edit cell
  s.ok("snap internal");
  CHECK(s.editor.snap == hefesto::editor::SnapMode::internal);  // session state, not a settings entry
  s.ok("box size 4 6");
  CHECK(*s.editor.box == Rect{1, 1, 5, 7});
  CHECK(s.run("box width").value.to_string() == "4");
  // cmd-coordinates-and-units "Numeric literal parsing": Magic assumes 0.
  CHECK(s.run("box 0 0 x 1").ok());
  CHECK(*s.editor.box == Rect{0, 0, 0, 1});
}

TEST_CASE("commands without an edit cell or box fail with MODEL codes", "[cap:layout-paint-semantics][req:Paint operation]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  CHECK(code(s.run("paint metal1")) == db::codes::kNoBox);
  s.ok("box 0 0 1 1");
  CHECK(code(s.run("paint nosuch")) == db::codes::kBadLayer);
  s.editor.edit_cell = nullptr;
  CHECK(code(s.run("paint metal1")) == db::codes::kNoEditCell);
}

TEST_CASE("splitpaint writes a triangle (MODEL-G-007/008)", "[cap:layout-geometry-model][req:Non-Manhattan (split tile) geometry]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.ok("snap internal");
  s.ok("box 0 0 10 10");
  s.ok("splitpaint nw metal1");
  auto t = db::test::tiles(s.editor.edit_cell->plane(mt.metal1));
  REQUIRE(t.size() == 1);
  CHECK(db::triangle_corner(t[0].diagonal, t[0].right_side) == db::TriangleCorner::nw);
  s.ok("splitpaint se metal1");
  CHECK(db::test::rects(s.editor.edit_cell->plane(mt.metal1)) == std::vector<Rect>{{0, 0, 10, 10}});
}

TEST_CASE("getcell ids, array and instance commands (MODEL-G-027/030)", "[cap:layout-hierarchy][req:Instance identifier assignment]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  db::CellDef* inv = s.cell("inv");
  REQUIRE(db::paint(*inv, *mt.tech, Rect{0, 0, 10, 10}, mt.metal1, db::PaintOptions{}).ok());
  db::recompute_bbox(*inv, true);  // getcell recomputes the child's box (DBReComputeBbox)
  s.ok("snap internal");
  // Magic refuses a getcell onto an exact copy, so the box moves between them.
  s.ok("box 100 0 110 10");
  CHECK(s.run("getcell inv").value.to_string() == "inv_0");
  s.ok("box 200 0 210 10");
  CHECK(s.run("getcell inv").value.to_string() == "inv_1");
  s.ok("box 300 0 310 10");
  CHECK(s.run("getcell inv").value.to_string() == "inv_2");
  s.ok("instance delete inv_1");
  s.ok("box 100 0 110 10");
  CHECK(s.run("getcell inv 90").value.to_string() == "inv_1");  // getcell selects it
  const db::CellUse* rotated = db::find_use(*s.editor.edit_cell, "inv_1");
  REQUIRE(rotated != nullptr);
  CHECK(rotated->transform == db::Transform{0, 1, 100, -1, 0, 10});
  CHECK(rotated->bbox == Rect{100, 0, 110, 10});
  CHECK(s.run("instance list").value.to_string() == "inv_0 inv_2 inv_1");
  s.ok("box 0 0 20 15");
  s.ok("array 0 3 0 1");  // arrays the selection, replacing the use
  rotated = db::find_use(*s.editor.edit_cell, "inv_1");
  REQUIRE(rotated != nullptr);
  CHECK(rotated->array.xlo == 0);
  CHECK(rotated->array.yhi == 3);  // 90-degree orientation swaps the ranges
  s.ok("instance lock inv_0");
  CHECK(db::find_use(*s.editor.edit_cell, "inv_0")->locked);
  // getcell reads an unknown cell from its file first; dbReadOpen reports it.
  CHECK(code(s.run("getcell nosuch")) == hefesto::cell::codes::kNotFound);
}

TEST_CASE("label, port and setlabel through commands (MODEL-G-038/043)", "[cap:layout-labels-ports][req:Automatic justification]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.editor.edit_cell->set_bboxes({0, 0, 30, 30}, {0, 0, 30, 30});
  s.ok("snap internal");
  s.ok("box 2 2 2 2");
  s.ok("label A");
  CHECK(s.editor.edit_cell->labels().back().justification == db::kNorthEast);
  s.ok("box 0 0 10 10");
  s.ok("paint metal1");
  s.ok("box 5 5 5 5");
  s.ok("label B north metal1");
  CHECK(s.editor.edit_cell->labels().back().type == mt.m1);
  CHECK(s.editor.edit_cell->labels().back().justification == db::kNorth);
  s.ok("port make");
  CHECK(s.editor.edit_cell->labels().back().port_index == 1);
  CHECK(s.editor.edit_cell->labels().back().port_directions == db::kPortNorth);
  s.ok("port use power");
  CHECK(s.run("port use").value.to_string() == "power");
  s.ok("box 4 4 6 6");
  s.ok("select area labels");  // setlabel addresses the selected labels
  s.ok("setlabel sticky 1");
  CHECK(s.editor.edit_cell->labels().back().sticky);
  CHECK(s.run("setlabel text").value.to_string() == "B");
  s.ok("port renumber");
  CHECK(s.editor.edit_cell->labels().back().port_index == 0);
}

TEST_CASE("a port is a label with direction bits; removing it clears every port flag",
          "[cap:layout-labels-ports][req:Ports]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.editor.edit_cell->set_bboxes({0, 0, 30, 30}, {0, 0, 30, 30});
  s.ok("snap internal");
  s.ok("box 0 0 4 4");
  s.ok("paint metal1");
  s.ok("box 10 0 14 4");
  s.ok("paint metal1");
  s.ok("box 2 2 2 2");
  s.ok("label VDD north metal1");
  s.ok("port make");
  s.ok("port use power");
  s.ok("port class inout");
  s.ok("port shape abutment");
  s.ok("setlabel sticky 1");
  s.ok("box 12 2 12 2");
  s.ok("label VDD north metal1");
  s.ok("port make");
  s.ok("port index 1");
  auto& labels = s.editor.edit_cell->labels();
  REQUIRE(labels.size() == 2);
  // The data model does not enforce index uniqueness: both VDD labels share
  // index 1 and so denote one terminal (port renumbering keeps that).
  labels[0].port_index = 1;
  CHECK(labels[0].is_port());
  CHECK(labels[1].is_port());
  CHECK(labels[0].port_index == labels[1].port_index);
  s.ok("port renumber");
  CHECK(labels[0].port_index == labels[1].port_index);

  s.ok("box 2 2 2 2");
  CHECK(s.run("port exists").value.to_string() == "1");
  s.editor.edit_cell->flags() = db::CellFlags{};
  s.ok("port remove");
  CHECK(s.run("port exists").value.to_string() == "0");
  const db::Label& removed = labels[0];
  CHECK_FALSE(removed.is_port());
  CHECK(removed.port_directions == 0);
  CHECK(removed.port_class == db::PortClass::default_class);
  CHECK(removed.port_use == db::PortUse::default_use);
  CHECK(removed.port_shape == db::PortShape::default_shape);
  CHECK_FALSE(removed.sticky);
  CHECK(s.editor.edit_cell->flags().modified);
}

TEST_CASE("property set and remove mark the cell modified and pending a new stamp",
          "[cap:layout-properties][req:Property changes and modification/undo]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.editor.edit_cell->flags() = db::CellFlags{};
  s.ok("property x 1");
  CHECK(s.editor.edit_cell->flags().modified);
  CHECK(s.editor.edit_cell->flags().needs_new_timestamp);
  s.editor.edit_cell->flags() = db::CellFlags{};
  cmd::Invocation remove;
  remove.name = "property";
  remove.args = {"x", ""};
  REQUIRE(s.dispatcher->execute(remove).ok());
  CHECK(s.editor.edit_cell->properties().find("x") == nullptr);
  CHECK(s.editor.edit_cell->flags().modified);
  CHECK(s.editor.edit_cell->flags().needs_new_timestamp);
  // Undo (layout-undo) arrives with M2; M1 has no undo history to record into.
}

TEST_CASE("port attributes take Magic's keywords and synonyms only",
          "[cap:cmd-labels-properties][req:Port attributes][regression]") {
  // Regression: `port use reset` was accepted (Magic prints its usage), and
  // the synonyms digital, inout and feedthru were refused.
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.editor.edit_cell->set_bboxes({0, 0, 30, 30}, {0, 0, 30, 30});
  s.ok("snap internal");
  s.ok("box 0 0 10 10");
  s.ok("paint metal1");
  s.ok("box 5 5 5 5");
  s.ok("label B north metal1");
  s.ok("port make");
  const auto bad = s.run("port use reset");
  CHECK(bad.ok());  // Magic prints the usage without a Tcl error
  REQUIRE_FALSE(bad.diagnostics.empty());
  CHECK(bad.diagnostics.back().text ==
        "Usage:  port use <type>, where <type> is one of:\n    default\n    analog\n    signal\n    digital\n"
        "    power\n    ground\n    clock");
  CHECK(s.editor.edit_cell->labels().back().port_use == db::PortUse::default_use);
  s.ok("port use digital");
  CHECK(s.run("port use").value.to_string() == "signal");
  s.ok("port class inout");
  CHECK(s.run("port class").value.to_string() == "bidirectional");
  s.ok("port shape feedthru");
  CHECK(s.run("port shape").value.to_string() == "feedthrough");
}

TEST_CASE("setlabel font fills the label's text bounding box", "[cap:font-labels][req:Font labels in bounding boxes and searches]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  constexpr std::string_view kFont = R"(%!PS-AdobeFont-1.0: Test
/FontBBox {0 0 1000 1000 }readonly def
/CharProcs 128 dict def
CharProcs begin
  /.notdef { 500 0 0 0 400 700 setcachedevice
	} bind def
  /A { 600 0 0 0 600 700 setcachedevice
	0 0 moveto
	 300 700 lineto
	 600 0 lineto
	closepath
	} bind def
currentdict end
/Test.Regular exch definefont
)";
  auto font = hefesto::display::parse_outline_font(kFont, "Test.pt3");
  REQUIRE(font.ok());
  font.value().name = "Test";
  s.editor.fonts.fonts.push_back(std::make_shared<const hefesto::display::OutlineFont>(std::move(font).value()));
  s.ok("snap internal");
  s.ok("box 0 0 10 10");
  s.ok("paint metal1");
  s.ok("label AA center metal1");
  CHECK_FALSE(s.editor.edit_cell->labels().back().text_bbox.has_value());
  s.ok("select area labels");  // setlabel addresses the selected labels
  s.ok("setlabel font Test");  // by name (DBNameToFont)
  s.ok("setlabel size 20");
  const auto& lab = s.editor.edit_cell->labels().back();
  CHECK(lab.font == 0);
  REQUIRE(lab.text_bbox.has_value());
  CHECK(lab.text_bbox->width() > 0);
  CHECK(db::include(s.editor.edit_cell->extended_bbox(), *lab.text_bbox) == s.editor.edit_cell->extended_bbox());
  s.ok("setlabel font default");
  CHECK_FALSE(s.editor.edit_cell->labels().back().text_bbox.has_value());
}

TEST_CASE("property command with reserved names (MODEL-G-046/047)", "[cap:layout-properties][req:Reserved-name type coercion]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.ok("snap internal");
  s.ok("property FIXED_BBOX 0 0 100 200");
  CHECK(s.editor.edit_cell->flags().fixed_bbox);
  CHECK(*s.editor.edit_cell->properties().find("FIXED_BBOX") == db::PropertyValue{db::DimensionList{{0, 0, 100, 200}}});
  s.ok("property integer n 7");
  CHECK(s.run("property n").value.to_string() == "7");
  // magic-compat: Magic's PROPERTY_TYPE_COMPAT is the index of "plane", so
  // `property compat false` queries a property named "false" and `property
  // plane` is the compat switch (MODEL-G-046).
  CHECK(s.run("property plane").value.to_string() == "1");
  const auto query = s.run("property compat false");
  CHECK(code(query) == db::codes::kWarning);
  CHECK(query.diagnostics.back().text == "Property name \"false\" is not defined");
  CHECK(s.editor.property_compat);
  s.project.compat.magic_compat = false;  // the documented keyword
  CHECK(s.run("property compat").value.to_string() == "1");
  s.ok("property compat false");
  CHECK(s.run("property compat").value.to_string() == "0");
  s.project.compat.magic_compat = true;
  s.ok("property n");
  s.editor.edit_cell->flags() = db::CellFlags{};
  s.ok("property x 1");
  CHECK(s.editor.edit_cell->flags().modified);
  // sscanf("%d") failure: Magic warns and stores 0.
  CHECK(code(s.run("property integer bad x")) == db::codes::kWarning);
  CHECK(*s.editor.edit_cell->properties().find("bad") == db::PropertyValue{db::IntegerList{{0}}});
}

TEST_CASE("cellname rename, delete and list (MODEL-G-036/037)", "[cap:layout-hierarchy][req:Renaming a cell definition]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.cell("a");
  s.cell("b");
  db::CellDef* inv = s.cell("inv");
  inv->set_bboxes({0, 0, 1, 1}, {0, 0, 1, 1});
  CHECK(code(s.run("cellname rename a b")) == db::codes::kCellExists);
  s.ok("cellname rename a c");
  CHECK(s.run("cellname exists c").value.to_string() == "1");
  s.ok("box 0 0 1 1");
  s.ok("getcell inv");
  CHECK(code(s.run("cellname delete inv")) == db::codes::kCellInUse);
  s.ok("instance delete inv_0");
  s.ok("cellname delete inv");
  CHECK(s.run("cellname exists inv").value.to_string() == "0");
  const std::string all = s.run("cellname list allcells").value.to_string();
  CHECK(all.find("top") != std::string::npos);
  CHECK(all.find("inv") == std::string::npos);
}

TEST_CASE("flatten and flatten -doinplace (MODEL-G-024/035)", "[cap:layout-hierarchy][req:Flattening into a new cell]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  db::CellDef* child = s.cell("child");
  s.ok("snap internal");
  db::CellDef* top = s.editor.edit_cell;
  s.editor.edit_cell = child;
  s.ok("box 0 0 4 4");
  s.ok("paint via1");
  s.editor.edit_cell = top;
  s.ok("box 100 0 120 4");
  s.ok("paint metal2");
  s.ok("box 100 0 104 4");
  s.ok("getcell child");
  s.ok("flatten flat");
  db::CellDef* flat = s.editor.cells.find("flat");
  REQUIRE(flat != nullptr);
  CHECK(flat->plane(mt.metal1).type_at({101, 1}) == mt.via1);
  CHECK(flat->plane(mt.metal2).type_at({110, 1}) == mt.m2);
  s.ok("flatten -doinplace child_0");
  CHECK(top->uses().empty());
  CHECK(top->plane(mt.metal2).type_at({101, 1}) == mt.via1);
  CHECK(code(s.run("flatten -doinplace nosuch")) == db::codes::kUseNotFound);
}

// --- Magic console behavior of the golden cases (regressions) -------------------

namespace {

std::string printed(const cmd::CommandResult& r) {
  std::string out;
  for (const auto& d : r.diagnostics) {
    out += d.text + "\n";
  }
  return out;
}

}  // namespace

TEST_CASE("box with coordinates prints CmdBox's report and returns nothing (TECH-G18)",
          "[cap:cmd-box][req:Default box report]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.ok("snap internal");
  const auto r = s.run("box 0 0 4 4");
  REQUIRE(r.ok());
  CHECK(r.value.to_string().empty());
  CHECK(printed(r) ==
        "Root cell box:\n"
        "           width x height  (   llx,  lly  ), (   urx,  ury  )  area (units^2)\n\n"
        "microns:   4.000 x 4.000   ( 0.000,  0.000), ( 4.000,  4.000)  16.000    \n"
        "lambda:        4 x 4       (     0,  0    ), (     4,  4    )  16        \n");
  s.editor.grid = tech::GridScale{2, 1};  // lambda line in reals, internal line in integers
  const auto scaled = s.run("box 0 0 4 8");
  CHECK(printed(scaled).find("lambda:     2.00 x 4.00    (  0.00,  0.00 ), (  2.00,  4.00 )  8.00      \n") !=
        std::string::npos);
  CHECK(printed(scaled).find("internal:      4 x 8       (     0,  0    ), (     4,  8    )  32        \n") !=
        std::string::npos);
  const auto empty = s.run("box 3 3 3 3");  // zero area: no area column
  CHECK(printed(empty).find("area") == std::string::npos);
}

TEST_CASE("snap internal warns once, switches units and returns nothing",
          "[cap:cmd-coordinates-and-units][req:Snap command]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  const auto first = s.run("snap internal");
  CHECK(first.value.to_string().empty());
  REQUIRE(first.diagnostics.size() == 1);
  CHECK(first.diagnostics.front().code == db::codes::kWarning);
  CHECK(first.diagnostics.front().text.starts_with("Warning:  snap setting is also changing units."));
  CHECK(s.run("snap internal").diagnostics.empty());  // units are no longer "default"
  s.ok("snap lambda");
  CHECK(s.run("snap").value.to_string() == "lambda");
  s.editor.grid = tech::GridScale{2, 1};
  s.ok("box 0 0 3 3");  // units stay internal: values are not lambda
  CHECK(*s.editor.box == Rect{0, 0, 3, 3});
  CHECK(printed(s.run("snap bogus")) == "Usage: snap [internal | lambda | user]\n");
}

TEST_CASE("a label typed without a layer moves onto the paint with a note (MODEL-G-009)",
          "[cap:layout-labels-ports][req:Label adjustment after paint changes]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.ok("snap internal");
  s.ok("box 0 0 10 20");
  s.ok("paint metal1");
  s.ok("box 5 10 5 10");
  const auto r = s.run("label N");
  REQUIRE(r.ok());
  CHECK(printed(r) == "Moving label \"N\" from space to metal1 in cell top.\n");
}

TEST_CASE("flatten copies labels in subcell search order with Magic's '.' separators (MODEL-G-034)",
          "[cap:layout-hierarchy][req:Flattening into a new cell]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.ok("snap internal");
  db::CellDef* top = s.editor.edit_cell;
  db::CellDef* leaf = s.cell("leaf");
  db::CellDef* mid = s.cell("mid");
  s.editor.edit_cell = leaf;
  s.ok("box 0 0 10 10");
  s.ok("paint metal1");
  s.ok("box 5 5 5 5");
  s.ok("label L");
  s.editor.edit_cell = mid;
  s.ok("box 0 0 0 0");
  s.ok("getcell leaf");
  s.ok("box 2 2 2 2");
  s.ok("label M");
  s.editor.edit_cell = top;
  s.ok("box 0 0 0 0");
  s.ok("getcell mid");
  s.ok("box 50 0 50 0");
  s.ok("getcell mid");
  const auto r = s.run("flatten flat");
  REQUIRE(r.ok());
  CHECK(r.value.to_string().empty());  // CmdFlatten sets no result
  std::vector<std::string> texts;
  for (const db::Label& l : s.editor.cells.find("flat")->labels()) {
    texts.push_back(l.text);
  }
  CHECK(texts == std::vector<std::string>{"mid_1.M", "mid_1.leaf_0.L", "mid_0.M", "mid_0.leaf_0.L"});
}

TEST_CASE("cellname list sorts naturally and cellname errors use Magic's text (MODEL-G-037/048)",
          "[cap:layout-hierarchy][req:Deleting a cell definition]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.cell("b10");
  s.cell("b9");
  CHECK(s.run("cellname list allcells").value.to_string() == "b9 b10 top");
  const auto bad = s.run("cellname readonly top");
  CHECK(error_text(bad).starts_with("Usage: cellname [list] children|parents|self|exists|instances|celldef|delete"));
  CHECK(error_text(s.run("cellname rename b9 b10")) == "Cannot rename; cell \"b10\" already exists!");
  CHECK(s.run("cellname writeable top").value.to_string().empty());
}

TEST_CASE("property lists flatten each value list; plane values follow tile order (MODEL-G-047, CELL-RD-20)",
          "[cap:layout-properties][req:Typed property store]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.ok("snap internal");
  s.ok("property MASKHINTS_X 0 0 4 4 6 6 8 8");
  s.ok("property GDS_START 1234");
  CHECK(s.run("property MASKHINTS_X").value.to_string() == "6 6 8 8 0 0 4 4");
  const std::string all = s.run("property").value.to_string();
  CHECK(all.find("{MASKHINTS_X 6 6 8 8 0 0 4 4}") != std::string::npos);
  CHECK(all.find("{GDS_START 1234}") != std::string::npos);
  CHECK(s.run("property s hello").ok());  // "s" is a prefix of the type "string": a query of "hello"
  CHECK(s.editor.edit_cell->properties().find("s") == nullptr);
}
