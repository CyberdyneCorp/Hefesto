// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `tcleval` values, the procedures xschem provides and the launcher
// permission (capability schematic-symbols-and-libraries "tcleval values",
// "Script execution permission"; Q-SCHEM-8, Q-SCHEM-25). Expected texts come
// from the spec tables "Expected values of the PDK uses"; the `@<attr>`
// substitution before evaluation is NETLIST's, so the inputs are written in.
//
// Owner: SYMLIB.

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/lib/xschemrc.hpp"
#include "hefesto/schem/tcl/permission.hpp"
#include "hefesto/schem/tcl/procedures.hpp"
#include "hefesto/schem/tcl/tcl_bridge.hpp"
#include "hefesto/schem/tcl/tcl_environment.hpp"

namespace schem = hefesto::schem;
namespace platform = hefesto::platform;
namespace script = hefesto::script;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTcleval = "[cap:schematic-symbols-and-libraries][req:tcleval values]";
constexpr const char* kPermission = "[cap:schematic-symbols-and-libraries][req:Script execution permission]";
constexpr const char* kRc = "[cap:schematic-symbols-and-libraries][req:xschemrc as data]";

class TempDir {
 public:
  TempDir() {
    static std::atomic<int> counter{0};
    path_ = fs::canonical(fs::temp_directory_path()) /
            ("hefesto-tcleval-" + std::to_string(::getpid()) + "-" + std::to_string(counter.fetch_add(1)));
    fs::remove_all(path_);
    fs::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ignored;
    fs::remove_all(path_, ignored);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  [[nodiscard]] const fs::path& path() const noexcept { return path_; }

 private:
  fs::path path_;
};

struct Env {
  hefesto::editor::EditorState editor;
  platform::LocalFileSystem files;
  schem::TclEnvironment tcl;

  explicit Env(script::SandboxPolicy policy = {}, schem::ProcedureHooks hooks = {})
      : tcl(editor, std::move(policy), files, std::move(hooks)) {}

  std::string eval(std::string_view value, const schem::TclContext& context = {}) {
    return tcl.bridge().evaluate(value, context);
  }
};

}  // namespace

TEST_CASE("tcleval steps 1 and 2 give the body", kTcleval) {
  CHECK(schem::tcleval_body("tcleval(abc)") == "abc");
  // Only `\}` is unescaped here; the property-string decoding before it
  // (schematic-file-format "Property strings") already removed escapes.
  CHECK(schem::tcleval_body("tcleval(A=[ev7 \\{ 1 \\} ])  \n") == "A=[ev7 \\{ 1 } ]");
  CHECK(schem::tcleval_body("tcleval(f(x) and g(y) ) tail") == "f(x) and g(y) tail");  // the last `)`
  CHECK(schem::tcleval_body("tcleval(") == "");
}

TEST_CASE("sky130 corner include", kTcleval) {
  Env env;
  REQUIRE(env.tcl.interpreter().evaluate("set ::SKYWATER_MODELS /pdk/sky130A/libs.tech/combined").ok());
  CHECK(env.eval("tcleval(.lib $::SKYWATER_MODELS/sky130.lib.spice tt )") ==
        ".lib /pdk/sky130A/libs.tech/combined/sky130.lib.spice tt ");  // the blank before `)` stays
}

TEST_CASE("IHP ev7 expression", kTcleval) {
  Env env;
  // `A=[ev7 \{ @w * @l \} ]` in the property text decodes to plain braces.
  CHECK(env.eval("tcleval(A=[ev7 { 2e-6 * 5e-6 } ])") == "A=1e-11");
}

TEST_CASE("IHP ptap1 resistance", kTcleval) {
  Env env;
  CHECK(env.eval("tcleval(R=[ev7 { 1.0 / ( 1.0 / ( 9.8e-10 / ( 0.78e-6 * 0.78e-6 ) ) + 1.0 / ( 9.8e-4 / ( 2.0 * "
                 "( 0.78e-6 + 0.78e-6 ) ) ) )}])") == "R=262.8473");
}

