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
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"

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
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    editor.technology = mt.tech;
    editor.edit_cell = cell("top");
  }

  db::CellDef* cell(const std::string& name) {
    return editor.cells.create(name, editor.technology->plane_count()).value();
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
  CHECK(s.run("box").value.to_string() == "10 0 20 10");
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
  CHECK(code(s.run("box 0 0 x 1")) == db::codes::kBadArgument);
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
  inv->set_bboxes({0, 0, 10, 10}, {0, 0, 10, 10});
  s.ok("snap internal");
  s.ok("box 100 0 110 10");
  CHECK(s.run("getcell inv").value.to_string() == "inv_0");
  CHECK(s.run("getcell inv").value.to_string() == "inv_1");
  CHECK(s.run("getcell inv").value.to_string() == "inv_2");
  s.ok("instance delete inv_1");
  CHECK(s.run("getcell inv 90").value.to_string() == "inv_1");
  const db::CellUse* rotated = db::find_use(*s.editor.edit_cell, "inv_1");
  REQUIRE(rotated != nullptr);
  CHECK(rotated->transform == db::Transform{0, 1, 100, -1, 0, 10});
  CHECK(rotated->bbox == Rect{100, 0, 110, 10});
  CHECK(s.run("instance list").value.to_string() == "inv_0 inv_2 inv_1");
  s.ok("box 0 0 20 15");
  s.ok("array 0 3 0 1");
  CHECK(rotated->array.xlo == 0);
  CHECK(rotated->array.yhi == 3);  // 90-degree orientation swaps the ranges
  s.ok("instance lock inv_0");
  CHECK(db::find_use(*s.editor.edit_cell, "inv_0")->locked);
  CHECK(code(s.run("getcell nosuch")) == db::codes::kCellNotFound);
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
  s.ok("setlabel sticky 1");
  CHECK(s.editor.edit_cell->labels().back().sticky);
  CHECK(s.run("setlabel text").value.to_string() == "B");
  s.ok("port renumber");
  CHECK(s.editor.edit_cell->labels().back().port_index == 0);
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
  CHECK(s.run("property compat").value.to_string() == "1");
  s.ok("property compat false");
  CHECK(s.run("property compat").value.to_string() == "0");
  s.ok("property n");
  s.editor.edit_cell->flags() = db::CellFlags{};
  s.ok("property x 1");
  CHECK(s.editor.edit_cell->flags().modified);
  CHECK(code(s.run("property integer bad x")) == db::codes::kPropertyValue);
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
