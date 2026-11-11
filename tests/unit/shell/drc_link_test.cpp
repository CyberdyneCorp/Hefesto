// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop host of the background DRC checker (DrcPump, DrcLink) over a
// real core session: the worker's ready callback posts one task to the
// event loop, which commits squares between commands; committed squares
// are marked for redisplay; the result equals `drc catchup`; and every DRC
// control of the chip menu and the browser is a logged, replayable command.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/drc_link.hpp"
#include "hefesto/shell/drc_models.hpp"
#include "hefesto/ui/line.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

std::string read_text(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

// tests/data/drc/rules/rules.tech with m1 width 3 and spacing 4.
std::string rules_tech() {
  return read_text(std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/data/drc/rules/rules.tech") + R"(
drc
style first
    width m1 3 "m1 width < 3"
    spacing m1 m1 4 touching_ok "m1 spacing < 4"
style second
    width m1 5 "m1 width < 5"
end

extract
end
)";
}

platform::PortableOptions test_options() {
  return platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true, .inline_tasks = true};
}

/// The UI event loop of the tests: tasks posted from any thread.
struct TaskQueue {
  std::mutex mutex;
  std::vector<std::function<void()>> tasks;
  void post(std::function<void()> task) {
    const std::lock_guard lock(mutex);
    tasks.push_back(std::move(task));
  }
  std::size_t run() {
    std::vector<std::function<void()>> now;
    {
      const std::lock_guard lock(mutex);
      now.swap(tasks);
    }
    for (auto& t : now) t();
    return now.size();
  }
};

struct DrcSession {
  CoreSession core{test_options()};

  DrcSession() {
    REQUIRE(core.files().write_atomic("/t/rules.tech", rules_tech()).ok());
    REQUIRE(run("tech load /t/rules.tech -noprompt").ok());
    REQUIRE(run("load top").ok());
    db::CellDef* top = core.editor().cells.find("top");
    REQUIRE(top != nullptr);
    if (core.editor().windows.current() == nullptr) REQUIRE(core.editor().windows.open_frame(top).ok());
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(core.dispatcher(), line, cmd::Origin::gui)) {
      if (!r.ok()) return std::move(r);
      last = std::move(r);
    }
    return last;
  }

  std::string value(std::string_view line) { return run(line).value.to_string(); }
  db::CellDef& top() { return *core.editor().cells.find("top"); }
};

}  // namespace

TEST_CASE("the ready callback posts one task to the event loop, coalescing repeats",
          "[cap:hefesto-architecture][req:Concurrency model]") {
  DrcSession s;
  TaskQueue queue;
  std::vector<std::size_t> serviced;
  DrcPump pump(
      s.core.editor(), [&queue](std::function<void()> task) { queue.post(std::move(task)); },
      [&serviced](std::size_t n) { serviced.push_back(n); });
  pump.notify();
  pump.notify();
  pump.notify();
  CHECK(queue.run() == 1);
  CHECK(serviced == std::vector<std::size_t>{0});
  pump.notify();  // after the task ran, the next callback posts again
  CHECK(queue.run() == 1);
}

