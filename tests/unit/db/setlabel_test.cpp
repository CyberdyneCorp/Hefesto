// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `setlabel` (on the selected edit-cell labels), the font form of `label`
// and font labels through flattening,
// driven through the dispatcher as the golden scripts do (font-labels
// "Font-related label commands", "Transforming font labels";
// cmd-labels-properties "Setlabel command", "Label command").

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "db_fixture.hpp"
#include "hefesto/cell/commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
using db::Point;
using db::Rect;

namespace {

// A two-glyph Type 3 font: `A` is a triangle 600 wide, 700 high.
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

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  explicit Session(const db::test::MtTech& mt) {
    REQUIRE(db::register_db_commands(registry));
    REQUIRE(hefesto::cell::register_cell_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    editor.technology = mt.tech;
    editor.edit_cell = cell("top");
    // The font file on the system path, where `setlabel font Test.pt3` finds it.
    REQUIRE(services.files().write_atomic("/sys/Test.pt3", kFont).ok());
    editor.search_paths.system = {"/sys"};
  }

  db::CellDef* cell(const std::string& name) {
    db::CellDef* def = editor.cells.create(name, editor.technology->plane_count()).value();
    def->flags().available = true;
    return def;
  }

  cmd::CommandResult run(const std::string& name, std::vector<std::string> args = {}) {
    cmd::Invocation inv;
    inv.name = name;
    inv.args = std::move(args);
    return dispatcher->execute(inv);
  }

  /// Runs the command; it must succeed and, for the label commands, report
  /// nothing (`box` always reports the box).
  cmd::CommandResult quiet(const std::string& name, std::vector<std::string> args = {}) {
    auto r = run(name, std::move(args));
    INFO(name << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
    if (name == "setlabel" || name == "label") {
      REQUIRE(r.diagnostics.empty());
    }
    return r;
  }

  /// Runs the command; it must succeed and its last message must be `text`.
  void says(const std::string& name, std::vector<std::string> args, const std::string& text) {
    const auto r = run(name, std::move(args));
    REQUIRE(r.ok());
    REQUIRE_FALSE(r.diagnostics.empty());
    CHECK(r.diagnostics.back().text == text);
  }

  void load_font() { quiet("setlabel", {"font", "Test.pt3", "0.5"}); }

  db::Label& label() { return editor.edit_cell->labels().back(); }

  /// Selects every label of the edit cell, as SELECT stores them (copies
  /// in root coordinates); `setlabel` addresses the selected labels.
  void select_labels() {
    editor.selection.set_root(editor.edit_cell, editor.technology->plane_count());
    editor.selection.clear();
    for (const db::Label& l : editor.edit_cell->labels()) {
      editor.selection.def()->labels().push_back(l);
      editor.selection.provenance().push_back(
          hefesto::select::Provenance{hefesto::select::Provenance::Kind::label, editor.edit_cell, {}, l.rect});
    }
  }
};

std::string result(const cmd::CommandResult& r) { return r.value.to_string(); }

#define FONTCMD "[cap:font-labels][req:Font-related label commands]"
#define SETLABEL "[cap:cmd-labels-properties][req:Setlabel command]"
#define LABELCMD "[cap:cmd-labels-properties][req:Label command]"
#define FLTRANS "[cap:font-labels][req:Transforming font labels]"

}  // namespace

TEST_CASE("setlabel font loads a font file, as the startup file does", FONTCMD) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  CHECK(result(s.quiet("setlabel", {"fontlist"})).empty());
  s.says("setlabel", {"font", "0"}, "No vector outline fonts are loaded.");
  // "Test.pt3" is not a loaded font name: the file is loaded (named "Test")
  // with the scale, and no label is changed. A second run loads it again (Q-MISC-26).
  s.load_font();
  s.load_font();
  CHECK(s.editor.fonts.fonts.size() == 2);
  CHECK(s.editor.fonts.fonts[0]->name == "Test");
  CHECK(s.editor.fonts.fonts[0]->extents.y1 == 500);  // trunc(1000 x 0.5)
  CHECK(result(s.quiet("setlabel", {"fontlist"})) == "Test Test");
  s.says("setlabel", {"font", "0"}, "Test");
  s.says("setlabel", {"font", "-1"}, "default");
  s.says("setlabel", {"font", "7"}, "Font index out of range (0 to 1)");
  s.says("setlabel", {"font", "nofont.pt3"}, "Error loading font \"nofont.pt3\"");
}

