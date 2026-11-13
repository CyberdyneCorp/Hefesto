// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CELL commands through the dispatcher, scripted like the inputs of
// tests/golden/CELL/CASES.md (tiny technology, in-memory files).

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "command_session.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/view/commands.hpp"

namespace cell = hefesto::cell;
namespace cmd = hefesto::cmd;
namespace db = hefesto::db;

namespace {

using Session = hefesto::cell::test::CommandSession;

std::string text_of(const cmd::CommandResult& r) {
  return hefesto::cell::test::last_text(r);
}

}  // namespace

TEST_CASE("load reads the cell, makes it the edit cell; save writes it back",
          "[cap:cell-file-format][req:Overall file grammar][golden:CELL-RD-01]") {
  Session s;
  const std::string text = "magic\ntech tiny\ntimestamp 5\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n";
  REQUIRE(s.files().write_atomic("/work/a.mag", text).ok());
  REQUIRE(s.run("load", {"a"}).ok());
  REQUIRE(s.editor.edit_cell != nullptr);
  CHECK(s.editor.edit_cell->name() == "a");
  CHECK(s.editor.edit_cell->bbox() == db::Rect{0, 0, 4, 4});
  REQUIRE(s.run("save").ok());
  CHECK(s.files().read("/work/a.mag").value() == text);  // unmodified: same stamp
}

TEST_CASE("save with a new name writes the file and renames the cell", "[cap:cell-io-safety][req:Write refusal for protected cells]") {
  Session s;
  REQUIRE(s.run("load", {"fresh"}).ok());
  REQUIRE(s.run("save", {"/work/other"}).ok());
  CHECK(s.editor.cells.find("other") != nullptr);
  CHECK(s.files().exists("/work/other.mag"));
}

TEST_CASE("save of (UNNAMED) needs a name", "[cap:cell-io-safety][req:Write refusal for protected cells]") {
  Session s;
  REQUIRE(s.run("load").ok());
  const auto r = s.run("save");
  REQUIRE_FALSE(r.ok());
  CHECK(text_of(r) == "Must specify name for cell (UNNAMED).");
}

TEST_CASE("writeall force saves modified cells and skips read-only ones",
          "[cap:cell-io-safety][req:Write refusal for protected cells][golden:CELL-IO-03]") {
  Session s;
  REQUIRE(s.files().write_atomic("/work/a.mag", "magic\ntech tiny\ntimestamp 5\n<< end >>\n").ok());
  REQUIRE(s.files().write_atomic("/work/b.mag", "magic\ntech tiny\ntimestamp 6\n<< end >>\n").ok());
  REQUIRE(s.run("load", {"a"}).ok());
  REQUIRE(s.run("load", {"b"}).ok());
  db::CellDef* a = s.editor.cells.find("a");
  db::CellDef* b = s.editor.cells.find("b");
  db::mark_modified(*a);
  db::mark_modified(*b);
  b->flags().no_edit = true;
  const auto r = s.run("writeall", {"force"});
  REQUIRE(r.ok());
  CHECK_FALSE(a->flags().modified);
  CHECK(b->flags().modified);
  bool skipped = false;
  for (const auto& d : r.diagnostics) {
    skipped = skipped || d.text == "Cell 'b' is not writeable;  modified cell not written.";
  }
  CHECK(skipped);
  CHECK(s.files().read("/work/b.mag").value() == "magic\ntech tiny\ntimestamp 6\n<< end >>\n");
}

