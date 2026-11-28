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
#include <sys/stat.h>
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
  const platform::StreamRead stream = local.read_stream(file);
  CHECK(stream.error == std::make_error_code(std::errc::permission_denied));
  CHECK(stream.text.empty());
}

TEST_CASE("a stream read gives the text or the errno of the failed fopen", FS_TAGS) {
  ScratchDir dir;
  platform::LocalFileSystem local;
  platform::MemoryFileSystem memory;
  for (platform::FileSystem* fs : {static_cast<platform::FileSystem*>(&local), static_cast<platform::FileSystem*>(&memory)}) {
    REQUIRE(fs->write_atomic(dir.path() / "syms", "a=1\nb=2\n").ok());
    const platform::StreamRead ok = fs->read_stream(dir.path() / "syms");
    CHECK_FALSE(ok.error);
    CHECK(ok.text == "a=1\nb=2\n");
    const auto missing = fs->read_stream(dir.path() / "nope");
    CHECK(missing.error == std::make_error_code(std::errc::no_such_file_or_directory));
    CHECK(fs->read(dir.path() / "nope").error().text == "no such file '" + (dir.path() / "nope").generic_string() + "'");
    // fopen opens a directory and reads nothing from it: no error.
    const auto folder = fs->read_stream(dir.path());
    CHECK_FALSE(folder.error);
    CHECK(folder.text.empty());
  }
  // A path through a plain file: fopen's ENOTDIR, not ENOENT.
  const auto through = local.read_stream(dir.path() / "syms" / "x");
  CHECK(through.error == std::make_error_code(std::errc::not_a_directory));
}

TEST_CASE("a stream read of a FIFO does not wait for a writer", FS_TAGS) {
  ScratchDir dir;
  const fs::path fifo = dir.path() / "pipe";
  REQUIRE(::mkfifo(fifo.c_str(), 0600) == 0);
  platform::LocalFileSystem local;
  const auto read = local.read_stream(fifo);
  CHECK_FALSE(read.error);
  CHECK(read.text.empty());
}
