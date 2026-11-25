// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Simulation launchers (openspec/changes/m8-schematic task 11.1, design D13:
// confirmation and sandboxed run; `sim` calls answer CORE-NOTIMPL until M9;
// capability schematic-simulation-integration "Simulation launchers"): the
// launcher runs in the project's sandboxed interpreter after one
// confirmation per document, `xschem` commands map to Hefesto's, an
// unsupported or not yet available one is skipped with SCHEM-SIM-003 and the
// rest of the script runs, and changes to `sim(...)` are ignored.
//
// Owner: SIMINT.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "../cmd/cmd_fixture.hpp"
#include "hefesto/schem/tcl/permission.hpp"
#include "sim_support.hpp"

namespace schem = hefesto::schem;
using hefesto::test::SchemSession;

#define LAUNCHERS "[cap:schematic-simulation-integration][req:Simulation launchers]"

namespace {

struct Launcher {
  SchemSession s;
  schem::LauncherPermission permission;
  int asked = 0;
  schem::ConfirmLauncher ask = [this](std::string_view) { return ++asked, true; };

  hefesto::cmd::CommandResult run(std::string_view tclcommand) {
    return schem::run_launcher(*s.editor().script.interp, permission, 1, tclcommand, ask);
  }
  std::string value(const std::string& script) { return s.tcl(script).value.to_string(); }
};

std::size_t count_code(const hefesto::cmd::CommandResult& result, std::string_view code) {
  std::size_t n = 0;
  for (const auto& d : result.diagnostics) n += d.code == code ? 1 : 0;
  return n;
}

}  // namespace

TEST_CASE("an unsupported xschem command is reported and the launcher goes on", LAUNCHERS) {
  Launcher l;
  const auto result = l.run("xschem no_such_command 1; set ::after 1");
  REQUIRE(result.ok());
  REQUIRE(count_code(result, "SCHEM-SIM-003") == 1);
  CHECK(result.diagnostics.back().text.find("xschem command 'no_such_command' is not supported") !=
        std::string::npos);
  CHECK(l.value("set ::after") == "1");
  CHECK(l.asked == 1);
}

// `sim` calls answer CORE-NOTIMPL until M9: the launcher of IHP's "OP
// annotate" (`xschem annotate_op`), `xschem raw_read` and `simulate` are
// skipped with their message, the rest runs.
TEST_CASE("simulation commands are skipped until M9", LAUNCHERS) {
  Launcher l;
  l.s.folder().write("w/tb.sch", "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n");
  REQUIRE(l.s.schematic({"open", l.s.folder().str("w/tb.sch")}).ok());
  const auto result = l.run("xschem raw_read dc.raw dc; xschem annotate_op; simulate; set ::after 2");
  INFO(SchemSession::message(result));
  REQUIRE(result.ok());
  CHECK(count_code(result, "SCHEM-SIM-003") == 3);
  for (const auto& d : result.diagnostics) {
    if (d.code == "SCHEM-SIM-003") CHECK(d.text.find("not implemented yet") != std::string::npos);
  }
  CHECK(l.value("set ::after") == "2");
}

TEST_CASE("xschem netlist runs schematic netlist", LAUNCHERS) {
  Launcher l;
  hefesto::test::sim::copy_devices(l.s.folder().path() / "w", {"res.sym", "lab_pin.sym"});
  l.s.folder().write("w/top.sch", "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n"
                                  "C {res.sym} 0 0 0 0 {name=R1 value=1k}\n");
  REQUIRE(l.s.schematic({"open", l.s.folder().str("w/top.sch")}).ok());
  const auto result = l.run("set ::netlisted [xschem netlist]");
  INFO(SchemSession::message(result));
  REQUIRE(result.ok());
  CHECK(count_code(result, "SCHEM-SIM-003") == 0);
  // The `xschem` command is the project's own again afterwards.
  CHECK(l.value("namespace exists ::hefesto_launcher") == "0");
  CHECK_FALSE(l.s.tcl("xschem no_such_command").ok());
}

TEST_CASE("changes to the simulator commands are ignored", LAUNCHERS) {
  Launcher l;
  REQUIRE(l.s.tcl("set ::sim(spice,0,cmd) {ngspice {$N}}").ok());
  const auto result = l.run("set ::sim(spice,0,cmd) {xterm -e ngspice}; set ::sim(spice,1,cmd) other");
  REQUIRE(result.ok());
  CHECK(l.value("set ::sim(spice,0,cmd)") == "ngspice {$N}");
  CHECK(l.value("info exists ::sim(spice,1,cmd)") == "0");
}

TEST_CASE("an error the launcher does not expect still stops it", LAUNCHERS) {
  Launcher l;
  const auto result = l.run("error boom; set ::after 3");
  CHECK_FALSE(result.ok());
  CHECK(l.value("info exists ::after") == "0");
  // The mapping is gone after a failed launcher too.
  CHECK(l.value("info procs ::simulate").empty());
}
