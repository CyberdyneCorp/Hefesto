// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Tcl console (docs/design/mockups/M4-Console.dc.html; openspec/changes/
// m4-scripting task 10.5): the transcript, multi-line input, shared
// history, running and interrupt state, and the console bound to a real
// project interpreter.

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <string>
#include <vector>

#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/shell/console_session.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/tcl_console.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

/// A scripted evaluator: completeness by the console's own scan, results
/// from a callback, a fake clock.
class FakeEvaluator final : public ConsoleEvaluator {
 public:
  bool complete(std::string_view text) override { return console_input_status(text).complete; }
  ConsoleEvaluation evaluate(std::string_view command) override {
    evaluated.emplace_back(command);
    if (during) during();
    if (command == "boom") {
      return {cmd::CommandResult::failure(diag::make("SCRIPT-TCL-001", "invalid command name \"boom\"")), false};
    }
    if (interrupted) {
      interrupted = false;  // the next evaluation runs normally
      steps += 412;
      return {cmd::CommandResult::failure(diag::make("SCRIPT-TCL-002", "eval canceled")), true};
    }
    return {cmd::CommandResult::success(cmd::Value{"r:" + std::string{command}}), false};
  }
  void interrupt() noexcept override { interrupted = true; }
  std::size_t undo_steps() const override { return steps; }

  std::vector<std::string> evaluated;
  std::function<void()> during;
  bool interrupted = false;
  std::size_t steps = 0;
};

const std::vector<std::string>& no_history() {
  static const std::vector<std::string> kEmpty;
  return kEmpty;
}

platform::PortableOptions test_options() {
  return platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true, .inline_tasks = true};
}

/// A core session with its project interpreter open and bound (as
/// app::run_startup does) and the console attached.
struct ConsoleFixture {
  explicit ConsoleFixture(script::SandboxPolicy policy = {}) {
    script::Interpreter& interp = script::open_project_interpreter(core.editor(), std::move(policy));
    (void)script::bind_registry(interp, core.registry(), core.dispatcher());
    session.set_transcript(&console.transcript());
    REQUIRE(session.attach(interp));
  }
  CoreSession core{test_options()};
  TclConsoleSession session{core.editor(), core.dispatcher()};
  std::vector<std::string> history;
  TclConsole console{session, [this]() -> const std::vector<std::string>& { return history; }};
};

std::vector<std::string> shown(const ConsoleTranscript& transcript) {
  std::vector<std::string> out;
  for (const ConsoleLine& line : transcript.lines()) out.push_back(console_line_text(line));
  return out;
}

}  // namespace

