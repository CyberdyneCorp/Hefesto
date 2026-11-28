// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What the `schematic` and `xschem` commands read from the session (M8
// round 3): netlisting through the session's environment, so `tcleval`
// values are evaluated (capability schematic-symbols-and-libraries "tcleval
// values"); `xschem translate` (text substitution tokens); `abs_sym_path`
// and `rel_sym_path` in the project interpreter (Q-SCHEM-30); the session's
// bus expansion budget (Q-SCHEM-41); and golden case SCHEM-PR-03 netlisted
// through `schematic netlist`.
//
// Owner: CMD.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "cmd_fixture.hpp"
#include "hefesto/schem/net/bus.hpp"

namespace schem = hefesto::schem;
using hefesto::test::SchemSession;

namespace {

constexpr const char* kTcleval = "[cap:schematic-symbols-and-libraries][req:tcleval values]";
constexpr const char* kXschem = "[cap:schematic-command][req:xschem compatibility command]";
constexpr const char* kReferences = "[cap:schematic-symbols-and-libraries][req:Symbol references]";
constexpr const char* kBus = "[cap:schematic-netlisting][req:Bus and vector notation]";
constexpr const char* kPinOrder = "[cap:schematic-symbols-and-libraries][req:Pin order]";

constexpr const char* kHeader = "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n";

// A two-pin resistor symbol whose element is `@name @pinlist @value`.
constexpr const char* kResistor =
    "v {xschem version=3.4.8RC file_version=1.3}\nG {}\n"
    "K {type=resistor\nformat=\"@name @pinlist @value\"\ntemplate=\"name=R1 value=1k\"}\nV {}\nS {}\nE {}\n"
    "B 5 -2.5 -32.5 2.5 -27.5 {name=P dir=inout}\nB 5 -2.5 27.5 2.5 32.5 {name=M dir=inout}\n";

std::string caught(SchemSession& s, const std::string& script) {
  const auto result = s.tcl("list [catch {" + script + "} msg] $msg");
  REQUIRE(result.ok());
  return result.value.to_string();
}

// The netlist line that starts with `head`, without its LF; empty if none.
std::string line_of(const std::string& text, const std::string& head) {
  const std::size_t found = text.find('\n' + head);
  if (found == std::string::npos) return {};
  const std::size_t at = found + 1;
  return text.substr(at, text.find('\n', at) - at);
}

std::string replace_all(std::string text, const std::string& from, const std::string& to) {
  for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
    text.replace(at, from.size(), to);
  }
  return text;
}

}  // namespace

// ---- tcleval values through `schematic netlist` -------------------------------

// "IHP ev7 expression": the value `A=[ev7 \{ 2e-6 * 5e-6 \} ]` of an instance
// is evaluated in the sandboxed interpreter while netlisting, so the element
// reads `A=1e-11`; before round 3 the run had no Tcl bridge and wrote `?`.
TEST_CASE("schematic netlist evaluates tcleval values in the session's interpreter", kTcleval) {
  SchemSession s;
  s.folder().write("w/r.sym", kResistor);
  s.folder().write("w/top.sch", std::string{kHeader} +
                                    "C {r.sym} 0 0 0 0 {name=R1 value=\"tcleval(A=[ev7 \\{ 2e-6 * 5e-6 \\} ])\"}\n");
  REQUIRE(s.schematic({"open", s.folder().str("w/top.sch")}).ok());
  const auto result = s.schematic({"netlist"});
  REQUIRE(result.ok());
  const std::string line = line_of(result.value.to_string(), "R1 ");
  CHECK(line.ends_with(" A=1e-11"));
}

TEST_CASE("schematic netlist: a failed tcleval value gives ?", kTcleval) {
  SchemSession s;
  s.folder().write("w/r.sym", kResistor);
  s.folder().write("w/top.sch", std::string{kHeader} +
                                    "C {r.sym} 0 0 0 0 {name=R2 value=\"tcleval([no_such_command])\"}\n");
  REQUIRE(s.schematic({"open", s.folder().str("w/top.sch")}).ok());
  const auto result = s.schematic({"netlist"});
  REQUIRE(result.ok());
  const std::string line = line_of(result.value.to_string(), "R2 ");
  CHECK(line.ends_with(" ?"));
}

