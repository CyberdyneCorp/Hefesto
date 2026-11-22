// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Loading and safe saving (capability schematic-file-format "Safe saving",
// "Round-trip guarantee"; openspec/changes/m8-schematic tasks.md 6.4).
//
// Owner: FORMAT.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "file_test_util.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/save.hpp"

namespace schem = hefesto::schem;
namespace platform = hefesto::platform;
namespace diag = hefesto::diag;

namespace {

constexpr std::string_view kFile = "v {xschem version=3.4.7RC file_version=1.2}\nN 10.50 0 0 0 {}\n";

// A file system whose atomic writes fail, like a volume that fills up
// during the save; everything else goes to a memory file system.
class FullVolume final : public platform::FileSystem {
 public:
  explicit FullVolume(platform::MemoryFileSystem& inner) : inner_(inner) {}

  diag::Outcome<std::string> read(const platform::Path& path) override { return inner_.read(path); }
  diag::Status write_atomic(const platform::Path& path, std::string_view /*data*/) override {
    return diag::make(diag::codes::kPlatformWrite, "no space left on device: " + path.generic_string());
  }
  diag::Status append(const platform::Path& path, std::string_view data) override { return inner_.append(path, data); }
  bool exists(const platform::Path& path) override { return inner_.exists(path); }
  diag::Outcome<std::uint64_t> size(const platform::Path& path) override { return inner_.size(path); }
  diag::Outcome<std::vector<platform::Path>> list(const platform::Path& directory) override {
    return inner_.list(directory);
  }
  diag::Status create_directories(const platform::Path& directory) override {
    return inner_.create_directories(directory);
  }
  diag::Status rename(const platform::Path& from, const platform::Path& to) override { return inner_.rename(from, to); }
  diag::Status remove(const platform::Path& path) override { return inner_.remove(path); }

 private:
  platform::MemoryFileSystem& inner_;
};

std::string contents(platform::FileSystem& fs, const platform::Path& path) {
  auto read = fs.read(path);
  REQUIRE(read.ok());
  return read.value();
}

}  // namespace

TEST_CASE("disk full", "[cap:schematic-file-format][req:Safe saving]") {
  platform::MemoryFileSystem memory;
  REQUIRE(memory.write_atomic("/w/a.sch", kFile).ok());
  auto loaded = schem::load_document(memory, "/w/a.sch", {});
  REQUIRE(loaded.ok());
  schem::Document doc = std::move(loaded).value();
  doc.modified = true;
  FullVolume full{memory};
  const auto saved = schem::save_document(full, doc, "/w/a.sch");
  REQUIRE_FALSE(saved.ok());
  CHECK(saved.error().code == diag::codes::kPlatformWrite);
  CHECK(contents(memory, "/w/a.sch") == kFile);  // the original file is unchanged
  CHECK(doc.modified);
}

TEST_CASE("a save writes the canonical text and updates the document", "[cap:schematic-file-format][req:Safe saving]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/w/a.sch", kFile).ok());
  auto doc = std::move(schem::load_document(fs, "/w/a.sch", {})).value();
  CHECK(doc.path == platform::Path{"/w/a.sch"});
  CHECK_FALSE(doc.disk_digest.empty());
  doc.modified = true;
  REQUIRE(schem::save_document(fs, doc, "/w/b.sch").ok());
  CHECK(contents(fs, "/w/b.sch") == std::string{schem::test::kEmptyHeader} + "N 0 0 10.5 0 {}\n");
  CHECK(doc.path == platform::Path{"/w/b.sch"});
  CHECK_FALSE(doc.modified);
  CHECK(contents(fs, "/w/a.sch") == kFile);
}