TEST_CASE("setlabel font gives a size-0 label the lambda numerator as size", FONTCMD) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.load_font();
  REQUIRE(s.run("snap", {"internal"}).ok());
  s.quiet("box", {"0", "0", "10", "10"});
  s.quiet("paint", {"metal1"});
  s.quiet("box", {"2", "2", "2", "2"});
  s.quiet("label", {"AA", "center", "metal1"});
  s.select_labels();
  CHECK_FALSE(s.label().text_bbox.has_value());
  CHECK(result(s.quiet("setlabel", {"font"})) == "default");
  s.quiet("setlabel", {"font", "test"});  // DBNameToFont is case-insensitive
  CHECK(s.label().font == 0);
  CHECK(s.label().size == 1);  // DBLambda[1] eighths (Q-MISC-22)
  CHECK(result(s.quiet("setlabel", {"font"})) == "Test");
  s.quiet("setlabel", {"size", "2"});
  CHECK(s.label().size == 16);
  REQUIRE(s.label().text_bbox.has_value());
  CHECK(*s.label().text_bbox ==
        hefesto::display::font_label_geometry(s.label(), *s.editor.fonts.fonts[0], true).bbox);
  CHECK(db::include(s.editor.edit_cell->extended_bbox(), *s.label().text_bbox) == s.editor.edit_cell->extended_bbox());
  s.quiet("setlabel", {"font", "default"});
  CHECK(s.label().font == -1);
  CHECK_FALSE(s.label().text_bbox.has_value());
}

TEST_CASE("setlabel font <index> prints the name and applies the default font in magic-compat", FONTCMD) {
  // Q-MISC-28: CmdSetLabel declares the index in an inner block, so the
  // font it applies to the labels afterwards is still -1.
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.project.compat.magic_compat = true;
  s.load_font();
  REQUIRE(s.run("snap", {"internal"}).ok());
  s.quiet("box", {"2", "2", "2", "2"});
  s.quiet("label", {"A", "Test"});
  REQUIRE(s.label().font == 0);
  s.select_labels();
  s.says("setlabel", {"font", "0"}, "Test");
  CHECK(s.label().font == -1);
}

TEST_CASE("setlabel font <index> only prints the name in native mode", FONTCMD) {
  // Q-MISC-28 fix: native mode does not reset the labels' font.
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.project.compat.magic_compat = false;
  s.load_font();
  REQUIRE(s.run("snap", {"internal"}).ok());
  s.quiet("box", {"2", "2", "2", "2"});
  s.quiet("label", {"A", "Test"});
  REQUIRE(s.label().font == 0);
  s.select_labels();
  s.says("setlabel", {"font", "0"}, "Test");
  CHECK(s.label().font == 0);
}