// The procedures an xschemrc defines are there for tcleval, as the PDK
// symbols need ("tcleval values": the variables and procedures of the
// active PDK's xschemrc).
TEST_CASE("schematic netlist: tcleval sees the procedures of the document's xschemrc", kTcleval) {
  SchemSession s;
  s.folder().write("w/xschemrc", "proc my_width {} { return 42 }\nset MY_MODEL nfet\n");
  s.folder().write("w/r.sym", kResistor);
  s.folder().write("w/top.sch", std::string{kHeader} +
                                    "C {r.sym} 0 0 0 0 {name=R3 value=\"tcleval(W=[my_width]$MY_MODEL)\"}\n");
  REQUIRE(s.schematic({"open", s.folder().str("w/top.sch")}).ok());
  const auto result = s.schematic({"netlist"});
  REQUIRE(result.ok());
  CHECK(line_of(result.value.to_string(), "R3 ").ends_with(" W=42nfet"));
}

// Regression (review of PR #88), Q-SCHEM-38 (a): netlisting evaluated the
// xschemrc under the project's sandbox policy, whose `env` has no HOME or
// PWD, so an rc reading `$env(HOME)` stopped at that line and the procedure
// defined after it was missing (the value gave `?`). The rc now runs under
// the policy it runs under when the document is opened: `env(HOME)` and
// `env(PWD)` are the document's folder.
TEST_CASE("schematic netlist: the xschemrc sees the synthetic env(HOME) and env(PWD)", kTcleval) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    SchemSession s;
    s.project().compat.xschem_compat = on;
    s.folder().write("w/xschemrc", "set H [lindex [split $env(HOME) /] end]\n"
                                   "set P [lindex [split $env(PWD) /] end]\n"
                                   "proc my_width {} { return 42 }\n");
    s.folder().write("w/r.sym", kResistor);
    s.folder().write("w/top.sch", std::string{kHeader} +
                                      "C {r.sym} 0 0 0 0 {name=R4 value=\"tcleval(W=[my_width]$H$P)\"}\n");
    REQUIRE(s.schematic({"open", s.folder().str("w/top.sch")}).ok());
    const auto result = s.schematic({"netlist"});
    REQUIRE(result.ok());
    CHECK(line_of(result.value.to_string(), "R4 ").ends_with(" W=42ww"));
  }
}

// ---- xschem translate --------------------------------------------------------

// "Token values from xschem translate" and the scenarios around it, on
// instances addressed by index as SCHEM-PR-04 does.
TEST_CASE("xschem translate substitutes the tokens of a text for an instance", kXschem) {
  SchemSession s;
  s.folder().write("w/res.sym", kResistor);
  s.folder().write("w/top.sch", std::string{kHeader} +
                                    "C {res.sym} 0 0 0 0 {name=R7 value=4.7k}\n"
                                    "C {res.sym} 100 0 0 0 {name=X1 value=1}\n"
                                    "C {res.sym} 200 0 0 0 {name=M1 W=2 L=1 value=2}\n");
  REQUIRE(s.schematic({"open", s.folder().str("w/top.sch")}).ok());
  CHECK(caught(s, "xschem translate 0 {@name @value}") == "0 {R7 4.7k}");
  CHECK(caught(s, "xschem translate 0 {@name=@value}") == "0 4.7k");
  CHECK(caught(s, "xschem translate 1 {a\\@b @name}") == "0 {a@b X1}");
  CHECK(caught(s, "xschem translate 2 {@W/@L}") == "0 1");
  CHECK(caught(s, "xschem translate 2 {@W @L}") == "0 {2 1}");
  CHECK(caught(s, "xschem translate 0 %undef") == "0 undef");
  CHECK(caught(s, "xschem translate 0 @undef") == "0 {}");
  CHECK(caught(s, "xschem translate 0 @symname") == "0 res");
  CHECK(caught(s, "xschem translate 0 @symname_ext") == "0 res.sym");
  CHECK(caught(s, "xschem translate 0 @#1:name") == "0 M");
  // `-1` substitutes with no instance: the text unchanged (SCHEM-PR-04).
  CHECK(caught(s, "xschem translate -1 {@name %undef}") == "0 {@name %undef}");
  // An instance by name, as getprop addresses it.
  CHECK(caught(s, "xschem translate X1 @value") == "0 1");
  // Regression (M8 round 6 review): any other instance that does not exist
  // gave the text unchanged; it raises "instance not found" (both modes).
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    s.project().compat.xschem_compat = on;
    CHECK(caught(s, "xschem translate 999 {@name}") == "1 {xschem translate: instance not found}");
    CHECK(caught(s, "xschem translate R9 {@name}") == "1 {xschem translate: instance not found}");
  }
  CHECK(caught(s, "xschem translate 0") == "1 {xschem translate: wrong number of arguments}");
}

