// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// End-to-end wiring of the M3 workstreams (openspec/changes/m3-drc tasks.md
// 11.1): `tech load` compiles the drc section (RULES) that the engine
// (ENGINE) and the kernel (CHECK) run under `drc check; drc catchup`, and
// the commands (COMMANDS) report; the grid-rescale hooks (CmdScalegrid and
// the finer-grid `.mag` read call DRCTechScale); the dispatcher's
// TxTclDispatch handling of the background state; the `.mag` format-error
// mark (dbCellReadDef's badfile path); and the wire tool's default width
// from the current style.
//
// Owner: INTEGRATION (openspec/changes/m3-drc/design.md D2).

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/lock.hpp"
#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/drc/rules/details.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"

namespace cmd = hefesto::cmd;
namespace drc = hefesto::drc;

namespace {

std::string read_text(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

// tests/data/drc/rules/rules.tech with two styles: m1 width 3 and spacing 4
// in "first", m1 width 5 in "second".
std::string two_style_tech() {
  std::string text = read_text(std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/data/drc/rules/rules.tech");
  text += R"(
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
  return text;
}

// rules.tech whose drc style binds the cifoutput style "second", which is
// not the current one.
std::string cif_bound_tech() {
  std::string text = read_text(std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/data/drc/rules/rules.tech");
  text += R"(
drc
    cifstyle second
    width m1 3 "m1 width < 3"
end

extract
end
)";
  return text;
}

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  Session() {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    file("/t/two.tech", two_style_tech());
    REQUIRE(run("tech load /t/two.tech -noprompt").ok());
  }

  void file(const std::string& path, const std::string& text) {
    REQUIRE(services.view().files.write_atomic(path, text).ok());
  }

  /// Runs a `;`-separated line; returns the first failure, else the last result.
  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      for (const auto& d : r.diagnostics) {
        UNSCOPED_INFO(d.code << ": " << d.text);
      }
      if (!r.ok()) {
        return std::move(r);
      }
      last = std::move(r);
    }
    return last;
  }

  std::string value(std::string_view line) {
    const auto r = run(line);
    REQUIRE(r.ok());
    return r.value.to_string();
  }
};

}  // namespace

TEST_CASE("tech load, drc check and drc catchup find width and spacing errors end to end",
          "[cap:drc-commands][req:drc catchup and drc check]") {
  Session s;
  CHECK(s.editor.drc.style == "first");
  REQUIRE(s.run("load top").ok());
  hefesto::db::CellDef* top = s.editor.cells.find("top");
  REQUIRE(top != nullptr);
  REQUIRE(s.editor.windows.open_frame(top).ok());  // `drc check` acts on the window's root
  REQUIRE(s.run("box 0 0 2 10; paint m1; box 5 0 10 10; paint m1").ok());
  REQUIRE(s.run("drc off; box -20 -20 30 30; drc check; drc catchup").ok());
  // The 2-wide bar breaks the width rule; the 3-unit gap breaks the spacing rule.
  // In DRCWhyAll's hash table order (Q-SCRIPT-8): bucket 0, then bucket 5.
  CHECK(s.value("drc listall why") == "{m1 spacing < 4} {{5 0 6 10}} {m1 width < 3} {{2 0 3 10}}");
  CHECK(s.value("drc list count total") == "2");
  CHECK(s.value("drc listall count") == "{top 2}");
  CHECK(s.editor.drc.pending.empty());
}

TEST_CASE("scalegrid rescales the rules, the halo and the *halo and *stepsize overrides",
          "[cap:drc-rule-language][req:Halo and step size]") {
  // Regression (golden CELL-WR-02): `scalegrid 1 2` left the DRC halo at the
  // old grid, so the check area an edit marks was half Magic's.
  Session s;
  const std::int32_t halo = drc::current_halo(s.editor);
  REQUIRE(halo > 0);
  REQUIRE(s.run("drc *halo 2; drc *stepsize 100").ok());
  REQUIRE(s.run("scalegrid 1 2").ok());
  CHECK(s.editor.drc.rules->halo().value == 2 * halo);
  CHECK(s.editor.drc.halo_override == 4);
  CHECK(s.editor.drc.step_size == 200);
}

