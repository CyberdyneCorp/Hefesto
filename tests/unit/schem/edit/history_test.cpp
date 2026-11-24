// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Undo and redo, read-only libraries, hierarchy navigation and path, net
// highlighting, and the editing session's bus budget (capability
// schematic-editing, tasks.md 9.2; Q-SCHEM-41).
//
// Owner: EDIT.

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>

#include "edit_fixture.hpp"
#include "hefesto/schem/edit/hier_path.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/net/bus.hpp"

namespace schem = hefesto::schem;
using hefesto::test::active;
using hefesto::test::new_schematic;
using hefesto::test::run;
using hefesto::test::SchemSession;
using hefesto::test::write_cell;

namespace {

constexpr const char* kUndo = "[cap:schematic-editing][req:Undo and redo]";
constexpr const char* kReadOnly = "[cap:schematic-editing][req:Read-only libraries]";
constexpr const char* kNavigation = "[cap:schematic-editing][req:Hierarchy navigation]";
constexpr const char* kPath = "[cap:schematic-editing][req:Hierarchy path]";
constexpr const char* kHilight = "[cap:schematic-editing][req:Net highlighting]";
constexpr const char* kPlacing = "[cap:schematic-editing][req:Placing instances]";
constexpr const char* kScripts = "[cap:schematic-command][req:Return values and errors in scripts]";

// A top schematic with instance `x1` of `inv.sym`, whose pin A is on the
// net IN (a label at the pin).
void top_with_cell(SchemSession& s, const std::string& child = {}) {
  write_cell(s, "inv", child);
  if (!child.empty()) write_cell(s, child);
  new_schematic(s);
  run(s, {"place", "inv.sym", "0", "0", "-props", "name=x1"});
  run(s, {"label", "-20", "0", "IN"});
}

}  // namespace

// ---- Undo and redo ----------------------------------------------------------------

TEST_CASE("Undo after ascending", kUndo) {
  SchemSession s;
  top_with_cell(s);
  run(s, {"wire", "0", "100", "50", "100"});
  run(s, {"select", "wire", "0"});
  run(s, {"move", "0", "10"});
  REQUIRE(active(s).wires[0].p1 == schem::Point{0, 110});
  run(s, {"descend", "x1"});
  run(s, {"ascend"});
  CHECK(run(s, {"undo"}) == "1");
  CHECK(active(s).wires[0].p1 == schem::Point{0, 100});
  CHECK(run(s, {"redo"}) == "1");
  CHECK(active(s).wires[0].p1 == schem::Point{0, 110});
}

TEST_CASE("each document keeps its own undo history", kUndo) {
  SchemSession s;
  top_with_cell(s);
  run(s, {"descend", "x1"});
  run(s, {"wire", "-100", "0", "100", "0"});  // an edit of inv.sch
  run(s, {"ascend"});
  // Undo in top.sch undoes top's last edit (the label), not the child's wire.
  CHECK(run(s, {"undo"}) == "1");
  CHECK(active(s).instances.size() == 1);
  run(s, {"descend", "x1"});
  CHECK(active(s).wires.size() == 1);
  CHECK(run(s, {"undo"}) == "1");
  CHECK(active(s).wires.empty());
  CHECK(run(s, {"undo", "5"}) == "0");
}

TEST_CASE("undo restores properties, order and the modified flag", kUndo) {
  SchemSession s;
  new_schematic(s);
  run(s, {"save", s.folder().str("top.sch")});
  CHECK_FALSE(active(s).modified);
  run(s, {"place", "res.sym", "0", "0"});
  run(s, {"setprop", "instance", "R1", "value", "2k"});
  CHECK(active(s).modified);
  run(s, {"undo"});
  CHECK(schem::get_token(active(s).instances[0].props, "value") == "1k");
  run(s, {"undo"});
  CHECK(active(s).instances.empty());
  CHECK_FALSE(active(s).modified);
  // A new edit discards what was undone.
  run(s, {"wire", "0", "0", "1", "0"});
  CHECK(run(s, {"redo"}) == "0");
}

TEST_CASE("the editor-wide undo reverts a schematic edit as one step", kUndo) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0"});
  run(s, {"place", "res.sym", "0", "100"});
  const auto undone = s.editor().undo.undo(s.editor());
  REQUIRE(undone);
  CHECK(undone.value().steps == 1);
  CHECK(active(s).instances.size() == 1);
  // The document history followed: its own redo brings R2 back.
  CHECK(run(s, {"redo"}) == "1");
  CHECK(active(s).instances.size() == 2);
  // Redoing the log's step again changes nothing more.
  (void)s.editor().undo.redo(s.editor());
  CHECK(active(s).instances.size() == 2);
}