// Regression (review of PR #88): `xschem translate` read the instance's
// symbol with xschem-compat ON whatever the project said, through its own
// local file system, and left it in the session's cache for later netlists.
// It now reads with the project's setting and file services. The symbol is
// a schematic used as a symbol whose file has no `v` record and starts with
// `C {ipin}`: OFF reads it as version 1.0 (`ipin.sym`, so pin A is
// generated), ON leaves the reference as written (Q-SCHEM-3), which names
// no file, so the symbol has no pin.
TEST_CASE("xschem translate reads the symbol with the project's xschem-compat", kXschem) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    SchemSession s;
    s.project().compat.xschem_compat = on;
    s.folder().write("w/ipin.sym", "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {type=ipin}\nV {}\nS {}\n"
                                   "E {}\nB 5 -2.5 -2.5 2.5 2.5 {name=p dir=in}\n");
    s.folder().write("w/cell.sch", "C {ipin} 0 0 0 0 {name=p1 lab=A}\n");
    s.folder().write("w/top.sch", std::string{kHeader} + "C {cell.sch} 0 0 0 0 {name=x1}\n");
    REQUIRE(s.schematic({"open", s.folder().str("w/top.sch")}).ok());
    CHECK(caught(s, "xschem translate 0 {pin @#0:name}") == (on ? "0 {pin }" : "0 {pin A}"));
  }
}

// ---- abs_sym_path and rel_sym_path (SCHEM-PR-05's tree) ----------------------

// Q-SCHEM-30 (fix, both modes): `./` and `../` resolve against the folder of
// the active schematic, never the start folder; `rel_sym_path` gives the
// reference Hefesto writes, in the alias form of its search-path folder.
TEST_CASE("abs_sym_path and rel_sym_path resolve against the active schematic", kReferences) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    SchemSession s;
    s.project().compat.xschem_compat = on;
    const std::string symbol = "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n";
    for (const char* file : {"input/x.sym", "input/libA/a.sym", "input/libB/x.sym", "input/sub/x.sym"}) {
      s.folder().write(file, symbol);
    }
    s.folder().write("input/sub/top.sch", std::string{kHeader} + "C {../x.sym} 0 0 0 0 {name=x1}\n");
    REQUIRE(s.schematic({"open", s.folder().str("input/sub/top.sch")}).ok());
    const std::string in = s.folder().str("input");
    REQUIRE(s.schematic({"path", "set", in + "/libA:" + in + "/missing:" + in + "/libB | lib:."}).ok());
    CHECK(caught(s, "abs_sym_path lib/x.sym") == "0 " + in + "/libB/x.sym");
    CHECK(caught(s, "abs_sym_path ../x.sym") == "0 " + in + "/x.sym");
    CHECK(caught(s, "abs_sym_path ./x.sym") == "0 " + in + "/sub/x.sym");
    CHECK(caught(s, "abs_sym_path a.sym") == "0 " + in + "/libA/a.sym");
    CHECK(caught(s, "abs_sym_path x.sym") == "0 " + in + "/libB/x.sym");
    CHECK(caught(s, "abs_sym_path lib/missing.sym") == "0 " + in + "/libB/missing.sym");
    CHECK(caught(s, "rel_sym_path [abs_sym_path x.sym]") == "0 lib/x.sym");
    CHECK(caught(s, "rel_sym_path [abs_sym_path a.sym]") == "0 a.sym");
    CHECK(caught(s, "rel_sym_path [abs_sym_path lib/missing.sym]") == "0 lib/missing.sym");
    CHECK(caught(s, "abs_sym_path") == "1 {wrong # args: should be \"abs_sym_path reference\"}");
  }
}