TEST_CASE("the PDK uses of ev and ev7 give the expected texts", kTcleval) {
  Env env;
  CHECK(env.eval("tcleval(A=[ev7 { 0.78e-6 * 0.78e-6 } ] P=[ev7 { 2 * ( 0.78e-6 + 0.78e-6 ) } ])") ==
        "A=6.084e-13 P=3.12e-06");
  CHECK(env.eval("tcleval(a=[ev7 { 0.7e-6 * 2.0e-6 }] p=[ev7 { ( 0.7e-6 + 2.0e-6 ) * 2 }] m=[ev { 1 }])") ==
        "a=1.4e-12 p=5.4e-06 m=1");
  CHECK(env.eval("tcleval(m=[ev { 1 * 1 } ])") == "m=1");
  CHECK(env.eval("tcleval(le=[ev { 1.0 * 1.0e-6 } ])") == "le=1e-06");
  CHECK(env.eval("tcleval(le=[ev { 2.5 * 1.0e-6 } ])") == "le=2.5e-06");
  CHECK(env.eval("tcleval(C=[ev {1 * (7.0e-6 * 7.0e-6 * 1.5e-3 + 2*( 7.0e-6 + 7.0e-6 ) * 40e-12)}])") == "C=7.462e-14");
  CHECK(env.eval("tcleval(C=[ev {1 * (10.0e-6 * 10.0e-6 * 1.5e-3 + 2*( 10.0e-6 + 10.0e-6 ) * 40e-12)}])") ==
        "C=1.516e-13");
  CHECK(env.eval("tcleval(C=[ev {1 * ( ((1 == 1) ? 0.372 : 0.305) + max(0, 5 - 1 ) * 0.305 ) * ( 5e-6 * 1e6 ) * "
                 "( 5e-6 * 1e6 ) * 1e-15}])") == "C=3.98e-14");
}

TEST_CASE("ev gives an expression that fails as written", kTcleval) {
  Env env;
  CHECK(env.eval("tcleval([ev @w * 2])") == "@w * 2");
  CHECK(env.eval("tcleval([ev abc])") == "abc");
}

TEST_CASE("Simulator selection in a code block", kTcleval) {
  Env env;
  CHECK(env.eval("tcleval( [if [sim_is_xyce] {return {X}} else {return {N}}] )") == " N ");
  CHECK(env.eval("tcleval([sim_is_ngspice][sim_is_xyce])") == "10");
}

TEST_CASE("a failed evaluation gives ? and LF", kTcleval) {
  Env env;
  CHECK(env.eval("tcleval([no_such_command])") == "?\n");
  CHECK(env.eval("tcleval([error boom])") == "?\n");
  CHECK(schem::TclBridge{}.evaluate("tcleval(x)", {}) == "?\n");
}

TEST_CASE("path, sch_basename, instname and symname are set as globals", kTcleval) {
  Env env;
  const schem::TclContext context{"x1.x2", "top", "R1", "res.sym"};
  CHECK(env.eval("tcleval($path $sch_basename $instname $symname)", context) == "x1.x2 top R1 res.sym");
  // A plain tcleval value (no instance) does not see the previous instance.
  CHECK(env.eval("tcleval([info exists ::instname][info exists ::symname])", {"", "top", "", ""}) == "00");
}

TEST_CASE("results before simulation results are loaded", kTcleval) {
  Env env;
  CHECK(env.eval("tcleval([ngspice::get_node v(out)]|[ngspice::get_current v3]|[ngspice::get_voltage s])") == "||");
  // to_eng of an empty result fails, so the displayed text is `?` LF.
  CHECK(env.eval("tcleval([to_eng [ngspice::get_current v3]])") == "?\n");
}

TEST_CASE("from_eng reads the SPICE scale suffixes case-insensitively", kTcleval) {
  Env env;
  auto eq = [&](const std::string& text, const std::string& expected) {
    const auto r = env.tcl.interpreter().evaluate("expr {[from_eng {" + text + "}] == " + expected + "}");
    INFO(text);
    REQUIRE(r.ok());
    CHECK(r.value.to_string() == "1");
  };
  eq("10k", "1e4");
  eq("3Meg", "3e6");
  eq("2.2u", "2.2e-6");
  eq("5 m", "5e-3");
  eq("7mil", "7e-3");  // only the first letter after the number counts, except `meg`
  eq("1.5", "1.5");
  eq("4x", "4");
}

TEST_CASE("abs_sym_path asks the host and textwindow shows the text", kTcleval) {
  std::string shown;
  schem::ProcedureHooks hooks;
  hooks.abs_sym_path = [](std::string_view ref) { return "/lib/" + std::string{ref}; };
  hooks.textwindow = [&shown](std::string_view text) { shown = text; };
  Env env({}, std::move(hooks));
  CHECK(env.eval("tcleval([abs_sym_path res.sym])") == "/lib/res.sym");
  CHECK(env.eval("tcleval([textwindow hello])") == "");
  CHECK(shown == "hello");
}

