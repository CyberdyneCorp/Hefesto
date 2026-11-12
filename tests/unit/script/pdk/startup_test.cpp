// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// scripting-pdk-integration: the script-host command line (Q-SCRIPT-37),
// the command-line file arguments and the startup files as scripts
// (tasks.md 6.1). Owner: PDKINT.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/script/pdk/consent.hpp"
#include "hefesto/script/pdk/startup.hpp"
#include "pdk_fixture.hpp"

namespace script = hefesto::script;
using script::test::PdkSession;
using script::test::TempDir;
using script::test::write_file;

namespace {

const script::ScriptHostInfo kInfo{"8.3.694", "fc5daa9e", "/usr/local/lib"};

script::ScriptHostCommandLine classify(std::vector<std::string> args) {
  return script::classify_command_line(args, kInfo);
}

std::vector<std::string> texts(const std::vector<hefesto::diag::Diagnostic>& diagnostics) {
  std::vector<std::string> out;
  for (const auto& d : diagnostics) {
    out.push_back(d.text);
  }
  return out;
}

}  // namespace

// ---- Command-line options in the script host --------------------------------

TEST_CASE("the script-host table keeps Magic's order", "[cap:scripting-pdk-integration][req:Command-line options in the script host]") {
  const auto table = script::command_line_table();
  REQUIRE(table.size() == 15);
  CHECK(table.front().pattern == "^-now(rap)?(per)?$");
  CHECK(table[1].pattern == "^-dnull");
  CHECK(table[2].pattern == "^-r(e)?(cover)?$");
  CHECK(table[3].pattern == "^-rc(file)?$");
  CHECK(table[9].pattern == "^-F");
  CHECK(table[9].arguments == 2);
  CHECK(table.back().pattern == "^-n*");
}

TEST_CASE("PDK rcfile invocation", "[cap:scripting-pdk-integration][req:Command-line options in the script host]") {
  // `magic -dnull -noconsole -rcfile ... run.tcl` after the launcher took -noconsole.
  const auto line = classify({"-dnull", "-rcfile", "/pdk/sky130A/libs.tech/magic/sky130A.magicrc", "run.tcl"});
  CHECK_FALSE(line.wrapper);
  CHECK_FALSE(line.recover);
  CHECK(line.engine_args ==
        std::vector<std::string>{"-dnull", "-rcfile", "/pdk/sky130A/libs.tech/magic/sky130A.magicrc", "run.tcl"});
  CHECK(line.files == std::vector<std::string>{"run.tcl"});
}

TEST_CASE("the script-host table's accidental spellings are replicated (Q-SCRIPT-37)",
          "[cap:scripting-pdk-integration][req:Command-line options in the script host]") {
  CHECK(classify({"-rcover"}).recover);           // `-rcover` is recovery, not -rcfile
  CHECK(classify({"-recover"}).recover);
  CHECK_FALSE(classify({"-rcfile", "x"}).recover);
  CHECK_FALSE(classify({"-dnullx"}).wrapper);      // `-dnullx` disables the wrapper like -dnull
  CHECK_FALSE(classify({"-nowrapper"}).wrapper);
  CHECK(classify({"-nowrapper"}).engine_args.empty());
  const auto unknown = classify({"-x", "cell"});   // `^-n*` passes every unknown option through
  CHECK(unknown.engine_args == std::vector<std::string>{"-x", "cell"});
  CHECK(unknown.files == std::vector<std::string>{"cell"});
  CHECK(classify({"-display", "NULL"}).engine_args == std::vector<std::string>{"-display", "NULL"});  // `^-d`
  CHECK(classify({"-F", "a", "b", "c"}).files == std::vector<std::string>{"c"});
  CHECK(classify({"-T"}).engine_args == std::vector<std::string>{"-T", ""});  // lindex past the end
  CHECK(classify({"-T", "sky130A", "--version", "late"}).print_and_exit == "8.3.694");
  CHECK(classify({"--commit"}).print_and_exit == "fc5daa9e");
  CHECK(classify({"--prefix"}).print_and_exit == "/usr/local/lib");
  CHECK(classify({"-D"}).engine_args == std::vector<std::string>{"-D"});
}

