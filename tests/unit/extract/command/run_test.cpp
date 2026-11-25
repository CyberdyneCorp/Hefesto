// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The extraction run driver (tasks.md 9.2-9.4; capability extract-commands;
// design D12, Q-EXTRACT-6, Q-EXTRACT-11, Q-EXTRACT-27): extract all, cell,
// parents and showparents, incremental extraction, cell properties, output
// locations, atomic output (cancel, symbolic links, read-only directories),
// the unique command and the summary messages, and the three locking
// phases.
//
// Owner: EXTCMD.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <string>
#include <thread>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/lock.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/extract/command/run.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "run_fixture.hpp"

namespace extract = hefesto::extract;
using extcmd_test::mag;
using extcmd_test::Session;
using extcmd_test::use;

namespace {

const std::string kLeaf = mag("<< metal1 >>\nrect 0 0 10 4\n<< labels >>\nrlabel metal1 2 2 2 2 0 a\n");

// leaf (paint), empty (a label only) and top (two leaves, two empties, paint).
void hierarchy(Session& s) {
  s.file("leaf.mag", kLeaf);
  s.file("empty.mag", mag("<< labels >>\nrlabel metal1 0 0 0 0 0 nothing\n"));
  s.load("top", mag("<< metal1 >>\nrect 0 10 4 14\n" + use("leaf", "l0") + use("leaf", "l1", 20, 0) +
                    use("empty", "e0", 40, 0) + use("empty", "e1", 50, 0)));
}

std::string first_line(const std::string& text) {
  return text.substr(0, text.find('\n'));
}

extract::RunRequest request(Session& s, extract::RunMode mode, const std::string& root) {
  extract::RunRequest r;
  r.mode = mode;
  r.root = root;
  r.sink = &s.printed;
  r.files = &s.files();
  r.loader = s.loader();
  return r;
}

}  // namespace

TEST_CASE("extract all writes every used cell once, bottom-up, and no empty cell",
          "[cap:extract-commands][req:Extract all, cell, parents and showparents]") {
  Session s;
  hierarchy(s);
  extract::RunRequest r = request(s, extract::RunMode::all, "top");
  auto prepared = extract::prepare_run(s.editor, r, nullptr);
  REQUIRE(prepared.ok());
  extract::extract_run(s.editor, prepared.value());
  const extract::RunReport report = extract::finish_run(s.editor, prepared.value());
  CHECK(report.extracted == std::vector<std::string>{"leaf", "top"});
  CHECK(s.exists("leaf.ext"));
  CHECK(s.exists("top.ext"));
  CHECK_FALSE(s.exists("empty.ext"));
  const std::string top = s.read("top.ext");
  CHECK(extcmd_test::lines_with(top, "use ") ==
        std::vector<std::string>{"use leaf l1 1 0 20 0 1 0", "use leaf l0 1 0 0 0 1 0"});  // DBCellEnum order
  CHECK(s.take_printed().find("Extracting leaf into /w/leaf.ext:\nProcessing leaf\nExtracting top into /w/top.ext:\n"
                              "Processing top\n") != std::string::npos);
}

TEST_CASE("extract all on a cell with nothing to extract warns", "[cap:extract-commands][req:Extract all, cell, parents and showparents]") {
  Session s;
  s.load("empty", mag("<< labels >>\nrlabel metal1 0 0 0 0 0 nothing\n"));
  REQUIRE(s.run("extract all").ok());
  CHECK(s.take_printed() == "Warning:  There is nothing here to extract.\n");
  CHECK_FALSE(s.exists("empty.ext"));
}

