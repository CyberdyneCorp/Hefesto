// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M3 scaffold (openspec/changes/m3-drc tasks.md 1.4, 11.4): the scaffold
// stub code CORE-NOTIMPL-003 is retired (the `drc` options are tested in
// tests/unit/drc/commands/), the database lock admits
// concurrent readers and excludes them while a command runs, a native-only
// diagnostic is printed in `text` and dropped in `magic` format, and the
// shared interfaces of design D4 exist with their defaults. Owners replace
// these [m3-scaffold] tests with [cap:][req:] ones as the stubs go away.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/db/lock.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/drc/check/basic_check.hpp"
#include "hefesto/drc/commands/command.hpp"
#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/drc/engine/marking.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/hints/hints.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/ui/bindings.hpp"
#include "hefesto/ui/input.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/ui/macros.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace diag = hefesto::diag;
namespace drc = hefesto::drc;
namespace editor = hefesto::editor;
namespace ui = hefesto::ui;

namespace {

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState state;
  cmd::Dispatcher dispatcher{registry, services.view(), project, script};

  explicit Session(bool all = true) {
    if (all) {
      REQUIRE(cmd::register_all_commands(registry));
    }
    dispatcher.set_editor(&state);
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : ui::run_command_line(dispatcher, line, cmd::Origin::key)) {
      last = std::move(r);
    }
    return last;
  }
};

}  // namespace

// --- tasks.md 11.4: the drc command stubs are gone --------------------------

TEST_CASE("CORE-NOTIMPL-003 is retired and the drc areas are declared", "[m3-scaffold]") {
  // Every drc option has its real handler (tests/unit/drc/commands/); the
  // scaffold code keeps its row and is never reused.
  const auto code = diag::find_code("CORE-NOTIMPL-003");
  REQUIRE(code.has_value());
  CHECK(code->summary.starts_with("Retired in M3"));
  REQUIRE(diag::find_code_area("drc").has_value());
  REQUIRE(diag::find_code_area("hints").has_value());
}

// --- tasks.md 1.4: the database lock -------------------------------------------

