// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs-setup-file: the language and runtime, evaluation (line accumulation,
// errors, the setup-file name forms) and the step budget and cancellation.
// Setup texts are Hefesto-authored.
//
// Owner: LVSSETUP.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>

#include "hefesto/lvs/setup/codes.hpp"
#include "hefesto/platform/file_system.hpp"
#include "setup_fixture.hpp"

namespace {

using setup_test::logged;
using setup_test::logged_error;
using setup_test::Pair;
namespace lvs = hefesto::lvs;
namespace codes = hefesto::lvs::setup_codes;

}  // namespace

TEST_CASE("Setup commands exist unqualified and in the netgen namespace",
          "[cap:lvs-setup-file][req:Setup file language and runtime]") {
  Pair pair;
  const auto state = pair.ok(
      "puts [llength [info commands ::netgen::permute]]\n"
      "puts [llength [info commands ::permute]]\n"
      "netgen::permute transistors\n"
      "puts [string length [::format %s abc]]\n");
  CHECK(logged(state, "1"));
  CHECK(state.errors == 0);
  CHECK(logged(state, "3"));  // Tcl's own `format`, named from the netgen namespace
  CHECK(pair.cell(1, "nfet").permutes.size() == 1);
}

TEST_CASE("Tcl control flow, lists and strings are available", "[cap:lvs-setup-file][req:Setup file language and runtime]") {
  Pair pair;
  const auto state = pair.ok(
      "set devs {nfet pfet}\n"
      "foreach dev $devs {\n"
      "  if {[lsearch $devs $dev] >= 0} {\n"
      "    property \"-circuit1 $dev\" delete L\n"
      "  }\n"
      "}\n"
      "if {[regexp {^n} nfet]} { puts matched }\n"
      "catch {nosuchcommand}\n"
      "lappend devs x\n"
      "puts [string toupper [lindex $devs end]]\n"
      "puts [info exists devs]\n");
  CHECK(state.errors == 0);
  CHECK(logged(state, "matched"));
  CHECK(logged(state, "X"));
  CHECK(setup_test::property(pair.cell(1, "nfet"), "L") == nullptr);
  CHECK(setup_test::property(pair.cell(2, "nfet"), "L") != nullptr);
}

TEST_CASE("env reads the sandbox's environment, not the process", "[cap:lvs-setup-file][req:Setup file language and runtime]") {
  Pair pair;
  auto ctx = pair.context();
  ctx.sandbox.environment["NETGEN_COLUMNS"] = "60";
  const auto state = lvs::evaluate_setup(lvs::Source{"s.tcl", std::string{"catch {format $env(NETGEN_COLUMNS)}\n"
                                                                            "puts [info exists env(HOME)]\n"}},
                                         ctx);
  REQUIRE(state.ok());
  CHECK(state.value().column_left == 60);
  CHECK(state.value().column_right == 60);
  CHECK(logged(state.value(), "0"));
}

TEST_CASE("The sandbox refuses exec and paths outside its roots",
          "[cap:lvs-setup-file][req:Setup file language and runtime]") {
  Pair pair;
  const auto state = pair.ok("exec ls\nopen /etc/passwd\n");
  CHECK(state.errors == 2);
}

TEST_CASE("An error is reported with its line and skipped", "[cap:lvs-setup-file][req:Setup file evaluation]") {
  Pair pair;
  std::string text;
  for (int i = 1; i < 12; ++i) {
    text += "# line " + std::to_string(i) + "\n";
  }
  text += "property nosuchcell tolerance {w 0.01}\n";
  text += "property \"-circuit1 nfet\" delete L\n";
  const auto state = pair.ok(text);
  CHECK(logged_error(state, "Error setup.tcl:12 (ignoring), No such cell!"));
  CHECK(logged(state, "Warning:  There were errors reading the setup file"));
  CHECK(state.errors == 1);
  CHECK(setup_test::property(pair.cell(1, "nfet"), "L") == nullptr);  // line 13 still ran
  REQUIRE(state.diagnostics.size() == 1);
  CHECK(state.diagnostics.front().code == codes::kLineIgnored);
}

TEST_CASE("Multi-line commands accumulate; the error names the last line read",
          "[cap:lvs-setup-file][req:Setup file evaluation]") {
  Pair pair;
  const auto state = pair.ok(
      "foreach x {1 2} {\n"
      "  puts $x\n"
      "}\n"
      "set y [\n"
      "  nosuch\n"
      "]\n");
  CHECK(logged(state, "1"));
  CHECK(logged(state, "2"));
  CHECK(logged_error(state, "Error setup.tcl:6 (ignoring), invalid command name \"nosuch\""));
}

