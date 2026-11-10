// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cmd-hierarchy commands through the dispatcher: naming rules, xload, edit,
// down and the edit frame, the placement syntax of getcell/dump, dump,
// flatten, flush, cellname and instance.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cmd/prompt.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/editor/edit_frame.hpp"
#include "hier_session.hpp"

namespace cell = hefesto::cell;
namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
using db::Rect;
using db::Transform;
using Session = cell::test::HierSession;
using cell::test::last_message;
using cell::test::mentions;

namespace {

constexpr Transform kR90{0, 1, 0, -1, 0, 0};

// top contains x1 = inv at translate(100,0); inv holds metal1 (0,0)-(10,10).
struct EditFixture {
  Session s;
  db::CellDef* inv;
  db::CellUse* x1;

  explicit EditFixture(bool magic_compat = true) : s(magic_compat), inv(s.cell("inv")), x1(nullptr) {
    s.paint(*inv, "metal1", Rect{0, 0, 10, 10});
    x1 = s.place(*s.top, *inv, db::translation(100, 0), "x1");
  }
};

}  // namespace

// --- naming rules ---------------------------------------------------------------------

TEST_CASE("cell names with [ ] , or control characters are rejected by load, xload and save",
          "[cap:cmd-hierarchy][req:Cell and instance naming rules]") {
  Session s;
  for (const char* command : {"load", "xload", "save"}) {
    const auto r = s.run(command, {"a[1]"});
    REQUIRE_FALSE(r.ok());
    CHECK(r.diagnostics.back().code == cell::codes::kInvalidName);
    CHECK(last_message(r) == "Cell name contains illegal character \"[\"");
  }
  CHECK(s.editor.cells.find("a[1]") == nullptr);
  CHECK(cell::illegal_use_id("a/b") == std::optional<std::string>{"Cell use id contains illegal character \"/\""});
  CHECK(cell::illegal_cell_name("ok_name") == std::nullopt);
  CHECK(cell::illegal_chars("a\tb", "", "Cell name") ==
        std::optional<std::string>{"Cell name contains illegal control character 0x9"});
}

TEST_CASE("a new instance takes the smallest free <cell>_<n> identifier",
          "[cap:cmd-hierarchy][req:Cell and instance naming rules][golden:CMDEDIT-HIER-01]") {
  Session s;
  db::CellDef* inv = s.cell("inv");
  s.paint(*inv, "metal1", Rect{0, 0, 10, 10});
  s.place(*s.top, *inv, db::translation(0, 0), "inv_0");
  s.place(*s.top, *inv, db::translation(20, 0), "inv_2");
  s.box(50, 0, 60, 10);
  const auto r = s.run("getcell inv");
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == "inv_1");
}

// --- load / xload ------------------------------------------------------------------------

TEST_CASE("load makes the cell the edit cell with an identity edit transform",
          "[cap:cmd-hierarchy][req:Load command][golden:CMDEDIT-HIER-04]") {
  EditFixture f;
  f.s.editor.edit_transform = db::translation(5, 5);
  f.s.ok("load newcell");
  db::CellDef* made = f.s.editor.cells.find("newcell");
  REQUIRE(made != nullptr);
  CHECK(f.s.editor.edit_cell == made);
  CHECK(f.s.editor.edit_root == made);
  CHECK(f.s.editor.edit_transform == db::kIdentity);
  const auto failed = f.s.run("load nosuch -fail");
  CHECK(f.s.editor.edit_cell == made);
  CHECK(f.s.editor.windows.current()->root() == made);
  (void)failed;
}

TEST_CASE("xload loads like load and leaves every instance unexpanded",
          "[cap:cmd-hierarchy][req:Xload command][golden:CMDEDIT-HIER-07]") {
  EditFixture f;
  f.s.write("/work/top2.mag", "magic\ntech tiny\ntimestamp 1\n<< end >>\n");
  hefesto::view::LayoutWindow* w = f.s.editor.windows.current();
  w->set_expanded(*f.x1, true);
  f.s.ok("xload top");
  CHECK(w->root() == f.s.top);
  CHECK_FALSE(w->is_expanded(*f.x1));
  CHECK(f.s.editor.edit_cell == f.s.top);
  CHECK(last_message(f.s.run("xload a b")) == "Usage: xload [name]");
}

// --- edit / down -------------------------------------------------------------------------