TEST_CASE("the database lock admits concurrent readers", "[m3-scaffold]") {
  db::DatabaseLock lock;
  std::atomic<int> inside{0};
  std::atomic<int> peak{0};
  const auto reader = [&] {
    const db::DatabaseLock::Shared guard{lock};
    const int now = ++inside;
    int seen = peak.load();
    while (seen < now && !peak.compare_exchange_weak(seen, now)) {
    }
    // Stay until the other reader is inside too (bounded wait).
    for (int i = 0; i < 2000 && inside.load() < 2; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    --inside;
  };
  std::thread a{reader};
  std::thread b{reader};
  a.join();
  b.join();
  CHECK(peak.load() == 2);
}

TEST_CASE("the dispatcher holds the database lock exclusively for every command", "[m3-scaffold]") {
  Session s{false};
  std::atomic<bool> reader_in{false};
  std::atomic<bool> held_during_command{false};
  std::thread reader;
  REQUIRE_FALSE(s.registry
                    .add(cmd::CommandSpec{"m3probe", "m3probe", "test command",
                                          [&](cmd::CommandContext& ctx, const cmd::Invocation&) {
                                            auto* ed = ctx.editor();
                                            held_during_command = ed->database_lock.held_exclusively_by_this_thread();
                                            // A shared request on the command thread is admitted at once.
                                            { const db::DatabaseLock::Shared nested{ed->database_lock}; }
                                            reader = std::thread{[ed, &reader_in] {
                                              const db::DatabaseLock::Shared guard{ed->database_lock};
                                              reader_in = true;
                                            }};
                                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                                            const bool excluded = !reader_in.load();
                                            return excluded ? cmd::CommandResult::success()
                                                            : cmd::CommandResult::failure(
                                                                  diag::make("CMD-INTERNAL-001", "reader got in"));
                                          }})
                    .has_value());
  const auto result = s.dispatcher.execute(cmd::Invocation{"m3probe", {}, cmd::Origin::script, std::nullopt});
  reader.join();
  CHECK(result.ok());
  CHECK(held_during_command.load());
  CHECK(reader_in.load());
  CHECK_FALSE(s.state.database_lock.held_exclusively_by_this_thread());
}

TEST_CASE("a waiting writer is visible to readers", "[m3-scaffold]") {
  db::DatabaseLock lock;
  std::atomic<bool> written{false};
  std::thread writer;
  {
    const db::DatabaseLock::Shared reader{lock};
    CHECK_FALSE(lock.writer_waiting());
    writer = std::thread{[&] {
      const db::DatabaseLock::Exclusive guard{lock};
      written = true;
    }};
    for (int i = 0; i < 2000 && !lock.writer_waiting(); ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(lock.writer_waiting());
    CHECK_FALSE(written.load());
    const drc::CancelToken token{nullptr, &lock};
    CHECK(token.requested());
  }
  writer.join();
  CHECK(written.load());
  CHECK_FALSE(lock.writer_waiting());
}

// --- tasks.md 1.4: native-only diagnostics ----------------------------------------

TEST_CASE("a native_only warning is printed in text and absent in magic format", "[m3-scaffold]") {
  const diag::Diagnostic lint = diag::make_native("CMD-USAGE-001", "lint: plane pair dropped");
  CHECK(lint.native_only);
  CHECK(diag::shown_in(lint, diag::Format::text));
  CHECK(diag::shown_in(lint, diag::Format::json));
  CHECK_FALSE(diag::shown_in(lint, diag::Format::magic));
  CHECK(diag::render(lint, diag::Format::text) == "error: CMD-USAGE-001: lint: plane pair dropped");
  CHECK(diag::render(lint, diag::Format::json).has_value());
  CHECK_FALSE(diag::render(lint, diag::Format::magic).has_value());
  const diag::Diagnostic plain = diag::make("CMD-USAGE-001", "usage");
  CHECK_FALSE(plain.native_only);
  CHECK(diag::render(plain, diag::Format::magic) == "usage");
}

// --- tasks.md 1.4: shared interfaces ---------------------------------------------

TEST_CASE("editor state carries the M3 members with their defaults", "[m3-scaffold]") {
  Session s{false};
  const drc::DrcSession& d = s.state.drc;
  REQUIRE(d.rules != nullptr);
  CHECK(d.rules->all_rules().empty());
  CHECK(d.styles.names == std::vector<std::string>{"default"});
  CHECK(d.background == drc::BackgroundMode::on);
  CHECK(d.pending.empty());
  REQUIRE(d.worker != nullptr);
  CHECK_FALSE(d.worker->running());
  CHECK_FALSE(d.halo_override.has_value());
  CHECK_FALSE(d.euclidean);
  CHECK(d.ignore.empty());
  CHECK(d.compat == &s.project.compat);  // set by Dispatcher::set_editor
  CHECK(d.compat_settings().magic_compat);
  CHECK(drc::current_halo(s.state) == 0);

  // check_this is the marking entry (a no-op until ENGINE lands it).
  auto cell = s.state.cells.create("m3cell", s.state.technology->plane_count());
  REQUIRE(cell.ok());
  drc::check_this(s.state, *cell.value(), db::Rect{0, 0, 10, 10});
  drc::check_this(s.state, *cell.value(), std::nullopt);
}

TEST_CASE("mask hints are hint regions behind the property API", "[m3-scaffold]") {
  // db::PlaneValue is hints::HintRegion: values are canonical strips.
  const db::PropertyValue value = db::coerce_reserved_property("MASKHINTS_SRAM", std::string{"0 0 10 10 5 5 15 15"});
  const auto* region = std::get_if<hefesto::hints::HintRegion>(&value);
  REQUIRE(region != nullptr);
  CHECK(region->strips() ==
        std::vector<db::Rect>{db::Rect{5, 10, 15, 15}, db::Rect{0, 5, 15, 10}, db::Rect{0, 0, 10, 5}});
  CHECK(db::property_text(value) == "5 10 15 15 0 5 15 10 0 0 10 5");
  Session s{false};
  auto cell = s.state.cells.create("hinted", s.state.technology->plane_count());
  REQUIRE(cell.ok());
  db::set_property(*cell.value(), "MASKHINTS_SRAM", value);
  const auto* found = hefesto::hints::find_hint(*cell.value(), "SRAM");
  REQUIRE(found != nullptr);
  CHECK(*found == *region);
}