// ---- Command-line file arguments ---------------------------------------------

// NULL graphics: Magic's C startup loads the file arguments (utils/main.c
// mainInitFinal), not magic.tcl.in's NULL-graphics loop, which never runs
// (user decision 2026-11-11). Texts checked with Magic 8.3.694
// `magic -dnull -noconsole <f>` in hefesto-magic-only:local.
TEST_CASE("Batch script argument", "[cap:scripting-pdk-integration][req:Command-line file arguments]") {
  const auto entry = script::script_host_file_entry("drc.tcl", false);
  CHECK(entry.command == "source drc.tcl");
  CHECK(texts(entry.messages) == std::vector<std::string>{"Loading \"drc.tcl\" from command line."});
  CHECK(entry.messages.front().code == "SCRIPT-PDK-006");
}

TEST_CASE("NULL graphics prints no handling file entry line",
          "[cap:scripting-pdk-integration][req:Command-line file arguments]") {
  // Regression: the NULL-graphics path printed magic.tcl.in's
  // `handling file entry <f> extension <ext>`, which Magic -dnull never prints.
  for (const char* file : {"drc.tcl", "inv", "inv.mag", "a.gds.gz", "a.lef", "a.cif", "x.weird"}) {
    INFO(file);
    const auto entry = script::script_host_file_entry(file, false);
    REQUIRE(entry.messages.size() == 1);
    CHECK(entry.messages.front().text.starts_with("Loading \""));
    CHECK(entry.messages.front().text.find("handling file entry") == std::string::npos);
    CHECK_FALSE(entry.command.empty());  // every file has a reader; SCRIPT-PDK-007 is wrapper-only
  }
}

TEST_CASE("NULL graphics loads file arguments as Magic's C startup does",
          "[cap:scripting-pdk-integration][req:Command-line file arguments]") {
  const auto load = [](const char* file) { return script::script_host_file_entry(file, false); };
  // A cell: `.mag` stripped from the announced and loaded name.
  CHECK(load("inv").command == "load inv");
  CHECK(texts(load("inv").messages) == std::vector<std::string>{"Loading \"inv\" from command line."});
  CHECK(load("inv.mag").command == "load inv");
  CHECK(texts(load("inv.mag").messages) == std::vector<std::string>{"Loading \"inv\" from command line."});
  // `.gz` is not a type: a cell named after the whole argument (Magic loads `a.gds`).
  CHECK(load("a.gds.gz").command == "load a.gds.gz");
  CHECK(texts(load("a.gds.gz").messages) == std::vector<std::string>{"Loading \"a.gds.gz\" from command line."});
  CHECK(load("x.weird").command == "load x.weird");
  CHECK(load("dir.v2/cell").command == "load dir.v2/cell");
  // LEF and DEF by extension, case-insensitively; the type is taken after `.mag` is stripped.
  CHECK(load("a.lef").command == "lef read a.lef");
  CHECK(texts(load("a.lef").messages) == std::vector<std::string>{"Loading \"a.lef\" from command line."});
  CHECK(load("a.DEF").command == "def read a.DEF");
  CHECK(load("a.lef.mag").command == "lef read a.lef");
  CHECK(load("RUN.TCL").command == "source RUN.TCL");
  // Q-UI-25 (fix, app-startup): CIF and GDS are read and only exact
  // extensions select a type. Magic announces `a.cif` and ignores it, and
  // takes any `.gd...` extension as GDS.
  CHECK(load("a.cif").command == "cif read a.cif");
  CHECK(texts(load("a.cif").messages) == std::vector<std::string>{"Loading \"a.cif\" from command line."});
  CHECK(load("a.GDS").command == "gds read a.GDS");
  CHECK(load("a.gdsii").command == "load a.gdsii");
}