TEST_CASE("edit on the selected instance sets the edit cell and its transform to the root",
          "[cap:cmd-hierarchy][req:Edit command][golden:CMDEDIT-HIER-05]") {
  EditFixture f;
  f.s.select_use(*f.x1);
  f.s.ok("edit");
  CHECK(f.s.editor.edit_cell == f.inv);
  CHECK(f.s.editor.edit_transform == db::translation(100, 0));
  CHECK(f.s.editor.edit_root == f.s.top);
  // The root box (100,0)-(110,10) is (0,0)-(10,10) in inv.
  CHECK(hefesto::editor::to_edit(f.s.editor, Rect{100, 0, 110, 10}) == Rect{0, 0, 10, 10});
  f.s.box(100, 0, 110, 10);
  f.s.ok("label L");
  CHECK(f.inv->labels().back().rect == Rect{0, 0, 10, 10});
  CHECK(cell::edit_transform_warning(f.s.editor, true) == std::nullopt);
}

TEST_CASE("edit without a selection or with an unknown path changes nothing",
          "[cap:cmd-hierarchy][req:Edit command]") {
  EditFixture f;
  auto r = f.s.run("edit");
  REQUIRE_FALSE(r.ok());
  CHECK(last_message(r) == "No cell selected to edit.");
  r = f.s.run("edit nosuch");
  REQUIRE_FALSE(r.ok());
  CHECK(last_message(r) == "Usage: edit\nMaybe you want the \"load\" command");
  CHECK(f.s.editor.edit_cell == f.s.top);
  f.inv->flags().no_edit = true;
  f.inv->set_file_path("/work/inv");
  f.s.select_use(*f.x1);
  r = f.s.run("edit");
  CHECK(last_message(r) == "File /work/inv is not writeable.  Edit not set.");
  CHECK(f.s.editor.edit_cell == f.s.top);
}

TEST_CASE("edit picks the selected instance under the pointer, else the last one",
          "[cap:cmd-hierarchy][req:Edit command]") {
  EditFixture f;
  db::CellUse* x2 = f.s.place(*f.s.top, *f.inv, db::translation(300, 0), "x2");
  f.s.select_use(*f.x1);
  f.s.select_use(*x2);
  f.s.editor.command_point = cmd::PointerState{f.s.window, db::Point{105, 5}, Rect{105, 5, 105, 5}};
  f.s.ok("edit");
  CHECK(f.s.editor.edit_transform == db::translation(100, 0));
  f.s.editor.command_point.reset();
  f.s.ok("edit");
  CHECK(f.s.editor.edit_transform == db::translation(300, 0));
}

TEST_CASE("edit by instance path keeps the previous edit transform with magic-compat ON and warns",
          "[cap:cmd-hierarchy][req:Edit command][golden:CMDEDIT-HIER-05]") {
  EditFixture f(true);
  f.s.ok("edit x1");
  CHECK(f.s.editor.edit_cell == f.inv);
  CHECK(f.s.editor.edit_transform == db::kIdentity);
  f.s.box(100, 0, 110, 10);
  const auto r = f.s.run("label L");
  REQUIRE(r.ok());
  CHECK(f.inv->labels().back().rect == Rect{100, 0, 110, 10});
  CHECK(mentions(r, "Warning: edit transform does not match instance x1; the box is mapped through the previous "
                    "edit transform"));
}

TEST_CASE("edit by instance path sets the full transform with magic-compat OFF",
          "[cap:cmd-hierarchy][req:Edit command]") {
  EditFixture f(false);
  f.s.ok("edit x1");
  CHECK(f.s.editor.edit_transform == db::translation(100, 0));
  f.s.box(100, 0, 110, 10);
  const auto r = f.s.run("label L");
  REQUIRE(r.ok());
  CHECK(f.inv->labels().back().rect == Rect{0, 0, 10, 10});
  CHECK_FALSE(mentions(r, "Warning: edit transform"));
}