TEST_CASE("Text still pending at end of file is evaluated for its error", "[cap:lvs-setup-file][req:Setup file evaluation]") {
  Pair pair;
  const auto state = pair.ok("puts ok\nforeach x {1 2} {\n  puts $x\n");
  CHECK(logged(state, "ok"));
  CHECK(state.errors == 1);
  CHECK(state.log.back().text == "Warning:  There were errors reading the setup file");
}

TEST_CASE("An empty setup name applies the trivial default setup", "[cap:lvs-setup-file][req:Setup file evaluation]") {
  Pair pair;
  auto ctx = pair.context();
  const auto state = lvs::evaluate_setup(lvs::Source{"", std::nullopt}, ctx);
  REQUIRE(state.ok());
  CHECK(logged(state.value(), "No setup file specified.  Using trivial default setup."));
  // property default deletes as/ad/ps/pd and reports the missing ones (capture of NG-SET-02).
  CHECK(logged(state.value(), "No property as found for device nfet"));
  CHECK(logged(state.value(), "No property pd found for device pfet"));
  CHECK(state.value().log.front().text.empty());
  CHECK_FALSE(state.value().evaluated);
  CHECK(pair.cell(1, "nfet").permutes.size() == 1);
  CHECK(setup_test::property(pair.cell(2, "nfet"), "W")->merge != 0);
}

TEST_CASE("Missing setup files: nosetup continues, others end the command", "[cap:lvs-setup-file][req:Setup file evaluation]") {
  Pair pair;
  hefesto::platform::LocalFileSystem files;
  auto ctx = pair.context();
  ctx.files = &files;
  const auto none = lvs::evaluate_setup(lvs::Source{"/nonexistent/dir/nosetup", std::nullopt}, ctx);
  REQUIRE(none.ok());
  CHECK(logged_error(none.value(), "No setup file specified.  Continuing without a setup."));
  CHECK(pair.cell(1, "nfet").permutes.empty());
  const auto missing = lvs::evaluate_setup(lvs::Source{"/nonexistent/dir/foo.tcl", std::nullopt}, ctx);
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == codes::kMissingFile);
  CHECK(missing.error().text == "Error:  Setup file /nonexistent/dir/foo.tcl does not exist.");
}

TEST_CASE("A readable setup file is read and announced", "[cap:lvs-setup-file][req:Setup file evaluation]") {
  Pair pair;
  const auto dir = std::filesystem::temp_directory_path() / "hefesto_lvs_setup_test";
  std::filesystem::create_directories(dir);
  const auto path = dir / "s.tcl";
  hefesto::platform::LocalFileSystem files;
  REQUIRE(files.write_atomic(path, "permute transistors\n").ok());
  auto ctx = pair.context();
  ctx.files = &files;
  const auto state = lvs::evaluate_setup(lvs::Source{path.string(), std::nullopt}, ctx);
  REQUIRE(state.ok());
  CHECK(logged(state.value(), "Reading setup file " + path.string()));
  CHECK(state.value().evaluated);
  std::filesystem::remove_all(dir);
}

TEST_CASE("A setup file above the size limit is not evaluated", "[cap:lvs-setup-file][req:Setup evaluation is bounded and cancellable]") {
  Pair pair;
  const std::string big(static_cast<std::size_t>(lvs::kMaxSetupFileBytes) + 1, '#');
  for (const bool compat : {true, false}) {
    const auto state = pair.run(big, compat);
    REQUIRE_FALSE(state.ok());
    CHECK(state.error().code == codes::kTooLarge);
  }
}

namespace {

// `while {1} {}` runs no command, so the wall-time backstop stops it.
void endless_loop_stops(bool compat) {
  Pair pair;
  auto ctx = pair.context(compat);
  ctx.time_budget = std::chrono::milliseconds{300};
  const auto state = lvs::evaluate_setup(lvs::Source{"loop.tcl", std::string{"puts start\nwhile {1} {}\n"}}, ctx);
  REQUIRE_FALSE(state.ok());
  CHECK(state.error().code == codes::kStepBudget);
}

void budget_counts_the_whole_file(bool compat) {
  Pair pair;
  auto ctx = pair.context(compat);
  ctx.step_budget = 2'000;
  std::string text = "proc f {} {}\n";
  for (int i = 0; i < 400; ++i) {
    text += "f; f; f\n";
  }
  const auto state = lvs::evaluate_setup(lvs::Source{"many.tcl", text}, ctx);
  REQUIRE_FALSE(state.ok());
  CHECK(state.error().code == codes::kStepBudget);
}

}  // namespace

