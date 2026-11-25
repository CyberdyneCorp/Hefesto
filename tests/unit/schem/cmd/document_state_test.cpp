// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What `schematic reload`, `close` and `netlist` do to the session (M8
// round 5, capability schematic-command "Subcommand table"): reload drops
// the editing state of the old content and the launcher confirmation
// (security review S8-06), re-reads changed symbol files (tasks.md 7.2), the
// front end's Interrupt cancels a netlist run (tasks.md 13.2, CMD side), and
// computed labels are stored escaped.
//
// Owner: CMD.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#include "../edit/edit_fixture.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/tcl/permission.hpp"
#include "hefesto/ui/state.hpp"

namespace schem = hefesto::schem;
namespace cmd = hefesto::cmd;
using hefesto::test::active;
using hefesto::test::new_schematic;
using hefesto::test::run;
using hefesto::test::SchemSession;

namespace {

constexpr const char* kTable = "[cap:schematic-command][req:Subcommand table]";
constexpr const char* kCache = "[cap:schematic-symbols-and-libraries][req:Symbol cache and reload]";

constexpr const char* kHeader = "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n";

std::string symbol_text(const std::string& value) {
  return std::string{"v {xschem version=3.4.8RC file_version=1.3}\nG {}\n"} +
         "K {type=resistor\nformat=\"@name @pinlist @value\"\ntemplate=\"name=R1 value=" + value +
         "\"}\nV {}\nS {}\nE {}\n"
         "B 5 -2.5 -32.5 2.5 -27.5 {name=P dir=inout}\nB 5 -2.5 27.5 2.5 32.5 {name=M dir=inout}\n";
}

// Rewrites `file` and moves its modification time on, so a file system with
// a coarse clock still sees the change.
void rewrite(SchemSession& s, const std::string& file, const std::string& text) {
  const std::filesystem::path path = s.folder().path() / file;
  const auto before = std::filesystem::last_write_time(path);
  s.folder().write(file, text);
  std::filesystem::last_write_time(path, before + std::chrono::seconds{2});
}

std::string code_of(const cmd::CommandResult& result) {
  for (const auto& d : result.diagnostics) {
    if (d.code.starts_with("SCHEM-")) return d.code;
  }
  return result.diagnostics.empty() ? std::string{} : result.diagnostics.back().code;
}

schem::DocumentId active_id(SchemSession& s) {
  return schem::active_document(s.schem())->id;
}

}  // namespace

// ---- reload and close end the document's editing state ---------------------------

// Regression (round-4 open item): `reload` replaced the document under the
// same id but kept its undo history (whose undo brought the old file's
// content back over the reloaded one), its highlights and its selection.
TEST_CASE("Reload clears selection, highlights and undo history", kTable) {
  SchemSession s;
  new_schematic(s);
  run(s, {"wire", "0", "0", "10", "0"});
  run(s, {"wire", "0", "10", "10", "10"});
  run(s, {"save"});
  run(s, {"wire", "0", "20", "10", "20"});  // an undo step after the save
  run(s, {"select", "wire", "0"});
  run(s, {"hilight", "net", "A"});
  s.folder().write("top.sch", std::string{kHeader} + "N 0 0 5 0 {}\n");
  run(s, {"reload"});
  REQUIRE(active(s).wires.size() == 1);
  CHECK(run(s, {"selection"}).empty());
  CHECK(s.schem().edit.highlights.empty());
  CHECK(run(s, {"hilight", "list"}).empty());
  CHECK(run(s, {"undo"}) == "0");
  CHECK(active(s).wires.size() == 1);
  // The editor-wide undo log no longer reaches the old content either.
  (void)s.editor().undo.undo(s.editor());
  CHECK(active(s).wires.size() == 1);
  CHECK_FALSE(active(s).modified);
}

// Only the highlights made in the reloaded schematic go: those of the
// parent, at another hierarchy path, stay.
TEST_CASE("reload drops the highlights shown in the reloaded schematic only", kTable) {
  SchemSession s;
  hefesto::test::write_cell(s, "inv");
  new_schematic(s);
  run(s, {"place", "inv.sym", "0", "0", "-props", "name=x1"});
  run(s, {"save"});
  run(s, {"hilight", "net", "TOPNET"});
  run(s, {"descend", "x1"});
  run(s, {"hilight", "net", "A"});
  REQUIRE(s.schem().edit.highlights.size() == 2);
  run(s, {"reload"});
  REQUIRE(s.schem().edit.highlights.size() == 1);
  CHECK(s.schem().edit.highlights.begin()->name == "TOPNET");
  CHECK(s.schem().edit.highlights.begin()->path == ".");
}

// Security review S8-06 (CMD half): a confirmation given for a document's
// launchers survived a reload of a file changed on disk, and a close.
TEST_CASE("Reload forgets the launcher confirmation", kTable) {
  SchemSession s;
  schem::LauncherPermission permission;
  s.schem().edit.launchers = &permission;
  const schem::ConfirmLauncher yes = [](std::string_view) { return true; };
  new_schematic(s);
  run(s, {"save"});
  const schem::DocumentId first = active_id(s);
  REQUIRE(permission.allow(first, "puts hi", yes));
  run(s, {"reload"});
  CHECK_FALSE(permission.confirmed(first));

  new_schematic(s, "second.sch");
  const schem::DocumentId second = active_id(s);
  REQUIRE(permission.allow(second, "puts hi", yes));
  REQUIRE(permission.allow(first, "puts hi", yes));
  run(s, {"close", "-discard"});
  CHECK_FALSE(permission.confirmed(second));
  CHECK(permission.confirmed(first));  // another document keeps its confirmation
  s.schem().edit.launchers = nullptr;
}

