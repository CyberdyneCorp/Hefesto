// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Timestamps (cell-timestamps), I/O safety helpers (cell-io-safety), cell
// loading and saving through the search paths (cell-library-resolution).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "cell_fixture.hpp"
#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"

namespace cell = hefesto::cell;
namespace db = hefesto::db;
using cell::test::CellIo;

namespace {

db::CellDef& make(CellIo& io, const std::string& name) {
  return *io.library.create(name, cell::test::tiny_technology()->plane_count()).value();
}

void place(db::CellDef& parent, db::CellDef& child) {
  db::PlaceOptions opts;
  REQUIRE(db::place(parent, child, opts).ok());
}

}  // namespace

TEST_CASE("stamps propagate to ancestors", "[cap:cell-timestamps][req:Generating new stamps]") {
  CellIo io;
  db::CellDef& top = make(io, "top");
  db::CellDef& mid = make(io, "mid");
  db::CellDef& leaf = make(io, "leaf");
  place(top, mid);
  place(mid, leaf);
  for (db::CellDef* d : {&top, &mid, &leaf}) {
    d->set_timestamp(100);
    d->flags().needs_new_timestamp = false;
  }
  leaf.flags().needs_new_timestamp = true;
  cell::update_timestamps(io.library, nullptr, 200);
  CHECK(leaf.timestamp() == 200);
  CHECK(mid.timestamp() == 200);
  CHECK(top.timestamp() == 200);
  CHECK(mid.flags().child_timestamps_changed);
  CHECK(top.flags().child_timestamps_changed);
}

TEST_CASE("read-only children do not propagate, fixed stamps do",
          "[cap:cell-timestamps][req:Generating new stamps][req:Fixed timestamps]") {
  CellIo io;
  db::CellDef& top = make(io, "top");
  db::CellDef& pdk = make(io, "pdk");
  db::CellDef& lef = make(io, "lef");
  place(top, pdk);
  place(top, lef);
  top.set_timestamp(100);
  pdk.set_timestamp(50);
  lef.set_timestamp(7);
  for (db::CellDef* d : {&top, &pdk, &lef}) {
    d->flags().needs_new_timestamp = false;
  }
  pdk.flags().no_edit = true;
  pdk.flags().needs_new_timestamp = true;
  cell::update_timestamps(io.library, nullptr, 300);
  CHECK(pdk.timestamp() == 50);
  CHECK_FALSE(pdk.flags().needs_new_timestamp);
  CHECK(top.timestamp() == 100);

  lef.flags().fixed_timestamp = true;
  lef.flags().needs_new_timestamp = true;
  cell::update_timestamps(io.library, nullptr, 300);
  CHECK(lef.timestamp() == 7);
  CHECK(top.timestamp() == 300);
}

TEST_CASE("a single-cell update leaves a cell without a pending stamp alone",
          "[cap:cell-timestamps][req:Generating new stamps][regression]") {
  // Regression: update_timestamps(library, def, t) stamped `def` even when it
  // was not pending, so `save` of an unmodified cell wrote a new timestamp.
  CellIo io;
  db::CellDef& a = make(io, "a");
  a.set_timestamp(5);
  a.flags().needs_new_timestamp = false;
  cell::update_timestamps(io.library, &a, 99);
  CHECK(a.timestamp() == 5);
  a.flags().needs_new_timestamp = true;
  cell::update_timestamps(io.library, &a, 99);
  CHECK(a.timestamp() == 99);
}

TEST_CASE("explicit stamps are not fixed in magic-compat mode", "[cap:cell-timestamps][req:Fixed timestamps]") {
  CellIo io;
  db::CellDef& a = make(io, "a");
  a.flags().needs_new_timestamp = true;
  cell::set_explicit_timestamp(a, 5, true);
  CHECK(a.timestamp() == 5);
  CHECK_FALSE(a.flags().needs_new_timestamp);
  CHECK_FALSE(a.flags().fixed_timestamp);
  cell::set_explicit_timestamp(a, 6, false);
  CHECK(a.flags().fixed_timestamp);
}

TEST_CASE("stamps are signed 32-bit seconds", "[cap:cell-timestamps][req:Timestamp value domain]") {
  using namespace std::chrono;
  CHECK(cell::stamp_from_time(system_clock::time_point{seconds{1723858470}}) == 1723858470);
  CHECK(cell::stamp_as_written(0x100000005LL) == 5);
  CHECK(cell::stamp_as_written(0xFFFFFFFFLL) == -1);
}