TEST_CASE("a coarser grid never leaves a step size of 0", "[cap:drc-rule-language][req:Halo and step size]") {
  // Q-DRC-25 (fix, both modes): DRCTechScale floors the style step and the
  // *stepsize override; a grid 4 times coarser leaves 0 in Magic and its
  // checker divides by zero. Hefesto keeps 1 and the check completes.
  const bool compat = GENERATE(true, false);
  INFO("magic-compat " << compat);
  Session s;
  s.project.compat.magic_compat = compat;
  s.file("/t/step.tech", read_text(std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/data/drc/rules/rules.tech") +
                             "\ndrc\n    stepsize 2\n    width m1 12 \"m1 width < 12\"\nend\n\nextract\nend\n");
  REQUIRE(s.run("tech load /t/step.tech -noprompt").ok());
  REQUIRE(s.editor.drc.rules->step_size() == 2);
  REQUIRE(s.run("drc *stepsize 3").ok());
  REQUIRE(s.run("scalegrid 4 1").ok());
  CHECK(s.editor.drc.rules->step_size() == 1);
  CHECK(s.editor.drc.step_size == 1);
  CHECK(drc::current_step_size(s.editor) == 1);
  REQUIRE(s.run("load top").ok());
  hefesto::db::CellDef* top = s.editor.cells.find("top");
  REQUIRE(top != nullptr);
  REQUIRE(s.editor.windows.open_frame(top).ok());
  REQUIRE(s.run("snap internal; box 0 0 2 10; paint m1; drc off; box -20 -20 30 30; drc check; drc catchup").ok());
  // The 2-wide bar breaks the width rule (12 / 4 = 3); checked with the step of 1, one error
  // area per unit square.
  CHECK(s.value("drc listall why") ==
        "{m1 width < 12} {{2 0 3 1} {2 1 3 2} {2 2 3 3} {2 3 3 4} {2 4 3 5} {2 5 3 6} {2 6 3 7} {2 7 3 8} "
        "{2 8 3 9} {2 9 3 10}}");
  CHECK(s.editor.drc.pending.empty());
}

TEST_CASE("tech load drops the *halo and *stepsize overrides", "[cap:drc-engine][req:Halo override]") {
  Session s;
  REQUIRE(s.run("drc *halo 1; drc *stepsize 50").ok());
  REQUIRE(s.editor.drc.halo_override.has_value());
  REQUIRE(s.run("tech load /t/two.tech -noprompt").ok());
  CHECK_FALSE(s.editor.drc.halo_override.has_value());
  CHECK_FALSE(s.editor.drc.step_size.has_value());
}

TEST_CASE("background checking is not set while a command runs and restored after it",
          "[cap:drc-engine][req:Background checker host mechanism]") {
  Session s;
  std::vector<drc::BackgroundMode> seen;
  REQUIRE_FALSE(s.registry
                    .add(cmd::CommandSpec{"probe", "probe", "records the DRC background state",
                                          [&seen](cmd::CommandContext& ctx, const cmd::Invocation&) {
                                            seen.push_back(ctx.editor()->drc.background);
                                            return cmd::CommandResult::success();
                                          }})
                    .has_value());
  REQUIRE(s.run("probe").ok());
  CHECK(s.editor.drc.background == drc::BackgroundMode::on);
  REQUIRE(s.run("drc off; probe").ok());
  CHECK(s.editor.drc.background == drc::BackgroundMode::off);
  REQUIRE(s.run("drc on").ok());
  CHECK(s.editor.drc.background == drc::BackgroundMode::on);
  REQUIRE(seen.size() == 2);
  CHECK(seen[0] == drc::BackgroundMode::not_set);  // TxTclDispatch: DRC_NOT_SET inside a command
  CHECK(seen[1] == drc::BackgroundMode::off);      // off stays off
}

TEST_CASE("a .mag file with a format error queues its cell for checking",
          "[cap:drc-engine][req:DRC check and error planes]") {
  Session s;
  s.file("bad.mag", "magic\ntech rulestest\ntimestamp 0\n<< metal1 >>\nrect 0 0 x\n<< end >>\n");
  (void)s.run("load bad");
  const hefesto::db::CellDef* bad = s.editor.cells.find("bad");
  REQUIRE(bad != nullptr);
  CHECK(s.editor.drc.pending.contains(*bad));
}

TEST_CASE("the wire default width follows the current drc style", "[cap:wiring-tool][req:Wire type selection]") {
  Session s;
  REQUIRE(s.run("load top; box 0 0 1 1").ok());
  REQUIRE(s.run("wire type m1").ok());
  CHECK(s.editor.wire.width == 3);
  REQUIRE(s.run("drc style second; box 0 0 1 1; wire type m1").ok());
  CHECK(s.editor.wire.width == 5);
}

TEST_CASE("two readers search one plane at the same time", "[cap:drc-engine][req:Background checker host mechanism]") {
  // Regression: a plane search moves the plane's search hint, so the DRC
  // worker and scene building, both holding the database lock shared,
  // wrote it concurrently (a data race under ThreadSanitizer).
  Session s;
  REQUIRE(s.run("load top").ok());
  hefesto::db::CellDef* top = s.editor.cells.find("top");
  REQUIRE(top != nullptr);
  REQUIRE(s.editor.windows.open_frame(top).ok());
  for (int i = 0; i < 20; ++i) {
    const std::string x = std::to_string(i * 10);
    REQUIRE(s.run("box " + x + " 0 " + std::to_string(i * 10 + 5) + " 5; paint m1").ok());
  }
  const auto plane = *s.editor.technology->home_plane(*s.editor.technology->find_type("m1").id);
  const auto count_tiles = [&] {
    const hefesto::db::DatabaseLock::Shared reader(s.editor.database_lock);
    std::size_t n = 0;
    for (int round = 0; round < 200; ++round) {
      top->plane(plane).enumerate(hefesto::db::Rect{round % 200, 0, 200, 5}, hefesto::tech::TypeMask::all(),
                                  [&n](const hefesto::db::TileView&) {
                                    ++n;
                                    return false;
                                  });
    }
    return n;
  };
  std::size_t other = 0;
  std::thread worker([&] { other = count_tiles(); });
  const std::size_t mine = count_tiles();
  worker.join();
  CHECK(mine == other);
}

TEST_CASE("drc statistics totals belong to the session", "[cap:drc-commands][req:drc rulestats and statistics]") {
  // Regression: the totals were process-wide, so a second session (another
  // window or document) printed the first one's counts.
  const auto statistics = [](Session& s) {
    std::string text;
    for (const auto& d : s.run("drc statistics").diagnostics) {
      text += d.text;
    }
    return text;
  };
  Session first;
  REQUIRE(first.run("load top").ok());
  REQUIRE(first.editor.windows.open_frame(first.editor.cells.find("top")).ok());
  REQUIRE(first.run("box 0 0 2 10; paint m1; box -20 -20 30 30; drc check; drc catchup").ok());
  CHECK(statistics(first).find("Squares processed: 0/0") == std::string::npos);
  const std::string again = statistics(first);  // the recent counters restart, the totals stay
  CHECK(again.find("Squares processed: 0/") != std::string::npos);
  CHECK(again.find("Squares processed: 0/0") == std::string::npos);
  Session second;
  CHECK(statistics(second).find("Squares processed: 0/0") != std::string::npos);
}

TEST_CASE("drc catchup returns Magic's after id when it had work", "[cap:drc-commands][req:drc catchup and drc check]") {
  // Golden captures record `after#N`: DRCContinuous schedules
  // `magic::drcstate busy` and `idle` with `after idle`.
  Session s;
  REQUIRE(s.run("load top").ok());
  REQUIRE(s.editor.windows.open_frame(s.editor.cells.find("top")).ok());
  REQUIRE(s.run("box 0 0 2 10; paint m1").ok());  // CmdPaint marks unless DRC is off
  REQUIRE_FALSE(s.editor.drc.pending.empty());
  CHECK(s.value("drc catchup") == "after#2");
  CHECK(s.value("drc catchup").empty());  // nothing pending
  REQUIRE(s.run("box 20 0 30 2; paint m1").ok());
  CHECK(s.value("drc catchup") == "after#4");
}

TEST_CASE("the first check under a cifstyle that is not current loads that output style",
          "[cap:drc-rule-language][req:Style selection at run time]") {
  // Regression (golden DRC-PDK-1..3, DRC-STAT-1, DRC-STD-*): drcCifCheck
  // prints "Loading DRC CIF style." once, at the end of the first basic check.
  Session s;
  s.file("/t/cif.tech", cif_bound_tech());
  REQUIRE(s.run("tech load /t/cif.tech -noprompt").ok());
  REQUIRE(s.editor.drc.rules->details() != nullptr);
  REQUIRE_FALSE(s.editor.drc.rules->details()->cif_bound);
  REQUIRE(s.run("load top").ok());
  REQUIRE(s.editor.windows.open_frame(s.editor.cells.find("top")).ok());
  REQUIRE(s.run("box 0 0 2 10; paint m1").ok());
  const auto first = s.run("drc catchup");
  std::size_t loading = 0;
  for (const auto& d : first.diagnostics) {
    loading += d.text == "Loading DRC CIF style." ? 1U : 0U;
  }
  CHECK(loading == 1);
  CHECK(s.editor.drc.rules->details()->cif_bound);  // reloaded under the bound style
  CHECK(s.editor.drc.cif.current()->name == "second");  // no mask-layer rule: drcCifValid stays false
  REQUIRE(s.run("box 20 0 22 10; paint m1").ok());
  for (const auto& d : s.run("drc catchup").diagnostics) {
    CHECK(d.text != "Loading DRC CIF style.");
  }
}

namespace {

// The layout of the `drc listall why` regressions (Desktop Alpha, sky130
// met1.6 and sg13g2 M2.d): one step square holds an interaction near the
// origin (use `a` abutting top paint) and the uses `c` above it and, with
// `below`, `d` below it, each holding a 2-wide m1 bar at its own origin.
// Magic 8.3.694 run on the same tech and commands gives the expected values.
void build_resolution_layout(Session& s, bool below) {
  REQUIRE(s.run("drc off; load c; box 0 0 2 10; paint m1").ok());
  REQUIRE(s.run("load d; box 0 0 2 10; paint m1").ok());
  REQUIRE(s.run("load a; box 0 0 10 10; paint m1").ok());
  REQUIRE(s.run("load top").ok());
  REQUIRE(s.editor.windows.open_frame(s.editor.cells.find("top")).ok());
  REQUIRE(s.run("box 0 0 0 0; getcell a; box 10 0 20 10; paint m1; box 0 100 0 100; getcell c").ok());
  if (below) {
    REQUIRE(s.run("box 0 -100 0 -100; getcell d").ok());
  }
  REQUIRE(s.run("drc *stepsize 1000; select top cell; drc check; drc catchup").ok());
}

}  // namespace

TEST_CASE("drc listall why reports a subcell's flattened errors at its own coordinates as Magic does",
          "[cap:drc-commands][req:drc why]") {
  // Regression (Desktop Alpha sky130, met1.6): resolving c's copied error
  // re-enters the check and leaves DRCDummyUse on c, so the flattened check
  // of the interaction near the origin yanks c's paint unmoved.
  Session s;
  build_resolution_layout(s, false);
  CHECK(s.value("drc listall why") == "{m1 width < 3} {{2 100 3 110} {2 0 3 10}}");
  CHECK(s.value("drc list why") == "{m1 width < 3}");
}

TEST_CASE("drc listall why misses the errors of a later strip's subcells as Magic does",
          "[cap:drc-commands][req:drc why]") {
  // Regression (Desktop Alpha sg13g2, M2.d): after the strip above resolves
  // c's error, the strip below copies the errors of c's children, not top's,
  // so d's error is never reported.
  Session s;
  build_resolution_layout(s, true);
  CHECK(s.value("drc listall why") == "{m1 width < 3} {{2 100 3 110} {2 0 3 10}}");
}

TEST_CASE("with magic-compat OFF drc listall why reports every subcell error once at root coordinates",
          "[cap:drc-engine][req:Interaction check over an area]") {
  Session s;
  const hefesto::compat::CompatSettings off{false, true};
  s.editor.drc.compat = &off;
  build_resolution_layout(s, true);
  // Both copied errors, at root coordinates, and nothing of c's paint near
  // the origin. (OFF, the width error rechecked in the copied rectangle only
  // touches it, so the copied text stands.)
  CHECK(s.value("drc listall why") == "{See error definition in the subcell} {{2 100 3 110} {2 -100 3 -90}}");
  s.editor.drc.compat = nullptr;
}
