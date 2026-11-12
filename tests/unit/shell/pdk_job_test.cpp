// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PDK manager panel's install job (hefesto-pdk-manager "Headless and
// scripted operation"): the panel's install runs `pdk install ...
// -progress` through a Dispatcher (here with an in-memory Fetcher), its
// progress events drive the bar (live, through a Dispatcher live sink), and
// the finished command lands in the UI session's command log.

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#include "../pdk/fakes.hpp"
#include "../pdk/temp_dir.hpp"
#include "hefesto/app/settings_layers.hpp"
#include "hefesto/pdk/commands.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/pdk_job.hpp"
#include "hefesto/shell/pdk_panel_model.hpp"

namespace fs = std::filesystem;
namespace pdk = hefesto::pdk;
using namespace hefesto;
using namespace hefesto::shell;
using hefesto::test::TarWriter;

namespace {

constexpr const char* kBase = "https://catalog.example/pdks";
const std::string kHash = "0fe599b2afb6708d281543108caf8310912f54af";

/// Forwards to a fetcher the test owns (the job's Dispatcher owns its copy).
class Forwarding final : public pdk::Fetcher {
 public:
  explicit Forwarding(pdk::Fetcher& to) : to_(to) {}
  diag::Outcome<std::string> get_text(std::string_view url) override { return to_.get_text(url); }
  diag::Status download(std::string_view url, const platform::Path& to,
                        const std::function<void(std::uint64_t, std::uint64_t)>& progress) override {
    return to_.download(url, to, progress);
  }

 private:
  pdk::Fetcher& to_;
};

void publish(test::MapFetcher& fetcher) {
  fetcher.documents[std::string{kBase} + "/sky130/manifest.json"] =
      R"({"pdk":"sky130","versions":[{"version":")" + kHash + R"(","date":"2024-08-17T01:34:30Z"}]})";
  std::string assets;
  const auto add = [&](const std::string& content, const std::string& tar) {
    const std::string url = std::string{kBase} + "/sky130/" + kHash + "/" + content + ".tar.zst";
    fetcher.documents[url] = tar;
    assets += std::string{assets.empty() ? "" : ","} + R"({"content":")" + content + R"(","filename":")" + content +
              R"(.tar.zst","url":")" + url + R"(","sha256":")" + pdk::sha256_hex(tar) + R"(","size":)" +
              std::to_string(tar.size()) + "}";
  };
  add("common", TarWriter{}.file("sky130A/libs.tech/magic/sky130A.tech", "tech\n").finish());
  add("sky130_fd_sc_hd", TarWriter{}.file("sky130A/libs.ref/sky130_fd_sc_hd/mag/x.mag", "magic\n").finish());
  fetcher.documents[std::string{kBase} + "/sky130/" + kHash + "/manifest.json"] = "{\"assets\":[" + assets + "]}";
}

}  // namespace

TEST_CASE("install progress is shown as a bar fraction and text", "[cap:hefesto-pdk-manager][req:Headless and scripted operation]") {
  pdk::InstallProgress p{pdk::InstallPhase::download, "common.tar.zst", 1, 2, 50, 100};
  const PdkProgressView v = pdk_progress_view(p);
  CHECK(v.fraction == 0.25);
  CHECK(v.text == "Downloading common.tar.zst (1/2) 50%");
  p = {pdk::InstallPhase::finalize, "", 2, 2, 0, 0};
  CHECK(pdk_progress_view(p).fraction == 1.0);
  p = {pdk::InstallPhase::download, "common.tar.zst", 1, 0, 30, 120};  // live, asset count unknown
  CHECK(pdk_progress_view(p).fraction == 0.25);
  CHECK(pdk_progress_view(p).text == "Downloading common.tar.zst (asset 1) 25%");
  CHECK(with_progress("pdk install sky130A " + kHash) == "pdk install sky130A " + kHash + " -progress");
  CHECK(with_progress("pdk enable sky130A " + kHash) == "pdk enable sky130A " + kHash);
}

TEST_CASE("the panel's install runs through a Dispatcher with progress and is logged in the session",
          "[cap:hefesto-pdk-manager][req:Headless and scripted operation]") {
  test::TempDir dir;
  test::MapFetcher fetcher;
  publish(fetcher);
  const fs::path root = dir.path() / "root";

  PdkJob::Setup setup;
  setup.options = platform::PortableOptions{.storage_root = dir.path() / "storage", .pdk_roots = {},
                                            .in_memory = false, .inline_tasks = true};
  setup.settings.set(app::SettingLayer::user, app::kPdkRoot, root.string());
  setup.hosts.fetcher = [&fetcher](platform::PlatformServices&) { return std::make_unique<Forwarding>(fetcher); };
  setup.hosts.decompressor = [](platform::PlatformServices&) { return std::make_unique<test::CopyDecompressor>(); };
  setup.hosts.install_host = [] { return std::make_unique<test::FakeHost>(); };

  // The line the panel builds for its Install button.
  const auto line = pdk_command(PdkAction::install, "sky130", "sky130A", kHash, {true, true});
  REQUIRE(line);
  PdkJob job;
  // The test catalog publishes one library: install it only.
  REQUIRE(job.start(with_progress(*line + " -lib sky130_fd_sc_hd -source " + kBase), std::move(setup)));
  job.wait();
  CHECK_FALSE(job.running());
  PdkJob::Finished done;
  REQUIRE(job.take_finished(done));
  PdkJob::Finished again;
  CHECK_FALSE(job.take_finished(again));
  const PdkJob::Finished* finished = &done;
  const std::string last = finished->result.diagnostics.empty() ? "" : finished->result.diagnostics.back().text;
  INFO(last);
  REQUIRE(finished->result.ok());
  // The last event is the command's finalize event (PDK-PROGRESS-001).
  pdk::InstallProgress last_event;
  REQUIRE(job.progress(last_event));
  const pdk::InstallProgress* progress = &last_event;
  CHECK(progress->phase == pdk::InstallPhase::finalize);
  CHECK(finished->invocation.name == "pdk");
  CHECK(finished->invocation.origin == cmd::Origin::gui);
  CHECK(fs::exists(root / "ciel" / "sky130" / "versions" / kHash / "sky130A" / "libs.tech" / "magic" / "sky130A.tech"));
  CHECK_FALSE(fetcher.downloads.empty());

  // The UI session records it like a command it ran (command log, listener).
  CoreSession core(platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true,
                                             .inline_tasks = true});
  int heard = 0;
  core.set_command_listener([&heard](const cmd::Invocation&, const cmd::CommandResult&) { ++heard; });
  core.record_command(finished->invocation, finished->result);
  CHECK(heard == 1);
  REQUIRE_FALSE(core.command_log().history().empty());
  CHECK(core.command_log().history().back().find("pdk install sky130A") != std::string::npos);
}