TEST_CASE("instance paths resolve nested instances and array elements", "[cap:cmd-hierarchy][req:Edit command]") {
  EditFixture f;
  db::CellDef* mid = f.s.cell("mid");
  f.s.place(*mid, *f.inv, db::translation(10, 0), "y1");
  db::CellUse* m = f.s.place(*f.s.top, *mid, db::translation(0, 50), "m");
  const auto found = cell::find_instance(*f.s.top, "m/y1");
  REQUIRE(found.has_value());
  CHECK(found->use->def == f.inv);
  CHECK(found->to_root == db::translation(10, 50));
  m->array = db::ArrayInfo{0, 2, 0, 0, 100, 0};
  CHECK(cell::find_instance(*f.s.top, "m[2]/y1")->to_root == db::translation(210, 50));
  // An array named without a subscript is its first element only as the
  // last component (DBTreeFindUse).
  CHECK(cell::find_instance(*f.s.top, "m")->to_root == db::translation(0, 50));
  CHECK_FALSE(cell::find_instance(*f.s.top, "m/y1").has_value());
  CHECK_FALSE(cell::find_instance(*f.s.top, "m[5]/y1").has_value());
  CHECK_FALSE(cell::find_instance(*f.s.top, "nosuch").has_value());
}

TEST_CASE("down loads the selected instance's cell as the window root and edit cell",
          "[cap:cmd-hierarchy][req:Down command][golden:CMDEDIT-HIER-05]") {
  EditFixture f;
  f.s.select_use(*f.x1);
  f.s.ok("down");
  CHECK(f.s.editor.windows.current()->root() == f.inv);
  CHECK(f.s.editor.edit_cell == f.inv);
  CHECK(f.s.editor.edit_transform == db::kIdentity);
  f.s.editor.selection.clear();
  const auto r = f.s.run("down");
  CHECK(mentions(r, "You haven't selected a new cell to edit."));
}

TEST_CASE("the edit frame change is recorded as one undo event", "[cap:cmd-hierarchy][req:Edit command]") {
  EditFixture f;
  const cell::EditFrame before = cell::edit_frame(f.s.editor);
  cell::set_edit_frame(f.s.editor, cell::EditFrame{f.inv, f.s.top, db::translation(100, 0)});
  CHECK(f.s.editor.edit_cell == f.inv);
  CHECK(cell::edit_frame(f.s.editor) != before);
  cell::set_edit_frame(f.s.editor, before);
  CHECK(cell::edit_frame(f.s.editor) == before);
}

// Regression (MODEL-G-050/057): dbwUndoChangeEdit prints the new edit cell
// and its use when an edit cell change is undone or redone.
TEST_CASE("undoing an edit cell change prints the new edit cell", "[cap:layout-undo][req:undo command][req:redo command]") {
  EditFixture f;
  hefesto::editor::EditorState& ed = f.s.editor;
  cell::set_edit_frame(ed, cell::EditFrame{f.inv, f.s.top, f.x1->transform});
  REQUIRE(ed.undo.close_step());
  REQUIRE(ed.undo.undo(ed).ok());
  CHECK(ed.undo.take_notes() == std::vector<std::string>{"Edit cell is now top (Topmost cell in the window)"});
  CHECK(ed.edit_cell == f.s.top);
  (void)ed.undo.redo(ed);
  CHECK(ed.undo.take_notes() == std::vector<std::string>{"Edit cell is now inv (x1)"});
  CHECK(ed.edit_cell == f.inv);
}

// --- getcell / dump ----------------------------------------------------------------------

TEST_CASE("getcell places the child label at the box lower-left corner",
          "[cap:cmd-hierarchy][req:Placement reference syntax for getcell and dump][golden:CMDEDIT-HIER-02]") {
  Session s;
  db::CellDef* pad = s.cell("pad");
  s.paint(*pad, "metal1", Rect{0, 0, 20, 20});
  s.label(*pad, "P", Rect{5, 5, 5, 5});
  s.box(100, 200, 110, 210);
  REQUIRE(s.run("getcell pad child label P").ok());
  CHECK(s.top->uses().back()->transform == db::translation(95, 195));
  const auto missing = s.run("getcell pad child label Q");
  CHECK(last_message(missing) == "Couldn't find label \"Q\" in cell \"pad\".");
}

