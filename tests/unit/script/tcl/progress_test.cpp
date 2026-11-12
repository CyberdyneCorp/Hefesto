// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Progress of long-running evaluations (script/progress.hpp: the
// interpreter's time-limit handler reports the file and line being executed
// on the evaluating thread, and an interrupt asked from that report stops
// the script) and the `caught` mark of sandbox denials a script absorbed.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <ctime>
#include <stdexcept>
#include <string>
#include <vector>

#include "hefesto/script/progress.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"
#include "tcl_fixture.hpp"

namespace script = hefesto::script;
using hefesto::test::codes;
using hefesto::test::SandboxedProject;
using namespace std::chrono_literals;

#define PROGRESS "[cap:hefesto-scripting][req:Long-running script progress and interrupt]"
#define SANDBOX "[cap:hefesto-security][req:Script sandbox enforcement]"

namespace {

// Records the reports of one project's interpreter.
struct Reports {
  std::vector<script::ScriptProgress> all;

  void install(SandboxedProject& p, std::chrono::milliseconds interval = 10ms) {
    p.state.script.progress.interval = interval;
    p.state.script.progress.handler = [this](const script::ScriptProgress& r) { all.push_back(r); };
  }
  [[nodiscard]] std::size_t running() const {
    std::size_t n = 0;
    for (const auto& r : all) n += r.done ? 0U : 1U;
    return n;
  }
};

constexpr std::string_view kBusyLoop = "set t [clock milliseconds]\nwhile {[clock milliseconds] - $t < 200} {}\n";

}  // namespace

TEST_CASE("a long evaluation reports its file and line while a pure Tcl loop runs", PROGRESS) {
  SandboxedProject p;
  Reports reports;
  reports.install(p);
  const auto file = p.project.write("long.tcl", kBusyLoop);
  const auto result = p.interp->source(file);
  REQUIRE(result.ok());
  REQUIRE(reports.running() >= 3);  // about one per 10 ms over 200 ms
  const script::ScriptProgress& during = reports.all[reports.all.size() / 2];
  CHECK_FALSE(during.done);
  CHECK(during.script == file.string());
  CHECK(during.location.file == file.string());
  CHECK(during.location.line == 2);
  CHECK(during.origin == hefesto::cmd::Origin::script);
  CHECK(during.elapsed > 0ms);
  // The last report says it ended (the front end hides its card).
  CHECK(reports.all.back().done);
  CHECK(reports.all.back().elapsed >= 150ms);  // the loop measures 200 ms with its own clock
}

TEST_CASE("a report names the line of the command a loop body calls", PROGRESS) {
  SandboxedProject p;
  Reports reports;
  reports.install(p);
  p.interp->define_command("noop", [](const hefesto::cmd::Invocation&) {
    return hefesto::cmd::CommandResult::success();
  });
  const auto file = p.project.write(
      "calls.tcl", "set t [clock milliseconds]\nwhile {[clock milliseconds] - $t < 200} {\n  noop\n}\n");
  REQUIRE(p.interp->source(file).ok());
  bool body_line = false;
  for (const auto& r : reports.all) body_line = body_line || (!r.done && r.location.line == 3);
  CHECK(body_line);
}

TEST_CASE("typed text reports its line and origin; a short evaluation reports nothing", PROGRESS) {
  SandboxedProject p;
  Reports reports;
  reports.install(p);
  CHECK(p.value("expr {6*7}") == "42");
  CHECK(reports.all.empty());  // no report before one interval, and so no `done` either
  REQUIRE(p.interp->evaluate(std::string{"\n"} + std::string{kBusyLoop}, hefesto::cmd::Origin::gui).ok());
  REQUIRE(reports.running() > 0);
  CHECK(reports.all.front().script.empty());
  CHECK(reports.all.front().location.line == 3);
  CHECK(reports.all.front().origin == hefesto::cmd::Origin::gui);
}

TEST_CASE("an interrupt asked from a progress report stops the script on the same thread", PROGRESS) {
  // The desktop runs scripts on the UI thread; its frame, drawn from the
  // report, takes the Interrupt button or Ctrl-C.
  SandboxedProject p;
  bool asked = false;
  bool done = false;
  p.state.script.progress.interval = 10ms;
  p.state.script.progress.handler = [&](const script::ScriptProgress& r) {
    done = done || r.done;
    if (!asked && r.elapsed >= 50ms) {
      asked = true;
      p.interp->request_interrupt();
    }
  };
  const auto result = p.run("catch {while 1 {incr n}}; set after yes");
  REQUIRE_FALSE(result.ok());
  CHECK(codes(result).front() == "SCRIPT-TCL-002");
  CHECK(asked);
  CHECK(done);
  CHECK_FALSE(p.interp->get_var("after").has_value());
  CHECK(p.value("expr {1+1}") == "2");
}

// The two tests below count reports, not milliseconds: a sleep on a loaded
// runner can last several intervals, so how many reports fit in a given span
// of wall-clock time is not fixed. What is fixed is that `after` checks the
// limit (and so reports) each time a slice of its sleep ends, and that an
// interrupt or the policy's deadline ends the evaluation.

TEST_CASE("sleeping in after reports progress and can be interrupted", PROGRESS) {
  SandboxedProject p;
  p.state.script.progress.interval = 10ms;
  std::size_t running = 0;
  bool done = false;
  p.state.script.progress.handler = [&](const script::ScriptProgress& r) {
    done = done || r.done;
    if (!r.done && ++running == 3) {
      p.interp->request_interrupt();
    }
  };
  // Ten minutes: only the interrupt asked from the third report ends it.
  const auto result = p.run("after 600000");
  REQUIRE_FALSE(result.ok());
  CHECK(codes(result).front() == "SCRIPT-TCL-002");
  CHECK(running == 3);  // `after` stops at the interrupt, before its next slice
  CHECK(done);
  CHECK(p.value("expr {1+1}") == "2");
}

