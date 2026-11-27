// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M7 round 6 (tasks.md 10.6): a `-S` symbol file that cannot be read is
// reported with the operating system's reason, as Magic's perror does
// (extflat/EFsym.c:104-108; Q-NETLIST-58). Owner: EXTFLAT.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <map>
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

/// A file system whose reads fail as the local one does for each errno.
class FailingFileSystem final : public platform::FileSystem {
 public:
  diag::Outcome<std::string> read(const platform::Path& path) override {
    const std::string p = path.generic_string();
    if (p == "missing") {
      return diag::make(diag::codes::kPlatformNotFound, "no such file '" + p + "'");
    }
    const std::errc reason = p == "locked" ? std::errc::permission_denied : std::errc::is_a_directory;
    const std::string what = p == "locked" ? "cannot read" : "not a regular file";
    return diag::make(diag::codes::kPlatformRead,
                      what + " '" + p + "': " + std::make_error_code(reason).message());
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
  CHECK(symbol_file_messages(fs, "folder") ==
        Strings{"folder: " + std::make_error_code(std::errc::is_a_directory).message()});
}

TEST_CASE("a -S directory read through the local file system says it is a directory",
          "[cap:ext-flattening-hierarchy][req:Common netlister arguments]") {
  std::random_device rd;
  const auto dir = std::filesystem::temp_directory_path() / ("hefesto-syms-" + std::to_string(rd()));
  std::filesystem::create_directories(dir);
  platform::LocalFileSystem fs;
  const Strings messages = symbol_file_messages(fs, dir.string());
  std::filesystem::remove_all(dir);
  CHECK(messages == Strings{dir.string() + ": " + std::make_error_code(std::errc::is_a_directory).message()});
}
