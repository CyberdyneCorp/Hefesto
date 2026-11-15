// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop host of long GDS and CIF reads and writes (shell::MaskJobHost;
// openspec/changes/m5-mask-gds-cif tasks.md 12.2, design D16): progress and
// the Interrupt card, a cancelled write leaving no file, an edit made during
// a write queued until it ends, the emit phase on its deep-stack worker
// giving the command thread's bytes, and (under TSan in CI) a write running
// while the background DRC checker works on the same hierarchy.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "hefesto/cif/write/write.hpp"
#include "hefesto/db/lock.hpp"
#include "hefesto/gds/write/write.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/shell/console_session.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/drc_link.hpp"
#include "hefesto/shell/mask_job_host.hpp"
#include "hefesto/shell/tcl_console.hpp"
#include "hefesto/ui/line.hpp"
#include "shell_fixture.hpp"

using namespace hefesto;
using namespace hefesto::shell;
using namespace std::chrono_literals;

namespace {

// metal1 written to GDS layer 49 and CIF layer CM1, with two DRC rules.
constexpr std::string_view kTech =
    "tech\n format 35\n maskjob\nend\nplanes\n metal1,m1\nend\ntypes\n metal1 metal1,m1\nend\n"
    "contact\nend\nstyles\n styletype mos\n metal1 20\nend\ncompose\nend\nconnect\nend\n"
    "cifoutput\nstyle gdsii\n scalefactor 10\n layer CM1 metal1\n  labels metal1\n  calma 49 0\nend\n"
    "cifinput\nstyle gdsii\n scalefactor 10\n layer metal1 CM1\n  labels CM1\n  calma CM1 49 0\nend\n"
    "drc\nstyle first\n width m1 3 \"m1 width < 3\"\n spacing m1 m1 4 touching_ok \"m1 spacing < 4\"\nend\n"
    "extract\nend\n";

platform::PortableOptions test_options() {
  return platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true, .inline_tasks = true};
}

/// A core session with a hierarchy: `top` holds a 3x3 array of `leaf`
/// (each with two metal1 bars) and bars of its own across the array, so the
/// writers generate interaction areas and the DRC checker has work.
struct JobSession {
  CoreSession core{test_options()};

  JobSession() {
    (void)hefesto::shell::test::bind_interpreter(core);
    REQUIRE(core.files().create_directories("/work").ok());
    REQUIRE(core.files().write_atomic("/work/maskjob.tech", std::string{kTech}).ok());
    ok("tech load /work/maskjob.tech -noprompt");
    ok("load leaf");
    db::CellDef* leaf = core.editor().cells.find("leaf");
    REQUIRE(leaf != nullptr);
    if (core.editor().windows.current() == nullptr) REQUIRE(core.editor().windows.open_frame(leaf).ok());
    ok("snap internal");
    ok("box 0 0 2 10; paint m1; box 5 0 8 10; paint m1");
    ok("load top");
    ok("box 0 0 0 0; getcell leaf; array 3 3 20 20");
    ok("box -5 3 70 6; paint m1; box 10 -5 12 70; paint m1");
  }

  void ok(std::string_view line) {
    for (auto& r : hefesto::ui::run_command_line(core.dispatcher(), line, cmd::Origin::gui)) {
      INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
      REQUIRE(r.ok());
    }
  }

  editor::EditorState& editor() { return core.editor(); }
  db::CellDef& top() { return *core.editor().cells.find("top"); }

  /// Fixed dates, so two writes of the same layout give the same bytes.
  gds::GdsWriteOptions gds_options() {
    gds::GdsWriteOptions options;
    options.settings = core.editor().mask.gds;
    options.settings.datestamp = 0;
    options.typed_name = "chip";
    options.files = &core.files();
    options.now = 0;
    return options;
  }

  cif::CifWriteOptions cif_options() {
    cif::CifWriteOptions options;
    options.settings = core.editor().mask.cif;
    options.typed_name = "chip";
    options.files = &core.files();
    options.now = 0;
    return options;
  }

  std::string file(const std::string& path) {
    auto read = core.files().read(path);
    REQUIRE(read.ok());
    return read.value();
  }

  /// gds::write on the calling thread under the exclusive lock (the command
  /// thread's way).
  std::string reference_gds(const std::string& path) {
    const db::DatabaseLock::Exclusive writing(core.editor().database_lock);
    REQUIRE(gds::write(core.editor(), top(), path, gds_options()).ok());
    return file(path);
  }
};