TEST_CASE("An endless loop stops at the step budget (netgen-compat ON)",
          "[cap:lvs-setup-file][req:Setup evaluation is bounded and cancellable]") {
  endless_loop_stops(true);
  budget_counts_the_whole_file(true);
}

TEST_CASE("An endless loop stops at the step budget (netgen-compat OFF)",
          "[cap:lvs-setup-file][req:Setup evaluation is bounded and cancellable]") {
  endless_loop_stops(false);
  budget_counts_the_whole_file(false);
}

TEST_CASE("A cancelled run stops setup evaluation", "[cap:lvs-setup-file][req:Setup evaluation is bounded and cancellable]") {
  for (const bool compat : {true, false}) {
    Pair pair;
    std::atomic<bool> cancel{true};
    auto ctx = pair.context(compat);
    ctx.cancel.flag = &cancel;
    const auto state = lvs::evaluate_setup(lvs::Source{"c.tcl", std::string{"permute\n"}}, ctx);
    REQUIRE_FALSE(state.ok());
    CHECK(state.error().code == codes::kCancelled);
    CHECK(pair.cell(1, "nfet").permutes.empty());
  }
}

TEST_CASE("Cancelling inside a running loop interrupts it", "[cap:lvs-setup-file][req:Setup evaluation is bounded and cancellable]") {
  Pair pair;
  std::atomic<bool> cancel{false};
  auto ctx = pair.context();
  ctx.cancel.flag = &cancel;
  ctx.step_budget = 2 * lvs::kSetupStepBudget;  // the budget alone would take much longer
  std::thread canceller([&cancel] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    cancel = true;
  });
  const auto state = lvs::evaluate_setup(lvs::Source{"c.tcl", std::string{"while {1} {}\n"}}, ctx);
  canceller.join();
  REQUIRE_FALSE(state.ok());
  CHECK(state.error().code == codes::kCancelled);
}

TEST_CASE("A loop that calls commands stops at the step budget", "[cap:lvs-setup-file][req:Setup evaluation is bounded and cancellable]") {
  for (const bool compat : {true, false}) {
    Pair pair;
    auto ctx = pair.context(compat);
    ctx.step_budget = 100'000;
    const auto state =
        lvs::evaluate_setup(lvs::Source{"loop.tcl", std::string{"proc f {} {}\nwhile {1} { f }\n"}}, ctx);
    REQUIRE_FALSE(state.ok());
    CHECK(state.error().code == codes::kStepBudget);
    CHECK(state.error().text.find("step budget") != std::string::npos);
  }
}

TEST_CASE("A context without circuits or with a bad budget is refused", "[cap:lvs-setup-file][req:Setup evaluation is bounded and cancellable]") {
  Pair pair;
  auto ctx = pair.context();
  ctx.step_budget = 0;
  CHECK(lvs::evaluate_setup(lvs::Source{"s", std::string{}}, ctx).error().code == codes::kBadContext);
  ctx.step_budget = 2 * lvs::kSetupStepBudget + 1;
  CHECK(lvs::evaluate_setup(lvs::Source{"s", std::string{}}, ctx).error().code == codes::kBadContext);
  ctx = pair.context();
  ctx.time_budget = std::chrono::milliseconds{0};
  CHECK(lvs::evaluate_setup(lvs::Source{"s", std::string{}}, ctx).error().code == codes::kBadContext);
  ctx = pair.context();
  ctx.second = nullptr;
  CHECK(lvs::evaluate_setup(lvs::Source{"s", std::string{}}, ctx).error().code == codes::kBadContext);
}

TEST_CASE("Successful commands are recorded in order", "[cap:lvs-setup-file][req:Setup file evaluation]") {
  Pair pair;
  const auto state = pair.ok("permute transistors\nproperty default\nignore class nfet\n");
  REQUIRE(state.permutes.size() == 1);
  CHECK(state.permutes.front() == std::vector<std::string>{"permute", "transistors"});
  CHECK(state.properties.size() == 1);
  CHECK(state.ignores.size() == 1);
}
