// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

#include "hefesto/diag/codes.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/platform/session_log.hpp"
#include "hefesto/platform/stack_thread.hpp"

namespace platform = hefesto::platform;
namespace codes = hefesto::diag::codes;
using namespace std::chrono_literals;

#define SERVICES_TAGS "[cap:hefesto-architecture][req:Platform services abstraction]"
#define LOGGING_TAGS "[cap:hefesto-diagnostics][req:Diagnostic presentation and logging]"

namespace {

// A unique scratch directory under the system temp dir, removed afterwards.
class ScratchDir {
 public:
  ScratchDir() {
    std::random_device rd;
    path_ = std::filesystem::temp_directory_path() / ("hefesto-test-" + std::to_string(rd()));
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

void exercise_file_system(platform::FileSystem& fs, const platform::Path& root) {
  REQUIRE(fs.create_directories(root / "docs").ok());
  REQUIRE(fs.write_atomic(root / "docs" / "b.mag", "magic\n").ok());
  REQUIRE(fs.write_atomic(root / "docs" / "a.mag", "old").ok());
  REQUIRE(fs.write_atomic(root / "docs" / "a.mag", "new").ok());
  REQUIRE(fs.append(root / "log.txt", "1").ok());
  REQUIRE(fs.append(root / "log.txt", "2").ok());
  CHECK(fs.read(root / "docs" / "a.mag").value() == "new");
  CHECK(fs.read(root / "log.txt").value() == "12");
  CHECK(fs.size(root / "docs" / "b.mag").value() == 6U);
  CHECK(fs.exists(root / "docs"));
  CHECK(fs.list(root / "docs").value() == std::vector<platform::Path>{root / "docs" / "a.mag", root / "docs" / "b.mag"});
  CHECK(fs.list(root).value() == std::vector<platform::Path>{root / "docs", root / "log.txt"});
  // Regression: a trailing separator names the same directory.
  const platform::Path docs_slash{(root / "docs").string() + "/"};
  REQUIRE(fs.list(docs_slash).ok());
  CHECK(fs.list(docs_slash).value() == std::vector<platform::Path>{root / "docs" / "a.mag", root / "docs" / "b.mag"});
  // Regression: reading a directory is an error, not an empty file.
  CHECK_FALSE(fs.read(root / "docs").ok());
  REQUIRE(fs.rename(root / "docs" / "b.mag", root / "docs" / "c.mag").ok());
  CHECK_FALSE(fs.exists(root / "docs" / "b.mag"));
  REQUIRE(fs.remove(root / "docs" / "c.mag").ok());
  CHECK(fs.remove(root / "docs" / "c.mag").ok());  // missing file: still ok
  const auto missing = fs.read(root / "nope");
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == codes::kPlatformNotFound);
  CHECK(fs.size(root / "nope").error().code == codes::kPlatformNotFound);
  CHECK_FALSE(fs.list(root / "nope").ok());
}

}  // namespace

TEST_CASE("no process execution: the service reports unsupported without crashing", SERVICES_TAGS) {
  platform::UnsupportedProcessRunner runner;
  CHECK_FALSE(runner.supported());
  const auto result = runner.run({.program = "ngspice", .args = {"-b", "inv.spice"}, .working_directory = {},
                                  .stdin_data = {}});
  REQUIRE_FALSE(result.ok());
  CHECK(result.error().code == "PLATFORM-UNSUPPORTED-001");
  CHECK(result.error().severity == hefesto::diag::Severity::error);
  CHECK(result.error().text.find("ngspice") != std::string::npos);
}

TEST_CASE("portable services default to no process execution", SERVICES_TAGS) {
  platform::PortableServices services({.storage_root = "/app", .pdk_roots = {}, .in_memory = true,
                                       .inline_tasks = true});
  CHECK_FALSE(services.view().processes.supported());
}

TEST_CASE("a front end replaces the PDK store before taking the view", SERVICES_TAGS) {
  struct FixedStore final : platform::PdkStore {
    [[nodiscard]] std::vector<platform::Path> roots() const override { return {"/pdk"}; }
    [[nodiscard]] hefesto::diag::Outcome<platform::Path> find_technology(std::string_view name) override {
      return platform::Path{"/pdk/" + std::string{name} + ".tech"};
    }
  };
  platform::PortableServices services({.storage_root = "/app", .pdk_roots = {}, .in_memory = true,
                                       .inline_tasks = true});
  CHECK_FALSE(services.view().pdks.find_technology("sky130A").ok());
  services.set_pdk_store(std::make_unique<FixedStore>());
  CHECK(services.view().pdks.find_technology("sky130A").value() == platform::Path{"/pdk/sky130A.tech"});
  services.set_pdk_store(nullptr);  // back to an empty directory store
  CHECK(services.view().pdks.roots().empty());
}

TEST_CASE("local file system", SERVICES_TAGS) {
  ScratchDir dir;
  platform::LocalFileSystem fs;
  exercise_file_system(fs, dir.path());
}

TEST_CASE("local file system rejects reading a directory", SERVICES_TAGS) {
  ScratchDir dir;
  platform::LocalFileSystem fs;
  const auto result = fs.read(dir.path());
  REQUIRE_FALSE(result.ok());
  CHECK(result.error().code == codes::kPlatformRead);
  REQUIRE(fs.write_atomic(dir.path() / "empty", "").ok());
  CHECK(fs.read(dir.path() / "empty").value().empty());  // an empty regular file is still fine
}

TEST_CASE("atomic writes leave no temp files behind", SERVICES_TAGS) {
  ScratchDir dir;
  platform::LocalFileSystem fs;
  for (int i = 0; i < 5; ++i) {
    REQUIRE(fs.write_atomic(dir.path() / "doc.mag", std::to_string(i)).ok());
  }
  CHECK(fs.read(dir.path() / "doc.mag").value() == "4");
  CHECK(fs.list(dir.path()).value() == std::vector<platform::Path>{dir.path() / "doc.mag"});
  CHECK_FALSE(fs.write_atomic(dir.path() / "missing-dir" / "doc.mag", "x").ok());
}

TEST_CASE("in-memory file system behaves like the local one", SERVICES_TAGS) {
  platform::MemoryFileSystem fs;
  exercise_file_system(fs, "/root");
}

TEST_CASE("PDK store searches roots in order", SERVICES_TAGS) {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/b/sky130A/libs.tech/magic/sky130A.tech", "tech").ok());
  REQUIRE(fs.write_atomic("/c/sky130A.tech", "tech").ok());
  platform::DirectoryPdkStore store(fs, {"/a", "/b", "/c"});
  CHECK(store.roots().size() == 3U);
  CHECK(store.find_technology("sky130A").value() == platform::Path{"/b/sky130A/libs.tech/magic/sky130A.tech"});
  CHECK(store.find_technology("gf180").error().code == codes::kPlatformTechNotFound);
  CHECK_FALSE(store.find_technology("../c/sky130A").ok());
}

TEST_CASE("autosave snapshots", SERVICES_TAGS) {
  platform::MemoryFileSystem fs;
  platform::FileAutosaveService autosave(fs, "/app/autosave");
  CHECK(autosave.list_snapshots().empty());
  REQUIRE(autosave.write_snapshot("inv", "v1").ok());
  REQUIRE(autosave.write_snapshot("inv", "v2").ok());
  REQUIRE(autosave.write_snapshot("nand2", "x").ok());
  CHECK(autosave.read_snapshot("inv").value() == "v2");
  CHECK(autosave.list_snapshots() == std::vector<std::string>{"inv", "nand2"});
  REQUIRE(autosave.discard_snapshot("inv").ok());
  CHECK(autosave.list_snapshots() == std::vector<std::string>{"nand2"});
  CHECK_FALSE(autosave.write_snapshot("../escape", "x").ok());
}

TEST_CASE("clocks", SERVICES_TAGS) {
  platform::FixedClock clock;
  const auto t0 = clock.monotonic_now();
  clock.advance(1500ms);
  CHECK(clock.monotonic_now() - t0 == 1500ms);
  CHECK(platform::format_utc_timestamp(clock.wall_now()) == "1970-01-01T00:00:01.500Z");
  CHECK(platform::format_utc_timestamp(std::chrono::sys_days{std::chrono::days{20399}} + 13h + 5min + 9s) ==
        "2025-11-07T13:05:09.000Z");
  CHECK(platform::format_utc_timestamp(std::chrono::system_clock::time_point{} - 1ms) == "1969-12-31T23:59:59.999Z");
  platform::SystemClock system;
  // Two statements: the operands of one expression are unsequenced, and GCC
  // evaluates the right-hand call first (regression seen on GCC 11).
  const auto first = system.monotonic_now();
  const auto second = system.monotonic_now();
  CHECK(first <= second);
}

TEST_CASE("task executors run every task", SERVICES_TAGS) {
  platform::InlineExecutor inline_exec;
  int n = 0;
  inline_exec.submit([&] { ++n; });
  CHECK(n == 1);

  std::atomic<int> count{0};
  {
    platform::ThreadPoolExecutor pool(4);
    for (int i = 0; i < 1000; ++i) {
      pool.submit([&] { count.fetch_add(1); });
    }
    pool.wait_idle();
    CHECK(count.load() == 1000);
    pool.submit([&] { count.fetch_add(1); });
  }  // destructor drains the queue
  CHECK(count.load() == 1001);
}

TEST_CASE("session log rotates within the app storage", LOGGING_TAGS) {
  platform::MemoryFileSystem fs;
  platform::FixedClock clock;
  platform::RotatingFileLogger logger(fs, clock, "/app/logs", "session", {.max_bytes = 100, .max_files = 3});
  const std::string message(40, 'x');  // each line is 24 + 1 + 4 + 1 + 40 + 1 = 71 bytes
  for (int i = 0; i < 5; ++i) {
    logger.write(hefesto::diag::Severity::info, message);
  }
  CHECK_FALSE(logger.last_error().has_value());
  CHECK(fs.list("/app/logs").value() ==
        std::vector<platform::Path>{"/app/logs/session.1.log", "/app/logs/session.2.log", "/app/logs/session.log"});
  CHECK(fs.read("/app/logs/session.log").value() == "1970-01-01T00:00:00.000Z info " + message + "\n");
  for (const char* name : {"/app/logs/session.log", "/app/logs/session.1.log", "/app/logs/session.2.log"}) {
    CHECK(fs.size(name).value() <= 100U);
  }
  logger.log(hefesto::diag::make(codes::kCmdDispatch, "unknown command \"x\""));
  CHECK(fs.read("/app/logs/session.1.log").value().find(message) != std::string::npos);
}

TEST_CASE("default storage dir is per user", LOGGING_TAGS) {
  const auto dir = platform::default_storage_dir();
  CHECK_FALSE(dir.empty());
#if defined(__APPLE__)
  CHECK(dir.filename() == "Hefesto");
#endif
}

TEST_CASE("install_session_log puts a rotating log in the given storage dir", LOGGING_TAGS) {
  platform::PortableServices services({.storage_root = "/app", .pdk_roots = {}, .in_memory = true,
                                       .inline_tasks = true});
  const platform::SessionLog log = platform::install_session_log(services, "/app/logs", "hefesto", codes::kPlatformLog);
  REQUIRE(log.logger != nullptr);
  CHECK_FALSE(log.warning);
  services.view().logger.write(hefesto::diag::Severity::info, "session start");
  CHECK(services.files().read("/app/logs/hefesto.log").value().find("info session start") != std::string::npos);
}

TEST_CASE("install_session_log warns and keeps going when the dir cannot be made", LOGGING_TAGS) {
  ScratchDir dir;
  std::ofstream(dir.path() / "file") << "x";
  platform::PortableServices services({.storage_root = dir.path(), .pdk_roots = {}, .in_memory = false,
                                       .inline_tasks = true});
  const platform::SessionLog log =
      platform::install_session_log(services, dir.path() / "file" / "logs", "hefesto", codes::kPlatformLog);
  CHECK(log.logger == nullptr);
  REQUIRE(log.warning);
  CHECK(log.warning->code == codes::kPlatformLog);
  services.view().logger.write(hefesto::diag::Severity::info, "dropped");  // NullLogger: no crash
}

namespace {

// About 1 KiB of stack per level.
std::size_t deep_recursion(std::size_t n) {
  std::array<char, 1024> frame{};
  frame[n % frame.size()] = 1;
  return n == 0 ? 0 : deep_recursion(n - 1) + static_cast<std::size_t>(frame[n % frame.size()]);
}

}  // namespace

TEST_CASE("run_on_stack runs deep recursion on a thread with the requested stack",
          "[cap:hefesto-architecture][req:Concurrency model]") {
  // About 16 MiB of frames: far past a secondary thread's default stack.
  std::size_t depth = 0;
  hefesto::platform::run_on_stack(hefesto::platform::kDeepRecursionStackBytes,
                                  [&depth] { depth = deep_recursion(16 * 1024); });
  CHECK(depth == 16 * 1024);
  CHECK_THROWS_AS(hefesto::platform::run_on_stack(1, [] { throw std::runtime_error("boom"); }), std::runtime_error);
}