TEST_CASE("a child edited out of context records a mismatch and is reported",
          "[cap:cell-timestamps][req:Timestamps on cell read][req:Deferred mismatch processing]") {
  CellIo io;
  db::CellDef& top = io.read("top", "magic\ntech tiny\ntimestamp 200\nuse inv inv_0\ntimestamp 100\n"
                                    "transform 1 0 0 0 1 0\nbox 0 0 8 12\n<< end >>\n");
  db::CellDef* inv = io.library.find("inv");
  REQUIRE(inv != nullptr);
  CHECK(inv->timestamp() == 100);
  REQUIRE(cell::read_mag(io.ctx, *inv, "magic\ntech tiny\ntimestamp 150\n<< metal1 >>\nrect 0 0 10 12\n<< end >>\n")
              .ok());
  CHECK(inv->timestamp() == 150);
  CHECK(top.flags().child_timestamps_changed);
  REQUIRE_FALSE(io.stamps.empty());
  const auto report = cell::process_mismatches(io.stamps, [](db::CellDef&) {});
  CHECK(report.messages() ==
        std::vector<std::string>{"Processing timestamp mismatches.", "Timestamp mismatches found in these cells: inv."});
  REQUIRE(report.rechecks.size() == 2);
  CHECK(report.rechecks[0].area == db::Rect{0, 0, 8, 12});
  CHECK(report.rechecks[1].area == db::Rect{0, 0, 10, 12});
}

TEST_CASE("a root cell is never mismatched", "[cap:cell-timestamps][req:Timestamps on cell read]") {
  CellIo io;
  (void)io.read("a", "magic\ntech tiny\ntimestamp 150\n<< end >>\n");
  CHECK(io.stamps.empty());
}

TEST_CASE("backup files parse and render with sorted records",
          "[cap:cell-io-safety][req:Crash backup file format][req:Backup and archive reading]") {
  const std::string text =
      "file /p/b.mag\nmagic\ntech tiny\ntimestamp 1\n<< end >>\n"
      "file a\nmagic\ntech tiny\ntimestamp 2\n<< end >>\nend top\n";
  const auto parsed = cell::parse_backup_file(text, "backup", "f");
  REQUIRE(parsed.ok());
  REQUIRE(parsed.value().entries.size() == 2);
  CHECK(parsed.value().entries[0].cell == "b");
  CHECK(parsed.value().root_cell == "top");
  const std::string rendered = cell::render_backup_file(parsed.value().entries, "top");
  CHECK(rendered.find("file a\n") < rendered.find("file /p/b.mag\n"));
  CHECK(rendered.ends_with("<< end >>\nend top\n"));

  const auto bad = cell::parse_backup_file("magic\n", "archive", "x");
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().text == "Error in archive file x; expected keyword \"file\", got \"magic\"!");
  CHECK_FALSE(cell::parse_backup_file("file a\nmagic\n", "backup", "x").ok());
  CHECK(cell::backup_cell_name("/x/inv.mag.d/inv.mag") == "inv");  // Q-CELL-31
}

TEST_CASE("crash backup policy, archive policy and locations",
          "[cap:cell-io-safety][req:Crash backup policy][req:Archive files][req:Crash backup location and session lock]") {
  CellIo io;
  db::CellDef& a = make(io, "a");
  a.flags().available = true;
  CHECK_FALSE(cell::crash_backup_includes(a));
  CHECK(cell::archive_includes(a, false));
  a.flags().modified = true;
  CHECK(cell::crash_backup_includes(a));
  a.flags().no_edit = true;
  CHECK_FALSE(cell::crash_backup_includes(a));
  CHECK_FALSE(cell::archive_includes(a, false));
  CHECK(cell::archive_includes(a, true));
  CHECK(cell::crash_backup_directory(false, std::nullopt, "/home/u") == "/home/u/.local/state/hefesto/crash-backups");
  CHECK(cell::crash_backup_directory(false, std::string{"/s"}, "/home/u") == "/s/hefesto/crash-backups");
  CHECK(cell::crash_backup_directory(true, std::nullopt, "/Users/u") ==
        "/Users/u/Library/Application Support/Hefesto/CrashBackups");
  CHECK(cell::magic_backup_pid("MAG4242.abc123") == 4242);
  CHECK_FALSE(cell::magic_backup_pid("MAGx.1").has_value());
}

TEST_CASE("two sessions in one process see each other's locks",
          "[cap:cell-io-safety][req:Advisory locking between sessions]") {
  auto registry = cell::make_lock_registry();
  auto a = cell::make_in_process_lock_service(registry);
  auto b = cell::make_in_process_lock_service(registry);
  CHECK(a->acquire("c.mag").value());
  CHECK(a->acquire("c.mag").value());  // never locked against itself
  std::string owner;
  CHECK_FALSE(b->acquire("c.mag", &owner).value());
  CHECK_FALSE(owner.empty());
  a->release("c.mag");
  CHECK(b->acquire("c.mag").value());
  CHECK(b->holds("c.mag"));
}