TEST_CASE("extract cell writes the selected cell into the named file only",
          "[cap:extract-commands][req:Extract all, cell, parents and showparents]") {
  Session s;
  hierarchy(s);
  REQUIRE(s.run("extract cell myleaf").ok());
  CHECK(s.take_printed() == "No cell selected\n");
  REQUIRE(s.run("select cell l0").ok());
  REQUIRE(s.run("extract cell").ok());
  CHECK(s.take_printed().ends_with("Wrong number of arguments in \"extract\" command.  Try \"extract help\" for help.\n"));
  REQUIRE(s.run("extract cell myleaf").ok());
  // ExtFileOpen(def, "myleaf"): myleaf.ext in the current directory.
  CHECK(Session::output_of(s.take_printed(), "leaf") == "myleaf.ext");
  CHECK_FALSE(s.exists("leaf.ext"));
  CHECK_FALSE(s.exists("top.ext"));
  auto text = s.files().read(std::filesystem::current_path() / "myleaf.ext");
  REQUIRE(text.ok());
  CHECK(extcmd_test::lines_with(text.value(), "node ") == std::vector<std::string>{"node \"a\" 0 0 0 0 m1"});
}

TEST_CASE("extract showparents prints and extract parents extracts the users of the selection",
          "[cap:extract-commands][req:Extract all, cell, parents and showparents]") {
  Session s;
  hierarchy(s);
  REQUIRE(s.run("select cell l0").ok());
  (void)s.take_printed();
  REQUIRE(s.run("extract showparents").ok());
  CHECK(s.take_printed() == "top, leaf\n");  // extDefParentFunc's stack, popped
  CHECK_FALSE(s.exists("top.ext"));
  REQUIRE(s.run("extract parents").ok());
  CHECK(s.exists("leaf.ext"));
  CHECK(s.exists("top.ext"));
}

TEST_CASE("an unchanged child is not re-extracted", "[cap:extract-commands][req:Incremental extraction]") {
  Session s;
  hierarchy(s);
  REQUIRE(s.run("extract all").ok());
  // A current .ext keeps its timestamp line; the marker shows it is not rewritten.
  s.file("leaf.ext", first_line(s.read("leaf.ext")) + "\nmarker\n");
  const std::string old_top = first_line(s.read("top.ext"));
  hefesto::db::CellDef& top = *s.editor.cells.find("top");
  top.flags().no_edit = false;  // the in-memory file is read-only, and DBUpdateStamps skips such a cell
  const auto metal1 = s.editor.technology->find_type("metal1");
  REQUIRE(hefesto::db::paint(top, *s.editor.technology, hefesto::db::Rect{0, 20, 4, 24}, *metal1.id).ok());
  hefesto::db::mark_modified(top);
  (void)s.take_printed();
  REQUIRE(s.run("extract").ok());
  CHECK(s.read("leaf.ext").ends_with("marker\n"));
  CHECK(first_line(s.read("top.ext")) != old_top);
  CHECK(s.take_printed().find("Extracting leaf") == std::string::npos);
}

TEST_CASE("a cell whose .ext timestamp differs is re-extracted", "[cap:extract-commands][req:Incremental extraction]") {
  Session s;
  s.load("leaf", kLeaf);
  s.file("leaf.ext", "timestamp 99\nstale\n");
  REQUIRE(s.run("extract").ok());
  CHECK(first_line(s.read("leaf.ext")) == "timestamp 100");
  s.file("leaf.ext", "garbage\n");
  REQUIRE(s.run("extract").ok());
  CHECK(first_line(s.read("leaf.ext")) == "timestamp 100");
}

TEST_CASE("an unnamed root is not extracted", "[cap:extract-commands][req:Incremental extraction]") {
  Session s;
  REQUIRE(s.run("load").ok());
  REQUIRE(s.run("box 0 0 4 4; paint metal1").ok());
  (void)s.take_printed();
  REQUIRE(s.run("extract").ok());
  CHECK(s.take_printed() == "Please name the cell before extracting.\n");
}

// Q-EXTRACT-27 (fix): the previous file survives a cancelled run, and no
// temporary file is left.
TEST_CASE("a cancelled run keeps the previous .ext and leaves no temporary file",
          "[cap:extract-commands][req:Incremental extraction]") {
  Session s;
  s.load("leaf", kLeaf);
  s.file("leaf.ext", "timestamp 1\nprevious\n");
  std::atomic<bool> cancel{false};
  hefesto::mask::Progress progress;
  progress.cancel = &cancel;
  auto prepared = extract::prepare_run(s.editor, request(s, extract::RunMode::all, "leaf"), &progress);
  REQUIRE(prepared.ok());
  CHECK(s.exists(".leaf.ext.hefesto-tmp"));  // opened in the prepare phase
  cancel = true;
  extract::extract_run(s.editor, prepared.value());
  const extract::RunReport report = extract::finish_run(s.editor, prepared.value());
  CHECK(report.cancelled);
  CHECK(report.extracted.empty());
  CHECK(s.read("leaf.ext") == "timestamp 1\nprevious\n");
  CHECK_FALSE(s.exists(".leaf.ext.hefesto-tmp"));
  // The next run re-extracts it: its timestamp differs.
  REQUIRE(s.run("extract").ok());
  CHECK(first_line(s.read("leaf.ext")) == "timestamp 100");
}

