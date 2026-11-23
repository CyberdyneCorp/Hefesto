// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `schematic` command (capability schematic-command "Command and option
// matching", "Subcommand table", "Return values and errors in scripts",
// "Command logging"; capability schematic-symbols-and-libraries "xschemrc as
// data" for the evaluation when a document is opened).
//
// Owner: CMD.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "cmd_fixture.hpp"
#include "hefesto/schem/file/read_write.hpp"

namespace cmd = hefesto::cmd;
namespace schem = hefesto::schem;
using hefesto::test::SchemSession;

namespace {

constexpr const char* kMatching = "[cap:schematic-command][req:Command and option matching]";
constexpr const char* kTable = "[cap:schematic-command][req:Subcommand table]";
constexpr const char* kScripts = "[cap:schematic-command][req:Return values and errors in scripts]";
constexpr const char* kLogging = "[cap:schematic-command][req:Command logging]";
constexpr const char* kRc = "[cap:schematic-symbols-and-libraries][req:xschemrc as data]";

std::string open_two_instances(SchemSession& s) {
  s.folder().write("w/top.sch", hefesto::test::kTwoInstances);
  const std::string path = s.folder().str("w/top.sch");
  REQUIRE(s.schematic({"open", path}).ok());
  return path;
}

std::vector<std::string> lines(const std::string& text) {
  std::vector<std::string> out;
  std::string::size_type start = 0;
  while (start <= text.size()) {
    const auto end = text.find('\n', start);
    out.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return out;
}

}  // namespace

// ---- Command and option matching --------------------------------------------

TEST_CASE("Prefix match: schematic netl acts as schematic netlist", kMatching) {
  SchemSession s;
  // Both need a document: the prefix reached `netlist`, not an error of its own.
  CHECK(SchemSession::message(s.schematic({"netl"})) == "schematic: no schematic is open");
  open_two_instances(s);
  const auto full = s.schematic({"netlist"});
  const auto prefix = s.schematic({"netl"});
  CHECK(prefix.status == full.status);
  // The session cache reports a missing symbol once (SCHEM-LIB-003), so the
  // runs compare by their netlist text.
  CHECK(prefix.value.to_string() == full.value.to_string());
  CHECK(s.schematic({"NETLIST", "-HIER"}).value.to_string() == full.value.to_string());
}

TEST_CASE("Ambiguous prefix: schematic s", kMatching) {
  SchemSession s;
  const auto result = s.schematic({"s"});
  CHECK_FALSE(result.ok());
  CHECK(result.raises);
  CHECK(SchemSession::message(result) == "schematic: ambiguous option \"s\"");
  CHECK(result.diagnostics.back().code == "SCHEM-CMD-001");
}

TEST_CASE("an unknown option names the help", kMatching) {
  SchemSession s;
  CHECK(SchemSession::message(s.schematic({"frobnicate"})) ==
        "schematic: unknown option \"frobnicate\". Use \"schematic help\".");
}

TEST_CASE("missing or extra arguments give the usage line of the option", kMatching) {
  SchemSession s;
  CHECK(SchemSession::message(s.schematic({"resolve"})) == "Usage: schematic resolve <reference>");
  CHECK(SchemSession::message(s.schematic({"resolve", "a", "b"})) == "Usage: schematic resolve <reference>");
  CHECK(SchemSession::message(s.schematic({"open"})) == "Usage: schematic open <file> [-window new|tab|current]");
  CHECK(SchemSession::message(s.schematic({"open", "x.sch", "-bogus"})) ==
        "Usage: schematic open <file> [-window new|tab|current]");
  CHECK(s.schematic({"open"}).diagnostics.back().code == "SCHEM-CMD-002");
}

TEST_CASE("options that need a document fail without one", kMatching) {
  SchemSession s;
  for (const auto* option : {"save", "close", "getprop", "instances", "netlist", "check", "place", "undo"}) {
    const auto result = s.schematic({option});
    CHECK(SchemSession::message(result) == "schematic: no schematic is open");
    CHECK(result.diagnostics.back().code == "SCHEM-CMD-003");
  }
}

TEST_CASE("keyword arguments match by unique prefix in any case", kMatching) {
  SchemSession s;
  CHECK(SchemSession::message(s.schematic({"new", "s"})) == "schematic: ambiguous option \"s\"");
  REQUIRE(s.schematic({"new", "SYM"}).ok());
  CHECK(s.schem().documents.back().document.kind == schem::FileKind::symbol);
  REQUIRE(s.schematic({"new", "sc"}).ok());
  CHECK(s.schem().documents.back().document.kind == schem::FileKind::schematic);
  CHECK(SchemSession::message(s.schematic({"getprop", "schematic", "x"})) ==
        "schematic: unknown option \"x\". Use \"schematic help\".");
}

TEST_CASE("file and instance names are matched exactly", kMatching) {
  SchemSession s;
  open_two_instances(s);
  CHECK(s.schematic({"getprop", "instance", "R1", "value"}).value.to_string() == "1k");
  CHECK(SchemSession::message(s.schematic({"getprop", "instance", "r1", "value"})) ==
        "schematic: no instance \"r1\"");
  CHECK(SchemSession::message(s.schematic({"getprop", "instance", "R", "value"})) == "schematic: no instance \"R\"");
}

TEST_CASE("failed matching leaves the session unchanged", kMatching) {
  SchemSession s;
  open_two_instances(s);
  const auto before = s.schem().documents.size();
  CHECK_FALSE(s.schematic({"s"}).ok());
  CHECK_FALSE(s.schematic({"new"}).ok());
  CHECK_FALSE(s.schematic({"close", "-x"}).ok());
  CHECK(s.schem().documents.size() == before);
}

TEST_CASE("schematic and schematic help print one line per option in table order", kMatching) {
  SchemSession s;
  const auto help = s.schematic({"help"});
  REQUIRE(help.ok());
  CHECK(s.schematic({}).value == help.value);
  CHECK(s.schematic({"HeLp"}).value == help.value);
  const auto text_lines = lines(help.value.to_string());
  const auto options = schem::schematic_options();
  REQUIRE(text_lines.size() == options.size());
  for (std::size_t i = 0; i < options.size(); ++i) CHECK(text_lines[i] == options[i].syntax);
  CHECK(text_lines.front().rfind("open ", 0) == 0);
  CHECK(text_lines.back() == "help");
}

// ---- Subcommand table -------------------------------------------------------

TEST_CASE("the option table holds every option of the Subcommand table", kTable) {
  std::vector<std::string> names;
  for (const auto& option : schem::schematic_options()) names.emplace_back(option.name);
  const std::vector<std::string> expected{
      "open",      "new",       "save",    "close",    "reload",  "reload_symbols", "path",    "resolve",
      "place",     "wire",      "line",    "box",      "arc",     "polygon",        "text",    "label",
      "select",    "selection", "delete",  "move",     "copy",    "rotate",         "flip",    "flipv",
      "align",     "getprop",   "setprop", "replace",  "instances", "nets",         "net",     "descend",
      "ascend",    "hierarchy", "hilight", "undo",     "redo",    "netlist",        "check",   "symbol",
      "simulate",  "annotate",  "probe",   "export",   "pdk",     "help"};
  CHECK(names == expected);
}

TEST_CASE("open returns the document id, save the absolute path, close drops the document", kTable) {
  SchemSession s;
  s.folder().write("w/top.sch", hefesto::test::kTwoInstances);
  const auto opened = s.schematic({"open", s.folder().str("w/top.sch")});
  REQUIRE(opened.ok());
  CHECK(opened.value == cmd::Value{1});
  REQUIRE(s.schem().documents.size() == 1);
  CHECK(s.schem().documents.back().document.instances.size() == 2);

  const auto saved = s.schematic({"save", s.folder().str("w/copy.sch")});
  REQUIRE(saved.ok());
  CHECK(saved.value.to_string() == s.folder().str("w/copy.sch"));
  CHECK(s.folder().read("w/copy.sch") == schem::write_document(s.schem().documents.back().document));

  CHECK(s.schematic({"close"}).ok());
  CHECK(s.schem().documents.empty());
  CHECK(s.schematic({"open", s.folder().str("w/top.sch")}).value == cmd::Value{2});  // ids are never reused
}

TEST_CASE("opening an open file makes it active again", kTable) {
  SchemSession s;
  s.folder().write("w/a.sch", hefesto::test::kTwoInstances);
  s.folder().write("w/b.sch", hefesto::test::kTwoInstances);
  REQUIRE(s.schematic({"open", s.folder().str("w/a.sch")}).value == cmd::Value{1});
  REQUIRE(s.schematic({"open", s.folder().str("w/b.sch"), "-window", "TAB"}).value == cmd::Value{2});
  CHECK(s.schematic({"open", s.folder().str("w/a.sch")}).value == cmd::Value{1});
  CHECK(schem::active_document(s.schem())->id == 1);
  CHECK(s.schem().documents.size() == 2);
}

TEST_CASE("open -window current replaces the active document", kTable) {
  SchemSession s;
  s.folder().write("w/a.sch", hefesto::test::kTwoInstances);
  s.folder().write("w/b.sch", hefesto::test::kTwoInstances);
  REQUIRE(s.schematic({"open", s.folder().str("w/a.sch")}).ok());
  s.schem().documents.back().document.modified = true;
  CHECK(SchemSession::message(s.schematic({"open", s.folder().str("w/b.sch"), "-w", "cur"})) ==
        "schematic: unsaved changes");
  s.schem().documents.back().document.modified = false;
  REQUIRE(s.schematic({"open", s.folder().str("w/b.sch"), "-w", "cur"}).ok());
  REQUIRE(s.schem().documents.size() == 1);
  CHECK(s.schem().documents.back().document.path.filename() == "b.sch");
}

TEST_CASE("a missing file fails and opens nothing", kTable) {
  SchemSession s;
  const auto result = s.schematic({"open", s.folder().str("w/missing.sch")});
  CHECK_FALSE(result.ok());
  CHECK(result.raises);
  CHECK(s.schem().documents.empty());
}

TEST_CASE("close refuses unsaved changes unless -discard", kTable) {
  SchemSession s;
  open_two_instances(s);
  s.schem().documents.back().document.modified = true;
  const auto refused = s.schematic({"close"});
  CHECK(SchemSession::message(refused) == "schematic: unsaved changes");
  CHECK(refused.diagnostics.back().code == "SCHEM-CMD-004");
  CHECK(s.schem().documents.size() == 1);
  CHECK(s.schematic({"close", "-disc"}).ok());
  CHECK(s.schem().documents.empty());
}

TEST_CASE("new creates an empty document; save without a file name needs one", kTable) {
  SchemSession s;
  REQUIRE(s.schematic({"new", "schematic"}).value == cmd::Value{1});
  CHECK(s.schem().documents.back().document.path.empty());
  CHECK(SchemSession::message(s.schematic({"save"})) == "Usage: schematic save [<file>] [-force]");
  REQUIRE(s.schematic({"new", "symbol", s.folder().str("new.sym")}).value == cmd::Value{2});
  const auto saved = s.schematic({"save"});
  REQUIRE(saved.ok());
  CHECK(saved.value.to_string() == s.folder().str("new.sym"));
  CHECK(s.folder().read("new.sym").rfind("v {xschem version=", 0) == 0);
}

TEST_CASE("save refuses a file changed on disk unless -force", kTable) {
  SchemSession s;
  const std::string path = open_two_instances(s);
  s.folder().write("w/top.sch", "v {xschem version=3.4.8RC file_version=1.3}\n");
  CHECK(s.schematic({"save"}).diagnostics.back().code == "SCHEM-FILE-006");
  CHECK(s.schematic({"save", "-force"}).ok());
  CHECK(s.folder().read("w/top.sch") == schem::write_document(s.schem().documents.back().document));
}

TEST_CASE("reload re-reads the file", kTable) {
  SchemSession s;
  open_two_instances(s);
  s.schem().documents.back().document.instances.clear();
  REQUIRE(s.schematic({"reload"}).ok());
  CHECK(s.schem().documents.back().document.instances.size() == 2);
}

TEST_CASE("getprop returns property strings and decoded values", kTable) {
  SchemSession s;
  open_two_instances(s);
  CHECK(s.schematic({"getprop", "instance", "M1"}).value.to_string() == "name=M1 model=nch w=1.0u l=0.18u");
  CHECK(s.schematic({"getprop", "instance", "M1", "w"}).value.to_string() == "1.0u");
  CHECK(s.schematic({"getprop", "instance", "M1", "nope"}).value.to_string().empty());
  CHECK(s.schematic({"getprop", "wire", "0", "lab"}).value.to_string() == "mid");
  CHECK(SchemSession::message(s.schematic({"getprop", "wire", "1"})) == "schematic: no wire \"1\"");
  CHECK(s.schematic({"getprop", "schematic", "k"}).ok());
  CHECK(SchemSession::message(s.schematic({"getprop", "instance"})).rfind("Usage: schematic getprop", 0) == 0);
}

TEST_CASE("instances returns name, symbol, position and orientation in file order", kTable) {
  SchemSession s;
  open_two_instances(s);
  const auto all = s.schematic({"instances"});
  REQUIRE(all.ok());
  CHECK(all.value.to_string() == "{R1 res.sym 0.0 0.0 0 0} {M1 nmos4.sym 100.0 0.0 1 0}");
  CHECK(s.schematic({"instances", "-pattern", "M*"}).value.to_string() == "{M1 nmos4.sym 100.0 0.0 1 0}");
  CHECK(s.schematic({"instances", "-pat", "?1"}).value.to_string() == all.value.to_string());
  CHECK(s.schematic({"instances", "-pattern", "[L-S]1"}).value.to_string() == all.value.to_string());
  // A backwards range is read in order, as Tcl does: [a-Q] is [Q-a].
  CHECK(s.schematic({"instances", "-pattern", "[a-Q]1"}).value.to_string() == "{R1 res.sym 0.0 0.0 0 0}");
  CHECK(s.schematic({"instances", "-pattern", "X*"}).value.to_string().empty());
}

TEST_CASE("path set, prepend and list; resolve finds a symbol or nothing", kTable) {
  SchemSession s;
  s.folder().write("libA/a.sym", "v {xschem version=3.4.8RC file_version=1.3}\n");
  s.folder().write("libB/b.sym", "v {xschem version=3.4.8RC file_version=1.3}\n");
  const auto set = s.schematic({"path", "set", s.folder().str("libB") + ":" + s.folder().str("missing")});
  REQUIRE(set.ok());
  CHECK(set.value.to_string() == s.folder().str("libB"));
  const auto prepended = s.schematic({"path", "prep", s.folder().str("libA")});
  CHECK(prepended.value.to_string() == s.folder().str("libA") + " " + s.folder().str("libB"));
  CHECK(s.schematic({"path", "list"}).value == prepended.value);
  CHECK(s.schematic({"path"}).value == prepended.value);
  CHECK(s.schematic({"resolve", "b.sym"}).value.to_string() == s.folder().str("libB/b.sym"));
  CHECK(s.schematic({"resolve", "nothing.sym"}).value.to_string().empty());
  CHECK(SchemSession::message(s.schematic({"path", "set"})) ==
        "Usage: schematic path [list|set <paths>|prepend <dir>]");
}

TEST_CASE("netlist options are checked before the run", kTable) {
  SchemSession s;
  open_two_instances(s);
  const std::string usage =
      "Usage: schematic netlist [<file>] [-hier|-top|-flat] [-lvs] [-topsubckt] [-lvsignore] [-noprefix] "
      "[-buschar <xy>] [-uppercase]";
  CHECK(SchemSession::message(s.schematic({"netlist", "-top", "-flat"})) == usage);
  CHECK(SchemSession::message(s.schematic({"netlist", "-buschar", "abc"})) == usage);
  CHECK(SchemSession::message(s.schematic({"netlist", "a.spice", "b.spice"})) == usage);
  CHECK(SchemSession::message(s.schematic({"netlist", "-buschar"})) == usage);
}

TEST_CASE("options another workstream implements answer CORE-NOTIMPL-007 until then", kTable) {
  SchemSession s;
  open_two_instances(s);
  for (const auto* option : {"simulate", "annotate", "probe", "export"}) {
    const auto result = s.schematic({option});
    CHECK_FALSE(result.ok());
    CHECK(result.raises);
    CHECK(result.diagnostics.back().code == "CORE-NOTIMPL-007");
  }
}

// ---- Return values and errors in scripts -----------------------------------

TEST_CASE("errors raise a script error that catch sees", kScripts) {
  SchemSession s;
  const auto caught = s.tcl("list [catch {schematic s} msg] $msg");
  REQUIRE(caught.ok());
  CHECK(caught.value.to_string() == "1 {schematic: ambiguous option \"s\"}");
}

TEST_CASE("a failed netlist run raises an error with the error list", kScripts) {
  // "Netlist error in a script": two labels name one wire A and B, so the
  // run reports the short and fails (Q-SCHEM-16).
  SchemSession s;
  s.folder().write("w/lbl.sym",
                   "v {xschem version=3.4.8RC file_version=1.3}\n"
                   "K {type=label format=\"*.alias @lab\" template=\"name=l1 lab=xxx\"}\n"
                   "B 5 -2.5 -2.5 2.5 2.5 {name=p dir=inout}\n");
  s.folder().write("w/top.sch",
                   "v {xschem version=3.4.8RC file_version=1.3}\n"
                   "G {}\nK {}\nV {}\nS {}\nE {}\n"
                   "N 0 0 100 0 {}\n"
                   "C {lbl.sym} 0 0 0 0 {name=l1 lab=A}\n"
                   "C {lbl.sym} 100 0 0 0 {name=l2 lab=B}\n");
  REQUIRE(s.schematic({"open", s.folder().str("w/top.sch")}).ok());
  const auto caught = s.tcl("list [catch {schematic netlist} msg] $msg");
  REQUIRE(caught.ok());
  CHECK(caught.value.to_string().rfind("1 ", 0) == 0);
  CHECK(caught.value.to_string().find("shorted: A - B") != std::string::npos);
  const auto direct = s.schematic({"netlist"});
  CHECK(direct.raises);
  CHECK(direct.diagnostics.back().code == "SCHEM-CMD-005");
}

TEST_CASE("results are Tcl lists in scripts", kScripts) {
  SchemSession s;
  open_two_instances(s);
  CHECK(s.tcl("llength [schematic instances]").value.to_string() == "2");
  CHECK(s.tcl("lindex [schematic instances] 1 0").value.to_string() == "M1");
  CHECK(s.tcl("schematic getprop instance M1 l").value.to_string() == "0.18u");
}

// ---- Command logging --------------------------------------------------------

TEST_CASE("Replay a session: a GUI command log replays to a byte-identical file", kLogging) {
  std::string saved;
  std::string history;
  std::string folder;
  {
    SchemSession gui;
    gui.folder().write("w/top.sch", hefesto::test::kTwoInstances);
    folder = gui.folder().str();
    REQUIRE(gui.schematic({"open", gui.folder().str("w/top.sch")}, cmd::Origin::gui).ok());
    REQUIRE(gui.schematic({"save", gui.folder().str("w/out.sch")}, cmd::Origin::gui).ok());
    saved = gui.folder().read("w/out.sch");
    history = gui.log().history_script();
    CHECK(history.find("schematic open") != std::string::npos);
    CHECK(history.find("schematic save") != std::string::npos);

    std::filesystem::remove(gui.folder().path() / "w/out.sch");
    gui.schem().documents.clear();
    REQUIRE(gui.tcl(history).ok());
    CHECK(gui.folder().read("w/out.sch") == saved);
  }
  CHECK_FALSE(saved.empty());
}

// ---- xschemrc evaluated when a document is opened --------------------------

TEST_CASE("the xschemrc of the document folder sets the search path when the document opens", kRc) {
  SchemSession s;
  s.folder().write("w/lib/x.sym", "v {xschem version=3.4.8RC file_version=1.3}\n");
  s.folder().write("w/xschemrc", "set XSCHEM_LIBRARY_PATH $env(PWD)/lib\nset MY_MODELS /models\n");
  open_two_instances(s);
  REQUIRE(s.schem().xschemrc.has_value());
  CHECK(s.schematic({"path"}).value.to_string() == s.folder().str("w/lib"));
  CHECK(s.tcl("set MY_MODELS").value.to_string() == "/models");  // kept for tcleval
  CHECK(s.schematic({"resolve", "x.sym"}).value.to_string() == s.folder().str("w/lib/x.sym"));
}

TEST_CASE("without any xschemrc, opening keeps the search path set before", kRc) {
  SchemSession s;
  s.folder().write("libB/b.sym", "v {xschem version=3.4.8RC file_version=1.3}\n");
  REQUIRE(s.schematic({"path", "set", s.folder().str("libB")}).ok());
  open_two_instances(s);
  CHECK_FALSE(s.schem().xschemrc.has_value());
  CHECK(s.schematic({"path"}).value.to_string() == s.folder().str("libB"));
}

TEST_CASE("schematic pdk selects the PDK whose xschemrc is evaluated", kRc) {
  hefesto::test::TempFolder pdks;
  pdks.write("demo/libs.tech/xschem/xschemrc", "set XSCHEM_LIBRARY_PATH $PDK_ROOT/$PDK/libs.tech/xschem\n");
  SchemSession s({{"PDK_ROOT", pdks.str()}, {"PDK", "other"}});
  CHECK(s.schematic({"pdk"}).value.to_string() == "other");  // the project's PDK
  const auto missing = s.schematic({"pdk", "nope"});
  CHECK(missing.diagnostics.back().code == "SCHEM-CMD-007");
  CHECK_FALSE(s.schem().xschemrc.has_value());
  REQUIRE(s.schematic({"pdk", "demo"}).value.to_string() == "demo");
  CHECK(s.schematic({"path"}).value.to_string() == pdks.str("demo/libs.tech/xschem"));
  CHECK(SchemSession::message(s.schematic({"pdk", "a", "b"})) == "Usage: schematic pdk [<name>]");
}

TEST_CASE("an opened document evaluates the project PDK's xschemrc with env(PWD) its folder", kRc) {
  hefesto::test::TempFolder pdks;
  pdks.write("demo/libs.tech/xschem/xschemrc", "set XSCHEM_LIBRARY_PATH $env(PWD)\n");
  SchemSession s({{"PDK_ROOT", pdks.str()}, {"PDK", "demo"}});
  open_two_instances(s);
  REQUIRE(s.schem().xschemrc.has_value());
  CHECK(s.schematic({"path"}).value.to_string() == s.folder().str("w"));
  CHECK(s.schematic({"pdk"}).value.to_string() == "demo");
}