TEST_CASE("write_data writes only inside the sandbox's granted folders", kTcleval) {
  TempDir granted;
  TempDir outside;
  script::SandboxPolicy policy;
  policy.granted_roots.emplace_back(granted.path().generic_string());
  Env env(policy);
  const std::string inside_file = (granted.path() / "out.txt").generic_string();
  const std::string outside_file = (outside.path() / "out.txt").generic_string();
  CHECK(env.tcl.interpreter().evaluate("write_data hello {" + inside_file + "}").ok());
  std::ifstream in(inside_file);
  std::stringstream text;
  text << in.rdbuf();
  CHECK(text.str() == "hello");
  CHECK_FALSE(env.tcl.interpreter().evaluate("write_data hello {" + outside_file + "}").ok());
  CHECK_FALSE(fs::exists(outside_file));
  CHECK_FALSE(env.tcl.interpreter().evaluate("write_data onlyone").ok());
}

TEST_CASE("tcleval runs in the sandbox", kTcleval) {
  Env env;
  CHECK(env.eval("tcleval([exec ls])") == "?\n");
  CHECK(env.eval("tcleval([open /etc/hosts r])") == "?\n");
}

// The inventory test of "tcleval values": every command an inventoried
// tcleval value calls is an xschem procedure Hefesto provides, a procedure
// the PDK defines, the `xschem` command (CMD) or a Tcl built-in.
// The union over the three PDKs of the inventory list `key`.
std::set<std::string> inventory_list(const std::string& key) {
  std::ifstream in(std::string{HEFESTO_SOURCE_DIR} + "/tests/data/schem/inventory.json");
  REQUIRE(in.good());
  std::stringstream buffer;
  buffer << in.rdbuf();
  const std::string json = buffer.str();
  std::set<std::string> names;
  const std::regex list("\"" + key + "\"\\s*:\\s*\\[([^\\]]*)\\]");
  const std::regex item("\"([^\"]+)\"");
  for (std::sregex_iterator it(json.begin(), json.end(), list), end; it != end; ++it) {
    const std::string body = (*it)[1];
    for (std::sregex_iterator i(body.begin(), body.end(), item); i != end; ++i) names.insert((*i)[1]);
  }
  return names;
}

TEST_CASE("every inventoried tcleval command is provided", kTcleval) {
  const auto lists = [](const std::string& key) { return inventory_list(key); };
  const std::set<std::string> commands = lists("tcleval_commands");
  const std::set<std::string> pdk_procs = lists("pdk_procs");
  REQUIRE_FALSE(commands.empty());
  const auto& provided = schem::xschem_procedure_names();
  Env env;
  for (const std::string& name : commands) {
    INFO(name);
    const bool xschem_proc = std::find(provided.begin(), provided.end(), name) != provided.end();
    const bool builtin = env.tcl.interpreter().evaluate("info commands ::" + name).value.to_string() == "::" + name;
    CHECK((xschem_proc || pdk_procs.count(name) != 0 || name == "xschem" || builtin));
  }
  for (const std::string& name : provided) {
    INFO(name);
    CHECK(env.tcl.interpreter().evaluate("info commands ::" + name).value.to_string() == "::" + name);
  }
}

TEST_CASE("Launcher confirmation", kPermission) {
  Env env;
  schem::LauncherPermission permission;
  int asked = 0;
  const schem::ConfirmLauncher yes = [&asked](std::string_view) { return ++asked, true; };
  CHECK(schem::run_launcher(env.tcl.interpreter(), permission, 1, "set ::ran 1", yes).ok());
  CHECK(schem::run_launcher(env.tcl.interpreter(), permission, 1, "set ::ran 2", yes).ok());
  CHECK(asked == 1);
  CHECK(env.tcl.interpreter().evaluate("set ::ran").value.to_string() == "2");
  // Another document asks again; closing a document ends its confirmation.
  CHECK(schem::run_launcher(env.tcl.interpreter(), permission, 2, "set ::ran 3", yes).ok());
  CHECK(asked == 2);
  permission.forget(1);
  CHECK_FALSE(permission.confirmed(1));
}