TEST_CASE("progress reports do not lift the policy's time limit", PROGRESS) {
  script::SandboxPolicy policy;
  policy.limits.time_limit = 300ms;
  SandboxedProject p{policy};
  Reports reports;
  reports.install(p, 10ms);
  // An endless loop and an endless sleep: only the deadline ends them, though
  // every report moves Tcl's limit one interval on.
  for (const char* script : {"catch {while 1 {}}", "catch {after 600000}"}) {
    INFO(script);
    reports.all.clear();
    const auto result = p.run(script);
    REQUIRE_FALSE(result.ok());
    CHECK(codes(result).front() == "SCRIPT-TCL-003");
    CHECK(reports.running() >= 1);
    REQUIRE_FALSE(reports.all.empty());
    CHECK(reports.all.back().done);
  }
  // Each evaluation has its own deadline: the expired one does not stop the next.
  CHECK(p.value("expr {1+1}") == "2");
}

TEST_CASE("a handler that throws or evaluates does not stop the script", PROGRESS) {
  SandboxedProject p;
  p.state.script.progress.interval = 5ms;
  int nested = 0;
  p.state.script.progress.handler = [&](const script::ScriptProgress& r) {
    if (r.done) return;
    // A frame that runs a command anyway: no report for it, no recursion.
    if (p.interp->evaluate("set nested 1").ok()) ++nested;
    throw std::runtime_error("frame failed");
  };
  const auto file = p.project.write("long.tcl", kBusyLoop);
  REQUIRE(p.interp->source(file).ok());
  CHECK(nested > 0);
}

TEST_CASE("after a reporting evaluation, a sleep without a time limit does not spin", PROGRESS) {
  // Regression: Tcl keeps an ended time limit's timer and time; `after N`
  // in a later evaluation without a limit then busy-waited for N ms.
  SandboxedProject p;
  Reports reports;
  reports.install(p);
  REQUIRE(p.run("set x 1").ok());
  p.state.script.progress.handler = {};
  const std::clock_t cpu = std::clock();
  REQUIRE(p.run("after 300").ok());
  const double cpu_ms = 1000.0 * static_cast<double>(std::clock() - cpu) / CLOCKS_PER_SEC;
  CHECK(cpu_ms < 150.0);
}

// ---- caught denials ------------------------------------------------------------

TEST_CASE("a denial the script caught is marked caught; one that ended it is not", SANDBOX) {
  SandboxedProject p;
  const auto ok = p.run("set r [catch {exec ls} msg]\nset r");
  REQUIRE(ok.ok());
  CHECK(ok.value.to_string() == "1");
  REQUIRE(p.recorder.denials.size() == 1);
  CHECK(p.recorder.denials[0].operation == "exec");
  CHECK(p.recorder.denials[0].caught);
  REQUIRE(codes(ok) == std::vector<std::string>{"SCRIPT-SANDBOX-001"});
  CHECK(ok.diagnostics[0].text.ends_with("caught by the script's catch"));
  // The Tcl error the script saw is unchanged.
  CHECK(p.value("set msg").find("caught") == std::string::npos);

  p.recorder.denials.clear();
  const auto failed = p.run("catch {socket example.com 80}\nexec ls");
  REQUIRE_FALSE(failed.ok());
  REQUIRE(p.recorder.denials.size() == 2);
  CHECK(p.recorder.denials[0].operation == "socket");
  CHECK(p.recorder.denials[0].caught);
  CHECK(p.recorder.denials[1].operation == "exec");
  CHECK_FALSE(p.recorder.denials[1].caught);
  CHECK(p.recorder.denials[1].location.line == 2);
  for (const auto& d : failed.diagnostics) {
    if (d.code == "SCRIPT-SANDBOX-001") CHECK_FALSE(d.text.ends_with("caught by the script's catch"));
    if (d.code == "SCRIPT-SANDBOX-002") CHECK(d.text.ends_with("caught by the script's catch"));
  }
}

TEST_CASE("a denial rethrown by the script and one in a proc are not caught", SANDBOX) {
  SandboxedProject p;
  const auto rethrown = p.run("if {[catch {exec ls} msg opts]} {return -options $opts $msg}");
  REQUIRE_FALSE(rethrown.ok());
  REQUIRE(p.recorder.denials.size() == 1);
  CHECK_FALSE(p.recorder.denials[0].caught);

  p.recorder.denials.clear();
  const auto in_proc = p.run("proc fetch {} { exec curl -O x }\nproc run {} { fetch }\nrun");
  REQUIRE_FALSE(in_proc.ok());
  REQUIRE(p.recorder.denials.size() == 1);
  CHECK_FALSE(p.recorder.denials[0].caught);

  p.recorder.denials.clear();
  REQUIRE(p.run("proc quiet {} { catch { exec curl -O x } }\nquiet").ok());
  REQUIRE(p.recorder.denials.size() == 1);
  CHECK(p.recorder.denials[0].caught);
}

TEST_CASE("denial events are published when the evaluation ends", SANDBOX) {
  SandboxedProject p;
  std::size_t seen_during = 99;
  p.interp->define_command("count_denials", [&](const hefesto::cmd::Invocation&) {
    seen_during = p.recorder.denials.size();
    return hefesto::cmd::CommandResult::success();
  });
  REQUIRE(p.run("catch {exec ls}\ncount_denials").ok());
  CHECK(seen_during == 0);  // held until `caught` is known
  CHECK(p.recorder.denials.size() == 1);
}
