// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// AtomicOutput (Q-EXTRACT-27, fix in both modes; capability
// extract-commands "Output path and file location", "Incremental
// extraction"; openspec/changes/m7-extraction tasks.md 5.5). The symlink and
// read-only directory tests use the local file system in a directory below
// the test's working directory. Owner: EXTNODE.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/file/atomic_output.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::test::extfile {
namespace {

namespace extract = hefesto::extract;
namespace fs = std::filesystem;

struct Memory {
  platform::MemoryFileSystem files;
  cell::SearchPaths paths{{"/w"}, {}, {}};
  cell::PathVariables vars;
  diag::CollectingSink sink;
  cell::LookupContext ctx{files, paths, vars, sink};
  db::CellDef cell{"inv", 8};
};

std::string read_or_empty(platform::FileSystem& files, const std::string& path) {
  auto text = files.read(path);
  return text.ok() ? text.value() : std::string{};
}

TEST_CASE("Commit replaces the target and leaves no temporary file",
          "[cap:extract-commands][req:Incremental extraction]") {
  Memory m;
  REQUIRE(m.files.write_atomic("/w/inv.ext", "old\n").ok());
  auto out = extract::AtomicOutput::open(m.ctx, m.cell, ".ext");
  REQUIRE(out.ok());
  CHECK(out.value().target() == "/w/inv.ext");
  REQUIRE(out.value().temporary().has_value());
  REQUIRE(out.value().write("timestamp 1\n").ok());
  REQUIRE(out.value().write("version 8.3\n").ok());
  CHECK(read_or_empty(m.files, "/w/inv.ext") == "old\n");
  REQUIRE(out.value().commit().ok());
  CHECK(read_or_empty(m.files, "/w/inv.ext") == "timestamp 1\nversion 8.3\n");
  CHECK(m.files.list("/w").value() == std::vector<platform::Path>{"/w/inv.ext"});
}

TEST_CASE("A cancelled run keeps the previous file and removes the temporary file",
          "[cap:extract-commands][req:Incremental extraction]") {
  Memory m;
  REQUIRE(m.files.write_atomic("/w/inv.ext", "old\n").ok());
  {
    auto out = extract::AtomicOutput::open(m.ctx, m.cell, ".ext");
    REQUIRE(out.ok());
    REQUIRE(out.value().write("timestamp 1\n").ok());
    out.value().abandon();
  }
  CHECK(read_or_empty(m.files, "/w/inv.ext") == "old\n");
  CHECK(m.files.list("/w").value().size() == 1);
  {
    // An output dropped without commit() is abandoned.
    auto out = extract::AtomicOutput::open(m.ctx, m.cell, ".res.ext");
    REQUIRE(out.ok());
    REQUIRE(out.value().write("x\n").ok());
  }
  CHECK(m.files.list("/w").value().size() == 1);
}

TEST_CASE("The target follows ExtLocalPath, then the cell's file, then the cell name",
          "[cap:extract-commands][req:Output path and file location]") {
  Memory m;
  extract::OutputLocation where;
  where.local_path = "/out";
  auto local = extract::AtomicOutput::open(m.ctx, m.cell, ".ext", where);
  REQUIRE(local.ok());
  CHECK(local.value().target() == "/out/inv.ext");
  CHECK(m.sink.take().size() == 1);  // Path "/out" does not exist;  attempting to create it.
  local.value().abandon();

  where.local_path = ".";
  where.current_directory = "/cwd";
  auto here = extract::AtomicOutput::open(m.ctx, m.cell, ".ext", where);
  REQUIRE(here.ok());
  CHECK(here.value().target() == "/cwd/inv.ext");
  here.value().abandon();

  m.cell.set_file_path("/lib/cells/inv.mag");
  auto beside = extract::AtomicOutput::open(m.ctx, m.cell, ".ext");
  REQUIRE(beside.ok());
  CHECK(beside.value().target() == "/lib/cells/inv.ext");
  beside.value().abandon();
}

/// A scratch directory below the working directory (never /tmp).
struct LocalDir {
  fs::path root = fs::current_path() / ("extract_file_test_" + std::to_string(::getpid()) + "_" + std::to_string(next()));
  platform::LocalFileSystem files;
  cell::SearchPaths paths;
  cell::PathVariables vars;
  diag::CollectingSink sink;
  cell::LookupContext ctx{files, paths, vars, sink};
  db::CellDef cell{"inv", 8};