TEST_CASE("setlabel size, offset and rotate are stored in eighths and degrees", SETLABEL) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.load_font();
  REQUIRE(s.run("snap", {"internal"}).ok());
  s.quiet("box", {"5", "5", "5", "5"});
  s.quiet("label", {"A", "Test", "10"});
  CHECK(s.label().size == 80);
  s.select_labels();
  // Regression: `setlabel size` stored the typed value without the x8.
  s.quiet("setlabel", {"size", "3"});
  CHECK(s.label().size == 24);
  CHECK(result(s.quiet("setlabel", {"size"})) == "3");
  s.quiet("setlabel", {"size", "0"});  // a size <= 0 changes nothing
  s.quiet("setlabel", {"size", "abc"});
  CHECK(s.label().size == 24);
  s.quiet("setlabel", {"offset", "1", "-2"});
  CHECK(s.label().offset == Point{8, -16});
  s.quiet("setlabel", {"offset", "2 3"});  // one word "x y"
  CHECK(s.label().offset == Point{16, 24});
  CHECK(result(s.quiet("setlabel", {"offset"})) == "{2 3}");  // Q-MISC-23: x and y
  s.says("setlabel", {"offset", "2"}, "Usage:  setlabel offset <x> <y>");
  s.quiet("setlabel", {"rotate", "45"});
  CHECK(s.label().rotation == 45);
  CHECK(result(s.quiet("setlabel", {"rotate"})) == "45");
  s.quiet("setlabel", {"rotate", "-90"});  // stored normalised, as DBFontLabelSetBBox does
  CHECK(s.label().rotation == 270);
  s.quiet("setlabel", {"rotate", "abc"});  // not an integer: rotation 0, as atoi
  CHECK(s.label().rotation == 0);
}

TEST_CASE("setlabel justify, sticky, layer, box and text", SETLABEL) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  REQUIRE(s.run("snap", {"internal"}).ok());
  s.quiet("box", {"0", "0", "10", "10"});
  s.quiet("paint", {"metal1"});
  s.quiet("box", {"4", "4", "4", "4"});
  s.quiet("label", {"A", "n", "metal1"});
  s.select_labels();
  CHECK(result(s.quiet("setlabel", {"justify"})) == "NORTH");
  s.quiet("setlabel", {"justify", "sw"});
  CHECK(s.label().justification == db::kSouthWest);
  CHECK(s.editor.edit_cell->flags().modified);
  s.says("setlabel", {"justify", "x"}, db::position_error_text("x", false));
  CHECK(s.label().justification == db::kSouthWest);
  s.says("setlabel", {"sticky", "maybe"}, "Unknown sticky option \"maybe\"");
  s.quiet("setlabel", {"sticky", "yes"});
  CHECK(s.label().sticky);
  CHECK(result(s.quiet("setlabel", {"sticky"})) == "1");
  CHECK(result(s.quiet("setlabel", {"layer"})) == "metal1");
  s.says("setlabel", {"layer", "nosuch"}, "Unrecognized layer (type) name \"nosuch\"");
  s.quiet("setlabel", {"layer", "metal2"});
  CHECK(s.label().type == mt.m2);
  CHECK(result(s.quiet("setlabel", {"box"})) == "{4 4 4 4}");
  s.says("setlabel", {"box", "4"}, "Label box requires four coordinates.");
  s.quiet("setlabel", {"box", "3 3 5 5"});
  CHECK(s.label().rect == Rect{3, 3, 5, 5});
  s.quiet("box", {"0", "0", "10", "10"});
  s.quiet("setlabel", {"box", "4", "4", "4", "4"});
  CHECK(s.label().rect == Rect{4, 4, 4, 4});
  // The text: a new label with the same attributes replaces the old one.
  s.quiet("setlabel", {"text", "B"});
  REQUIRE(s.editor.edit_cell->labels().size() == 1);
  CHECK(s.label().text == "B");
  CHECK(s.label().type == mt.m2);
  CHECK(s.label().sticky);
  CHECK(result(s.quiet("setlabel", {"text"})) == "B");
}

