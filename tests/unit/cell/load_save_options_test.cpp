// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cmd-hierarchy leftovers through the dispatcher: `load ... scaled N [D]`
// and unknown load options, `save NAME` over an existing file or cell name
// (cmdCheckNewName), `cellname dereference`, the logged form of `down`, the
// selection and box `getcell`/`dump` leave (through the SELECT API, undoable,
// redrawn), and `select save` through cell::save_def_as.

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cmd/prompt.hpp"
#include "hefesto/select/commands.hpp"
#include "hefesto/view/highlights.hpp"
#include "hier_session.hpp"

namespace cell = hefesto::cell;
namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
using db::Rect;
using Session = cell::test::HierSession;
using cell::test::last_message;
using cell::test::mentions;

namespace {

constexpr std::string_view kSquare = "magic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n";

struct AnswerHost final : cmd::PromptHost {
  std::size_t answer = 0;
  int asked = 0;
  std::string question;
  std::size_t ask(std::string_view q, std::span<const std::string>, std::size_t) override {
    ++asked;
    question = std::string{q};
    return answer;
  }
};

}  // namespace

TEST_CASE("load scaled reads the cell and its subcells at N/D of their size",
          "[cap:cmd-hierarchy][req:Load command]") {
  Session s;
  s.write("/work/leaf.mag", std::string{kSquare});
  s.write("/work/big.mag",
          "magic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 2 2\n"
          "use leaf leaf_0\ntimestamp 5\ntransform 1 0 10 0 1 0\nbox 0 0 4 4\n<< end >>\n");
  const hefesto::tech::GridScale grid = s.editor.grid;
  const auto r = s.run("load big scaled 3");
  REQUIRE(r.ok());
  CHECK(mentions(r, "Recursively reading all cells at new scale."));
  db::CellDef* big = s.editor.cells.find("big");
  db::CellDef* leaf = s.editor.cells.find("leaf");
  REQUIRE(big != nullptr);
  REQUIRE(leaf != nullptr);
  CHECK(leaf->flags().available);  // the whole hierarchy was read
  CHECK(leaf->bbox() == Rect{0, 0, 12, 12});
  CHECK(big->uses().front()->transform == db::translation(30, 0));
  CHECK(big->bbox() == Rect{0, 0, 42, 12});
  CHECK(s.editor.grid == grid);  // the read ratio is restored
  CHECK(s.editor.windows.current()->root() == big);

  s.write("/work/half.mag", "magic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 8 8\n<< end >>\n");
  // D > 1 makes the reader refine the internal grid instead of dividing,
  // as Magic's dbReadOpen does; the command itself succeeds.
  s.ok("load half scaled 1 2");

  CHECK(last_message(s.run("load half scaled 2 x")).starts_with("Usage: load name scaled n [d]"));
  CHECK(last_message(s.run("load half extra")).starts_with("Usage: load name [scaled n [d]]"));
}

TEST_CASE("load reports an unknown option and goes on", "[cap:cmd-hierarchy][req:Load command]") {
  Session s;
  s.write("/work/a.mag", std::string{kSquare});
  const auto r = s.run("load a -bogus");
  REQUIRE(r.ok());
  CHECK(mentions(r, "No such option \"-bogus\"."));
  CHECK(s.editor.windows.current()->root() == s.editor.cells.find("a"));
}

TEST_CASE("load takes options from the end of the line only", "[cap:cmd-hierarchy][req:Load command]") {
  // Regression (M2 review): an option before the name was consumed as an
  // option; CmdLoad strips options from the end only, so `-x` is the name.
  Session s;
  s.write("/work/a.mag", std::string{kSquare});
  const auto r = s.run("load -x a");
  CHECK_FALSE(mentions(r, "No such option"));
  CHECK(s.editor.windows.current()->root() != s.editor.cells.find("a"));
}

TEST_CASE("save into a directory compares the name with its directory", "[cap:cmd-hierarchy][req:Save command]") {
  // Regression (M2 review): the directory was stripped before comparing with
  // the cell name, so `save sub/top` overwrote sub/top.mag without asking.
  Session s;
  s.paint(*s.top, "metal1", Rect{0, 0, 10, 10});
  s.write("sub/top.mag", std::string{kSquare});
  const auto r = s.run("save sub/top");
  CHECK(mentions(r, "Cell not saved."));
  CHECK(s.services.files().read("sub/top.mag").value() == kSquare);
  AnswerHost host;
  host.answer = 1;
  s.editor.prompt = &host;
  REQUIRE(s.run("save sub/top.mag").ok());
  CHECK(host.asked == 1);
  CHECK(s.services.files().read("sub/top.mag").value() != kSquare);
  CHECK(s.top->name() == "top");
}

