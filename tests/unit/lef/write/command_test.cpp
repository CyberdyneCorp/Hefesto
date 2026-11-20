// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef-write: the `lef write`, `lef writeall` and `lef nocheck` command
// surface, output file naming, writeall scope and order, and the write
// side effects (tasks.md 6.1, 6.6; Q-LEFDEF-41, 43).
//
// Owner: LEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <atomic>

#include "hefesto/db/use_index.hpp"
#include "hefesto/lef/command/command.hpp"
#include "write_fixture.hpp"

namespace hefesto::lef::write_test {
namespace {

/// A session with `top` (metal1 and a port) shown in a window.
struct Shown {
  Session s;
  db::CellDef* top = nullptr;

  explicit Shown(bool magic_compat = true) : s(magic_compat) {
    top = &s.cell("top");
    s.paint(*top, "metal1", db::Rect{0, 0, 100, 200});
    s.label(*top, Port{"A", "metal1", db::Rect{0, 0, 10, 10}, 0});
    s.window(*top);
    s.editor.edit_root = top;
  }

  [[nodiscard]] std::string printed() {
    std::string text;
    for (const auto& d : s.printed.take()) {
      text += d.text + "\n";
    }
    return text;
  }
};

std::string help_text() {
  std::string text = "The \"lef\" options are:\n";
  for (const std::string_view entry : lef_options()) {
    text += "    lef " + std::string{entry} + "\n";
  }
  return text;
}

TEST_CASE("lef write writes the window's root, and lef writea selects writeall",
          "[cap:lef-write][req:Command dispatch and target cell]") {
  Shown w;
  REQUIRE(w.s.run("lef write").ok());
  CHECK(w.s.exists("top.lef"));
  REQUIRE(w.s.run("lef writea").ok());
  CHECK(w.s.read("top.lef").ends_with("END top\nEND LIBRARY\n\n"));
  (void)w.printed();
  REQUIRE(w.s.run("lef wr").ok());  // ambiguous: write and writeall
  CHECK(w.printed().starts_with("\"wr\" isn't a valid lef option.\n" + help_text()));
}

TEST_CASE("a selected instance takes precedence over the window", "[cap:lef-write][req:Command dispatch and target cell]") {
  Shown w;
  db::CellDef& nand2 = w.s.cell("nand2");
  w.s.paint(nand2, "metal2", db::Rect{0, 0, 10, 10});
  w.s.editor.selection.set_root(w.top, w.s.editor.technology->plane_count());
  REQUIRE(w.s.editor.selection.add_use(nand2, "n1", db::kIdentity, db::ArrayInfo{}) != nullptr);
  REQUIRE(w.s.run("lef write").ok());
  CHECK(w.s.exists("nand2.lef"));
  CHECK_FALSE(w.s.exists("top.lef"));
  // nand2 is not the edit root: no header.
  CHECK(w.s.read("nand2.lef").starts_with("MACRO nand2\n"));
}

TEST_CASE("without a window, selection or box the command asks for a window",
          "[cap:lef-write][req:Command dispatch and target cell]") {
  Session s;
  const auto r = s.run("lef write");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.front().text == "Point to a window first");
  // The box's cell stands in for the window.
  db::CellDef& boxed = s.cell("boxed");
  s.paint(boxed, "metal1", db::Rect{0, 0, 10, 10});
  s.editor.box = db::Rect{0, 0, 10, 10};
  s.editor.box_root = &boxed;
  REQUIRE(s.run("lef write").ok());
  CHECK(s.exists("boxed.lef"));
}

TEST_CASE("the file name comes first, and option words match by their leading text",
          "[cap:lef-write][req:lef write argument grammar]") {
  Shown w;
  REQUIRE(w.s.run("lef write -hide out.lef").ok());
  CHECK(w.printed() == help_text());
  CHECK_FALSE(w.s.exists("out.lef"));
  CHECK_FALSE(w.s.exists("top.lef"));
  REQUIRE(w.s.run("lef write abs -technology").ok());
  CHECK(w.s.read("abs.lef").find("UNITS\n  DATABASE MICRONS 1000 ;\nEND UNITS\n") != std::string::npos);
  (void)w.printed();
  REQUIRE(w.s.run("lef write a b").ok());
  CHECK(w.printed() == help_text());
  REQUIRE(w.s.run("lef write -bogus").ok());
  CHECK(w.printed() == help_text());
}

TEST_CASE("def write options are ignored with a note", "[cap:lef-write][req:lef write argument grammar]") {
  Shown w;
  REQUIRE(w.s.run("lef write -allspecial -units -analretentive").ok());
  CHECK(w.s.texts("LEF-WRITE-010") == std::vector<std::string>{"The \"-allspec\" option is only for def write",
                                                                "The \"-units\" option is only for def write",
                                                                "The \"-anal\" option is only for def write"});
  CHECK(w.s.exists("top.lef"));
}

TEST_CASE("lef write -hide and -pinonly take a following number as their distance", "[cap:lef-write][req:Distance option values]") {
  Shown w;
  w.top->flags().fixed_bbox = true;
  w.s.property(*w.top, "FIXED_BBOX", db::DimensionList{{0, 0, 100, 200}});
  REQUIRE(w.s.run("lef write plain").ok());
  REQUIRE(w.s.run("lef write neg -hide -5").ok());
  CHECK(w.s.read("neg.lef") == w.s.read("plain.lef"));
  REQUIRE(w.s.run("lef write ten -hide 10").ok());
  // A 10-unit setback: the pin is the label plus the net within 10 of B.
  CHECK(w.s.read("ten.lef").find("  PIN A\n    PORT\n      LAYER met1 ;\n        RECT 0.000 1.900 1.000 2.000 ;\n") !=
        std::string::npos);
  REQUIRE(w.s.run("lef write zero -pinonly").ok());
  CHECK(w.s.read("zero.lef").find("  PIN A\n    PORT\n      LAYER met1 ;\n        RECT 0.000 0.000 0.100 0.100 ;\n") !=
        std::string::npos);
}

TEST_CASE("lef writeall takes options only", "[cap:lef-write][req:lef writeall arguments and scope]") {
  Shown w;
  REQUIRE(w.s.run("lef writeall out.lef").ok());
  CHECK(w.printed() == help_text());
  CHECK_FALSE(w.s.exists("top.lef"));
  REQUIRE(w.s.run("lef writeall -bogus").ok());
  CHECK(w.printed() == help_text());
}

/// top: two inv, one nand2; nand2: one nmos4.
struct Library {
  Session s;
  db::CellDef* top = nullptr;
  db::CellDef* inv = nullptr;
  db::CellDef* nand2 = nullptr;
  db::CellDef* nmos4 = nullptr;

