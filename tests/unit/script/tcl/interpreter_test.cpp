// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The project interpreter (openspec/changes/m4-scripting tasks.md 4.1, 4.4):
// value conversion and native commands (capability hefesto-scripting "Host-
// independent scripting binding"), the embedded library, one interpreter per
// open project, interrupt and evaluation limits.

#include <unistd.h>

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "hefesto/script/tcl/embedded_library.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"
#include "tcl_fixture.hpp"

namespace script = hefesto::script;
namespace cmd = hefesto::cmd;
using hefesto::test::codes;
using hefesto::test::error_text;
using hefesto::test::has_code;
using hefesto::test::SandboxedProject;

#define BINDING "[cap:hefesto-scripting][req:Host-independent scripting binding]"
#define PER_PROJECT "[cap:hefesto-scripting][req:One interpreter per open project]"
#define EMBEDDED "[cap:hefesto-scripting][req:Embedded sandboxed Tcl on all platforms]"

// ---- values and native commands ----------------------------------------------------

TEST_CASE("results keep their Tcl types: numbers, strings and lists", BINDING) {
  SandboxedProject p;
  CHECK(p.run("expr {1+2}").value == cmd::Value{3});
  CHECK(p.run("expr {1.5*2}").value == cmd::Value{3.0});
  CHECK(p.run("set s hello").value == cmd::Value{"hello"});
  CHECK(p.run("list a {b c} {}").value ==
        cmd::Value{cmd::Value::List{"a", "b c", ""}});
  CHECK(p.run("list 1 [list 2 3]").value ==
        cmd::Value{cmd::Value::List{"1", cmd::Value::List{"2", "3"}}});
  CHECK(p.run("list").value == cmd::Value{""});  // Tcl's empty result has no list type
  // A number-looking string Tcl has not used as a number stays a string.
  CHECK(p.run("set n 42").value == cmd::Value{"42"});
  // Integers beyond 64 bits are Tcl bignums: their string form.
  CHECK(p.run("expr {2**70}").value == cmd::Value{"1180591620717411303424"});
}

TEST_CASE("values set from C++ arrive in Tcl with their types; lists stay lists", BINDING) {
  SandboxedProject p;
  REQUIRE(p.interp->set_var("v", cmd::Value{cmd::Value::List{1, 2.5, "two words", cmd::Value::List{"x", "y"}}}));
  CHECK(p.value("llength $v") == "4");
  CHECK(p.value("lindex $v 2") == "two words");
  CHECK(p.value("lindex $v 3 1") == "y");
  CHECK(p.value("expr {[lindex $v 0] + [lindex $v 1]}") == "3.5");
  CHECK(p.interp->get_var("v") ==
        std::optional<cmd::Value>{cmd::Value{cmd::Value::List{1, 2.5, "two words", cmd::Value::List{"x", "y"}}}});
  REQUIRE(p.interp->set_var("::magic::defined_later", cmd::Value{"x"}) == false);
  REQUIRE(p.run("namespace eval ::magic {}").ok());
  REQUIRE(p.interp->set_var("::magic::scale", cmd::Value{7}));
  CHECK(p.interp->get_var("::magic::scale") == std::optional<cmd::Value>{cmd::Value{7}});
  CHECK_FALSE(p.interp->get_var("nosuch").has_value());
}

TEST_CASE("a native command gets the words as called and returns its value as the Tcl result", BINDING) {
  SandboxedProject p;
  cmd::Invocation seen;
  p.interp->define_command("::magic::probe", [&](const cmd::Invocation& call) {
    seen = call;
    return cmd::CommandResult::success(cmd::Value{cmd::Value::List{10, 20, 30, 40}});
  });
  CHECK(p.value("set r [::magic::probe values {a b} $tcl_version]; lindex $r 3") == "40");
  CHECK(seen.name == "::magic::probe");
  CHECK(seen.args == std::vector<std::string>{"values", "a b", "8.6"});
  CHECK(seen.origin == cmd::Origin::script);
  // The value is the same four-element result as from C++ ("Return value parity").
  CHECK(p.run("::magic::probe values").value == cmd::Value{cmd::Value::List{10, 20, 30, 40}});
}

TEST_CASE("a native command runs with the origin of the evaluation that calls it", BINDING) {
  SandboxedProject p;
  std::vector<cmd::Origin> origins;
  p.interp->define_command("probe", [&](const cmd::Invocation& call) {
    origins.push_back(call.origin);
    return cmd::CommandResult::success();
  });
  REQUIRE(p.interp->evaluate("probe", cmd::Origin::gui).ok());
  REQUIRE(p.interp->evaluate("probe", cmd::Origin::cli).ok());
  REQUIRE(p.interp->evaluate("probe").ok());
  CHECK(origins == std::vector<cmd::Origin>{cmd::Origin::gui, cmd::Origin::cli, cmd::Origin::script});
}