TEST_CASE("wrapper mode announces extensions and reads compressed files",
          "[cap:scripting-pdk-integration][req:Command-line file arguments]") {
  CHECK(script::script_host_file_entry("inv", true).messages.empty());
  CHECK(texts(script::script_host_file_entry("a.lef", true).messages) ==
        std::vector<std::string>{"Handling file entry a.lef extension .lef"});
  const auto gds = script::script_host_file_entry("a.gds.gz", true);
  CHECK(gds.command == "gds read a.gds.gz");
  CHECK(texts(gds.messages) == std::vector<std::string>{"Handling compressed entry a.gds.gz extension .gds"});
  CHECK(script::script_host_file_entry("a.mag.gz", true).command == "magic::load a.mag.gz");
  const auto other = script::script_host_file_entry("a.txt.gz", true);
  CHECK(other.command.empty());
  CHECK(texts(other.messages).back() == "only compressed .mag and .gds are handled.");
  CHECK(texts(script::script_host_file_entry("notes.txt", true).messages).back() ==
        "don't know how to load file notes.txt");
}

// ---- Startup files are scripts -----------------------------------------------

TEST_CASE("PDK overrides power names", "[cap:scripting-pdk-integration][req:Startup files are scripts]") {
  PdkSession s;
  TempDir dir;
  write_file(dir.path() / "sys.magicrc", "set GND gnd!\nset VDD vdd!\n");
  write_file(dir.path() / ".magicrc", "if {1} {set VDD VPWR}\nproc pick {} {return VGND}\nset GND [pick]\n");
  script::StartupPlan plan;
  plan.files = {{dir.path() / "sys.magicrc", script::StartupKind::system, {}, false, std::nullopt},
                {dir.path() / ".magicrc", script::StartupKind::local, ".magicrc", false, std::nullopt}};
  const auto run = script::run_startup_files(s.editor, plan);
  REQUIRE(run.ok());
  CHECK(run.diagnostics.empty());
  CHECK(s.eval("set VDD") == "VPWR");
  CHECK(s.eval("set GND") == "VGND");
}

TEST_CASE("Error in rc file", "[cap:scripting-pdk-integration][req:Startup files are scripts]") {
  PdkSession s;
  TempDir dir;
  write_file(dir.path() / "home.magicrc", "set a 1\nset b 2\nfrobnicate\nset c 3\n");
  write_file(dir.path() / "local.magicrc", "set d 4\n");
  script::StartupPlan plan;
  plan.files = {{dir.path() / "home.magicrc", script::StartupKind::user, ".magicrc", false, std::nullopt},
                {dir.path() / "local.magicrc", script::StartupKind::local, ".magicrc", false, std::nullopt}};
  std::vector<std::string> after;
  plan.after_file = [&after](const script::StartupFile& file) { after.push_back(file.path.filename().string()); };
  const auto run = script::run_startup_files(s.editor, plan);
  REQUIRE(run.ok());
  CHECK(s.eval("info exists b") == "1");
  CHECK(s.eval("info exists c") == "0");  // lines after the error do not run
  CHECK(s.eval("set d") == "4");          // the next file still runs
  CHECK(after == std::vector<std::string>{"home.magicrc", "local.magicrc"});
  REQUIRE(run.diagnostics.size() == 1);
  CHECK(run.diagnostics.front().code == "SCRIPT-PDK-001");
  CHECK(run.diagnostics.front().text == R"(Error parsing user ".magicrc": invalid command name "frobnicate")");
  REQUIRE(run.value.data().index() == 4);  // the files that ran without an error
  CHECK(run.value.to_string() == (dir.path() / "local.magicrc").string());
}

TEST_CASE("startup errors use Magic's text for each kind of file",
          "[cap:scripting-pdk-integration][req:Startup files are scripts]") {
  using K = script::StartupKind;
  const auto error = [](K kind, std::string name, bool legacy) {
    return texts(script::startup_error({"/cad/magic/sys/.magicrc", kind, std::move(name), legacy, std::nullopt}, "oops"));
  };
  CHECK(error(K::pre, "", false) == std::vector<std::string>{R"(Error parsing pre-startup file "/cad/magic/sys/.magicrc": oops)"});
  CHECK(error(K::system, "", false) ==
        std::vector<std::string>{R"(Error parsing system startup file "/cad/magic/sys/.magicrc": oops)"});
  CHECK(error(K::user, ".magic", true) == std::vector<std::string>{R"(Error parsing user ".magic": oops)"});
  CHECK(error(K::local, ".magicrc", false) ==
        std::vector<std::string>{R"(Error parsing ".magicrc": oops)",
                                 R"(Bad local startup file ".magicrc", continuing without.)"});
  CHECK(error(K::local, "magic_setup", true) == std::vector<std::string>{R"(Error parsing local "magic_setup": oops)"});
}

