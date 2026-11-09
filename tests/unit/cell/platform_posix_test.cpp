// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The POSIX host services of src/platform_posix (cell-io-safety "Locking
// mechanism", "Crash recovery", "Import of Magic crash backups") on a
// temporary directory. Built only where the library is linked.

#include <catch2/catch_test_macros.hpp>

#if defined(HEFESTO_HAVE_PLATFORM_POSIX)

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "hefesto/platform_posix/file_host.hpp"
#include "hefesto/platform_posix/lock_service.hpp"

namespace fs = std::filesystem;

namespace {

struct TempDir {
  fs::path path;
  TempDir() {
    std::random_device rd;
    path = fs::temp_directory_path() / ("hefesto-posix-" + std::to_string(rd()));
    fs::create_directories(path);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

void touch(const fs::path& p) {
  std::ofstream{p} << "x";
}

}  // namespace

TEST_CASE("the POSIX lock service locks existing files and releases them",
          "[cap:cell-io-safety][req:Locking mechanism]") {
  TempDir dir;
  const fs::path file = dir.path / "a.mag";
  auto locks = hefesto::platform_posix::make_lock_service();
  CHECK_FALSE(locks->acquire(file).value());  // missing: cannot open for update
  touch(file);
  CHECK(locks->acquire(file).value());
  CHECK(locks->holds(file));
  CHECK(locks->acquire(file).value());  // already held
  locks->release(file);
  CHECK_FALSE(locks->holds(file));
}

TEST_CASE("the POSIX file host reports ownership, change time and process liveness",
          "[cap:cell-io-safety][req:Crash recovery][req:Import of Magic crash backups]") {
  TempDir dir;
  const fs::path file = dir.path / "MAG1.abcdef";
  touch(file);
  auto host = hefesto::platform_posix::make_file_host();
  CHECK(host->owned_by_current_user(file));
  CHECK_FALSE(host->owned_by_current_user(dir.path / "missing"));
  CHECK(host->change_time(file).value_or(0) > 0);
  CHECK_FALSE(host->change_time(dir.path / "missing").has_value());
  CHECK(host->process_running(::getpid()) == true);
  CHECK(host->process_running(0) == false);
  CHECK(host->process_running(std::int64_t{1} << 40) == false);
  CHECK(host->is_directory(dir.path));
}

#endif