// Regression (round-7 cross-platform determinism, SCHEM-PR-05 stdout): an
// absolute reference is returned as written. libstdc++ folded `//lib//./x.sym`
// to `/lib/./x.sym` where libc++ kept it, so Linux and macOS printed different
// paths; xschem prints the reference unchanged.
TEST_CASE("abs_sym_path returns an absolute reference as written", kReferences) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    SchemSession s;
    s.project().compat.xschem_compat = on;
    CHECK(caught(s, "abs_sym_path //lib//./x.sym") == "0 //lib//./x.sym");
    CHECK(caught(s, "abs_sym_path /lib/x.sym") == "0 /lib/x.sym");
  }
}

// Regression (review of PR #88), both modes: with an unsaved document the
// procedures resolved `./x.sym` and the fallback against the folder Hefesto
// was started in. An unsaved document has no folder, so a reference that
// needs one is returned unchanged, as the symbol cache leaves it missing
// (Q-SCHEM-30, Q-SCHEM-38 (a)).
TEST_CASE("abs_sym_path of an unsaved document never uses the start folder", kReferences) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    SchemSession s;
    s.project().compat.xschem_compat = on;
    REQUIRE(s.schematic({"new", "schematic"}).ok());
    const std::string lib = s.folder().str("lib");
    s.folder().write("lib/a.sym", kResistor);
    REQUIRE(s.schematic({"path", "set", lib}).ok());
    CHECK(caught(s, "abs_sym_path ./x.sym") == "0 ./x.sym");
    CHECK(caught(s, "abs_sym_path ../x.sym") == "0 ../x.sym");
    CHECK(caught(s, "abs_sym_path missing.sym") == "0 missing.sym");
    CHECK(caught(s, "abs_sym_path a.sym") == "0 " + lib + "/a.sym");
  }
}

// ---- the session's bus budget (Q-SCHEM-41) -----------------------------------

// User decision M8-D3 (2026-11-22), both modes: the `xschem` expansions of
// an editing session share one budget of 64 x kMaxBusBytes; past it a name
// is used unchanged with width 1, like a name over kMaxBusBytes.
TEST_CASE("Total bus expansion limit of an editing session", kBus) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    SchemSession s;
    s.project().compat.xschem_compat = on;
    CHECK(s.schem().bus_budget.remaining() == schem::kMaxBusTotalBytes);
    s.schem().bus_budget = schem::BusBudget{30};
    CHECK(caught(s, "xschem expandlabel A\\[3:0\\]") == "0 {{A[3],A[2],A[1],A[0]} 4}");
    CHECK(s.schem().bus_budget.remaining() == 11);
    CHECK(caught(s, "xschem expandlabel B\\[3:0\\]") == "0 {{B[3:0]} 1}");
    CHECK(caught(s, "xschem expandlabel plain") == "0 {plain 1}");
    CHECK(s.schem().bus_budget.remaining() == 11);
  }
}

// ---- golden case SCHEM-PR-03 through `schematic netlist` ---------------------