TEST_CASE("a refused launcher runs nothing and asks again next time", kPermission) {
  Env env;
  schem::LauncherPermission permission;
  int asked = 0;
  const schem::ConfirmLauncher no = [&asked](std::string_view) { return ++asked, false; };
  const auto refused = schem::run_launcher(env.tcl.interpreter(), permission, 1, "set ::ran 1", no);
  CHECK_FALSE(refused.ok());
  REQUIRE_FALSE(refused.diagnostics.empty());
  CHECK(refused.diagnostics.front().code == "SCHEM-TCL-002");
  CHECK(env.tcl.interpreter().evaluate("info exists ::ran").value.to_string() == "0");
  CHECK_FALSE(schem::run_launcher(env.tcl.interpreter(), permission, 1, "set ::ran 1", no).ok());
  CHECK(asked == 2);
  CHECK_FALSE(schem::run_launcher(env.tcl.interpreter(), permission, 1, "set ::ran 1", {}).ok());
}

TEST_CASE("a launcher runs in the sandbox", kPermission) {
  Env env;
  schem::LauncherPermission permission;
  const schem::ConfirmLauncher yes = [](std::string_view) { return true; };
  CHECK_FALSE(schem::run_launcher(env.tcl.interpreter(), permission, 1, "exec ls", yes).ok());
}

// Q-SCHEM-38 option (a), approved 2026-11-22: the GUI-only list is derived
// from the inventory. Every command the PDK xschemrc files (and the files
// they source) call is a Tcl built-in of the sandbox, an xschem procedure
// Hefesto provides, a procedure the PDK defines, the `xschem` command (CMD)
// or a GUI-only command; a GUI-only command is none of the others. The
// gf180mcu menu procedure (`xschem-menu`) runs the external program `mkdir`,
// which the sandbox refuses like any other unknown command (no GUI stub).
TEST_CASE("every inventoried xschemrc command is provided or GUI-only", kRc) {
  const std::set<std::string> external_programs{"mkdir"};
  const std::set<std::string> commands = inventory_list("rc_commands");
  const std::set<std::string> pdk_procs = inventory_list("pdk_procs");
  REQUIRE_FALSE(commands.empty());
  const auto& provided = schem::xschem_procedure_names();
  const auto& gui = schem::xschemrc_gui_commands();
  CHECK(std::is_sorted(gui.begin(), gui.end()));
  Env env;
  const auto builtin = [&env](std::string_view name) {
    return env.tcl.interpreter().evaluate("info commands ::" + std::string{name}).value.to_string() ==
           "::" + std::string{name};
  };
  for (const std::string& name : commands) {
    INFO(name);
    const bool xschem_proc = std::find(provided.begin(), provided.end(), name) != provided.end();
    const bool gui_only = std::find(gui.begin(), gui.end(), name) != gui.end();
    const bool external = external_programs.count(name) != 0;
    CHECK((xschem_proc || pdk_procs.count(name) != 0 || name == "xschem" || builtin(name) || gui_only || external));
  }
  for (const std::string_view name : gui) {
    INFO(name);
    CHECK(commands.count(std::string{name}) == 1);
    CHECK_FALSE(builtin(name));
    CHECK(pdk_procs.count(std::string{name}) == 0);
  }
}

TEST_CASE("GUI-only command in a PDK xschemrc", kRc) {
  for (const bool compat : {true, false}) {  // the same in both modes of xschem-compat
    CAPTURE(compat);
    TempDir dir;
    std::string rc;
    for (const std::string_view name : schem::xschemrc_gui_commands()) rc += std::string{name} + " .pdk -text x\n";
    rc += "set AFTER_GUI [menu .m]\n";
    std::ofstream(dir.path() / "xschemrc") << rc;
    schem::XschemrcInput input;
    input.document_folder = dir.path();
    Env env(schem::xschemrc_policy(input));
    const auto result = env.tcl.load_xschemrc(input);
    for (const auto& d : result.diagnostics) {
      INFO(d.text);
      CHECK(d.code != "SCHEM-LIB-006");
    }
    REQUIRE(result.variables.count("AFTER_GUI") == 1);
    CHECK(result.variables.at("AFTER_GUI").empty());
    // The no-ops exist only while the rc files run.
    CHECK(env.tcl.interpreter().evaluate("info commands ::menu").value.to_string().empty());
    CHECK(env.eval("tcleval([info commands ::wm])").empty());
  }
}