// Regression (UI-G-033, UI-G-036): `openwindow PATH/NAME` reads the cell as
// DBWloadWindow does: the cell is NAME, an existing one is reused, and the
// edit cell stays.
TEST_CASE("openwindow reads its cell through the search paths", "[cap:ui-windows-and-views][req:Opening and closing windows]") {
  Session s;
  REQUIRE(hefesto::view::register_view_commands(s.registry));
  REQUIRE(s.files().write_atomic("/work/lib/nand2.mag", "magic\ntech tiny\ntimestamp 5\n<< end >>\n").ok());
  REQUIRE(s.files().write_atomic("/work/a.mag", "magic\ntech tiny\ntimestamp 5\n<< end >>\n").ok());
  REQUIRE(s.run("load", {"a"}).ok());
  db::CellDef* a = s.editor.cells.find("a");
  const auto opened = s.run("openwindow", {"lib/nand2"});
  REQUIRE(opened.ok());
  db::CellDef* nand2 = s.editor.cells.find("nand2");
  REQUIRE(nand2 != nullptr);
  CHECK(nand2->flags().available);
  CHECK(s.editor.cells.find("lib/nand2") == nullptr);
  CHECK(s.editor.windows.current()->root() == nand2);
  CHECK(s.editor.edit_cell == a);
  REQUIRE(s.run("openwindow", {"a"}).ok());
  CHECK(s.editor.windows.current()->root() == a);  // reused, not a new "a"
}

TEST_CASE("path and addpath show, set and append the search paths",
          "[cap:cell-library-resolution][req:Search path syntax and expansion]") {
  Session s;
  REQUIRE(s.run("path", {"search", "/a:/b"}).ok());
  REQUIRE(s.run("addpath", {"/c"}).ok());
  REQUIRE(s.run("path", {"cell", "+/lib"}).ok());
  CHECK(s.run("path", {"search"}).value.to_string() == "/a /b /c");
  CHECK(s.run("path", {"cell"}).value.to_string() == "/lib");
  CHECK_FALSE(s.run("path", {"bogus", "x"}).ok());
  // Regression (CMDEDIT-HIER-10): `path` alone prints three lines and
  // returns nothing.
  const auto all = s.run("path", {});
  CHECK(all.value.is_none());
  REQUIRE_FALSE(all.diagnostics.empty());
  CHECK(all.diagnostics.back().text.starts_with("Search path for cells is \"/a /b /c\"\nCell library search path is \"/lib\"\n"));
}

TEST_CASE("locking shows and sets the setting with unique prefixes",
          "[cap:cell-io-safety][req:Advisory locking between sessions]") {
  Session s;
  CHECK(s.run("locking").value.to_string() == "enabled");
  REQUIRE(s.run("locking", {"of"}).ok());
  CHECK(s.run("locking").value.to_string() == "disabled");
  REQUIRE(s.run("locking", {"1"}).ok());
  CHECK(s.run("locking").value.to_string() == "enabled");
  CHECK_FALSE(s.run("locking", {"maybe"}).ok());
}

TEST_CASE("crashbackups disable then resume schedules nothing", "[cap:ui-gui-managers][req:Crash backup timer]") {
  Session s;
  auto& session = s.editor.cell_session;
  REQUIRE(s.run("crashbackups").ok());
  CHECK(session.backup_timer == cell::CellSession::BackupTimer::running);
  REQUIRE(s.run("crashbackups", {"disable"}).ok());
  REQUIRE(s.run("crashbackups", {"resume"}).ok());
  CHECK(session.backup_timer == cell::CellSession::BackupTimer::disabled);
  CHECK_FALSE(session.backup_interval_ms.has_value());
  REQUIRE(s.run("crashbackups", {"start"}).ok());
  CHECK(session.backup_interval_ms == 600000);
}