// The SCHEM-PR-03 inputs, with the netlisting test devices as the system
// symbol folder ahead of the input folder (what the case's xschemrc sets up
// for xschem), netlisted by `schematic netlist`, give the captured
// pr03.spice once the input folder is written as /work/input.
TEST_CASE("SCHEM-PR-03 netlisted through schematic netlist", kPinOrder) {
  const std::filesystem::path source{HEFESTO_SOURCE_DIR};
  const std::filesystem::path golden = source / "tests/golden/SCHEM/cases/SCHEM-PR-03";
  SchemSession s;
  std::filesystem::copy(source / "tests/data/schem/net/devices", s.folder().path() / "devices");
  std::filesystem::copy(golden / "input", s.folder().path() / "input", std::filesystem::copy_options::recursive);
  std::filesystem::create_directories(s.folder().path() / "out");
  const std::string input = s.folder().str("input");
  REQUIRE(s.schematic({"open", input + "/pr03.sch"}).ok());
  REQUIRE(s.schematic({"path", "set", s.folder().str("devices") + ":" + input}).ok());
  const auto result = s.schematic({"netlist", s.folder().str("out/pr03.spice")});
  INFO(SchemSession::message(result));
  REQUIRE(result.ok());
  const std::string expected = [&] {
    std::ifstream in(golden / "expected/pr03.spice", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
  }();
  CHECK(replace_all(s.folder().read("out/pr03.spice"), input, "/work/input") == expected);
}

// ---- Q-SCHEM-52: the default search path without an xschemrc ------------------

namespace {

constexpr const char* kSearchPath = "[cap:schematic-symbols-and-libraries][req:Library search path]";

// A system library holding res.sym under the session folder, which stands
// for the installation root (CAD_ROOT); returns its devices folder.
std::filesystem::path install_devices(SchemSession& s) {
  const std::filesystem::path devices = s.folder().path() / "assets/schematic/xschem_library/devices";
  std::filesystem::create_directories(devices);
  std::filesystem::copy_file(
      std::filesystem::path{HEFESTO_SOURCE_DIR} / "assets/schematic/xschem_library/devices/res.sym",
      devices / "res.sym");
  s.editor().script.sandbox.environment["CAD_ROOT"] = s.folder().str();
  return devices;
}

}  // namespace

// Regression (M8 round-7 integration): a document in a folder with no
// xschemrc kept the session's earlier (empty) search path, so `res.sym` was
// not found; Q-SCHEM-52 (a) makes it the system devices library.
TEST_CASE("Default search path without an xschemrc", kSearchPath) {
  for (const bool on : {true, false}) {
    SchemSession s;
    s.project().compat.xschem_compat = on;
    const std::filesystem::path devices = install_devices(s);
    std::filesystem::create_directories(s.folder().path() / "w/s");
    REQUIRE(s.schematic({"new", "schematic", s.folder().str("w/s/div.sch")}).ok());
    const auto listed = s.schematic({"path", "list"});
    REQUIRE(listed.ok());
    CHECK(listed.value.to_string() == devices.generic_string());
    const auto resolved = s.schematic({"resolve", "res.sym"});
    REQUIRE(resolved.ok());
    CHECK(resolved.value.to_string() == (devices / "res.sym").generic_string());
  }
}

// Scenario "Script reads the default search path" (Q-SCHEM-52): with no
// xschemrc the project interpreter still gets XSCHEM_LIBRARY_PATH, the
// system devices library, so a script that reads it raises nothing.
TEST_CASE("Script reads the default search path", kSearchPath) {
  for (const bool on : {true, false}) {
    CAPTURE(on);
    SchemSession s;
    s.project().compat.xschem_compat = on;
    const std::filesystem::path devices = install_devices(s);
    std::filesystem::create_directories(s.folder().path() / "w/s");
    REQUIRE(s.schematic({"new", "schematic", s.folder().str("w/s/div.sch")}).ok());
    CHECK(caught(s, "set XSCHEM_LIBRARY_PATH") == "0 " + devices.generic_string());
  }
}

// `xschem clear` makes an untitled document: a session with no search path
// yet takes the working folder's (the system library when it has no
// xschemrc), as SCHEM-PR-02 needs.
TEST_CASE("Default search path of an untitled document", kSearchPath) {
  SchemSession s;
  const std::filesystem::path devices = install_devices(s);
  REQUIRE(s.schematic({"new", "schematic"}).ok());
  const auto resolved = s.schematic({"resolve", "res.sym"});
  REQUIRE(resolved.ok());
  CHECK(resolved.value.to_string() == (devices / "res.sym").generic_string());
}
