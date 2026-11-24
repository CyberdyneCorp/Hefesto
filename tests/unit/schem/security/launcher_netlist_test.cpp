// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Security review, task 12.1 (design D14): launcher `tclcommand` scripts
// (confirmed once per document, never replayed in another document, always
// in the sandbox) and hostile `tcleval` values netlisted end to end through
// `schematic netlist` (schem/cmd, read only here).
//
// Owner: SECURITY.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "../cmd/cmd_fixture.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/tcl/permission.hpp"
#include "hefesto/schem/tcl/tcl_environment.hpp"
#include "security_support.hpp"

namespace schem = hefesto::schem;
namespace platform = hefesto::platform;
namespace script = hefesto::script;
namespace sec = hefesto::test::security;

namespace {

struct Launchers {
  hefesto::editor::EditorState editor;
  platform::LocalFileSystem files;
  schem::TclEnvironment tcl{editor, script::SandboxPolicy{}, files};
  schem::LauncherPermission permission;
  int asked = 0;
  bool answer = true;
  schem::ConfirmLauncher ask = [this](std::string_view) { return ++asked, answer; };

  hefesto::cmd::CommandResult run(schem::DocumentId document, std::string_view tclcommand) {
    return schem::run_launcher(tcl.interpreter(), permission, document, tclcommand, ask);
  }
  std::string global(const std::string& name) {
    return tcl.interpreter().evaluate("if {[info exists " + name + "]} {set " + name + "}").value.to_string();
  }
};

constexpr const char* kHeader = "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n";
constexpr const char* kResistor =
    "v {xschem version=3.4.8RC file_version=1.3}\nG {}\n"
    "K {type=resistor\nformat=\"@name @pinlist @value\"\ntemplate=\"name=R1 value=1k\"}\nV {}\nS {}\nE {}\n"
    "B 5 -2.5 -32.5 2.5 -27.5 {name=P dir=inout}\nB 5 -2.5 27.5 2.5 32.5 {name=M dir=inout}\n";

}  // namespace

TEST_CASE("Launcher confirmed once", sec::kPermissionModel) {
  Launchers l;
  CHECK(l.run(1, "set ::first 1").ok());
  CHECK(l.run(1, "set ::second 1").ok());
  CHECK(l.asked == 1);
  CHECK(l.global("::second") == "1");
}

// Launcher replay: a confirmation belongs to one document; another document
// (also one opened later from the same file, which gets a new id) asks
// again, and a refusal there runs nothing.
TEST_CASE("a confirmed launcher is not replayed in another document", sec::kPermissionModel) {
  Launchers l;
  REQUIRE(l.run(1, "set ::a 1").ok());
  l.answer = false;
  const auto refused = l.run(2, "set ::b 1");
  CHECK_FALSE(refused.ok());
  REQUIRE(refused.diagnostics.size() == 1);
  CHECK(refused.diagnostics[0].code == "SCHEM-TCL-002");
  CHECK(l.global("::b").empty());
  CHECK(l.asked == 2);
  l.permission.forget(1);  // the document was closed
  CHECK_FALSE(l.run(1, "set ::c 1").ok());
  CHECK(l.asked == 3);
  CHECK(l.global("::c").empty());
  // A tcleval value (no prompt) cannot confirm launchers: the permission is
  // not reachable from Tcl.
  CHECK(l.tcl.bridge().evaluate("tcleval([info commands *aunch*][info commands *onfirm*])", {}).empty());
}

TEST_CASE("a confirmed launcher still runs in the sandbox", sec::kSandbox) {
  for (const sec::Hostile& hostile : sec::hostile_operations()) {
    INFO(hostile.name);
    Launchers l;
    sec::TempFolder outside;
    const auto result = l.run(1, sec::with_outside(hostile.script, outside.str()));
    CHECK_FALSE(result.ok());
    const auto denials = sec::with_code_prefix(result.diagnostics, "SCRIPT-SANDBOX-");
    REQUIRE(denials.size() == 1);
    CHECK(denials[0].code == hostile.code);
  }
}

// End to end: a hostile symbol value netlisted by `schematic netlist` in a
// session whose project folder is granted (writable) gives `?`, starts no
// process and writes nothing outside the project.
TEST_CASE("schematic netlist: hostile tcleval values give ?", sec::kPermissionModel) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    hefesto::test::SchemSession s;
    s.project().compat.xschem_compat = on;
    sec::TempFolder outside;
    s.folder().write("w/r.sym", kResistor);
    std::string sch = kHeader;
    int n = 0;
    for (const sec::Hostile& hostile : sec::hostile_operations()) {
      const std::string body = sec::with_outside(hostile.script, outside.str());
      sch += "C {r.sym} " + std::to_string(100 * n) + " 0 0 0 {name=R" + std::to_string(n) + " value=\"tcleval([" +
             body + "])\"}\n";
      ++n;
    }
    s.folder().write("w/top.sch", sch);
    REQUIRE(s.schematic({"open", s.folder().str("w/top.sch")}).ok());
    const auto result = s.schematic({"netlist"});
    REQUIRE(result.ok());
    const std::string text = result.value.to_string();
    for (int i = 0; i < n; ++i) {
      INFO(sec::hostile_operations()[static_cast<std::size_t>(i)].name);
      const std::string head = "\nR" + std::to_string(i) + " ";
      const std::size_t at = text.find(head);
      REQUIRE(at != std::string::npos);
      const std::string line = text.substr(at + 1, text.find('\n', at + 1) - at - 1);
      CHECK(line.ends_with(" ?"));
    }
    CHECK_FALSE(std::filesystem::exists(outside.path() / "written"));
    CHECK_FALSE(std::filesystem::exists(outside.path() / "made"));
  }
}