namespace {

/// Holds every download until released, after reporting half of it, so the
/// test can look at the job's progress while the command is still running.
class HeldFetcher final : public pdk::Fetcher {
 public:
  explicit HeldFetcher(test::MapFetcher& to) : to_(to) {}
  diag::Outcome<std::string> get_text(std::string_view url) override { return to_.get_text(url); }
  diag::Status download(std::string_view url, const platform::Path& to,
                        const std::function<void(std::uint64_t, std::uint64_t)>& progress) override {
    const std::uint64_t size = to_.documents[std::string{url}].size();
    if (progress) progress(size / 2, size);
    {
      std::unique_lock lock{mutex_};
      opened_.wait(lock, [this] { return open_; });
    }
    return to_.download(url, to, progress);
  }
  void release() {
    {
      const std::lock_guard guard{mutex_};
      open_ = true;
    }
    opened_.notify_all();
  }

 private:
  test::MapFetcher& to_;
  std::mutex mutex_;
  std::condition_variable opened_;
  bool open_ = false;
};

}  // namespace

// Regression (8.1 SHELL leftover): the bar followed the job's Fetcher, so a
// download showed "(asset N)" without the asset count; the job now takes the
// command's PDK-PROGRESS-001 events live from a Dispatcher live sink.
TEST_CASE("the panel's install job shows live download progress with the asset count",
          "[cap:hefesto-pdk-manager][req:Headless and scripted operation]") {
  test::TempDir dir;
  test::MapFetcher fetcher;
  publish(fetcher);
  HeldFetcher held{fetcher};
  PdkJob::Setup setup;
  setup.options = platform::PortableOptions{.storage_root = dir.path() / "storage", .pdk_roots = {},
                                            .in_memory = false, .inline_tasks = true};
  setup.settings.set(app::SettingLayer::user, app::kPdkRoot, (dir.path() / "root").string());
  setup.hosts.fetcher = [&held](platform::PlatformServices&) { return std::make_unique<Forwarding>(held); };
  setup.hosts.decompressor = [](platform::PlatformServices&) { return std::make_unique<test::CopyDecompressor>(); };
  setup.hosts.install_host = [] { return std::make_unique<test::FakeHost>(); };
  const auto line = pdk_command(PdkAction::install, "sky130", "sky130A", kHash, {true, true});
  REQUIRE(line);
  PdkJob job;
  REQUIRE(job.start(with_progress(*line + " -lib sky130_fd_sc_hd -source " + kBase), std::move(setup)));

  pdk::InstallProgress live;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{20};
  bool seen = false;
  while (!seen && std::chrono::steady_clock::now() < deadline) {
    seen = job.progress(live) && live.phase == pdk::InstallPhase::download && live.bytes > 0;
    if (!seen) std::this_thread::sleep_for(std::chrono::milliseconds{5});
  }
  const bool still_running = job.running();
  held.release();
  job.wait();
  REQUIRE(seen);
  CHECK(still_running);  // observed while the download was held
  CHECK(live.parts == 2);
  CHECK(live.part == 1);
  CHECK(live.asset == "common.tar.zst");
  CHECK(pdk_progress_view(live).text.starts_with("Downloading common.tar.zst (1/2) "));
  PdkJob::Finished done;
  REQUIRE(job.take_finished(done));
  CHECK(done.result.ok());
}

// m4-scripting tasks.md 13.2: the job's line is one command whose words are
// a Tcl list (the worker has no interpreter); several commands or a
// malformed list start nothing.
TEST_CASE("a PDK job takes one command whose words are a Tcl list",
          "[cap:hefesto-pdk-manager][req:Headless and scripted operation]") {
  PdkJob job;
  CHECK_FALSE(job.start("pdk list; pdk list", PdkJob::Setup{}));
  CHECK_FALSE(job.start("pdk list\npdk list", PdkJob::Setup{}));
  CHECK_FALSE(job.start("pdk list {unclosed", PdkJob::Setup{}));
  CHECK_FALSE(job.start("   ", PdkJob::Setup{}));
  CHECK_FALSE(job.running());
  // A braced word is one word. The job's registry has only the PDK
  // commands, so this one fails at once (nothing is fetched or written).
  PdkJob::Setup setup;
  setup.options = platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true,
                                            .inline_tasks = true};
  REQUIRE(job.start("nosuch {sky 130} x", std::move(setup)));
  job.wait();
  PdkJob::Finished finished;
  REQUIRE(job.take_finished(finished));
  CHECK(finished.invocation.name == "nosuch");
  CHECK(finished.invocation.args == std::vector<std::string>{"sky 130", "x"});
  CHECK_FALSE(finished.result.ok());
}