TEST_CASE("console lines keep their kind by shape and the transcript is capped",
          "[cap:scripting-auxiliary-tools][req:Console integration]") {
  ConsoleTranscript transcript(4);
  transcript.add_input("foreach v {VDD GND} {\n  puts $v\n}");
  transcript.add(ConsoleLineKind::err, "Mos width must be >= 0.42 um\n");
  REQUIRE(transcript.lines().size() == 4);  // 3 input lines + 1 stderr line; trailing newline adds none
  CHECK(shown(transcript) == std::vector<std::string>{"% foreach v {VDD GND} {", ">   puts $v", "> }",
                                                      "▲ Mos width must be >= 0.42 um"});
  transcript.add(ConsoleLineKind::error, "process execution (exec) is not permitted");
  CHECK(transcript.lines().size() == 4);
  CHECK(transcript.dropped() == 1);
  CHECK(transcript.visible(ConsoleFilter::stderr_only) == std::vector<std::size_t>{2, 3});
  CHECK(transcript.text().ends_with("■ process execution (exec) is not permitted\n"));
  CHECK(ConsoleTranscript::kDefaultLimit == 10'000);
}

TEST_CASE("Return runs only a complete command; Shift-Return inserts a line",
          "[cap:scripting-auxiliary-tools][req:Console integration]") {
  FakeEvaluator evaluator;
  TclConsole console(evaluator, no_history);
  console.set_input("proc ring {n} {");
  CHECK(console.input_status().open_braces == 1);
  CHECK(console.input_status().hint().starts_with("1 open brace · ⏎ run when complete"));
  CHECK(console.press_return(false) == ConsoleReturn::newline);
  CHECK(console.input() == "proc ring {n} {\n");
  console.set_input(console.input() + "  paint m1\n}");
  CHECK(console.input_status().complete);
  CHECK(console.press_return(true) == ConsoleReturn::newline);  // Shift-Return
  console.set_input("proc ring {n} {\n  paint m1\n}");
  CHECK(console.press_return(false) == ConsoleReturn::ran);
  CHECK(evaluator.evaluated == std::vector<std::string>{"proc ring {n} {\n  paint m1\n}"});
  CHECK(console.input().empty());
  CHECK(console.press_return(false) == ConsoleReturn::empty);
}

TEST_CASE("input completeness follows Tcl's word rules",
          "[cap:scripting-auxiliary-tools][req:Console integration]") {
  CHECK(console_input_status("puts \"a {\"").complete);  // a brace inside quotes
  CHECK(console_input_status("set x {a \"b}").complete);  // a quote inside braces
  CHECK(console_input_status("# comment {").complete);    // a comment
  CHECK_FALSE(console_input_status("set a [list 1").complete);
  CHECK(console_input_status("set a [list 1").open_brackets == 1);
  CHECK(console_input_status("puts \"abc").open_quote);
  CHECK(console_input_status("puts a \\\n").continued);
  CHECK(console_input_status("set x \\{").complete);  // an escaped brace
  CHECK(console_input_status("foreach v {VDD GND SUB} {\n  puts \"$v = [set $v]\"\n}").complete);
}

TEST_CASE("Up and Down walk the history shared with the command bar",
          "[cap:scripting-auxiliary-tools][req:Console integration]") {
  FakeEvaluator evaluator;
  std::vector<std::string> history = {"box values 0 0 84 330", "paint m1"};
  TclConsole console(evaluator, [&]() -> const std::vector<std::string>& { return history; });
  console.set_input("draft");
  console.history_older();
  CHECK(console.input() == "paint m1");
  console.history_older();
  CHECK(console.input() == "box values 0 0 84 330");
  console.history_older();  // the oldest: unchanged
  CHECK(console.input() == "box values 0 0 84 330");
  console.history_newer();
  CHECK(console.input() == "paint m1");
  console.history_newer();
  CHECK(console.input() == "draft");
}

TEST_CASE("a long run shows the Interrupt card after 2 s, queues input and reports the interrupt",
          "[cap:scripting-auxiliary-tools][req:Console integration]") {
  FakeEvaluator evaluator;
  ConsoleClock::time_point now{};
  TclConsole console(evaluator, no_history, [&] { return now; });
  CHECK_FALSE(console.interrupt());  // only while running
  evaluator.during = [&] {
    REQUIRE(console.running());
    now += std::chrono::milliseconds{1999};
    CHECK_FALSE(console.current_run()->card_visible(now));
    now += std::chrono::milliseconds{2201};
    CHECK(console.current_run()->card_visible(now));
    CHECK(console.current_run()->header_text(now) == "● running 4.2 s");
    console.set_input("box");  // typed while it runs: queued
    CHECK(console.press_return(false) == ConsoleReturn::queued);
    CHECK(console.interrupt());
    CHECK_FALSE(console.interrupt());  // once
    evaluator.during = {};
  };
  console.set_input("ring 100000");
  console.press_return(false);
  CHECK_FALSE(console.running());
  const auto lines = shown(console.transcript());
  CHECK(lines == std::vector<std::string>{"% ring 100000", "% box", "queued until it ends",
                                          "■ eval canceled · interrupted by the user (412 edits kept; "
                                          "undo reverts them as one step)",
                                          "r:box"});
  CHECK(evaluator.evaluated == std::vector<std::string>{"ring 100000", "box"});
}

TEST_CASE("window command entry directs the console's lines to a window",
          "[cap:ui-gui-managers][req:Per-window command entry]") {
  FakeEvaluator evaluator;
  TclConsole console(evaluator, no_history);
  CHECK(console.prompt() == "%");
  console.set_target_window(".layout1");
  CHECK(console.prompt() == ".layout1>");
  // Scenario "Directed console".
  console.run("paint m1");
  CHECK(evaluator.evaluated == std::vector<std::string>{".layout1 paint m1"});
}

TEST_CASE("the console evaluates in the project interpreter and captures puts",
          "[cap:scripting-api][req:Output, flushing and escaping]") {
  ConsoleFixture f;
  f.console.run("set PDKPATH /pdk/sky130A; foreach v {a b} { puts \"$v = $PDKPATH\" }");
  f.console.run("puts stderr {Mos width must be >= 0.42 um}");
  f.console.run("puts -nonewline {a$b}; puts {c}");  // a literal `$` stays (Q-SCRIPT-4)
  f.console.run("expr {6*7}");
  CHECK(shown(f.console.transcript()) ==
        std::vector<std::string>{"% set PDKPATH /pdk/sky130A; foreach v {a b} { puts \"$v = $PDKPATH\" }",
                                 "a = /pdk/sky130A", "b = /pdk/sky130A",
                                 "% puts stderr {Mos width must be >= 0.42 um}",
                                 "▲ Mos width must be >= 0.42 um", "% puts -nonewline {a$b}; puts {c}",
                                 "a$bc", "% expr {6*7}", "42"});
  // The interpreter's `info complete` decides Return.
  CHECK_FALSE(f.session.complete("proc p {} {"));
  CHECK(f.session.complete("proc p {} {}"));
}

TEST_CASE("the console shares the project's procedures with magic:: commands",
          "[cap:hefesto-scripting][req:One interpreter per open project]") {
  ConsoleFixture f;
  f.console.run("proc twice {x} { return [expr {2*$x}] }");
  f.console.run("twice 21");
  CHECK(shown(f.console.transcript()).back() == "42");
  REQUIRE(f.core.editor().script.interp->has_command("twice"));
  f.console.run("info commands ::magic::box");
  CHECK(shown(f.console.transcript()).back() == "::magic::box");
}

TEST_CASE("sandbox denials and Tcl errors are error lines with their code",
          "[cap:hefesto-security][req:Script sandbox enforcement]") {
  ConsoleFixture f;
  f.console.run("exec ls");
  const auto lines = shown(f.console.transcript());
  REQUIRE(lines.size() >= 2);
  CHECK(lines.back().starts_with("■ "));
  INFO(lines.back());
  CHECK(lines.back().find("SCRIPT-SANDBOX-") != std::string::npos);
  f.console.run("nosuchcommand_xyz");
  CHECK(shown(f.console.transcript()).back().starts_with("■ "));
}

TEST_CASE("an interrupt from the console stops a running script",
          "[cap:scripting-auxiliary-tools][req:Console integration]") {
  script::SandboxPolicy policy;
  policy.limits.time_limit = std::chrono::milliseconds{20000};  // a safety net only
  ConsoleFixture f(std::move(policy));
  // A native command the script calls stands in for the Interrupt button
  // pressed during a frame the run pumps.
  f.core.editor().script.interp->define_command("::press_interrupt", [&f](const cmd::Invocation&) {
    CHECK(f.console.interrupt());
    return cmd::CommandResult::success();
  });
  const auto start = std::chrono::steady_clock::now();
  f.console.run("press_interrupt; set i 0; while 1 { incr i }");
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{10});
  CHECK_FALSE(f.console.running());
  const std::string last = shown(f.console.transcript()).back();
  CHECK(last.starts_with("■ "));
  CHECK(last.find("interrupted by the user") != std::string::npos);
  // The interpreter is usable afterwards.
  f.console.run("expr {1+1}");
  CHECK(shown(f.console.transcript()).back() == "2");
}

