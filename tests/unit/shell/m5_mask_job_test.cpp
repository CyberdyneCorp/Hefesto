// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop host of long GDS and CIF reads and writes (shell::MaskJobHost;
// openspec/changes/m5-mask-gds-cif tasks.md 12.2, design D16): progress and
// the Interrupt card, and the `gds write` / `cif write` commands run through
// the session's hosts exactly as the main window wires them (write_host:
// the whole write on the command thread; emit_host: the emit on the
// deep-stack worker covered by the command's exclusive hold): a cancelled
// write leaves no file, a console line typed during a write waits, and
// (under TSan in CI) the covered emit runs while the calling thread draws
// frames over the same layout, use index and vendor cells with the Tcl
// interpreter attached, and while the background DRC has work.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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

#include "hefesto/db/lock.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/display/scene.hpp"
#include "hefesto/mask/session.hpp"
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

  std::vector<cmd::CommandResult> run(std::string_view line) {
    return hefesto::ui::run_command_line(core.dispatcher(), line, cmd::Origin::gui);
  }

  std::string file(const std::string& path) {
    auto read = core.files().read(path);
    REQUIRE(read.ok());
    return read.value();
  }

  /// The hosts the main window sets (MainWindow::build_m4b_console).
  void wire(MaskJobHost& host) {
    mask::MaskSession& m = core.editor().mask;
    m.read_host = [&host](std::string_view label, const mask::ProgressWork& work) {
      return host.run_read(label, work);
    };
    m.write_host = [&host](std::string_view label, const mask::ProgressWork& work) {
      return host.run_job(MaskJobKind::write, label, work);
    };
    m.emit_host = [&host](std::string_view label, const mask::ProgressWork& work) {
      return host.run_emit(label, work);
    };
  }

  void unwire() {
    mask::MaskSession& m = core.editor().mask;
    m.read_host = {};
    m.write_host = {};
    m.emit_host = {};
  }

  /// `gds write /work/<name>` with the random() state reset (the full
  /// dump's prefixes, Q-MASK-211), returning the bytes with the dates
  /// zeroed.
  std::string gds_write(const std::string& name) {
    ok("random seed 1");
    ok("gds write /work/" + name);
    return without_dates(file("/work/" + name + ".gds"));
  }

  /// Two abstract views of one vendor library, `pad` and `via_x`
  /// (LEFview, GDS_FILE "$LIBDIR/lib.gds" with ::LIBDIR set in Tcl),
  /// placed in `top`: a write fully dumps the library and compares
  /// GDS_FILE values (Q-MASK-128), which resolves the path variables.
  void add_vendor_views() {
    ok("load via_x");
    ok("box 0 0 4 4; paint m1");
    ok("load pad");
    ok("box 0 0 0 0; getcell via_x");
    ok("gds write /work/lib");
    script::Interpreter* interp = core.editor().script.interp.get();
    REQUIRE(interp != nullptr);
    REQUIRE(interp->set_var("::LIBDIR", cmd::Value{"/work"}));
    for (const char* name : {"pad", "via_x"}) {
      db::CellDef* def = core.editor().cells.find(name);
      REQUIRE(def != nullptr);
      def->properties().put("LEFview", std::string{"TRUE"});
      def->properties().put("GDS_FILE", std::string{"$LIBDIR/lib.gds"});
      def->properties().put("GDS_START", std::string{"0"});
    }
    ok("load top");
    ok("box 100 0 100 0; getcell pad; box 120 0 120 0; getcell via_x");
  }

  /// What a frame draw reads (LayoutCanvas::rebuild_scene): the scene of
  /// `top` with every instance expanded, built as a database reader, and
  /// each cell's uses in search order (the use index).
  void draw_frame(const display::StyleSet& styles) {
    editor::EditorState& ed = core.editor();
    const db::DatabaseLock::Shared reading(ed.database_lock);
    display::SceneOptions options;
    options.expanded = [](const db::CellUse&) { return true; };
    const auto scene = display::build_scene(top(), *ed.technology, styles, ed.fonts, options);
    frames_drawn += scene.ok() && !scene.value().layers.empty() ? 1U : 0U;
    ed.cells.for_each([&](db::CellDef& def) { uses_seen += db::uses_in_search_order(def, ed.drc.compat_settings()).size(); });
  }

  std::size_t frames_drawn = 0;
  std::size_t uses_seen = 0;

  static std::string without_dates(std::string bytes) {
    std::size_t at = 0;
    while (at + 4 <= bytes.size()) {
      const std::size_t length = (static_cast<std::size_t>(static_cast<unsigned char>(bytes[at])) << 8U) |
                                 static_cast<unsigned char>(bytes[at + 1]);
      if (length < 4 || at + length > bytes.size()) break;
      const auto type = static_cast<unsigned char>(bytes[at + 2]);
      if (type == 0x01 || type == 0x05) {  // BGNLIB, BGNSTR
        std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(at + 4),
                  bytes.begin() + static_cast<std::ptrdiff_t>(at + length), '\0');
      }
      at += length;
    }
    return bytes;
  }
};