TEST_CASE("the desktop host checks in the background and ends where drc catchup ends",
          "[cap:hefesto-architecture][req:Concurrency model]") {
  DrcSession s;
  DrcLink link(s.core.editor());
  link.set_root(&s.top());
  TaskQueue queue;
  std::size_t committed = 0;
  DrcPump pump(
      s.core.editor(), [&queue](std::function<void()> task) { queue.post(std::move(task)); },
      [&committed](std::size_t n) { committed += n; });
  CHECK_FALSE(link.root_checked());
  REQUIRE(s.run("box 0 0 2 10; paint m1; box 5 0 10 10; paint m1").ok());
  // The event loop: run posted tasks until the checker is idle.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while ((pump.busy() || !s.core.editor().drc.pending.empty()) && std::chrono::steady_clock::now() < deadline) {
    if (queue.run() == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE_FALSE(pump.busy());
  CHECK(s.core.editor().drc.pending.empty());
  CHECK(committed > 0);
  CHECK(link.take_committed() > 0);  // squares; finished cells commit without a square event
  CHECK(link.root_checked());
  const auto job = link.take_finished_job();
  REQUIRE(job);
  CHECK(*job >= 0.0);
  // Each committed square was marked for redisplay in the window showing it.
  const view::LayoutWindow* win = s.core.editor().windows.current();
  REQUIRE(win != nullptr);
  CHECK_FALSE(win->pending_redisplay().empty());
  // The same errors as a synchronous `drc check; drc catchup`.
  const std::string background = s.value("box -20 -20 30 30; drc listall why");
  CHECK(background == "{m1 width < 3} {{2 0 3 10}} {m1 spacing < 4} {{5 0 6 10}}");
  REQUIRE(s.run("drc off; box -20 -20 30 30; drc check; drc catchup").ok());
  CHECK(s.value("box -20 -20 30 30; drc listall why") == background);
  CHECK(s.value("drc list count total") == "2");
}

TEST_CASE("drc off keeps the worker idle; drc on resumes it", "[cap:hefesto-architecture][req:Concurrency model]") {
  DrcSession s;
  DrcLink link(s.core.editor());
  TaskQueue queue;
  DrcPump pump(
      s.core.editor(), [&queue](std::function<void()> task) { queue.post(std::move(task)); }, {});
  REQUIRE(s.run("drc off").ok());
  // CmdPaint does not mark the area while DRC is off (Magic).
  REQUIRE(s.run("box 0 0 2 10; paint m1").ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  (void)queue.run();
  CHECK_FALSE(pump.busy());
  CHECK_FALSE(link.busy());
  REQUIRE(s.run("drc on; box 5 0 7 10; paint m1").ok());
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while ((pump.busy() || !s.core.editor().drc.pending.empty()) && std::chrono::steady_clock::now() < deadline) {
    if (queue.run() == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(s.core.editor().drc.pending.empty());
  CHECK_FALSE(link.busy());
  CHECK(s.value("box -20 -20 30 30; drc listall why").find("m1 width < 3") != std::string::npos);
}

TEST_CASE("a style change and edits change the browser's stamp", "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  DrcSession s;
  DrcLink link(s.core.editor());
  TaskQueue queue;
  DrcPump pump(
      s.core.editor(), [&queue](std::function<void()> task) { queue.post(std::move(task)); }, {});
  const DrcLink::Stamp before = link.stamp();
  REQUIRE(s.run("box 0 0 2 10; paint m1").ok());
  CHECK(link.stamp() != before);  // the edit generation moved: the list is stale
  const DrcLink::Stamp edited = link.stamp();
  const std::uint64_t serial = link.serial();
  REQUIRE(s.run("drc style second").ok());
  CHECK(link.stamp() != edited);
  CHECK(link.serial() != serial);
}

TEST_CASE("every DRC control is a logged, replayable command", "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  DrcSession s;
  REQUIRE(s.run("box 0 0 2 10; paint m1; box 5 0 10 10; paint m1").ok());
  std::vector<std::string> lines;
  DrcMenuInput menu_in{true, false, true, {"first", "second"}, "first"};
  for (const MenuEntry& e : drc_chip_menu(menu_in)) {
    if (e.command != kDrcOpenPanelCommand) lines.push_back(e.command);  // `panel` is the window's (view_commands_test)
  }
  const DrcStepper stepper{std::nullopt, 2};
  lines.push_back(stepper.next_command());
  lines.push_back(stepper.prev_command());
  lines.push_back(drc_ignore_command("m1 width"));
  lines.emplace_back(kDrcClearIgnoresCommand);
  DrcBrowserModel model;
  model.set_list({DrcGroup{"m1 width < 3", {db::Rect{2, 0, 3, 10}}, false}});
  REQUIRE(model.toggle(0));
  lines.emplace_back(kDrcRecheckCommand);
  lines.push_back(*model.command(1));
  CoreCommandSink sink(s.core.dispatcher());
  for (const std::string& line : lines) {
    INFO(line);
    CHECK(sink.submit(line).ok());
  }
  const std::string script = s.core.command_log().history_script();
  for (const std::string& line : lines) {
    INFO(line);
    // Each command of each line is in the command log, as a script line.
    std::size_t from = 0;
    while (from < line.size()) {
      std::size_t end = line.find("; ", from);
      if (end == std::string::npos) end = line.size();
      const std::string command = line.substr(from, end - from);
      // The option word is enough: the log quotes arguments its own way.
      const std::size_t words = command.find(' ', command.find(' ') + 1);
      CHECK(script.find(command.substr(0, words)) != std::string::npos);
      from = end + 2;
    }
  }
  // The area row moved the box onto the error.
  CHECK(s.core.editor().box == db::Rect{2, 0, 3, 10});
}