  Library() {
    nmos4 = &s.cell("nmos4");
    s.paint(*nmos4, "metal1", db::Rect{0, 0, 4, 4});
    inv = &s.cell("inv");
    s.paint(*inv, "metal1", db::Rect{0, 0, 10, 10});
    nand2 = &s.cell("nand2");
    s.paint(*nand2, "metal1", db::Rect{0, 0, 20, 10});
    s.place(*nand2, *nmos4);
    top = &s.cell("top");
    s.place(*top, *inv);
    s.place(*top, *inv, db::Transform{1, 0, 50, 0, 1, 0});
    s.place(*top, *nand2, db::Transform{1, 0, 100, 0, 1, 0});
  }

  cmd::CommandResult write_all(bool top_cell, bool recurse) {
    LefWriteOptions o;
    o.top_cell = top_cell;
    o.recurse = recurse;
    o.files = &s.services.view().files;
    return lef::write_all(s.editor, *top, "lib", o);
  }

  [[nodiscard]] std::vector<std::string> macros() {
    std::vector<std::string> names;
    const std::string text = s.read("lib.lef");
    for (std::size_t at = text.find("MACRO "); at != std::string::npos; at = text.find("MACRO ", at + 1)) {
      names.push_back(text.substr(at + 6, text.find('\n', at) - at - 6));
    }
    return names;
  }