/// One layout style drawing metal1 (the frame draws of the tests).
display::StyleSet metal1_styles(const tech::Technology& technology) {
  display::StyleSet set;
  set.colors.entries.resize(8);
  set.table.layout_count = 1;
  set.table.styles.resize(display::kFixedStyleCount + 2);
  set.table.styles[display::kFixedStyleCount].color = 1;
  set.table.styles[display::kFixedStyleCount + 1].color = 1;
  set.types.styles_of_type.resize(technology.type_count());
  const auto metal1 = technology.find_type("metal1");
  REQUIRE(metal1.ok());
  set.types.styles_of_type[*metal1.id].push_back(display::kFixedStyleCount);
  return set;
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
  s.wire(host);
  // The Interrupt arrives while the subtree loads: the first frame the
  // walk's tick draws presses it.
  host.set_pump([&host] { (void)host.request_cancel(); }, 0ms);
  std::vector<cmd::CommandResult> stopped = s.run("gds write /work/early");
  REQUIRE(stopped.size() == 1);
  CHECK_FALSE(stopped.front().ok());
  CHECK(stopped.front().diagnostics.back().text == "GDS write cancelled.");
  CHECK_FALSE(s.core.files().exists("/work/early.gds"));
  CHECK_FALSE(s.run("cif write /work/early").front().ok());
  CHECK_FALSE(s.core.files().exists("/work/early.cif"));

  // The Interrupt arrives during the emit phase (the card sets an atomic
  // flag, which the worker polls).
  host.set_pump({});
  std::atomic<bool> cancelled{false};
  std::atomic<bool> shown{false};
  s.editor().mask.emit_host = [&](std::string_view label, const mask::ProgressWork& work) {
    return host.run_emit(label, [&](const mask::Progress* progress) {
      cancelled = host.request_cancel();
      const std::optional<MaskJobState> state = host.state();
      shown = state && state->cancel_requested && state->kind == MaskJobKind::write;
      return work(progress);
    });
  };
  CHECK_FALSE(s.run("gds write /work/chip").front().ok());
  CHECK(cancelled);
  CHECK(shown);
  CHECK_FALSE(s.core.files().exists("/work/chip.gds"));
  CHECK_FALSE(s.run("cif write /work/chip").front().ok());
  CHECK_FALSE(s.core.files().exists("/work/chip.cif"));
  const auto listed = s.core.files().list("/work");
  REQUIRE(listed.ok());
  CHECK(listed.value().size() == 1);  // maskjob.tech only: no temporary file is left

  // The next write starts with the flag cleared.
  s.wire(host);
  s.ok("gds write /work/chip");
  CHECK(s.core.files().exists("/work/chip.gds"));
  s.unwire();
}

TEST_CASE("a covered GDS write runs while frames draw the layout, its use index and vendor views",
          "[cap:hefesto-architecture][req:Concurrency model]") {
  // The desktop's path (design "Round 5 contract updates"): the command
  // holds the lock exclusively, the emit runs on the worker under that
  // hold, and the command thread keeps drawing frames, which read the
  // layout and the use index at the same time. With the Tcl interpreter
  // attached, the worker must not resolve path variables through it (the
  // vendor views' full dump compares GDS_FILE values): TSan checks both.
  JobSession s;
  s.add_vendor_views();
  const std::string reference = s.gds_write("reference");
  const display::StyleSet styles = metal1_styles(*s.editor().technology);
  MaskJobHost host(s.editor().database_lock);
  s.wire(host);
  host.set_pump([&] { s.draw_frame(styles); }, 0ms);  // back to back while the worker emits
  for (int round = 0; round < 3; ++round) {
    const std::string name = "chip" + std::to_string(round);
    CHECK(s.gds_write(name) == reference);
  }
  s.unwire();
  CHECK(s.frames_drawn > 0);
  CHECK(s.uses_seen > 0);
  CHECK_FALSE(host.busy());
}

TEST_CASE("a GDS write runs while the background DRC has work on the same hierarchy",
          "[cap:hefesto-architecture][req:Concurrency model]") {
  JobSession s;
  const std::string reference = s.gds_write("reference");
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
  s.wire(host);
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
    CHECK(s.gds_write("chip" + std::to_string(round)) == reference);
    pump.resume();
    (void)run_tasks();
  }
  const auto deadline = std::chrono::steady_clock::now() + 30s;
  while ((pump.busy() || !s.editor().drc.pending.empty()) && std::chrono::steady_clock::now() < deadline) {
    if (run_tasks() == 0) std::this_thread::sleep_for(1ms);
  }
  s.unwire();
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
  const std::string reference = s.gds_write("reference");
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
  s.wire(host);
  // The emit waits on the worker until the line was typed.
  s.editor().mask.emit_host = [&](std::string_view label, const mask::ProgressWork& work) {
    return host.run_emit(label, [&](const mask::Progress* progress) {
      const auto deadline = std::chrono::steady_clock::now() + 10s;
      while (!typed && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
      return work(progress);
    });
  };
  CHECK(s.gds_write("chip") == reference);
  s.unwire();
  CHECK(typed);
  CHECK(typed_return == ConsoleReturn::queued);
  CHECK(s.top().bbox().x1 < 110);  // not run yet
  CHECK(console.run_held() == 1);  // the window runs it once the job ended
  CHECK(s.top().bbox().x1 >= 110);
}