  static int next() {
    static int counter = 0;
    return ++counter;
  }
  LocalDir() {
    fs::create_directories(root);
    paths.path = {root.string()};
  }
  ~LocalDir() {
    std::error_code ec;
    fs::permissions(root, fs::perms::owner_all, fs::perm_options::add, ec);
    for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
      if (entry.is_directory(ec) && !entry.is_symlink(ec)) {
        fs::permissions(entry.path(), fs::perms::owner_all, fs::perm_options::add, ec);
      }
    }
    fs::remove_all(root, ec);
  }
  LocalDir(const LocalDir&) = delete;
  LocalDir& operator=(const LocalDir&) = delete;

  static void write(const fs::path& p, const std::string& text) {
    std::ofstream out(p, std::ios::binary);
    out << text;
  }
  static std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
  }
};

TEST_CASE("A symlinked target keeps the link and its target receives the new file",
          "[cap:extract-commands][req:Incremental extraction]") {
  LocalDir d;
  fs::create_directories(d.root / "real");
  LocalDir::write(d.root / "real" / "inv.ext", "old\n");
  fs::create_symlink(d.root / "real" / "inv.ext", d.root / "inv.ext");
  auto out = extract::AtomicOutput::open(d.ctx, d.cell, ".ext");
  REQUIRE(out.ok());
  CHECK(out.value().target() == (d.root / "inv.ext").string());
  REQUIRE(out.value().write("new\n").ok());
  REQUIRE(out.value().commit().ok());
  CHECK(fs::is_symlink(d.root / "inv.ext"));
  CHECK(LocalDir::read(d.root / "real" / "inv.ext") == "new\n");
  CHECK(LocalDir::read(d.root / "inv.ext") == "new\n");
  std::size_t entries = 0;
  for ([[maybe_unused]] const auto& e : fs::directory_iterator(d.root / "real")) {
    ++entries;
  }
  CHECK(entries == 1);
}

TEST_CASE("A read-only directory is written in place",
          "[cap:extract-commands][req:Incremental extraction]") {
  LocalDir d;
  fs::create_directories(d.root / "ro");
  LocalDir::write(d.root / "ro" / "inv.ext", "old\n");
  fs::permissions(d.root / "ro", fs::perms::owner_read | fs::perms::owner_exec);
  if (std::ofstream(d.root / "ro" / "probe").is_open()) {
    SKIP("the directory is still writable (running as root)");
  }
  d.paths.path = {(d.root / "ro").string()};
  auto out = extract::AtomicOutput::open(d.ctx, d.cell, ".ext");
  REQUIRE(out.ok());
  CHECK(out.value().in_place());
  CHECK_FALSE(out.value().temporary().has_value());
  REQUIRE(out.value().write("new\n").ok());
  REQUIRE(out.value().commit().ok());
  CHECK(LocalDir::read(d.root / "ro" / "inv.ext") == "new\n");
}

TEST_CASE("No writable place on the search path: the stripped name in the current directory",
          "[cap:extract-commands][req:Output path and file location]") {
  LocalDir d;
  fs::create_directories(d.root / "ro");
  fs::create_directories(d.root / "cwd");
  fs::permissions(d.root / "ro", fs::perms::owner_read | fs::perms::owner_exec);
  if (std::ofstream(d.root / "ro" / "probe").is_open()) {
    SKIP("the directory is still writable (running as root)");
  }
  d.paths.path = {(d.root / "ro").string()};
  extract::OutputLocation where;
  where.current_directory = (d.root / "cwd").string();
  auto out = extract::AtomicOutput::open(d.ctx, d.cell, ".ext", where);
  REQUIRE(out.ok());
  CHECK(out.value().target() == (d.root / "cwd" / "inv.ext").string());
  REQUIRE(out.value().write("x\n").ok());
  REQUIRE(out.value().commit().ok());
  CHECK(LocalDir::read(d.root / "cwd" / "inv.ext") == "x\n");
}

}  // namespace
}  // namespace hefesto::test::extfile