TEST_CASE("an orientation keeps the child bounding box lower-left in place",
          "[cap:cmd-hierarchy][req:Placement reference syntax for getcell and dump][golden:CMDEDIT-HIER-02]") {
  Session s;
  db::CellDef* c = s.cell("c");
  s.paint(*c, "metal1", Rect{0, 0, 10, 20});
  s.box(0, 0, 1, 1);
  REQUIRE(s.run("getcell c 90").ok());
  CHECK(s.top->uses().back()->bbox == Rect{0, 0, 20, 10});
  CHECK(s.top->uses().back()->transform.a == kR90.a);
  // Each keyword maps to its transform; the lower-left stays at the box.
  const std::vector<std::pair<std::string, Transform>> table = {
      {"180", {-1, 0, 0, 0, -1, 0}}, {"270", {0, -1, 0, 1, 0, 0}}, {"v", {1, 0, 0, 0, -1, 0}},
      {"0v", {1, 0, 0, 0, -1, 0}},   {"90v", {0, 1, 0, 1, 0, 0}},  {"180v", {-1, 0, 0, 0, 1, 0}},
      {"270v", {0, -1, 0, -1, 0, 0}}, {"h", {-1, 0, 0, 0, 1, 0}},  {"0h", {-1, 0, 0, 0, 1, 0}},
      {"90h", {0, -1, 0, -1, 0, 0}}, {"180h", {1, 0, 0, 0, -1, 0}}, {"270h", {0, 1, 0, 1, 0, 0}}};
  db::Coord x = 100;
  for (const auto& [word, t] : table) {
    s.box(x, 0, x + 1, 1);
    INFO(word);
    REQUIRE(s.run("getcell", {"c", word}).ok());
    const db::CellUse& u = *s.top->uses().back();
    CHECK((u.transform.a == t.a && u.transform.b == t.b && u.transform.d == t.d && u.transform.e == t.e));
    CHECK(u.bbox.x0 == x);
    CHECK(u.bbox.y0 == 0);
    x += 100;
  }
}

TEST_CASE("child and parent reference corners and coordinates", "[cap:cmd-hierarchy][req:Placement reference syntax for getcell and dump]") {
  Session s;
  db::CellDef* c = s.cell("c");
  s.paint(*c, "metal1", Rect{0, 0, 10, 20});
  s.box(50, 50, 80, 90);
  REQUIRE(s.run("getcell c child ur parent ll").ok());
  CHECK(s.top->uses().back()->bbox == Rect{40, 30, 50, 50});
  REQUIRE(s.run("getcell c child 0 0 parent 7 8").ok());
  CHECK(s.top->uses().back()->transform == db::translation(7, 8));
  const auto bad = s.run("getcell c bogus");
  CHECK(mentions(bad, "Unrecognized parent/child keyword: \"bogus\""));
  s.label(*s.top, "ANCHOR", Rect{300, 300, 300, 300});
  REQUIRE(s.run("getcell c parent label ANCHOR").ok());
  CHECK(s.top->uses().back()->transform == db::translation(300, 300));
}

TEST_CASE("getcell sets the box and the selection to the new instance and refuses exact copies",
          "[cap:cmd-hierarchy][req:Getcell command][golden:CMDEDIT-HIER-01]") {
  Session s;
  db::CellDef* inv = s.cell("inv");
  s.paint(*inv, "metal1", Rect{0, 0, 10, 10});
  s.box(30, 40, 31, 41);
  const auto r = s.run("getcell inv");
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == "inv_0");
  CHECK(s.editor.box == Rect{30, 40, 40, 50});
  REQUIRE(s.editor.selection.def() != nullptr);
  REQUIRE(s.editor.selection.def()->uses().size() == 1);
  CHECK(s.editor.selection.def()->uses().front()->id == "inv_0");
  s.box(30, 40, 31, 41);
  const auto again = s.run("get inv");
  REQUIRE_FALSE(again.ok());
  CHECK(last_message(again) == "Can't place a cell on an exact copy of itself.");
  CHECK(last_message(s.run("getcell top")).find("circular") != std::string::npos);
  s.editor.edit_cell = nullptr;
  CHECK(last_message(s.run("getcell inv")) == "No cell def being edited; cannot place cell use!");
}

TEST_CASE("getcell through a non-identity edit transform places in edit coordinates",
          "[cap:cmd-hierarchy][req:Getcell command]") {
  EditFixture f;
  db::CellDef* leaf = f.s.cell("leaf");
  f.s.paint(*leaf, "metal1", Rect{0, 0, 2, 2});
  f.s.select_use(*f.x1);
  f.s.ok("edit");
  f.s.box(105, 5, 106, 6);
  REQUIRE(f.s.run("getcell leaf").ok());
  CHECK(f.inv->uses().back()->transform == db::translation(5, 5));
  CHECK(f.s.editor.box == Rect{105, 5, 107, 7});
}