TEST_CASE("save NAME asks before overwriting another file and refuses a taken cell name",
          "[cap:cmd-hierarchy][req:Save command]") {
  Session s;
  s.paint(*s.top, "metal1", Rect{0, 0, 10, 10});
  s.write("other.mag", std::string{kSquare});
  // Headless: the default answer "no" abandons the save.
  auto r = s.run("save other");
  CHECK(mentions(r, "Cell not saved."));
  CHECK(s.top->name() == "top");
  CHECK(s.services.files().read("other.mag").value() == kSquare);

  AnswerHost host;
  host.answer = 1;  // yes
  s.editor.prompt = &host;
  r = s.run("save other");
  REQUIRE(r.ok());
  CHECK(host.asked == 1);
  CHECK(host.question == "File other.mag already exists.\n  Overwrite it with top? ");
  CHECK(s.top->name() == "other");
  CHECK(s.services.files().read("other.mag").value() != kSquare);

  // A cell of that name exists: no file is written, the cell keeps its name.
  s.cell("taken");
  r = s.run("save taken");
  CHECK(mentions(r, "Can't rename cell 'other' to 'taken' because that cell already exists."));
  CHECK(mentions(r, "Cell not saved."));
  CHECK_FALSE(s.services.files().exists("taken.mag"));
  CHECK(s.top->name() == "other");
}

TEST_CASE("cellname dereference re-reads a cell through the search paths",
          "[cap:cmd-hierarchy][req:Cellname and instance commands]") {
  Session s;
  s.write("/lib/a.mag", "magic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 20 20\n<< end >>\n");
  s.write("/work/a.mag", std::string{kSquare});
  s.ok("load /lib/a");
  db::CellDef* a = s.editor.cells.find("a");
  REQUIRE(a != nullptr);
  CHECK(a->bbox() == Rect{0, 0, 20, 20});
  s.ok("cellname dereference a");
  CHECK(a->bbox() == Rect{0, 0, 4, 4});  // found on the search path /work
  CHECK(s.editor.selection.empty());
  CHECK(last_message(s.run("cellname dereference nosuch")) == "No such cell \"nosuch\"");
  CHECK(last_message(s.run("cellname dereference")) == "Dereference cell command missing cellname");
}

TEST_CASE("down at the selection is logged with the instance it went down into",
          "[cap:cmd-hierarchy][req:Down command][cap:ui-command-dispatch][req:Command logging]") {
  Session s;
  db::CellDef* inv = s.cell("inv");
  s.paint(*inv, "metal1", Rect{0, 0, 10, 10});
  db::CellUse* x1 = s.place(*s.top, *inv, db::translation(100, 0), "x1");
  s.select_use(*x1);
  const auto r = s.run("down");
  REQUIRE(r.ok());
  REQUIRE(r.logged_as.has_value());
  CHECK(r.logged_as->name == "down");
  CHECK(r.logged_as->args == std::vector<std::string>{"x1"});
  s.ok("load top");
  CHECK_FALSE(s.run("down x1").logged_as.has_value());  // an explicit path is logged as typed
}

TEST_CASE("getcell's selection and box changes are undone with the placement and redrawn",
          "[cap:cmd-hierarchy][req:Getcell command][cap:cmd-selection][req:Selection model]") {
  Session s;
  db::CellDef* inv = s.cell("inv");
  s.paint(*inv, "metal1", Rect{0, 0, 10, 10});
  s.editor.box = Rect{30, 40, 31, 41};
  s.editor.box_root = s.top;
  (void)s.editor.highlights.take_frame(s.window);
  s.ok("getcell inv");
  REQUIRE(s.editor.selection.def() != nullptr);
  CHECK(s.editor.selection.def()->uses().size() == 1);
  CHECK(s.editor.highlights.frame(s.window) != nullptr);  // box and selection redrawn
  REQUIRE(s.editor.undo.undo(s.editor).ok());
  CHECK(s.top->uses().empty());
  CHECK(s.editor.selection.empty());
  CHECK(s.editor.box == Rect{30, 40, 31, 41});
}

TEST_CASE("select save writes the selection as a cell file", "[cap:cmd-selection][req:Selection utility options]") {
  Session s;
  REQUIRE(hefesto::select::register_select_commands(s.registry));
  s.paint(*s.top, "metal1", Rect{0, 0, 10, 10});
  s.box(0, 0, 10, 10);
  s.ok("select area metal1");
  s.ok("select save picked");
  const auto text = s.services.files().read("picked.mag");
  REQUIRE(text.ok());
  CHECK(text.value().find("rect 0 0 10 10") != std::string::npos);
  CHECK(s.editor.cells.find("picked") == nullptr);  // the selection cell stays internal
}