TEST_CASE("a file changed on disk is not overwritten without confirmation", "[cap:schematic-file-format][req:Safe saving]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/w/a.sch", kFile).ok());
  auto doc = std::move(schem::load_document(fs, "/w/a.sch", {})).value();
  REQUIRE(fs.write_atomic("/w/a.sch", "N 1 1 2 2 {}\n").ok());  // another program saved it
  const auto refused = schem::save_document(fs, doc, "/w/a.sch");
  REQUIRE_FALSE(refused.ok());
  CHECK(refused.error().code == "SCHEM-FILE-006");
  CHECK(refused.error().severity == diag::Severity::error);
  CHECK(contents(fs, "/w/a.sch") == "N 1 1 2 2 {}\n");
  REQUIRE(schem::save_document(fs, doc, "/w/a.sch", {.overwrite_changed_file = true}).ok());
  CHECK(contents(fs, "/w/a.sch") == schem::write_document(doc));
  // Saved since: an unchanged file on disk is overwritten without asking.
  REQUIRE(schem::save_document(fs, doc, "/w/a.sch").ok());
}

TEST_CASE("loading never writes and takes the kind from the file name", "[cap:schematic-file-format][req:Round-trip guarantee]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/w/old.sym", "G {type=subcircuit}\n").ok());
  diag::CollectingSink sink;
  schem::ReadOptions options;
  options.warnings = &sink;
  auto doc = schem::load_document(fs, "/w/old.sym", options);
  REQUIRE(doc.ok());
  CHECK(doc.value().kind == schem::FileKind::symbol);
  CHECK(doc.value().globals.k == "type=subcircuit");
  CHECK_FALSE(doc.value().modified);
  CHECK(contents(fs, "/w/old.sym") == "G {type=subcircuit}\n");
  auto listed = fs.list("/w");
  REQUIRE(listed.ok());
  CHECK(listed.value().size() == 1);
  CHECK(schem::file_kind_of("x/y.sch") == schem::FileKind::schematic);
  CHECK_FALSE(schem::load_document(fs, "/w/missing.sch", {}).ok());
}

TEST_CASE("warnings of a loaded file name the file", "[cap:schematic-file-format][req:Malformed input]") {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/w/bad.sch", "N 0 0\n").ok());
  diag::CollectingSink sink;
  schem::ReadOptions options;
  options.warnings = &sink;
  REQUIRE(schem::load_document(fs, "/w/bad.sch", options).ok());
  const auto warnings = sink.take();
  REQUIRE(warnings.size() == 1);
  const auto* location = std::get_if<diag::SourceLocation>(&warnings[0].location);
  REQUIRE(location != nullptr);
  CHECK(location->file == "/w/bad.sch");
}

TEST_CASE("pin boxes saved in sim_pinnumber order stay in that order in the model",
          "[cap:schematic-file-format][req:Canonical write order]") {
  // The shape of IHP nmoscl_2.sym (SCHEM-RT-02): pins D, G, S numbered 2, 1, 3.
  const std::string boxes =
      "B 5 0 0 1 1 {name=D sim_pinnumber=2}\nB 5 2 0 3 1 {name=G sim_pinnumber=1}\nB 5 4 0 5 1 {name=S sim_pinnumber=3}\n";
  const std::string sorted =
      "B 5 2 0 3 1 {name=G sim_pinnumber=1}\nB 5 0 0 1 1 {name=D sim_pinnumber=2}\nB 5 4 0 5 1 {name=S sim_pinnumber=3}\n";
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/w/n.sym", std::string{schem::test::kEmptyHeader} + boxes).ok());
  auto doc = std::move(schem::load_document(fs, "/w/n.sym", {})).value();
  REQUIRE(doc.boxes.size() == 3);
  CHECK(doc.boxes[0].props == "name=D sim_pinnumber=2");  // loading does not reorder
  REQUIRE(schem::save_document(fs, doc, "/w/n.sym").ok());
  CHECK(contents(fs, "/w/n.sym") == std::string{schem::test::kEmptyHeader} + sorted);
  CHECK(doc.boxes[0].props == "name=G sim_pinnumber=1");
  CHECK(doc.boxes[1].props == "name=D sim_pinnumber=2");
  CHECK_FALSE(doc.modified);
  // The model now holds the written order, so a second save writes the same bytes.
  REQUIRE(schem::save_document(fs, doc, "/w/n.sym").ok());
  CHECK(contents(fs, "/w/n.sym") == std::string{schem::test::kEmptyHeader} + sorted);
}