/// The host phases of a GDS write of `root` to `path`.
MaskWritePhases gds_phases(editor::EditorState& ed, db::CellDef& root, const gds::GdsWriteOptions& options,
                           const std::string& path, const MaskWork& before_emit = {}) {
  auto prepared = std::make_shared<std::optional<gds::PreparedWrite>>();
  MaskWritePhases phases;
  phases.prepare = [&ed, &root, options, prepared]() -> std::optional<cmd::CommandResult> {
    diag::Outcome<gds::PreparedWrite> out = gds::prepare_write(ed, root, options);
    if (!out) return cmd::CommandResult::failure(out.error());
    *prepared = std::move(out).value();
    return std::nullopt;
  };
  phases.emit = [prepared, path, before_emit](const mask::Progress* progress) {
    if (before_emit) (void)before_emit(progress);
    return gds::emit_write(**prepared, path, progress);
  };
  phases.finish = [&ed, prepared] { gds::finish_write(ed, **prepared); };
  return phases;
}

MaskWritePhases cif_phases(editor::EditorState& ed, db::CellDef& root, const cif::CifWriteOptions& options,
                           const std::string& path) {
  auto prepared = std::make_shared<std::optional<cif::PreparedWrite>>();
  MaskWritePhases phases;
  phases.prepare = [&ed, &root, options, prepared]() -> std::optional<cmd::CommandResult> {
    diag::Outcome<cif::PreparedWrite> out = cif::prepare_write(ed, root, options);
    if (!out) return cmd::CommandResult::failure(out.error());
    *prepared = std::move(out).value();
    return std::nullopt;
  };
  phases.emit = [prepared, path](const mask::Progress* progress) {
    return cif::emit_write(**prepared, path, progress);
  };
  phases.finish = [&ed, options] { cif::finish_write(ed, options); };
  return phases;
}

}  // namespace

TEST_CASE("the mask job card and its progress text",
          "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  MaskJobState state;
  state.label = "/work/chip.gds";
  state.elapsed = 1500ms;
  CHECK_FALSE(mask_job_card(state));  // shown only after kLongRunning, as a script's
  state.elapsed = 14s;
  auto card = mask_job_card(state);
  REQUIRE(card);
  CHECK(card->title == "Writing chip.gds · 14 s");
  CHECK(card->detail.empty());
  CHECK(card->action == "Interrupt");
  CHECK(card->shortcut == "Ctrl-C");
  state.fraction = 0.429;
  CHECK(mask_job_card(state)->detail == "Completed 42%");
  state.cancel_requested = true;
  CHECK(mask_job_card(state)->detail == "Stopping: no file is written");
  state.kind = MaskJobKind::read;
  state.cancel_requested = false;
  CHECK(mask_job_card(state)->title == "Reading chip.gds · 14 s");
  CHECK(completed_text(1.0) == "Completed 100%");
  CHECK(completed_text(-0.5) == "Completed 0%");
}