TEST_CASE("load finds cells through the search path; expansion reads children",
          "[cap:cell-library-resolution][req:Cell file lookup order][req:Cell file path bookkeeping]") {
  CellIo io;
  hefesto::platform::MemoryFileSystem& mem = dynamic_cast<hefesto::platform::MemoryFileSystem&>(io.ctx.fs);
  REQUIRE(mem.write_atomic("/lib/inv.mag", "magic\ntech tiny\ntimestamp 3\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n")
              .ok());
  REQUIRE(mem.write_atomic("/work/top.mag", "magic\ntech tiny\ntimestamp 9\nuse inv inv_0\ntimestamp 3\n"
                                            "transform 1 0 0 0 1 0\nbox 0 0 4 4\n<< end >>\n")
              .ok());
  io.paths.path = {"/work", "/lib"};
  const auto top = cell::load_cell(io.ctx, "top");
  REQUIRE(top.ok());
  CHECK(top.value()->file_path() == "/work/top");
  db::CellDef* inv = io.library.find("inv");
  REQUIRE(inv != nullptr);
  // Magic's load does not expand: the child is a placeholder until needed.
  CHECK_FALSE(inv->flags().available);
  CHECK(inv->bbox() == db::Rect{0, 0, 4, 4});  // from the use line
  cell::expand_all(io.ctx, *top.value());
  CHECK(inv->flags().available);
  CHECK(inv->file_path() == "/lib/inv");
  CHECK(io.stamps.empty());
}

TEST_CASE("a missing cell is reported once and a new cell is created",
          "[cap:cell-library-resolution][req:Missing cell handling]") {
  CellIo io;
  const auto made = cell::load_cell(io.ctx, "newcell");
  REQUIRE(made.ok());
  CHECK(made.value()->flags().available);
  CHECK(made.value()->file_path() == "newcell.mag");
  db::CellDef& ghost = make(io, "ghost");
  CHECK_FALSE(cell::read_cell(io.ctx, ghost).ok());
  CHECK_FALSE(cell::read_cell(io.ctx, ghost).ok());
  int reported = 0;
  for (const auto& d : io.sink.snapshot()) {
    reported += d.text == "Cell ghost couldn't be read" ? 1 : 0;
  }
  CHECK(reported == 1);
  CHECK(ghost.flags().not_found);
}

TEST_CASE("save writes the file, records the path and clears the flags",
          "[cap:cell-io-safety][req:Atomic save through a temporary file][req:Post-write verification]") {
  CellIo io;
  db::CellDef& a = io.read("a", "magic\ntech tiny\ntimestamp 1\n<< metal1 >>\nrect 0 0 2 2\n<< end >>\n");
  db::mark_modified(a);
  REQUIRE(cell::save_cell(io.ctx, a, "/work/a").ok());
  CHECK(a.file_path() == "/work/a");
  CHECK_FALSE(a.flags().modified);
  CHECK_FALSE(a.flags().needs_new_timestamp);
  CHECK(a.timestamp() == 1000);  // the fixture clock
  const auto text = io.ctx.fs.read("/work/a.mag");
  REQUIRE(text.ok());
  CHECK(text.value() == "magic\ntech tiny\ntimestamp 1000\n<< metal1 >>\nrect 0 0 2 2\n<< end >>\n");
  CHECK(io.locks->holds("/work/a.mag"));
}

TEST_CASE("read-only cells are not written", "[cap:cell-io-safety][req:Write refusal for protected cells]") {
  CellIo io;
  db::CellDef& a = io.read("a", "magic\ntech tiny\ntimestamp 1\n<< end >>\n");
  a.flags().no_edit = true;
  const auto saved = cell::save_cell(io.ctx, a, "/work/a");
  REQUIRE_FALSE(saved.ok());
  CHECK(saved.error().code == cell::codes::kWriteRefused);
  CHECK_FALSE(io.ctx.fs.exists("/work/a.mag"));
}

TEST_CASE("a .tcl target writes a command script and leaves the cell modified",
          "[cap:cell-file-format][req:Command-script save variant]") {
  CellIo io;
  db::CellDef& top = io.read("top", "magic\ntech tiny\ntimestamp 1\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n");
  db::mark_modified(top);
  REQUIRE(cell::save_cell(io.ctx, top, "/work/top.tcl").ok());
  CHECK(top.flags().modified);
  CHECK(io.ctx.fs.exists("/work/top.tcl"));
  CHECK(io.has_code(cell::codes::kSaved));
}

TEST_CASE("Magic's default fonts load and flabels round-trip", "[cap:cell-file-format][req:Label records]") {
  CellIo io;
  if (!io.load_fonts()) {
    SKIP("Magic's default fonts (build/ref/magic/graphics) not present");
  }
  CHECK(io.fonts.find("FreeSans") == 0);
  CHECK(io.fonts.find("FreeMono") == 2);
  const std::string text = "magic\ntech tiny\ntimestamp 1\n<< labels >>\n"
                           "flabel metal1 s 0 0 10 10 0 FreeSans 80 0 0 0 VPWR\n<< end >>\n";
  db::CellDef& c = io.read("c", text);
  CHECK(io.write(c) == text);
}