  // The uses of `def` in DBCellEnum order.
  static std::vector<const db::CellDef*> children(const db::CellDef& def) {
    std::vector<const db::CellDef*> out;
    (void)def.use_index().enumerate_all([&](db::CellUse& u) {
      out.push_back(u.def);
      return true;
    });
    return out;
  }
};

TEST_CASE("writeall writes the children, and with -all every level", "[cap:lef-write][req:lef writeall arguments and scope]") {
  Library lib;
  REQUIRE(lib.write_all(false, false).ok());
  std::vector<std::string> names = lib.macros();
  std::sort(names.begin(), names.end());
  CHECK(names == std::vector<std::string>{"inv", "nand2"});
  REQUIRE(lib.write_all(false, true).ok());
  names = lib.macros();
  std::sort(names.begin(), names.end());
  CHECK(names == std::vector<std::string>{"inv", "nand2", "nmos4"});
  const std::string text = lib.s.read("lib.lef");
  CHECK(text.starts_with("VERSION 5.7 ;\n"));
  CHECK(text.ends_with("END LIBRARY\n\n"));
}

TEST_CASE("writeall writes the macros in reverse collection order", "[cap:lef-write][req:writeall macro order]") {
  Library lib;
  // Pre-order collection: the top, then each child as DBCellEnum gives it,
  // recursing at once with -all; the macros come out reversed.
  std::vector<std::string> collected{"top"};
  for (const db::CellDef* child : Library::children(*lib.top)) {
    if (std::find(collected.begin(), collected.end(), child->name()) != collected.end()) {
      continue;
    }
    collected.push_back(child->name());
    if (child == lib.nand2) {
      collected.push_back("nmos4");
    }
  }
  REQUIRE(lib.write_all(true, true).ok());
  CHECK(lib.macros() == std::vector<std::string>(collected.rbegin(), collected.rend()));
  CHECK(lib.macros().back() == "top");
}

TEST_CASE("an extension other than .lef is kept, a dot in a directory is not one",
          "[cap:lef-write][req:Output file naming]") {
  Shown w;
  REQUIRE(w.s.run("lef write out.lef").ok());
  CHECK(w.s.exists("out.lef"));
  REQUIRE(w.s.run("lef write dir.v1/out").ok());
  CHECK(w.s.exists("dir.v1/out.lef"));
}

TEST_CASE("writeall reads PROPERTYDEFINITIONS from Magic's last stack block only with magic-compat ON",
          "[cap:lef-write][req:Library header]") {
  for (const bool on : {true, false}) {
    Session s(on);
    db::CellDef& top = s.cell("top");
    for (int i = 0; i < 101; ++i) {
      db::CellDef& c = s.cell("c" + std::to_string(i));
      s.paint(c, "metal1", db::Rect{0, 0, 4, 4});
      s.property(c, "LEFproperties", "K" + std::to_string(i) + " 1");
      s.place(top, c, db::Transform{1, 0, 10 * i, 0, 1, 0});
    }
    LefWriteOptions o;
    o.top_cell = false;
    o.files = &s.services.view().files;
    REQUIRE(lef::write_all(s.editor, top, "lib", o).ok());
    const std::string defs = block(s.read("lib.lef"), "PROPERTYDEFINITIONS", "END PROPERTYDEFINITIONS");
    std::size_t keys = 0;
    for (std::size_t at = defs.find("  MACRO "); at != std::string::npos; at = defs.find("  MACRO ", at + 1)) {
      ++keys;
    }
    CHECK(keys == (on ? 1U : 101U));
  }
}

TEST_CASE("lef write names files from the name given or the cell", "[cap:lef-write][req:Output file naming]") {
  Shown w;
  REQUIRE(w.s.run("lef write cells.tlef").ok());
  CHECK(w.s.exists("cells.tlef"));
  CHECK_FALSE(w.s.exists("cells.lef"));
  REQUIRE(w.s.run("lef write").ok());
  CHECK(w.s.texts("LEF-WRITE-001").back() == "Diagnostic:  Scale value is 0.010000");
  CHECK(std::find(w.s.texts("LEF-WRITE-001").begin(), w.s.texts("LEF-WRITE-001").end(),
                  "Generating LEF output top.lef for cell top:") != w.s.texts("LEF-WRITE-001").end());
}

TEST_CASE("a name that cannot be opened as given gets .lef, then the cell name with magic-compat ON",
          "[cap:lef-write][req:Output file naming]") {
  for (const bool on : {true, false}) {
    Shown w(on);
    FailingFiles files(w.s.services.view().files, {"/nodir/", "x.v1"});
    LefWriteOptions o;
    o.files = &files;
    REQUIRE(lef::write(w.s.editor, *w.top, "x.v1", o).ok());
    CHECK(w.s.exists("x.lef"));
    const auto r = lef::write(w.s.editor, *w.top, "/nodir/x", o);
    CHECK(r.ok() == on);
    CHECK(w.s.exists("top.lef") == on);
    if (!on) {
      REQUIRE(r.diagnostics.size() == 2);
      CHECK(r.diagnostics[0].text == "Generating LEF output /nodir/x.lef for cell top:");
      CHECK(r.diagnostics[1].text == "Cannot open output file /nodir/x.lef (refused)." );
    }
  }
}

TEST_CASE("a failed open changes nothing", "[cap:lef-write][req:Write side effects]") {
  Shown w(false);
  FailingFiles files(w.s.services.view().files, {"/nodir/"});
  LefWriteOptions o;
  o.files = &files;
  w.s.editor.lefdef.units = LefUnits{100};
  REQUIRE_FALSE(lef::write(w.s.editor, *w.top, "/nodir/x", o).ok());
  CHECK(w.s.editor.lefdef.units == LefUnits{100});
}

TEST_CASE("writeall names its file from the root's file path", "[cap:lef-write][req:Output file naming]") {
  Library lib;
  lib.top->set_file_path("/work/top");
  LefWriteOptions o;
  o.files = &lib.s.services.view().files;
  const auto r = lef::write_all(lib.s.editor, *lib.top, {}, o);
  REQUIRE(r.ok());
  CHECK(lib.s.exists("/work/top.lef"));
  REQUIRE_FALSE(r.diagnostics.empty());
  CHECK(r.diagnostics.front().text == "Generating LEF output /work/top.lef for hierarchy rooted at cell top:");
  // A layout path never becomes the LEF name.
  lib.top->set_file_path("/work/top2.mag");
  REQUIRE(lef::write_all(lib.s.editor, *lib.top, {}, o).ok());
  CHECK(lib.s.exists("/work/top2.lef"));
}

TEST_CASE("writeall stops when a cell of the subtree cannot be read", "[cap:lef-write][req:lef writeall arguments and scope]") {
  Library lib;
  lib.nmos4->flags().available = false;
  const auto r = lib.write_all(true, true);
  REQUIRE_FALSE(r.ok());
  REQUIRE(r.diagnostics.size() == 2);
  CHECK(r.diagnostics[0].text == "Could not read entire subtree of the cell.");
  CHECK(r.diagnostics[1].text == "Failed on cell nmos4.");
  CHECK_FALSE(lib.s.exists("lib.lef"));
}

TEST_CASE("regression: lef write reads the subcells it flattens", "[cap:lef-write][req:Macro source geometry and layer masks]") {
  // `load buf` leaves the subcells unread; lef write flattened them as
  // empty (LEFDEF-128). Magic's DBCellCopyAllPaint reads them first.
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s{on};
    db::CellDef& top = s.cell("top");
    db::CellDef& leaf = s.cell("leaf");
    s.paint(leaf, "metal2", db::Rect{0, 0, 10, 10});  // gives the use its box
    s.place(top, leaf);
    leaf.flags().available = false;
    std::vector<std::string> asked;
    LefWriteOptions o;
    o.loader.read = [&](db::CellDef& def) {
      asked.push_back(def.name());
      def.flags().available = true;
      s.paint(def, "metal1", db::Rect{0, 0, 10, 10});
      return true;
    };
    const std::string text = s.macro(top, o);
    CHECK(asked == std::vector<std::string>{"leaf"});
    CHECK(text.find("LAYER met1 ;") != std::string::npos);
  }
}