TEST_CASE("dump copies paint, labels and instances into the edit cell and selects them",
          "[cap:cmd-hierarchy][req:Dump command][golden:CMDEDIT-HIER-03]") {
  Session s;
  db::CellDef* blk = s.cell("blk");
  db::CellDef* leaf = s.cell("leaf");
  s.paint(*leaf, "metal1", Rect{0, 0, 2, 2});
  s.paint(*blk, "metal1", Rect{0, 0, 10, 10});
  s.label(*blk, "B", Rect{1, 1, 1, 1});
  s.place(*blk, *leaf, db::translation(20, 0), "leaf_0");
  s.place(*s.top, *leaf, db::translation(500, 0), "leaf_0");  // the copy needs a new id
  s.box(50, 50, 51, 51);
  REQUIRE(s.run("dump blk").ok());
  const auto metal = s.type("metal1");
  bool covered = true;
  s.top->plane(*s.editor.technology->home_plane(metal)).enumerate(Rect{50, 50, 60, 60}, hefesto::tech::TypeMask::all(),
                                                                 [&](const db::TileView& t) {
                                                                   covered = covered && t.type == metal;
                                                                   return true;
                                                                 });
  CHECK(covered);
  for (const auto& u : s.top->uses()) {
    CHECK(u->def != blk);
  }
  CHECK(s.top->labels().back().text == "B");
  CHECK(s.top->labels().back().rect == Rect{51, 51, 51, 51});
  REQUIRE(s.top->uses().size() == 2);
  CHECK(s.top->uses().back()->id != "leaf_0");
  CHECK(s.top->uses().back()->transform == db::translation(70, 50));
  CHECK_FALSE(s.editor.selection.empty());
}

// --- flatten -------------------------------------------------------------------------------

TEST_CASE("flatten copies two levels of paint into a new cell and refuses an existing name",
          "[cap:cmd-hierarchy][req:Flatten command][golden:CMDEDIT-HIER-06]") {
  Session s;
  db::CellDef* mid = s.cell("mid");
  db::CellDef* leaf = s.cell("leaf");
  s.paint(*leaf, "metal1", Rect{0, 0, 5, 5});
  s.place(*mid, *leaf, db::translation(10, 0), "l");
  s.place(*s.top, *mid, db::translation(100, 0), "x1");
  REQUIRE(s.run("flatten topflat").ok());
  db::CellDef* flat = s.editor.cells.find("topflat");
  REQUIRE(flat != nullptr);
  CHECK(flat->uses().empty());
  CHECK(flat->bbox() == Rect{110, 0, 115, 5});
  const auto again = s.run("flatten topflat");
  REQUIRE_FALSE(again.ok());
  CHECK(last_message(again) == "topflat already exists");
  const auto bad = s.run("flatten -xyz f2");
  CHECK(last_message(bad) == "Bad flatten option \"-xyz\";  must start with \"-no\" or \"-do\"");
}

TEST_CASE("flatten -dobox adds only the box area to the destination",
          "[cap:cmd-hierarchy][req:Flatten command][golden:CMDEDIT-HIER-06]") {
  Session s;
  db::CellDef* leaf = s.cell("leaf");
  s.paint(*leaf, "metal1", Rect{0, 0, 10, 10});
  s.place(*s.top, *leaf, db::translation(0, 0), "l");
  s.cell("f4");
  s.box(0, 0, 5, 10);
  REQUIRE(s.run("flatten -dobox f4").ok());
  CHECK(s.editor.cells.find("f4")->bbox() == Rect{0, 0, 5, 10});
}

TEST_CASE("flatten -doinplace replaces the instance and is not recorded with magic-compat ON",
          "[cap:cmd-hierarchy][req:Flatten command]") {
  Session s;
  db::CellDef* leaf = s.cell("leaf");
  s.paint(*leaf, "metal1", Rect{0, 0, 5, 5});
  s.place(*s.top, *leaf, db::translation(100, 0), "x1");
  REQUIRE(s.run("flatten -doinplace x1").ok());
  CHECK(s.top->uses().empty());
  CHECK(s.top->bbox() == Rect{100, 0, 105, 5});
  CHECK_FALSE(s.editor.undo.suspended());  // the suspension ends with the command
  CHECK(last_message(s.run("flatten -doinplace nosuch")) == "No cell use nosuch found in edit cell.");
}

// --- flush ------------------------------------------------------------------------------------