TEST_CASE("a write reports progress while the calling thread pumps frames",
          "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  db::DatabaseLock lock;
  MaskJobHost host(lock);
  std::atomic<bool> reported{false};
  std::vector<std::optional<MaskJobState>> seen;
  const std::thread::id caller = std::this_thread::get_id();
  bool pumped_elsewhere = false;
  host.set_pump(
      [&] {
        pumped_elsewhere = pumped_elsewhere || std::this_thread::get_id() != caller;
        seen.push_back(host.state());
        if (seen.back() && seen.back()->fraction) reported = true;
      },
      1ms);
  CHECK_FALSE(host.state());
  // Catch2 assertions stay on the test thread: the worker records.
  std::atomic<bool> on_worker{false};
  const cmd::CommandResult result = host.run_emit("/work/chip.gds", [&](const mask::Progress* progress) {
    on_worker = std::this_thread::get_id() != caller;  // the deep-stack worker
    if (progress == nullptr) return cmd::CommandResult::failure(diag::make("TEST-001", "no progress"));
    progress->update(0.42);
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (!reported && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
    return cmd::CommandResult::success();
  });
  CHECK(result.ok());
  CHECK(on_worker);
  REQUIRE(reported);
  CHECK_FALSE(pumped_elsewhere);
  const MaskJobState& last = *seen.back();
  CHECK(last.kind == MaskJobKind::write);
  CHECK(last.label == "/work/chip.gds");
  CHECK(*last.fraction == 0.42);
  CHECK_FALSE(host.busy());
  CHECK_FALSE(host.state());

  // A read runs on the calling thread; its reports pump frames there.
  std::size_t frames = 0;
  host.set_pump([&] { ++frames; }, 0ms);
  const cmd::CommandResult read_result = host.run_read("chip.gds", [&](const mask::Progress* progress) {
    CHECK(std::this_thread::get_id() == caller);
    CHECK(host.state()->kind == MaskJobKind::read);
    progress->update(0.5);
    progress->update(1.0);
    return cmd::CommandResult::success();
  });
  CHECK(read_result.ok());
  CHECK(frames == 2);
}

TEST_CASE("an exception thrown by the emit phase reaches the caller",
          "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  db::DatabaseLock lock;
  MaskJobHost host(lock);
  CHECK_THROWS_AS(host.run_emit("x.gds", [](const mask::Progress*) -> cmd::CommandResult { throw std::bad_alloc(); }),
                  std::bad_alloc);
  CHECK_FALSE(host.busy());
}

TEST_CASE("a cancelled GDS or CIF write leaves no output file",
          "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  JobSession s;
  MaskJobHost host(s.editor().database_lock);
  CHECK_FALSE(host.request_cancel());  // nothing runs
  // The Interrupt arrives during the emit phase (the card sets an atomic
  // flag, which the worker polls).
  std::atomic<bool> cancelled{false};
  std::atomic<bool> shown{false};
  const MaskWork interrupt = [&](const mask::Progress*) {
    cancelled = host.request_cancel();
    const std::optional<MaskJobState> state = host.state();
    shown = state && state->cancel_requested;
    return cmd::CommandResult::success();
  };
  const cmd::CommandResult gds_result =
      host.run_write("chip.gds", gds_phases(s.editor(), s.top(), s.gds_options(), "/work/chip.gds", interrupt));
  CHECK(cancelled);
  CHECK(shown);
  CHECK_FALSE(gds_result.ok());
  CHECK_FALSE(s.core.files().exists("/work/chip.gds"));
  const auto listed = s.core.files().list("/work");
  REQUIRE(listed.ok());
  CHECK(listed.value().size() == 1);  // maskjob.tech only: no temporary file is left

  // The next write starts with the flag cleared.
  REQUIRE(host.run_write("chip.gds", gds_phases(s.editor(), s.top(), s.gds_options(), "/work/chip.gds")).ok());
  CHECK(s.core.files().exists("/work/chip.gds"));

  MaskWritePhases cif_write = cif_phases(s.editor(), s.top(), s.cif_options(), "/work/chip.cif");
  const MaskWork emit = cif_write.emit;
  cif_write.emit = [&host, emit](const mask::Progress* progress) {
    (void)host.request_cancel();
    return emit(progress);
  };
  CHECK_FALSE(host.run_write("chip.cif", cif_write).ok());
  CHECK_FALSE(s.core.files().exists("/work/chip.cif"));
}

TEST_CASE("an edit made during a write is queued and never reaches the file",
          "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  JobSession s;
  // The command thread's write (gds::write, all three phases inline).
  const std::string reference = s.reference_gds("/work/reference.gds");
  MaskJobHost host(s.editor().database_lock);
  // The user paints in a frame drawn while the worker emits: the edit waits.
  std::atomic<bool> deferred{false};
  bool queued = false;
  bool edited = false;
  host.set_pump(
      [&] {
        if (deferred) return;
        host.defer([&] {
          s.ok("box 100 100 110 110; paint m1");
          edited = true;
        });
        queued = !edited && host.busy();
        deferred = true;
      },
      1ms);
  // The emit starts once the edit was made (the worker waits for it).
  const MaskWork edit_during = [&](const mask::Progress*) {
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (!deferred && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
    return cmd::CommandResult::success();
  };
  const cmd::CommandResult result =
      host.run_write("chip.gds", gds_phases(s.editor(), s.top(), s.gds_options(), "/work/chip.gds", edit_during));
  INFO((result.diagnostics.empty() ? std::string{} : result.diagnostics.front().text));
  REQUIRE(result.ok());
  CHECK(queued);
  CHECK(edited);  // ran once the write ended
  CHECK(s.top().bbox().x1 >= 110);
  // The worker's bytes are the command thread's (the edit is not in them).
  CHECK(s.file("/work/chip.gds") == reference);
  // Idle: an edit runs at once.
  bool now = false;
  host.defer([&] { now = true; });
  CHECK(now);
}

TEST_CASE("a write inside a command emits on the worker under the command's lock",
          "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  JobSession s;
  const std::string reference = s.reference_gds("/work/reference.gds");
  MaskJobHost host(s.editor().database_lock);
  // As the gds command would: prepare, then run_emit, then finish, all on
  // the command thread that holds the lock exclusively.
  const db::DatabaseLock::Exclusive command(s.editor().database_lock);
  diag::Outcome<gds::PreparedWrite> prepared = gds::prepare_write(s.editor(), s.top(), s.gds_options());
  REQUIRE(prepared.ok());
  std::size_t frames = 0;
  host.set_pump([&] { ++frames; }, 1ms);
  const cmd::CommandResult result = host.run_emit("chip.gds", [&](const mask::Progress* progress) {
    std::this_thread::sleep_for(20ms);  // a few frames
    return gds::emit_write(prepared.value(), "/work/chip.gds", progress);
  });
  gds::finish_write(s.editor(), prepared.value());
  REQUIRE(result.ok());
  CHECK(frames > 0);
  CHECK(s.file("/work/chip.gds") == reference);
}

TEST_CASE("a GDS write on the worker runs while the background DRC checks the same hierarchy",
          "[cap:hefesto-architecture][req:Concurrency model]") {
  JobSession s;
  const std::string reference = s.reference_gds("/work/reference.gds");
  // The UI event loop of the test: the checker's ready tasks.
  std::mutex mutex;
  std::vector<std::function<void()>> tasks;
  const auto run_tasks = [&] {
    std::vector<std::function<void()>> now;
    {
      const std::lock_guard lock(mutex);
      now.swap(tasks);
    }
    for (auto& t : now) t();
    return now.size();
  };
  MaskJobHost host(s.editor().database_lock);
  std::size_t committed = 0;
  std::size_t committed_during_job = 0;
  DrcPump pump(
      s.editor(),
      [&](std::function<void()> task) {
        const std::lock_guard lock(mutex);
        tasks.push_back(std::move(task));
      },
      [&](std::size_t n) {
        committed += n;
        if (host.busy()) committed_during_job += n;
      });
  // The frames a job pumps run the posted tasks, as the window's do; the
  // checker's squares wait (DrcPump's hold) until the job ended.
  pump.set_hold([&host] { return host.busy(); });
  host.set_pump([&] { (void)run_tasks(); }, 1ms);
  for (int round = 0; round < 4; ++round) {
    // New work for the checker over the whole hierarchy, then a write.
    s.ok("load top; box -10 -10 80 80; drc check");
    const std::string path = "/work/chip" + std::to_string(round) + ".gds";
    const cmd::CommandResult result =
        host.run_write("chip.gds", gds_phases(s.editor(), s.top(), s.gds_options(), path));
    INFO((result.diagnostics.empty() ? std::string{} : result.diagnostics.front().text));
    REQUIRE(result.ok());
    CHECK(s.file(path) == reference);
    pump.resume();
    (void)run_tasks();
  }
  const auto deadline = std::chrono::steady_clock::now() + 30s;
  while ((pump.busy() || !s.editor().drc.pending.empty()) && std::chrono::steady_clock::now() < deadline) {
    if (run_tasks() == 0) std::this_thread::sleep_for(1ms);
  }
  CHECK(s.editor().drc.pending.empty());
  CHECK(committed > 0);
  CHECK(committed_during_job == 0);
}

TEST_CASE("console lines typed while a mask job runs wait until it ends",
          "[cap:mask-hierarchical-generation][req:Progress reporting and interruptibility]") {
  JobSession s;
  script::Interpreter* interp = s.editor().script.interp.get();
  REQUIRE(interp != nullptr);
  TclConsoleSession session(s.editor(), s.core.dispatcher());
  REQUIRE(session.attach(*interp));
  std::vector<std::string> history;
  TclConsole console(session, [&history]() -> const std::vector<std::string>& { return history; });
  MaskJobHost host(s.editor().database_lock);
  session.set_external_busy([&host] { return host.busy(); });
  const std::string reference = s.reference_gds("/work/reference.gds");
  // A frame drawn during the write: the user types a paint.
  std::atomic<bool> typed{false};
  ConsoleReturn typed_return = ConsoleReturn::empty;
  host.set_pump(
      [&] {
        if (typed) return;
        console.set_input("box 100 100 110 110; paint m1");
        typed_return = console.press_return(false);
        typed = true;
      },
      1ms);
  const MaskWork wait_for_typing = [&](const mask::Progress*) {
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (!typed && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
    return cmd::CommandResult::success();
  };
  REQUIRE(host.run_write("chip.gds",
                         gds_phases(s.editor(), s.top(), s.gds_options(), "/work/chip.gds", wait_for_typing))
              .ok());
  CHECK(typed);
  CHECK(typed_return == ConsoleReturn::queued);
  CHECK(s.file("/work/chip.gds") == reference);
  CHECK(s.top().bbox().x1 < 110);  // not run yet
  CHECK(console.run_held() == 1);  // the window runs it once the job ended
  CHECK(s.top().bbox().x1 >= 110);
}