TEST_CASE("a failed native command is a Tcl error that catch absorbs", BINDING) {
  SandboxedProject p;
  p.interp->define_command("fails", [](const cmd::Invocation&) {
    return cmd::CommandResult::failure(hefesto::diag::make("SCRIPT-TCL-001", "nothing selected"));
  });
  p.interp->define_command("throws", [](const cmd::Invocation&) -> cmd::CommandResult {
    throw std::runtime_error("internal failure");
  });
  const auto failed = p.run("fails");
  REQUIRE_FALSE(failed.ok());
  CHECK(error_text(failed) == "nothing selected");
  CHECK(p.value("catch {fails} message; set message") == "nothing selected");
  CHECK(p.value("catch {throws} message; set message") == "internal failure");
}

TEST_CASE("Tcl errors are SCRIPT-TCL-001 with the script line", BINDING) {
  SandboxedProject p;
  const auto error = p.run("set a 1\nset b 2\nerror boom");
  REQUIRE_FALSE(error.ok());
  REQUIRE(codes(error) == std::vector<std::string>{"SCRIPT-TCL-001"});
  CHECK(error.diagnostics.front().text == "boom");
  const auto* where = std::get_if<hefesto::diag::SourceLocation>(&error.diagnostics.front().location);
  REQUIRE(where != nullptr);
  CHECK(where->line == 3);
  CHECK(error_text(p.run("break")) == "invoked \"break\" outside of a loop");
  CHECK(p.run("return 7").value == cmd::Value{"7"});
}

TEST_CASE("an evaluated view ends at its length, also inside an open quoted word", BINDING) {
  // Regression (fuzz_tcl_sandbox): Tcl's parser looked at the byte after the
  // view for the closing quote; here that byte is one, so the unterminated
  // word must not be read as closed.
  SandboxedProject p;
  const std::string buffer = "set a \"x\"; set b 1";
  const auto result = p.run(std::string_view{buffer}.substr(0, 8));  // set a "x
  REQUIRE_FALSE(result.ok());
  CHECK(error_text(result) == "missing \"");
  CHECK(p.value("info exists a") == "0");
  CHECK(p.value("info exists b") == "0");
}

TEST_CASE("source names the file in info script and locates its errors", BINDING) {
  SandboxedProject p;
  const auto file = p.project.write("lib/setup.tcl", "set inside [info script]\n\nnosuchcommand\n");
  const auto result = p.interp->source(file);
  REQUIRE_FALSE(result.ok());
  CHECK(p.interp->get_var("inside") == std::optional<cmd::Value>{cmd::Value{file.string()}});
  const auto* where = std::get_if<hefesto::diag::SourceLocation>(&result.diagnostics.front().location);
  REQUIRE(where != nullptr);
  CHECK(where->file == file.string());
  CHECK(where->line == 3);
  CHECK(p.interp->info_script().empty());
}

TEST_CASE("a native command can ask for the calling script line", BINDING) {
  SandboxedProject p;
  hefesto::diag::SourceLocation seen;
  p.interp->define_command("where", [&](const cmd::Invocation&) {
    seen = script::current_script_location(*p.interp);
    return cmd::CommandResult::success();
  });
  const auto file = p.project.write("where.tcl", "# comment\nset x 1\nwhere\n");
  REQUIRE(p.interp->source(file).ok());
  CHECK(seen.file == file.string());
  CHECK(seen.line == 3);
}

// ---- the embedded library -------------------------------------------------------------

TEST_CASE("the interpreter reads Tcl's library from memory and no host Tcl file", EMBEDDED) {
  // Host settings Tcl would consult must not matter.
  ::setenv("TCL_LIBRARY", "/nonexistent/tcl-library", 1);
  ::setenv("TCLLIBPATH", "/nonexistent/tcllibpath", 1);
  std::vector<script::FilesystemAccess> accesses;
  script::set_filesystem_audit([&](const script::FilesystemAccess& access) { accesses.push_back(access); });
  {
    SandboxedProject p;
    const std::string root{script::kVirtualLibraryRoot};
    CHECK(p.value("info library") == root);
    CHECK(p.value("set ::auto_path") == root);
    CHECK(p.value("package require msgcat") == "1.6.1");
    CHECK(p.value("package require opt") == "0.4.9");
    CHECK(p.value("clock format 0 -format %Y -gmt 1") == "1970");
    CHECK(p.value("string length [clock format [clock seconds]]") != "0");
    CHECK(p.value("auto_load parray; info commands parray") == "parray");  // from parray.tcl
    CHECK(p.value("lsort [glob -tails -directory [info library] *.tcl]").find("init.tcl") != std::string::npos);
    CHECK(p.value("set f [open [file join [info library] tclIndex]]; set n [string length [read $f]]; close $f; "
                  "expr {$n > 0}") == "1");
    CHECK(p.value("info exists env(TCL_LIBRARY)") == "0");
    CHECK(p.value("::tcl::tm::path list") ==
          p.value("lsearch -all -inline -glob [::tcl::tm::path list] /__hefesto_embedded__/*"));
  }
  script::set_filesystem_audit({});
  ::unsetenv("TCL_LIBRARY");
  ::unsetenv("TCLLIBPATH");
  REQUIRE_FALSE(accesses.empty());
  for (const auto& access : accesses) {
    INFO(access.path);
    // Nothing outside the embedded tree reached the host filesystem.
    CHECK(access.verdict != script::FilesystemAccess::Verdict::allowed);
    CHECK(access.path.find("nonexistent") == std::string::npos);
  }
}