namespace {

struct YesHost final : cmd::PromptHost {
  int asked = 0;
  std::size_t ask(std::string_view, std::span<const std::string>, std::size_t) override {
    ++asked;
    return 1;  // yes
  }
};

}  // namespace

TEST_CASE("flush re-reads the cell from its file and clears the modified flag",
          "[cap:cmd-hierarchy][req:Flush command][golden:CMDEDIT-HIER-09]") {
  Session s;
  s.write("/work/a.mag", "magic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n");
  s.ok("load a");
  db::CellDef* a = s.editor.cells.find("a");
  REQUIRE(a != nullptr);
  s.paint(*a, "metal1", Rect{0, 0, 40, 40});
  db::mark_modified(*a);
  // Headless: the confirmation gets its default answer, "no".
  auto r = s.run("flush a");
  CHECK(a->bbox() == Rect{0, 0, 40, 40});
  YesHost yes;
  s.editor.prompt = &yes;
  r = s.run("flush a");
  CHECK(yes.asked == 1);
  CHECK(a->bbox() == Rect{0, 0, 4, 4});
  CHECK_FALSE(a->flags().modified);
  CHECK(mentions(r, "[Flushed Modifications were Discarded]"));
  r = s.run("flush a -noprompt");
  CHECK(mentions(r, "[Flushed]"));
  CHECK(yes.asked == 1);
  CHECK(s.run("flush nosuch").ok());
  CHECK(last_message(s.run("flush a b c")) == "Usage: flush [cellname] [-noprompt] [-dereference]");
}

TEST_CASE("flush refuses the parent of the edit instance", "[cap:cmd-hierarchy][req:Flush command]") {
  EditFixture f;
  f.s.select_use(*f.x1);
  f.s.ok("edit");
  const auto r = f.s.run("flush top -noprompt");
  CHECK(mentions(r, "Cannot flush cell whose subcell is being edited."));
}

// --- cellname / instance ---------------------------------------------------------------------

TEST_CASE("cellname reports children, parents, instances, the window and cell data",
          "[cap:cmd-hierarchy][req:Cellname and instance commands][golden:CMDEDIT-HIER-08]") {
  EditFixture f;
  Session& s = f.s;
  CHECK(s.run("cellname list children top").value.to_string() == "inv");
  CHECK(s.run("cellname list parents inv").value.to_string() == "top");
  CHECK(s.run("cellname list instances inv").value.to_string() == "x1");
  CHECK(s.run("cellname list childinst top").value.to_string() == "x1");
  CHECK(s.run("cellname celldef inv").value.to_string() == "inv");
  CHECK(s.run("cellname window").value.to_string() == "top");
  CHECK(s.run("cellname list exists nosuch").value.to_string() == "0");
  CHECK(s.run("cellname filepath inv").value.to_string() == "default");
  s.ok("cellname filepath inv /lib");
  CHECK(f.inv->file_path().generic_string() == "/lib/inv");
  CHECK(s.run("cellname filepath inv").value.to_string() == "/lib");
  s.ok("cellname timestamp inv 42");
  CHECK(s.run("cellname timestamp inv").value.to_string() == "42");
  CHECK(last_message(s.run("cellname timestamp inv x")) == "Invalid timestamp \"x\".");
  CHECK(s.run("cellname flags inv").value.to_string().find("available") != std::string::npos);
  s.ok("cellname property inv foo bar");
  CHECK(f.inv->properties().find("foo") != nullptr);
  CHECK(last_message(s.run("cellname orientation inv")).find("do not have orientations") != std::string::npos);
  CHECK(last_message(s.run("cellname allcells x")).starts_with("Usage: cellname"));
}

TEST_CASE("instance orientation, position, lock and the selected instance",
          "[cap:cmd-hierarchy][req:Cellname and instance commands][golden:CMDEDIT-HIER-08]") {
  EditFixture f;
  Session& s = f.s;
  f.x1->transform = Transform{-1, 0, 110, 0, -1, 10};
  s.select_use(*f.x1);
  CHECK(s.run("instance orientation -def").value.to_string() == "S");
  CHECK(s.run("instance orientation").value.to_string() == "180");
  CHECK(s.run("instance celldef x1").value.to_string() == "inv");
  CHECK(s.run("instance position x1").value.to_string() == "110 10");
  s.ok("instance lock x1");
  CHECK(f.x1->locked);
  s.ok("instance unlock");
  CHECK_FALSE(f.x1->locked);
  CHECK(last_message(s.run("instance topcells")) == "Instances do not have a top level.  Use \"cellname\"?");
  CHECK(last_message(s.run("instance rename x1 y")).find("Use \"identify\"") != std::string::npos);
  CHECK(last_message(s.run("instance orientation x1 90")) == "Cannot set orientation by name.  Use selection.");
  CHECK(s.run("instance list").value.to_string() == "x1");
}