TEST_CASE("the console pumps frames between the commands of a long run",
          "[cap:scripting-auxiliary-tools][req:Console integration]") {
  ConsoleFixture f;
  int pumps = 0;
  f.session.set_pump([&] { ++pumps; }, std::chrono::milliseconds{0});
  f.core.set_command_listener([&](const cmd::Invocation&, const cmd::CommandResult&) { f.session.on_command(); });
  f.session.on_command();  // not evaluating: no pump
  CHECK(pumps == 0);
  f.console.run("for {set i 0} {$i < 3} {incr i} { magic::tech name }");
  CHECK(pumps >= 3);
  f.core.set_command_listener({});
}

TEST_CASE("a console line's outcome summarizes the result for the status bar",
          "[cap:scripting-auxiliary-tools][req:Console integration]") {
  const CommandOutcome ok = TclConsoleSession::outcome_of({cmd::CommandResult::success(cmd::Value{"42"}), false});
  CHECK(ok.ok());
  CHECK(ok.message == "42");
  const CommandOutcome failed = TclConsoleSession::outcome_of(
      {cmd::CommandResult::failure(diag::make("SCRIPT-TCL-001", "invalid command name \"x\"")), false});
  CHECK_FALSE(failed.ok());
  CHECK(failed.message == "invalid command name \"x\"");
  CHECK(failed.diagnostics.size() == 1);
}