TEST_CASE("the embedded library is read-only", EMBEDDED) {
  SandboxedProject p;
  const auto write = p.run("open [file join [info library] init.tcl] w");
  REQUIRE_FALSE(write.ok());
  CHECK(has_code(write, "SCRIPT-SANDBOX-004"));
  CHECK_FALSE(p.run("file delete [file join [info library] init.tcl]").ok());
  CHECK(p.value("file exists [file join [info library] init.tcl]") == "1");
  CHECK(p.value("file isdirectory [file join [info library] msgcat]") == "1");
}

// ---- one interpreter per open project --------------------------------------------------

TEST_CASE("Two projects on different PDKs", PER_PROJECT) {
  // Stand-ins for the PDK helpers each project's device script defines.
  SandboxedProject a;
  SandboxedProject b;
  REQUIRE(a.run("set PDK sky130A; namespace eval magic {}\n"
                "proc magic::i2u {value} {return [expr {$value * 0.005}]}\n"
                "array set Opts {toolbar 1}")
              .ok());
  REQUIRE(b.run("set PDK ihp-sg13g2; namespace eval magic {}\n"
                "proc magic::i2u {value} {return [expr {$value * 0.001}]}")
              .ok());
  CHECK(a.value("magic::i2u 34") == "0.17");
  CHECK(b.value("magic::i2u 34") == "0.034");
  CHECK(a.value("set PDK") == "sky130A");
  CHECK(b.value("set PDK") == "ihp-sg13g2");
  CHECK(b.value("info exists Opts") == "0");
  // Each project's commands act on its own session.
  CHECK(a.state.script.interp.get() == a.interp);
  CHECK(b.state.script.interp.get() == b.interp);
  CHECK(a.recorder.denials.empty());
  CHECK(b.recorder.denials.empty());
}

TEST_CASE("an interpreter is deleted when its project closes; the others keep running", PER_PROJECT) {
  SandboxedProject a;
  SandboxedProject b;
  REQUIRE(a.run("proc hello {} {return a}").ok());
  bool deleted = false;
  std::shared_ptr<void> token(nullptr, [&deleted](void*) { deleted = true; });
  a.interp->define_command("probe", [token](const cmd::Invocation&) { return cmd::CommandResult::success(); });
  token.reset();
  CHECK_FALSE(deleted);
  script::close_project_interpreter(a.state);
  CHECK(a.state.script.interp == nullptr);
  CHECK(deleted);  // its native commands went with it
  CHECK(b.value("expr {6*7}") == "42");
  // Reopening gives a fresh interpreter.
  auto& reopened = script::open_project_interpreter(a.state, script::SandboxPolicy{});
  CHECK_FALSE(reopened.has_command("hello"));
}

// ---- interrupt and limits ---------------------------------------------------------

TEST_CASE("an interrupt from another thread stops a runaway evaluation; catch cannot absorb it", PER_PROJECT) {
  SandboxedProject p;
  p.interp->define_command("started", [](const cmd::Invocation&) { return cmd::CommandResult::success(); });
  std::thread interrupter([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    p.interp->request_interrupt();
  });
  const auto result = p.run("catch {while 1 {}}; set after yes");
  interrupter.join();
  REQUIRE_FALSE(result.ok());
  CHECK(codes(result).front() == "SCRIPT-TCL-002");
  CHECK_FALSE(p.interp->get_var("after").has_value());
  // The interpreter is usable again, and an interrupt while idle is ignored.
  p.interp->request_interrupt();
  CHECK(p.value("expr {1+1}") == "2");
}

TEST_CASE("the command limit stops one evaluation and resets for the next", PER_PROJECT) {
  script::SandboxPolicy policy;
  policy.limits.command_limit = 10000;
  SandboxedProject p{policy};
  const auto runaway = p.run("catch {while 1 {incr i}}");
  REQUIRE_FALSE(runaway.ok());
  CHECK(codes(runaway).front() == "SCRIPT-TCL-003");
  CHECK(p.value("set j 0; for {set k 0} {$k < 100} {incr k} {incr j}; set j") == "100");
}

TEST_CASE("the time limit stops loops and sleeps", PER_PROJECT) {
  script::SandboxPolicy policy;
  policy.limits.time_limit = std::chrono::milliseconds(200);
  SandboxedProject p{policy};
  const auto start = std::chrono::steady_clock::now();
  const auto loop = p.run("while 1 {}");
  const auto sleep = p.run("after 60000");
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(codes(loop).front() == "SCRIPT-TCL-003");
  CHECK(codes(sleep).front() == "SCRIPT-TCL-003");
  CHECK(elapsed < std::chrono::seconds(10));
  CHECK(p.value("expr {2+2}") == "4");
}