TEST_CASE("setlabel usage: help, unknown and ambiguous options, too many values", SETLABEL) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  const auto help = s.run("setlabel");
  REQUIRE(help.ok());
  REQUIRE_FALSE(help.diagnostics.empty());
  const std::string& text = help.diagnostics.back().text;
  CHECK(text.starts_with("Usage:  setlabel [option], where [option] is one of:\n    box <lx> <ly> <ux> <uy>\t"));
  CHECK(text.ends_with("\n    help\t\t\tprint this help info"));
  CHECK(s.run("setlabel", {"text", "a", "b", "c"}).diagnostics.back().text == text);
  s.says("setlabel", {"nosuch"}, "Unknown setlabel option \"nosuch\"");
  s.says("setlabel", {"s", "1"}, "Unknown setlabel option \"s\"");  // size or sticky
  // Without a selection no label is addressed: queries are empty.
  CHECK(result(s.quiet("setlabel", {"text"})).empty());
  s.editor.edit_cell = nullptr;
  s.quiet("setlabel", {"rotate", "90"});
}

TEST_CASE("setlabel -default sets the values later labels start from", LABELCMD) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.load_font();
  REQUIRE(s.run("snap", {"internal"}).ok());
  CHECK(result(s.quiet("setlabel", {"-default", "justify"})) == "*ILLEGAL*");  // -1: automatic
  CHECK(result(s.quiet("setlabel", {"-default", "layer"})) == "default");
  s.says("setlabel", {"-default", "text", "x"}, "Cannot set a default label text.");
  s.says("setlabel", {"-default", "box"}, "Cannot set a default label box.");
  s.quiet("setlabel", {"-default", "font", "Test"});
  s.quiet("setlabel", {"-default", "size", "2"});
  s.quiet("setlabel", {"-default", "rotate", "90"});
  s.quiet("setlabel", {"-default", "sticky", "true"});
  CHECK(result(s.quiet("setlabel", {"-default", "font"})) == "Test");
  CHECK(result(s.quiet("setlabel", {"-default", "size"})) == "2");  // Q-MISC-23: ÷8 like the labels
  s.quiet("box", {"3", "3", "3", "3"});
  s.quiet("label", {"A"});
  const db::Label& lab = s.label();
  CHECK(lab.font == 0);
  CHECK(lab.size == 16);
  CHECK(lab.rotation == 90);
  CHECK(lab.sticky);
  CHECK(lab.text_bbox.has_value());
}

TEST_CASE("label with a font: size, rotation, offset, position and layer", LABELCMD) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.load_font();
  REQUIRE(s.run("snap", {"internal"}).ok());
  s.quiet("box", {"0", "0", "10", "10"});
  s.quiet("paint", {"metal1"});
  s.quiet("box", {"5", "5", "5", "5"});
  s.quiet("label", {"A", "Test", "2", "90", "1", "0", "n", "metal1"});
  db::Label lab = s.label();
  CHECK(lab.font == 0);
  CHECK(lab.size == 16);
  CHECK(lab.rotation == 90);
  CHECK(lab.offset == Point{8, 0});
  CHECK(lab.justification == db::kNorth);
  CHECK(lab.type == mt.m1);
  REQUIRE(lab.text_bbox.has_value());
  // The same place and text replaces the label; the offset may be one word.
  s.quiet("label", {"A", "0", "1", "0", "1 1", "s", "-metal1"});
  REQUIRE(s.editor.edit_cell->labels().size() == 1);
  lab = s.label();
  CHECK(lab.size == 8);
  CHECK(lab.offset == Point{8, 8});
  CHECK(lab.sticky);
  CHECK(lab.justification == db::kSouth);  // the position after a one-word offset
  // Without a size the label gets the lambda numerator (Q-MISC-22).
  s.quiet("box", {"20", "20", "20", "20"});
  s.quiet("label", {"B", "Test"});
  CHECK(s.label().size == 1);
  s.says("label", {"C", "nofont"}, "Unknown vector outline font \"nofont\"");
  s.says("label", {"C", "Test", "1", "0", "x", "y"}, "Uninterpretable offset value \"x y\"");
  s.says("label", {"C", "Test", "1", "0", "0", "0", "n", "nosuch"}, "Unknown layer: nosuch");
  s.says("label", {"a/b"}, "Label name contains illegal character \"/\"");
  // An index that is not loaded is reported; the label gets the default font.
  s.says("label", {"D", "5"}, "Font value out of range (0 to 0)");
  CHECK(s.label().text == "D");
  CHECK(s.label().font == -1);
  CHECK(s.label().size == 0);
}

