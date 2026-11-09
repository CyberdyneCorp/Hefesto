// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop session's host services (hefesto-files-and-pdks "PDK store";
// cell-io-safety crash backups): the managed PDK store and install host,
// the POSIX lock service, and the crash-backup clock polled by the frame
// loop.

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <string>

#include "hefesto/cell/crash.hpp"
#include "hefesto/shell/backup_timer.hpp"
#include "hefesto/shell/core_command_sink.hpp"

namespace fs = std::filesystem;
using namespace hefesto;
using namespace hefesto::shell;

namespace {

struct TempDir {
  fs::path path;
  TempDir() {
    path = fs::temp_directory_path() / ("hefesto-host-" + cell::make_session_id());
    fs::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
};

}  // namespace

TEST_CASE("the deadline timer fires once when due", "[cap:cell-io-safety]") {
  DeadlineTimer::Clock::time_point now{};
  DeadlineTimer timer([&now] { return now; });
  CHECK_FALSE(timer.seconds_left());
  timer.schedule(1500);
  CHECK(timer.armed());
  CHECK(*timer.seconds_left() == 1.5);
  CHECK_FALSE(timer.take_due());
  now += std::chrono::milliseconds(1500);
  CHECK(*timer.seconds_left() == 0.0);
  CHECK(timer.take_due());
  CHECK_FALSE(timer.take_due());  // one-shot
  timer.schedule(10);
  timer.cancel();
  CHECK_FALSE(timer.armed());
}

TEST_CASE("a desktop session installs the managed PDK store, the lock service and the backup clock",
          "[cap:hefesto-files-and-pdks][req:PDK store]") {
  TempDir dir;
  CoreSession core(platform::PortableOptions{.storage_root = dir.path / "storage", .pdk_roots = {}, .in_memory = false,
                                             .inline_tasks = true},
                   std::nullopt, HostServices{true, dir.path / "pdks"});
  REQUIRE(core.install_host() != nullptr);
  REQUIRE(core.pdk_manager() != nullptr);
  CHECK(core.pdk_manager()->root() == dir.path / "pdks");
  CHECK(core.services().processes.supported());
  REQUIRE(core.editor().locks != nullptr);
  CHECK(core.editor().cell_session.import_magic_backups);
  // `crashbackups start` arms the desktop's clock (the core's default interval).
  CHECK_FALSE(core.backup_timer_seconds());
  const auto started =
      core.dispatcher().execute(cmd::Invocation{"crashbackups", {"start"}, cmd::Origin::gui, std::nullopt});
  REQUIRE(started.ok());
  REQUIRE(core.backup_timer_seconds());
  CHECK(*core.backup_timer_seconds() > 0.0);
  CHECK_FALSE(core.poll_backup_timer());  // not due yet
  core.end_crash_session();
}

TEST_CASE("without host services the session stays hermetic", "[cap:hefesto-files-and-pdks][req:PDK store]") {
  CoreSession core(platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true,
                                             .inline_tasks = true});
  CHECK(core.install_host() == nullptr);
  CHECK_FALSE(core.services().processes.supported());
}