// DBWloadWindow deletes the window's untouched "(UNNAMED)" only while the
// display runs: with NULL graphics (`magic -dnull`, hefesto-cli) Magic keeps
// it, under X11 `cellname list topcells` no longer lists it after `load top`
// (both checked in Magic 8.3.694). Regression: the desktop's Cells manager
// listed "(UNNAMED)" next to the loaded cell.
TEST_CASE("load deletes an untouched (UNNAMED) while the display runs", "[cap:cmd-hierarchy][req:Load command]") {
  Session s;
  REQUIRE(s.editor.windows.open_frame(nullptr).ok());
  REQUIRE(s.run("load").ok());
  REQUIRE(s.editor.windows.current()->root() == s.editor.cells.find("(UNNAMED)"));

  SECTION("display running") {
    s.editor.display_running = true;
    REQUIRE(s.run("load", {"top"}).ok());
    db::CellDef* top = s.editor.cells.find("top");
    REQUIRE(top != nullptr);
    CHECK(s.editor.cells.find("(UNNAMED)") == nullptr);
    CHECK(s.editor.cells.find("__UNNAMED__") == nullptr);
    CHECK(s.editor.windows.current()->root() == top);
    CHECK(s.editor.edit_cell == top);
  }
  SECTION("NULL graphics keeps it") {
    REQUIRE(s.run("load", {"top"}).ok());
    CHECK(s.editor.cells.find("(UNNAMED)") != nullptr);
  }
  SECTION("a modified (UNNAMED) is kept") {
    s.editor.display_running = true;
    s.editor.cells.find("(UNNAMED)")->flags().modified = true;
    REQUIRE(s.run("load", {"top"}).ok());
    CHECK(s.editor.cells.find("(UNNAMED)") != nullptr);
  }
  SECTION("a suspended display keeps it") {
    s.editor.display_running = true;
    s.editor.windows.set_display_suspended(true);
    REQUIRE(s.run("load", {"top"}).ok());
    CHECK(s.editor.cells.find("(UNNAMED)") != nullptr);
  }
}

// Regression: the desktop opens its layout window before the command-line
// cell is loaded, with no root. `load` created "(UNNAMED)" for it without
// showing it, so DBWloadWindow's deleteDef rule never saw it and the Cells
// manager listed "(UNNAMED)" next to the cell. Magic's DBWcreate shows
// "(UNNAMED)" in every new window, and the load deletes it while the display
// runs (Magic 8.3.694 under X11: `magic -T scmos top` lists only `top`).
TEST_CASE("a window opened without a cell loses its (UNNAMED) to the first load",
          "[cap:cmd-hierarchy][req:Load command]") {
  Session s;
  REQUIRE(s.editor.windows.open_frame(nullptr).ok());
  REQUIRE(s.editor.windows.current()->root() == nullptr);
  SECTION("display running: only the loaded cell is left") {
    s.editor.display_running = true;
    REQUIRE(s.run("load", {"top"}).ok());
    db::CellDef* top = s.editor.cells.find("top");
    REQUIRE(top != nullptr);
    CHECK(s.editor.cells.find("(UNNAMED)") == nullptr);
    CHECK(s.editor.cells.find("__UNNAMED__") == nullptr);
    CHECK(s.editor.windows.current()->root() == top);
    CHECK(s.editor.edit_cell == top);
    CHECK(s.editor.cells.size() == 1U);
  }
  SECTION("NULL graphics keeps it, as Magic's batch runs do") {
    REQUIRE(s.run("load", {"top"}).ok());
    CHECK(s.editor.cells.find("(UNNAMED)") != nullptr);
    CHECK(s.editor.windows.current()->root() == s.editor.cells.find("top"));
  }
}

// DBWloadWindow cuts `.gz` and `.mag` off the name in place, so the path it
// records (and prints when the file is missing) has neither (Magic 8.3.694:
// `magic -dnull /w/inv.mag.gz` prints "File /w/inv couldn't be read").
TEST_CASE("load records a missing cell's path without its .mag.gz extension",
          "[cap:cell-library-resolution][req:Missing cell handling]") {
  Session s;
  const auto r = s.run("load", {"/work/sub/inv.mag.gz"});
  REQUIRE(r.ok());
  CHECK(hefesto::cell::test::has_text(r, "File /work/sub/inv couldn't be read"));
  db::CellDef* inv = s.editor.cells.find("inv");
  REQUIRE(inv != nullptr);
  CHECK(inv->file_path().generic_string() == "/work/sub/inv");
  // A name with only `.mag` is a plain cell name: no recorded path.
  const auto plain = s.run("load", {"nand.mag"});
  CHECK(hefesto::cell::test::has_text(plain, "Cell nand couldn't be read"));
}