TEST_CASE("info script names the startup file while it runs, and its sourced files",
          "[cap:scripting-pdk-integration][req:Startup files are scripts]") {
  PdkSession s;
  TempDir dir;
  write_file(dir.path() / "inner.tcl", "set inner [info script]\n");
  write_file(dir.path() / "rc", "set outer [info script]\nsource [file join [file dirname [info script]] inner.tcl]\n"
                                "set after [info script]\n");
  script::StartupPlan plan;
  plan.files = {{dir.path() / "rc", script::StartupKind::local, "rc", false, std::nullopt}};
  REQUIRE(script::run_startup_files(s.editor, plan).ok());
  CHECK(s.eval("set outer") == (dir.path() / "rc").string());
  CHECK(s.eval("set inner") == (dir.path() / "inner.tcl").string());
  CHECK(s.eval("set after") == (dir.path() / "rc").string());  // back to the outer file
  CHECK(s.eval("info script").empty());
}

TEST_CASE("startup stops after a file requests quit", "[cap:scripting-pdk-integration][req:Script-host startup sequence]") {
  PdkSession s;
  TempDir dir;
  write_file(dir.path() / "a", "set a 1\n");
  write_file(dir.path() / "b", "set b 1\n");
  script::StartupPlan plan;
  plan.files = {{dir.path() / "a", script::StartupKind::user, "a", false, std::nullopt},
                {dir.path() / "b", script::StartupKind::local, "b", false, std::nullopt}};
  plan.stop_requested = [] { return true; };
  REQUIRE(script::run_startup_files(s.editor, plan).ok());
  CHECK(s.eval("info exists b") == "0");
}

TEST_CASE("startup files need the project's interpreter", "[cap:scripting-pdk-integration][req:Startup files are scripts]") {
  hefesto::editor::EditorState editor;
  const auto run = script::run_startup_files(editor, {});
  REQUIRE_FALSE(run.ok());
  CHECK(run.diagnostics.front().code == "SCRIPT-PDK-008");
}

TEST_CASE("a PDK rc file in the startup sequence runs only after consent",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  PdkSession s;
  TempDir dir;
  const auto rc = dir.path() / "ciel/sky130/versions/abc/sky130A/libs.tech/magic/sky130A.magicrc";
  write_file(rc, "set ::pdk_rc ran\n");
  script::TestConsentHost host;  // declines
  script::StartupPlan plan;
  plan.files = {{rc, script::StartupKind::pdk, {}, false, script::pdk_identity_of(rc)}};
  plan.consent = &host;
  auto run = script::run_startup_files(s.editor, plan);
  REQUIRE(run.ok());
  CHECK(s.eval("info exists ::pdk_rc") == "0");
  REQUIRE(run.diagnostics.size() == 1);
  CHECK(run.diagnostics.front().code == "SCRIPT-TRUST-001");
  REQUIRE(host.requests().size() == 1);
  CHECK(host.requests().front().pdk == script::PdkIdentity{"sky130", "sky130A", "abc"});

  // Asked once per version and session: declined again without a prompt
  // until the decline is forgotten ("Review scripts...").
  run = script::run_startup_files(s.editor, plan);
  CHECK(s.eval("info exists ::pdk_rc") == "0");
  CHECK(host.requests().size() == 1);
  s.editor.script.pdk_trust.forget_decline(script::PdkIdentity{"sky130", "sky130A", "abc"});
  host.push_answer(script::ConsentDecision::run_once);
  run = script::run_startup_files(s.editor, plan);
  CHECK(s.eval("set ::pdk_rc") == "ran");
  CHECK(host.requests().size() == 2);
}