TEST_CASE("close drops the closed document's selection and undo history", kTable) {
  SchemSession s;
  new_schematic(s);
  run(s, {"wire", "0", "0", "10", "0"});
  run(s, {"select", "wire", "0"});
  const schem::DocumentId id = active_id(s);
  run(s, {"close", "-discard"});
  CHECK_FALSE(s.schem().edit.selections.contains(id));
  CHECK_FALSE(s.schem().edit.histories.contains(id));
}

// ---- reload re-reads changed symbol files (tasks.md 7.2) ---------------------------

TEST_CASE("reload re-reads a changed symbol file and keeps an unchanged one", kCache) {
  SchemSession s;
  s.folder().write("w/r.sym", symbol_text("1k"));
  s.folder().write("w/k.sym", symbol_text("5k"));
  s.folder().write("w/top.sch", std::string{kHeader} + "C {r.sym} 0 0 0 0 {name=R1}\nC {k.sym} 100 0 0 0 {name=R2}\n");
  run(s, {"open", s.folder().str("w/top.sch")});
  run(s, {"path", "set", s.folder().str("w")});
  CHECK(run(s, {"getprop", "symbol", "r.sym", "template"}) == "name=R1 value=1k");
  hefesto::platform::LocalFileSystem fs;
  const schem::ResolveContext context{s.schem().search_path, fs, nullptr, true};
  const schem::Symbol* unchanged = &s.schem().symbols.resolve(schem::Reference{"k.sym"}, active(s), context);

  rewrite(s, "w/r.sym", symbol_text("22k"));
  // The cache keeps what it read until the symbols are reloaded.
  CHECK(run(s, {"getprop", "symbol", "r.sym", "template"}) == "name=R1 value=1k");
  run(s, {"reload"});
  CHECK(run(s, {"getprop", "symbol", "r.sym", "template"}) == "name=R1 value=22k");
  // k.sym did not change: it was not read again (the same cached symbol).
  CHECK(&s.schem().symbols.resolve(schem::Reference{"k.sym"}, active(s), context) == unchanged);
  CHECK(run(s, {"netlist"}).find("R1 ") != std::string::npos);
}

// ---- netlist interrupt (tasks.md 13.2, CMD side) ---------------------------------------

TEST_CASE("an Interrupt pending before the run cancels schematic netlist", kTable) {
  SchemSession s;
  s.folder().write("w/r.sym", symbol_text("1k"));
  s.folder().write("w/top.sch", std::string{kHeader} + "C {r.sym} 0 0 0 0 {name=R1}\n");
  run(s, {"open", s.folder().str("w/top.sch")});
  (void)hefesto::ui::request_interrupt(s.editor());
  const auto result = s.schematic({"netlist", s.folder().str("w/top.spice")});
  CHECK_FALSE(result.ok());
  CHECK(code_of(result) == "SCHEM-CMD-011");
  CHECK_FALSE(std::filesystem::exists(s.folder().path() / "w/top.spice"));
  (void)hefesto::ui::request_interrupt(s.editor());
  CHECK(code_of(s.schematic({"check"})) == "SCHEM-CMD-011");
  // The dispatcher cleared the Interrupt after the command: the next run works.
  run(s, {"netlist", s.folder().str("w/top.spice")});
  CHECK(std::filesystem::exists(s.folder().path() / "w/top.spice"));
}

// An Interrupt made while a `tcleval` value loops stops the value at once
// (not after its 10 s limit) and the run writes nothing.
TEST_CASE("an Interrupt during the run stops a looping tcleval value and cancels the netlist", kTable) {
  SchemSession s;
  s.folder().write("w/r.sym", symbol_text("1k"));
  s.folder().write("w/top.sch",
                   std::string{kHeader} + "C {r.sym} 0 0 0 0 {name=R1 value=\"tcleval([while 1 \\{\\}])\"}\n");
  run(s, {"open", s.folder().str("w/top.sch")});
  hefesto::editor::EditorState& editor = s.editor();
  int reports = 0;
  editor.script.progress.handler = [&editor, &reports](const hefesto::script::ScriptProgress& report) {
    if (!report.done && ++reports == 1) (void)hefesto::ui::request_interrupt(editor);
  };
  const auto start = std::chrono::steady_clock::now();
  const auto result = s.schematic({"netlist", s.folder().str("w/top.spice")});
  const auto elapsed = std::chrono::steady_clock::now() - start;
  editor.script.progress.handler = {};
  CHECK(reports >= 1);
  CHECK(code_of(result) == "SCHEM-CMD-011");
  CHECK(elapsed < std::chrono::seconds{5});
  CHECK_FALSE(std::filesystem::exists(s.folder().path() / "w/top.spice"));
}

// ---- computed labels are stored escaped --------------------------------------------------

// Regression: the `lab` a netlist run computes for a wire was stored as the
// raw net name, so a label `a"b` gave the wire `lab=a"b`, read back as `ab`.
TEST_CASE("computed wire labels keep quotes and backslashes", kTable) {
  for (const std::string name : {"a\"b", "c\\d"}) {
    INFO(name);
    SchemSession s;
    new_schematic(s);
    run(s, {"wire", "0", "0", "0", "100"});
    run(s, {"label", "0", "0", name});
    (void)s.schematic({"netlist"});
    CHECK(schem::get_token(active(s).wires[0].props, "lab") == name);
  }
}