TEST_CASE("extract path creates the directory and writes there", "[cap:extract-commands][req:Output path and file location]") {
  Session s;
  s.load("leaf", kLeaf);
  REQUIRE(s.run("extract path /w/ext").ok());
  REQUIRE(s.run("extract all").ok());
  CHECK(s.exists("ext/leaf.ext"));
  CHECK(s.take_printed().starts_with("Path \"/w/ext\" does not exist;  attempting to create it.\n"));
}

TEST_CASE("without a path the .ext goes next to the cell's file", "[cap:extract-commands][req:Output path and file location]") {
  Session s;
  s.file("sub/leaf.mag", kLeaf);
  REQUIRE(s.run("addpath /w/sub").ok());
  s.open("leaf");
  REQUIRE(s.run("extract all").ok());
  CHECK(s.exists("sub/leaf.ext"));
  CHECK_FALSE(s.exists("leaf.ext"));
}

// Q-EXTRACT-11 (platform): a sandboxed host resolves `.` against the
// project folder.
TEST_CASE("a sandboxed host writes relative outputs into the project folder",
          "[cap:extract-commands][req:Output path and file location]") {
  Session s;
  s.load("leaf", kLeaf);
  s.editor.extract.options.path = ".";
  extract::RunRequest r = request(s, extract::RunMode::all, "leaf");
  r.current_directory = "/project";
  REQUIRE(extract::run(s.editor, r, nullptr).ok());
  CHECK(s.files().exists("/project/leaf.ext"));
  CHECK(s.take_printed().find("Extracting leaf into leaf.ext:") != std::string::npos);  // relative to the folder
}

TEST_CASE("a symbolic link to the .ext survives and its target gets the new file",
          "[cap:extract-commands][req:Output path and file location]") {
  Session s(true, false, "");
  s.load("leaf", kLeaf);
  namespace fs = std::filesystem;
  const fs::path dir{s.dir};
  fs::create_directories(dir / "real");
  { std::ofstream(dir / "real" / "leaf.ext") << "timestamp 1\nold\n"; }
  fs::create_symlink(dir / "real" / "leaf.ext", dir / "leaf.ext");
  REQUIRE(s.run("extract all").ok());
  CHECK(fs::is_symlink(dir / "leaf.ext"));
  CHECK(first_line(s.read("real/leaf.ext")) == "timestamp 100");
  fs::remove_all(dir);
}

TEST_CASE("a read-only directory with a writable .ext is written in place",
          "[cap:extract-commands][req:Output path and file location]") {
  Session s(true, false, "");
  s.load("leaf", kLeaf);
  namespace fs = std::filesystem;
  const fs::path dir{s.dir};
  { std::ofstream(dir / "leaf.ext") << "timestamp 1\nold\n"; }
  fs::permissions(dir, fs::perms::owner_read | fs::perms::owner_exec);
  const auto result = s.run("extract all");
  fs::permissions(dir, fs::perms::owner_all);
  REQUIRE(result.ok());
  CHECK(first_line(s.read("leaf.ext")) == "timestamp 100");
  fs::remove_all(dir);
}

TEST_CASE("a noextract cell is not extracted", "[cap:extract-commands][req:Cell properties affecting extraction]") {
  Session s;
  s.load("leaf", mag("<< metal1 >>\nrect 0 0 10 4\n<< properties >>\nstring noextract true\n"));
  REQUIRE(s.run("extract all").ok());
  CHECK_FALSE(s.exists("leaf.ext"));
  CHECK(s.take_printed().empty());
}