TEST_CASE("lef nocheck keeps a list of nets", "[cap:lef-write][req:lef nocheck net list]") {
  Shown w;
  const auto empty = w.s.run("lef nocheck");
  REQUIRE(empty.ok());
  CHECK(empty.value == cmd::Value{"none"});
  REQUIRE(w.s.run("lef nocheck VPWR none VGND").ok());
  CHECK(w.s.editor.lefdef.nocheck == std::vector<std::string>{"VGND"});
  REQUIRE(w.s.run("lef nocheck a NONE b c").ok());
  CHECK(w.s.editor.lefdef.nocheck == std::vector<std::string>{"c", "b"});
  CHECK(w.s.run("lef nocheck").value == cmd::Value{cmd::Value::List{"c", "b"}});
}

TEST_CASE("lef help lists the lef options", "[cap:lef-write][req:Help text]") {
  Shown w;
  REQUIRE(w.s.run("lef help").ok());
  CHECK(w.printed() == help_text());
}

TEST_CASE("a write leaves the cell, the undo log and the cell list alone and clears the selection",
          "[cap:lef-write][req:Write side effects]") {
  Shown w;
  db::CellDef& inv = w.s.cell("inv");
  w.s.paint(inv, "metal1", db::Rect{0, 0, 10, 10});
  w.s.place(*w.top, inv, db::Transform{1, 0, 200, 0, 1, 0});
  w.top->flags().modified = false;
  w.s.editor.selection.set_root(w.top, w.s.editor.technology->plane_count());
  REQUIRE(db::paint_type(*w.s.editor.selection.def(), *w.s.editor.technology, db::Rect{0, 0, 5, 5},
                         w.s.type("metal1"))
              .ok());
  REQUIRE_FALSE(w.s.editor.selection.empty());
  const std::size_t cells = w.s.editor.cells.size();
  const std::size_t labels = w.top->labels().size();
  const db::Rect bbox = w.top->bbox();
  const std::size_t steps = w.s.editor.undo.step_count();
  REQUIRE(w.s.write(*w.top, "top").ok());
  CHECK(w.s.editor.undo.step_count() == steps);
  CHECK(w.s.editor.selection.empty());
  CHECK(w.s.editor.cells.size() == cells);
  CHECK(w.top->labels().size() == labels);
  CHECK(w.top->bbox() == bbox);
  CHECK_FALSE(w.top->flags().modified);
}

TEST_CASE("an interrupt skips the remaining macros of writeall", "[cap:lef-write][req:Write side effects]") {
  Library lib;
  std::atomic<bool> cancel{true};
  mask::Progress progress;
  progress.cancel = &cancel;
  LefWriteOptions o;
  o.files = &lib.s.services.view().files;
  o.progress = &progress;
  REQUIRE(lef::write_all(lib.s.editor, *lib.top, "lib", o).ok());
  const std::string text = lib.s.read("lib.lef");
  CHECK(text.find("MACRO") == std::string::npos);
  CHECK(text.ends_with("END LIBRARY\n\n"));
}

}  // namespace
}  // namespace hefesto::lef::write_test