// --- load options / save / writeall ---------------------------------------------------------

TEST_CASE("load -fail creates no cell for a missing file", "[cap:cmd-hierarchy][req:Load command][golden:CMDEDIT-HIER-04]") {
  Session s;
  const auto r = s.run("load nosuch -fail");
  REQUIRE_FALSE(r.ok());
  CHECK(last_message(r) == "No file \"nosuch\" found or readable.");
  CHECK(s.editor.cells.find("nosuch") == nullptr);
  CHECK(s.editor.windows.current()->root() == s.top);
  s.write("/work/there.mag", "magic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n");
  REQUIRE(s.run("load there -fail").ok());
  CHECK(s.editor.windows.current()->root()->name() == "there");
  CHECK(s.editor.windows.current()->root()->bbox() == Rect{0, 0, 4, 4});
}

namespace {

struct ScriptedHost final : cmd::PromptHost {
  std::vector<std::size_t> answers;  // in order; the default once exhausted
  std::vector<std::string> questions;
  std::size_t ask(std::string_view question, std::span<const std::string>, std::size_t fallback) override {
    questions.emplace_back(question);
    if (answers.empty()) {
      return fallback;
    }
    const std::size_t a = answers.front();
    answers.erase(answers.begin());
    return a;
  }
};

}  // namespace

TEST_CASE("writeall asks for each changed cell; autowrite writes the rest",
          "[cap:cmd-hierarchy][req:Writeall command][golden:CMDEDIT-HIER-09]") {
  Session s;
  db::CellDef* a = s.cell("a");
  db::CellDef* b = s.cell("b");
  a->set_file_path("/work/a");
  b->set_file_path("/work/b");
  s.top->flags().modified = false;
  db::mark_modified(*a);
  db::mark_modified(*b);
  ScriptedHost host;
  host.answers = {2, 2};  // skip both
  s.editor.prompt = &host;
  REQUIRE(s.run("writeall").ok());
  CHECK(host.questions.size() == 2);
  CHECK(host.questions.front().ends_with(": write, autowrite, flush, skip, or abort command? "));
  CHECK_FALSE(s.services.files().exists("/work/a.mag"));
  host.questions.clear();
  host.answers = {4};  // autowrite
  const auto r = s.run("writeall");
  CHECK(host.questions.size() == 1);
  CHECK(s.services.files().exists("/work/a.mag"));
  CHECK(s.services.files().exists("/work/b.mag"));
  CHECK(mentions(r, "Writing '"));
}

TEST_CASE("writeall modified writes only modified cells; names select exactly those",
          "[cap:cmd-hierarchy][req:Writeall command]") {
  Session s;
  db::CellDef* a = s.cell("a");
  a->set_file_path("/work/a");
  s.cell("b")->set_file_path("/work/b");
  s.top->flags().modified = false;
  db::mark_modified(*a);
  REQUIRE(s.run("writeall modified").ok());
  CHECK(s.services.files().exists("/work/a.mag"));
  CHECK_FALSE(s.services.files().exists("/work/b.mag"));
  REQUIRE(s.run("writeall force b").ok());
  CHECK(s.services.files().exists("/work/b.mag"));
  const auto r = s.run("writeall force nosuch");
  CHECK(last_message(r) == "No such cell \"nosuch\".");
  CHECK(last_message(s.run("writeall bogus")) == "Usage: writeall [force|modified [cellname ...]]");
}

TEST_CASE("load -nowindow reads the cell without changing the window or the edit cell",
          "[cap:cmd-hierarchy][req:Load command]") {
  Session s;
  s.write("/work/side.mag", "magic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n");
  REQUIRE(s.run("load side -nowindow").ok());
  REQUIRE(s.editor.cells.find("side") != nullptr);
  CHECK(s.editor.cells.find("side")->flags().available);
  CHECK(s.editor.windows.current()->root() == s.top);
  CHECK(s.editor.edit_cell == s.top);
}
