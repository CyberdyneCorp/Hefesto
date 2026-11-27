// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LocalFileSystem read errors carry the OS reason (capability
// hefesto-architecture "Platform services abstraction"; M7 tasks.md 10.6).
// Owner: PLATFORM.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <random>
#include <string>
#include <system_error>
#include <unistd.h>

#include "hefesto/diag/codes.hpp"
#include "hefesto/platform/file_system.hpp"

namespace platform = hefesto::platform;
namespace codes = hefesto::diag::codes;
namespace fs = std::filesystem;

#define FS_TAGS "[cap:hefesto-architecture][req:Platform services abstraction]"

namespace {

class ScratchDir {
 public:
  ScratchDir() {
    std::random_device rd;
    path_ = fs::temp_directory_path() / ("hefesto-fs-test-" + std::to_string(rd()));
    fs::create_directories(path_);
  }
  ScratchDir(const ScratchDir&) = delete;
  ScratchDir& operator=(const ScratchDir&) = delete;
  ~ScratchDir() {
    std::error_code ec;
    fs::permissions(path_ / "locked", fs::perms::owner_all, ec);
    fs::remove_all(path_, ec);
  }
  [[nodiscard]] const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

std::string reason(std::errc e) {
  return std::make_error_code(e).message();
}

}  // namespace

TEST_CASE("a local read of a directory ends in the OS text 'Is a directory'", FS_TAGS) {
  ScratchDir dir;
  platform::LocalFileSystem local;
  for (const auto& result : {local.read(dir.path()), local.read_at_most(dir.path(), 10)}) {
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == codes::kPlatformRead);
    CHECK(result.error().text == "not a regular file '" + dir.path().generic_string() + "': " +
                                     reason(std::errc::is_a_directory));
    CHECK(platform::os_error_text(result.error()) == reason(std::errc::is_a_directory));
  }
}

TEST_CASE("a local read of a file without read permission ends in the OS text", FS_TAGS) {
  if (::geteuid() == 0) {
    SKIP("root reads any file");
  }
  ScratchDir dir;
  platform::LocalFileSystem local;
  const fs::path file = dir.path() / "locked";
  REQUIRE(local.write_atomic(file, "x=1\n").ok());
  fs::permissions(file, fs::perms::none);
  const auto result = local.read(file);
  REQUIRE_FALSE(result.ok());
  CHECK(result.error().code == codes::kPlatformRead);
  CHECK(result.error().text == "cannot read '" + file.generic_string() + "': " + reason(std::errc::permission_denied));
  CHECK(platform::os_error_text(result.error()) == reason(std::errc::permission_denied));
}

TEST_CASE("a missing file keeps its message and gives the ENOENT text", FS_TAGS) {
  ScratchDir dir;
  platform::LocalFileSystem local;
  platform::MemoryFileSystem memory;
  for (platform::FileSystem* fs : {static_cast<platform::FileSystem*>(&local), static_cast<platform::FileSystem*>(&memory)}) {
    const auto result = fs->read(dir.path() / "nope");
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == codes::kPlatformNotFound);
    CHECK(result.error().text == "no such file '" + (dir.path() / "nope").generic_string() + "'");
    CHECK(platform::os_error_text(result.error()) == reason(std::errc::no_such_file_or_directory));
  }
  CHECK(platform::os_error_text(hefesto::diag::make(codes::kPlatformRead, "cannot read 'x'")).empty());
}