TEST_CASE("at least 80 undo steps are kept per document", kUndo) {
  SchemSession s;
  new_schematic(s);
  for (int i = 0; i < 85; ++i) run(s, {"wire", "0", std::to_string(i), "1", std::to_string(i)});
  CHECK(run(s, {"undo", "85"}) == "85");
  CHECK(active(s).wires.empty());
}

TEST_CASE("a failing option leaves no partial edit and no undo step", kScripts) {
  SchemSession s;
  new_schematic(s);
  run(s, {"wire", "0", "0", "1", "0"});
  const auto bad = s.schematic({"setprop", "instance", "R9", "value", "1"});
  CHECK_FALSE(bad.ok());
  CHECK(bad.raises);
  CHECK(s.schem().edit.histories[schem::active_document(s.schem())->id].records.size() == 1);
}

// ---- Read-only libraries ----------------------------------------------------------------

TEST_CASE("Edit PDK symbol", kReadOnly) {
  hefesto::test::TempFolder pdk;
  pdk.write("sky130A/libs.tech/xschem/sky130_fd_pr/nfet_01v8.sym",
            "v {xschem version=3.4.8RC file_version=1.3}\nK {type=nmos}\nL 4 0 0 10 0 {}\n");
  SchemSession s({{"PDK_ROOT", pdk.str()}});
  run(s, {"open", pdk.str("sky130A/libs.tech/xschem/sky130_fd_pr/nfet_01v8.sym")});
  run(s, {"select", "all"});  // viewing and selecting are allowed
  const auto moved = s.schematic({"move", "10", "0"});
  CHECK_FALSE(moved.ok());
  CHECK(moved.diagnostics.back().code == "SCHEM-EDIT-001");
  CHECK(SchemSession::message(moved).find("schematic save <file>") != std::string::npos);
  CHECK(active(s).lines[0].p1 == schem::Point{0, 0});
  // "Save as" to a user folder makes it editable.
  run(s, {"save", s.folder().str("my_nfet.sym")});
  run(s, {"move", "10", "0"});
  CHECK(active(s).lines[0].p1 == schem::Point{10, 0});
}

TEST_CASE("the system library opens read-only", kReadOnly) {
  SchemSession s;
  run(s, {"open", hefesto::test::devices_folder(s) + "/res.sym"});
  CHECK(s.schematic({"wire", "0", "0", "1", "0"}).diagnostics.back().code == "SCHEM-EDIT-001");
}

// ---- Hierarchy navigation and path ------------------------------------------------------

TEST_CASE("Descend into vector element", kNavigation) {
  SchemSession s;
  write_cell(s, "inv");
  new_schematic(s);
  run(s, {"place", "inv.sym", "0", "0", "-props", "name=xinv[3:0]"});
  CHECK(run(s, {"descend", "xinv[3:0]", "-1"}) == ".xinv[0].");
  run(s, {"ascend"});
  CHECK(run(s, {"descend", "xinv[3:0]", "2"}) == ".xinv[2].");
  run(s, {"ascend"});
  CHECK(run(s, {"descend", "xinv[3:0]", "9"}) == ".xinv[3].");  // invalid: element 1
  run(s, {"ascend"});
  CHECK(run(s, {"descend", "xinv[3:0]", "xinv[1]"}) == ".xinv[1].");
}

TEST_CASE("descend keeps the parent's unsaved changes and refuses devices", kNavigation) {
  SchemSession s;
  top_with_cell(s);
  run(s, {"place", "res.sym", "200", "0"});
  run(s, {"descend", "x1"});
  CHECK(active(s).path.filename() == "inv.sch");
  CHECK(run(s, {"ascend"}) == ".");
  CHECK(active(s).modified);
  CHECK(active(s).instances.size() == 3);
  const auto device = s.schematic({"descend", "R1"});
  CHECK_FALSE(device.ok());
  CHECK(device.diagnostics.back().code == "SCHEM-EDIT-002");
  CHECK(s.schematic({"ascend"}).diagnostics.back().code == "SCHEM-EDIT-004");
  CHECK(run(s, {"descend", "-symbol", "R1"}) == ".R1.");
  CHECK(active(s).path.filename() == "res.sym");
}

TEST_CASE("descending is limited to 40 levels", kNavigation) {
  SchemSession s;
  // `loop.sch` holds an instance of itself.
  write_cell(s, "loop", "loop");
  new_schematic(s);
  run(s, {"place", "loop.sym", "0", "0", "-props", "name=x2"});
  for (int i = 0; i < 40; ++i) REQUIRE(s.schematic({"descend", "x2"}).ok());
  const auto deeper = s.schematic({"descend", "x2"});
  CHECK_FALSE(deeper.ok());
  CHECK(deeper.diagnostics.back().code == "SCHEM-EDIT-003");
}

