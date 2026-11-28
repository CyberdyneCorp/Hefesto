// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M7 round 6 (tasks.md 10.6): a `-S` symbol file that fopen cannot open is
// reported with the errno text, as Magic's perror does; a directory opens
// and reads nothing (extflat/EFsym.c:104-118; Q-NETLIST-58). Owner: EXTFLAT.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "hefesto/diag/codes.hpp"
#include "hefesto/extflat/args.hpp"
#include "hefesto/platform/file_system.hpp"

namespace extflat = hefesto::extflat;
namespace platform = hefesto::platform;
namespace diag = hefesto::diag;
using Strings = std::vector<std::string>;

namespace {

/// A file system whose fopen fails with the errno each name stands for;
/// "folder" is a directory, which fopen opens and reads nothing from.
class FailingFileSystem final : public platform::FileSystem {
 public:
  diag::Outcome<std::string> read(const platform::Path& path) override {
    return diag::make(diag::codes::kPlatformRead, "cannot read '" + path.generic_string() + "'");
  }
  platform::StreamRead read_stream(const platform::Path& path) override {
    const std::string p = path.generic_string();
    if (p == "folder") {
      return {};
    }
    const std::errc reason = p == "locked"   ? std::errc::permission_denied
                             : p == "a/file" ? std::errc::not_a_directory
                                             : std::errc::no_such_file_or_directory;
    return {{}, std::make_error_code(reason)};
  }
  diag::Status write_atomic(const platform::Path&, std::string_view) override { return diag::Done{}; }
  diag::Status append(const platform::Path&, std::string_view) override { return diag::Done{}; }
  bool exists(const platform::Path&) override { return false; }
  diag::Outcome<std::uint64_t> size(const platform::Path& path) override { return read(path).error(); }
  diag::Outcome<std::vector<platform::Path>> list(const platform::Path&) override {
    return std::vector<platform::Path>{};
  }
  diag::Status create_directories(const platform::Path&) override { return diag::Done{}; }
  diag::Status rename(const platform::Path&, const platform::Path&) override { return diag::Done{}; }
  diag::Status remove(const platform::Path&) override { return diag::Done{}; }
};

class ScratchDir {
 public:
  ScratchDir() {
    std::random_device rd;
    path_ = std::filesystem::temp_directory_path() / ("hefesto-syms-" + std::to_string(rd()));
    std::filesystem::create_directories(path_);
  }
  ScratchDir(const ScratchDir&) = delete;
  ScratchDir& operator=(const ScratchDir&) = delete;
  ~ScratchDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

Strings symbol_file_messages(platform::FileSystem& fs, const std::string& file) {
  const Strings args{"-S", file, "top"};
  const auto parsed = extflat::parse_common_args(std::span<const std::string>{args}, {},
                                                 extflat::ArgOptions{false, false, extflat::file_reader(fs)});
  REQUIRE(parsed.ok());
  CHECK(parsed.value().cell == std::optional<std::string>{"top"});  // the parse goes on
  Strings out;
  for (const auto& d : parsed.value().messages) {
    out.push_back(d.text);
  }
  return out;
}

}  // namespace

TEST_CASE("an unreadable -S file is reported with the OS error text",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  FailingFileSystem fs;
  CHECK(symbol_file_messages(fs, "missing") == Strings{"missing: No such file or directory"});
  CHECK(symbol_file_messages(fs, "locked") ==
        Strings{"locked: " + std::make_error_code(std::errc::permission_denied).message()});
  CHECK(symbol_file_messages(fs, "a/file") ==
        Strings{"a/file: " + std::make_error_code(std::errc::not_a_directory).message()});
  CHECK(symbol_file_messages(fs, "folder").empty());
}

// Regression (review of PR #141): fopen opens a directory and fgets reads
// nothing from it, so Magic prints nothing and adds no symbol; the earlier
// fix printed `<dir>: Is a directory`. Compat-neutral (Q-NETLIST-58).
TEST_CASE("a -S directory read through the local file system adds nothing and says nothing",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  ScratchDir dir;
  platform::LocalFileSystem fs;
  CHECK(symbol_file_messages(fs, dir.path().string()).empty());
}

TEST_CASE("a -S path through a plain file is reported as not a directory",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  ScratchDir dir;
  platform::LocalFileSystem fs;
  REQUIRE(fs.write_atomic(dir.path() / "plain", "x=1\n").ok());
  const std::string through = (dir.path() / "plain" / "syms").string();
  CHECK(symbol_file_messages(fs, through) ==
        Strings{through + ": " + std::make_error_code(std::errc::not_a_directory).message()});
}