TEST_CASE("the summary counts the duplicate-label warnings", "[cap:extract-commands][req:Progress and summary messages]") {
  Session s;
  s.load("ex", extcmd_test::case_input("EX-024", "ex.mag"));
  REQUIRE(s.run("extract no unique").ok());
  REQUIRE(s.run("extract all").ok());
  const std::string printed = s.take_printed();
  CHECK(printed.find("ex: 4 warnings\n") != std::string::npos);
  CHECK(printed.ends_with("Total of 4 warnings.\n"));
  CHECK(s.editor.feedback.size() == 4);
}

// Q-EXTRACT-6 (fix): every temporarily renamed cell is reverted, the top
// cell included (Magic leaves the top cell's _uqN labels).
TEST_CASE("extract do unique reverts the temporary labels of the top cell",
          "[cap:extract-commands][req:Unique command and temporary unique option]") {
  for (const bool compat : {true, false}) {
    Session s(compat);
    s.load("ex", extcmd_test::case_input("EX-024", "ex.mag"));
    REQUIRE(s.run("extract do unique").ok());
    REQUIRE(s.run("extract all").ok());
    CHECK(extcmd_test::lines_with(s.read("ex.ext"), "node ").size() == 4);
    CHECK(s.read("ex.ext").find("net1_uq0") != std::string::npos);
    for (const auto& label : s.editor.cells.find("ex")->labels()) {
      CHECK(label.text.find("_uq") == std::string::npos);
    }
    CHECK_FALSE(s.editor.cells.find("ex")->flags().modified);
  }
}

TEST_CASE("extract unique renames permanently and turns do unique off",
          "[cap:extract-commands][req:Unique command and temporary unique option]") {
  Session s;
  s.load("ex", extcmd_test::case_input("EX-024", "ex.mag"));
  REQUIRE(s.run("extract unique").ok());
  const std::string printed = s.take_printed();
  CHECK(printed.starts_with("Warning:  Extract option \"do unique\" disabled because \"extract unique\" was run.\n"));
  CHECK_FALSE(s.editor.extract.options.unique);
  int renamed = 0;
  for (const auto& label : s.editor.cells.find("ex")->labels()) {
    renamed += label.text.find("_uq") != std::string::npos ? 1 : 0;
  }
  CHECK(renamed == 2);
  REQUIRE(s.run("extract unique bogus").ok());
  CHECK(s.take_printed().starts_with("Usage: extract unique [option]\nwhere option is one of:\n"));
}

// Design D12: the extract phase holds the shared lock, so readers (the
// renderer) are admitted while it runs.
TEST_CASE("rendering reads the cell during the extract phase", "[cap:extract-commands][req:Incremental extraction]") {
  Session s;
  hierarchy(s);
  std::atomic<bool> read_during{false};
  hefesto::mask::Progress progress;
  progress.report = [&](double) {
    auto reader = std::async(std::launch::async, [&] {
      const hefesto::db::DatabaseLock::Shared guard(s.editor.database_lock);
      return s.editor.cells.find("top")->name();
    });
    read_during = reader.wait_for(std::chrono::seconds(5)) == std::future_status::ready && reader.get() == "top";
  };
  auto prepared = extract::prepare_run(s.editor, request(s, extract::RunMode::all, "top"), &progress);
  REQUIRE(prepared.ok());
  std::thread worker([&] { extract::extract_run(s.editor, prepared.value()); });
  worker.join();
  CHECK(read_during);
  (void)extract::finish_run(s.editor, prepared.value());
  CHECK(s.exists("top.ext"));
}

TEST_CASE("ensure_extracted brings a tree up to date and fails on an unknown cell",
          "[cap:extract-commands][req:Incremental extraction]") {
  Session s;
  hierarchy(s);
  const auto missing = extract::ensure_extracted(s.editor, "nosuch", nullptr);
  CHECK_FALSE(missing.ok());
  const auto done = extract::ensure_extracted(s.editor, "top", nullptr, request(s, extract::RunMode::all, "top"));
  REQUIRE(done.ok());
  CHECK(done.value().extracted == std::vector<std::string>{"leaf", "top"});
}