TEST_CASE("Two-level path", kPath) {
  SchemSession s;
  write_cell(s, "inv", "buf");
  write_cell(s, "buf");
  new_schematic(s);
  run(s, {"place", "inv.sym", "0", "0", "-props", "name=x1"});
  CHECK(run(s, {"hierarchy"}) == ".");
  run(s, {"descend", "x1"});
  CHECK(run(s, {"descend", "x2"}) == ".x1.x2.");
  CHECK(run(s, {"hierarchy", "path"}) == ".x1.x2.");
  schem::HierPath path;
  path.elements = {"x1", "x2"};
  CHECK(schem::path_text(path) == ".x1.x2.");
  CHECK(schem::at_path(path) == "x1.x2.");
  CHECK(schem::at_path(schem::HierPath{}).empty());
  const std::string list = run(s, {"hierarchy", "list"});
  CHECK(list.find("top.sch") != std::string::npos);
  CHECK(list.find("buf.sch") != std::string::npos);
  CHECK(run(s, {"ascend"}) == ".x1.");
}

// ---- Net highlighting ------------------------------------------------------------------------

TEST_CASE("Highlight crosses into child", kHilight) {
  SchemSession s;
  top_with_cell(s);
  run(s, {"hilight", "net", "IN"});
  CHECK(run(s, {"hilight", "list"}) == "{net IN 0}");
  run(s, {"descend", "x1"});
  CHECK(run(s, {"hilight", "list"}) == "{net A 0}");
  // A highlight made in the child shows on the connected parent net.
  run(s, {"hilight", "net", "Y"});
  run(s, {"ascend"});
  const std::string shown = run(s, {"hilight", "list"});
  CHECK(shown.find("{net IN 0}") != std::string::npos);
  INFO(shown);
  CHECK(shown.find("{net {#net1} 1}") != std::string::npos);  // the unnamed net on pin Y
}

TEST_CASE("highlights cycle colours, are cleared, and are no undo step", kHilight) {
  SchemSession s;
  top_with_cell(s);
  const std::size_t steps = s.schem().edit.histories[schem::active_document(s.schem())->id].records.size();
  run(s, {"hilight", "net", "IN"});
  run(s, {"hilight", "instance", "x1"});
  CHECK(run(s, {"hilight", "list"}) == "{net IN 0} {instance x1 1}");
  run(s, {"select", "instance", "p1"});
  run(s, {"hilight", "clear"});
  CHECK(run(s, {"hilight", "list"}) == "{instance x1 1}");
  run(s, {"hilight", "clear", "all"});
  CHECK(run(s, {"hilight", "list"}).empty());
  CHECK(s.schem().edit.histories[schem::active_document(s.schem())->id].records.size() == steps);
  CHECK_FALSE(s.schematic({"hilight", "instance", "nope"}).ok());
}

TEST_CASE("global nets are highlighted at every level", kHilight) {
  SchemSession s;
  write_cell(s, "inv");
  s.folder().write("inv.sch",
                   "v {xschem version=3.4.8RC file_version=1.3}\n"
                   "C {ipin.sym} -100 0 0 0 {name=p1 lab=A}\n"
                   "C {opin.sym} 100 0 0 0 {name=p2 lab=Y}\n"
                   "C {gnd.sym} 0 50 0 0 {name=l1 lab=GND}\n");
  new_schematic(s);
  run(s, {"place", "inv.sym", "0", "0", "-props", "name=x1"});
  run(s, {"place", "gnd.sym", "0", "50", "-props", "name=l1 lab=GND"});
  run(s, {"hilight", "net", "GND"});
  run(s, {"descend", "x1"});
  CHECK(run(s, {"hilight", "list"}).find("{net GND 0}") != std::string::npos);
}

// ---- Q-SCHEM-41: the editing session's bus budget ------------------------------------------

TEST_CASE("an editing session's instance names take from the session bus budget", kPlacing) {
  // Regression (round-3 open item): EDIT names instances with an
  // InstanceNamer over Session::bus_budget, so all expansions of an editing
  // session share one total (Q-SCHEM-41 / M8-D3).
  SchemSession s;
  new_schematic(s);
  s.schem().bus_budget = schem::BusBudget{1000};
  run(s, {"place", "res.sym", "0", "0", "-props", "name=R[3:0]"});
  const std::uint64_t after_first = s.schem().bus_budget.remaining();
  CHECK(after_first < 1000);
  run(s, {"place", "res.sym", "0", "100", "-props", "name=Q[3:0]"});
  CHECK(s.schem().bus_budget.remaining() < after_first);
  // Past the budget a bus name is kept as written: R[3:0] is no longer
  // known as used bits, and the new name stays unexpanded.
  s.schem().bus_budget = schem::BusBudget{0};
  CHECK(run(s, {"place", "res.sym", "0", "200", "-props", "name=S[1:0]"}) == "S[1:0]");
  CHECK(s.schem().bus_budget.remaining() == 0);
}