TEST_CASE("flatten -dotoplabels transforms rotation and offset; plain flatten keeps them", FLTRANS) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.load_font();
  REQUIRE(s.run("snap", {"internal"}).ok());
  db::CellDef* top = s.editor.edit_cell;
  db::CellDef* child = s.cell("child");
  s.editor.edit_cell = child;
  s.quiet("box", {"0", "0", "4", "4"});
  s.quiet("paint", {"metal1"});
  s.quiet("box", {"2", "2", "2", "2"});
  s.quiet("label", {"F", "Test", "2", "30", "1", "0", "center", "metal1"});
  s.editor.edit_cell = top;
  s.quiet("box", {"0", "0", "0", "0"});
  REQUIRE(s.run("getcell", {"child", "90"}).ok());
  const db::Transform t = top->uses().front()->transform;
  const db::Point rotated_offset = db::apply(db::Transform{t.a, t.b, 0, t.d, t.e, 0}, Point{8, 0});

  // flatten -dotoplabels -nolabels: CmdFlatten prefers the top labels
  // (DBCellCopyAllLabels), which transform rotation and offset; the top
  // cell has none, so nothing is copied.
  s.quiet("flatten", {"-dotoplabels", "-nolabels", "flat_top"});
  CHECK(s.editor.cells.find("flat_top")->labels().empty());

  // Plain flatten (flatCopyAllLabels): rotation and offset unchanged
  // (Q-MISC-21), the text box recomputed for the copy.
  s.quiet("flatten", {"flat"});
  const db::CellDef* flat = s.editor.cells.find("flat");
  REQUIRE(flat->labels().size() == 1);
  const db::Label& plain = flat->labels().front();
  CHECK(plain.rotation == 30);
  CHECK(plain.offset == Point{8, 0});
  // Regression: flattened font labels lost their text box (and with it the
  // extended bounding box).
  REQUIRE(plain.text_bbox.has_value());
  CHECK(*plain.text_bbox == hefesto::display::font_label_geometry(plain, *s.editor.fonts.fonts[0], true).bbox);
  CHECK(db::include(flat->extended_bbox(), *plain.text_bbox) == flat->extended_bbox());

  // flatten -doinplace -dotoplabels: DBFlattenInPlace prefers all labels
  // (dolabels stays on), copied untransformed.
  s.quiet("flatten", {"-doinplace", "-dotoplabels", "child_0"});
  REQUIRE(top->labels().size() == 1);
  CHECK(top->labels().front().text == "child_0/F");
  CHECK(top->labels().front().rotation == 30);

  // flatten -doinplace -nolabels -dotoplabels: the child's own labels
  // through DBCellCopyAllLabels, rotation and offset transformed.
  top->labels().clear();
  s.quiet("box", {"0", "0", "0", "0"});
  REQUIRE(s.run("getcell", {"child", "90"}).ok());
  s.quiet("flatten", {"-doinplace", "-nolabels", "-dotoplabels", "child_0"});
  REQUIRE(top->labels().size() == 1);
  const db::Label& moved = top->labels().front();
  CHECK(moved.text == "child_0/F");
  CHECK(moved.rotation == db::transform_angle(t, 30, true));
  CHECK(moved.rotation == 120);  // getcell 90: clockwise, +90 (Q-MISC-24)
  CHECK(moved.offset == rotated_offset);
  REQUIRE(moved.text_bbox.has_value());
  CHECK(*moved.text_bbox == hefesto::display::font_label_geometry(moved, *s.editor.fonts.fonts[0], true).bbox);
}
