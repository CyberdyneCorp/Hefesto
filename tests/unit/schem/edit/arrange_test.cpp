// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Object order, the clipboard and label names (capability schematic-editing
// "Object order", "Clipboard", "Labels and ports"; tasks.md 9.2, M8 round 5):
// `schematic order`, `clipboard`, `paste` and `merge`, and the escaping of
// the net name `schematic label` writes.
//
// Owner: EDIT.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "edit_fixture.hpp"
#include "hefesto/schem/edit/edit_state.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/read_write.hpp"

namespace schem = hefesto::schem;
using hefesto::test::active;
using hefesto::test::new_schematic;
using hefesto::test::records;
using hefesto::test::run;
using hefesto::test::saved;
using hefesto::test::SchemSession;

namespace {

constexpr const char* kOrder = "[cap:schematic-editing][req:Object order]";
constexpr const char* kClipboard = "[cap:schematic-editing][req:Clipboard]";
constexpr const char* kLabels = "[cap:schematic-editing][req:Labels and ports]";

std::string name_of(const schem::InstanceRecord& instance) {
  return schem::get_token(instance.props, "name").value_or("<none>");
}

std::size_t history_size(SchemSession& s) {
  return s.schem().edit.histories[schem::active_document(s.schem())->id].records.size();
}

std::string code_of(const hefesto::cmd::CommandResult& result) {
  return result.diagnostics.empty() ? std::string{} : result.diagnostics.back().code;
}

}  // namespace

// ---- Object order -------------------------------------------------------------

TEST_CASE("Reorder instance", kOrder) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0", "-props", "name=R1 value=1k"});
  run(s, {"place", "res.sym", "100", "0", "-props", "name=R2 value=2k"});
  run(s, {"wire", "0", "-30", "100", "-30"});
  run(s, {"wire", "0", "30", "100", "30"});
  run(s, {"select", "instance", "R2"});
  CHECK(run(s, {"order", "0"}) == "{instance R2}");
  const std::vector<std::string> c = records(saved(s), "C ");
  REQUIRE(c.size() == 2);
  CHECK(c[0].find("name=R2") != std::string::npos);
  CHECK(c[1].find("name=R1") != std::string::npos);
  const std::string netlist = run(s, {"netlist"});
  INFO(netlist);
  const auto r2 = netlist.find("\nR2 ");
  const auto r1 = netlist.find("\nR1 ");
  REQUIRE(r2 != std::string::npos);
  REQUIRE(r1 != std::string::npos);
  CHECK(r2 < r1);
}

TEST_CASE("order moves the selected objects of each kind as one block and is one undo step", kOrder) {
  SchemSession s;
  new_schematic(s);
  for (int i = 0; i < 5; ++i) run(s, {"wire", "0", std::to_string(i * 10), "5", std::to_string(i * 10)});
  run(s, {"select", "wire", "1"});
  run(s, {"select", "wire", "3"});
  const std::size_t steps = history_size(s);
  CHECK(run(s, {"order", "99"}) == "{wire 3} {wire 4}");  // past the end: last, in their order
  const auto& wires = active(s).wires;
  CHECK(wires[3].p1.y == 10);
  CHECK(wires[4].p1.y == 30);
  CHECK(wires[0].p1.y == 0);
  CHECK(history_size(s) == steps + 1);
  // Already in place: nothing changes and no undo step is recorded.
  run(s, {"order", "3"});
  CHECK(history_size(s) == steps + 1);
  run(s, {"undo"});
  CHECK(active(s).wires[1].p1.y == 10);
  CHECK(active(s).wires[3].p1.y == 30);
  CHECK(code_of(s.schematic({"order"})) == "SCHEM-CMD-002");
  CHECK(code_of(s.schematic({"order", "-1"})) == "SCHEM-CMD-002");
}

TEST_CASE("order is refused on a read-only document", kOrder) {
  SchemSession s;
  run(s, {"open", hefesto::test::devices_folder(s) + "/res.sym"});
  run(s, {"select", "all"});
  CHECK(code_of(s.schematic({"order", "0"})) == "SCHEM-EDIT-001");
}

// ---- Clipboard ------------------------------------------------------------------

TEST_CASE("Paste from xschem", kClipboard) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0", "-props", "name=R1 value=1k"});
  run(s, {"clipboard", "set",
          "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n"
          "C {res.sym} 0 0 0 0 {name=R1}\n"});
  CHECK(run(s, {"paste", "200", "100"}) == "{instance R2}");
  REQUIRE(active(s).instances.size() == 2);
  const schem::InstanceRecord& pasted = active(s).instances.back();
  CHECK(name_of(pasted) == "R2");
  CHECK(pasted.symbol == "res.sym");
  CHECK(pasted.origin == schem::Point{200, 100});
}

TEST_CASE("clipboard copy writes records relative to the selection's anchor; paste inserts them", kClipboard) {
  SchemSession s;
  new_schematic(s);
  run(s, {"wire", "40", "70", "40", "100"});
  run(s, {"place", "res.sym", "40", "130", "-props", "name=R1 value=1k"});
  run(s, {"text", "40", "50", "0", "0", "0.4", "0.4", "hello"});
  run(s, {"select", "all"});
  const std::string text = run(s, {"clipboard", "copy"});
  CHECK(run(s, {"clipboard"}) == text);
  const auto clip = schem::read_document(text, schem::ReadOptions{});
  REQUIRE(clip.ok());
  REQUIRE(clip.value().wires.size() == 1);
  REQUIRE(clip.value().instances.size() == 1);
  REQUIRE(clip.value().texts.size() == 1);
  // A rigid move: the clipboard keeps the objects' relative positions, and
  // the anchor (lower-left of the bounding box, snapped) is the origin.
  const schem::Point shift{clip.value().wires[0].p1.x - 40, clip.value().wires[0].p1.y - 70};
  CHECK(clip.value().instances[0].origin == schem::Point{40 + shift.x, 130 + shift.y});
  CHECK(clip.value().texts[0].origin == schem::Point{40 + shift.x, 50 + shift.y});
  CHECK(clip.value().texts[0].origin.y == 0);  // the text is the lowest object
  // Paste at (500, 500): moved by (500, 500), names unique, selected.
  const std::string selection = run(s, {"paste", "500", "500"});
  CHECK(selection == "{text 1} {wire 1} {instance R2}");
  CHECK(active(s).wires[1].p1 == schem::Point{clip.value().wires[0].p1.x + 500, clip.value().wires[0].p1.y + 500});
  CHECK(active(s).texts[1].text == "hello");
  CHECK(name_of(active(s).instances[1]) == "R2");
  // One undo step removes the whole paste.
  run(s, {"undo"});
  CHECK(active(s).wires.size() == 1);
  CHECK(active(s).instances.size() == 1);
  CHECK(active(s).texts.size() == 1);
}

TEST_CASE("the clipboard carries the embedded symbols of the copied instances", kClipboard) {
  SchemSession s;
  s.folder().write("emb.sch",
                   "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n"
                   "C {cell.sym} 0 0 0 0 {name=x1 embed=true}\n"
                   "[\nv {xschem version=3.4.8RC file_version=1.3}\nK {type=subcircuit\nformat=\"@name @pinlist "
                   "@symname\"}\nL 4 0 0 10 0 {}\n]\n");
  run(s, {"open", s.folder().str("emb.sch")});
  run(s, {"select", "all"});
  const std::string text = run(s, {"clipboard", "copy"});
  CHECK(text.find("\n[\n") != std::string::npos);
  new_schematic(s, "other.sch");
  run(s, {"paste", "0", "0"});
  REQUIRE(active(s).embedded.size() == 1);
  CHECK(active(s).embedded[0].reference == "cell.sym");
  // A second paste does not add the block again.
  run(s, {"paste", "100", "0"});
  CHECK(active(s).embedded.size() == 1);
  CHECK(name_of(active(s).instances[1]) == "x2");
}

// Regression (review of round 5, finding 11): the pasted block was added
// whenever the document had none, and an embedded block resolves its
// reference document-wide, so the existing library instance changed too.
TEST_CASE("Pasted embedded symbol does not redefine the document's symbol", kClipboard) {
  SchemSession s;
  s.folder().write("emb.sch",
                   "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n"
                   "C {res.sym} 0 0 0 0 {name=R9 embed=true}\n"
                   "[\nv {xschem version=3.4.8RC file_version=1.3}\nK {type=resistor\nformat=\"@name @pinlist "
                   "EMBEDDED\"}\nL 4 0 0 10 0 {}\n]\n");
  run(s, {"open", s.folder().str("emb.sch")});
  run(s, {"select", "all"});
  (void)run(s, {"clipboard", "copy"});
  new_schematic(s, "other.sch");
  run(s, {"place", "res.sym", "0", "0", "-props", "name=R1 value=1k"});
  const std::string before = run(s, {"getprop", "symbol", "res.sym", "format"});
  run(s, {"paste", "200", "0"});
  CHECK(active(s).instances.size() == 2);
  CHECK(active(s).embedded.empty());
  CHECK(run(s, {"getprop", "symbol", "res.sym", "format"}) == before);
  // Into a document that does not use the reference, the block still comes along.
  new_schematic(s, "third.sch");
  run(s, {"paste", "0", "0"});
  REQUIRE(active(s).embedded.size() == 1);
  CHECK(active(s).embedded[0].reference == "res.sym");
}

TEST_CASE("Merging a file inserts its objects like a paste", kClipboard) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0", "-props", "name=R1 value=1k"});
  s.folder().write("part.sch",
                   "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n"
                   "N 0 0 0 50 {lab=A}\nC {res.sym} 0 80 0 0 {name=R1 value=3k}\n");
  CHECK(run(s, {"merge", s.folder().str("part.sch"), "10", "20"}) == "{wire 0} {instance R2}");
  CHECK(active(s).wires[0].p1 == schem::Point{10, 20});
  CHECK(active(s).instances[1].origin == schem::Point{10, 100});
  CHECK(schem::get_token(active(s).instances[1].props, "value") == "3k");
  CHECK(run(s, {"merge", s.folder().str("part.sch")}) == "{wire 1} {instance R3}");
  CHECK(active(s).wires[1].p1 == schem::Point{0, 0});
  run(s, {"undo"});
  run(s, {"undo"});
  CHECK(active(s).wires.empty());
  CHECK(active(s).instances.size() == 1);
  CHECK(code_of(s.schematic({"merge", s.folder().str("missing.sch")})) != "");
  CHECK(code_of(s.schematic({"merge", s.folder().str("part.sch"), "1"})) == "SCHEM-CMD-002");
}

TEST_CASE("paste and merge are refused on a read-only document; copy is allowed", kClipboard) {
  SchemSession s;
  s.folder().write("part.sch", "v {xschem version=3.4.8RC file_version=1.3}\nN 0 0 0 50 {}\n");
  run(s, {"open", hefesto::test::devices_folder(s) + "/res.sym"});
  run(s, {"select", "all"});
  CHECK_FALSE(run(s, {"clipboard", "copy"}).empty());
  const std::size_t lines = active(s).lines.size();
  CHECK(code_of(s.schematic({"paste", "0", "0"})) == "SCHEM-EDIT-001");
  CHECK(code_of(s.schematic({"merge", s.folder().str("part.sch")})) == "SCHEM-EDIT-001");
  CHECK(active(s).lines.size() == lines);
}

TEST_CASE("paste needs schematic objects on the clipboard", kClipboard) {
  SchemSession s;
  new_schematic(s);
  CHECK(code_of(s.schematic({"paste", "0", "0"})) == "SCHEM-EDIT-008");
  run(s, {"clipboard", "set", "v {xschem version=3.4.8RC file_version=1.3}\nG {}\n"});
  CHECK(code_of(s.schematic({"paste", "0", "0"})) == "SCHEM-EDIT-008");
  CHECK(code_of(s.schematic({"paste", "0"})) == "SCHEM-CMD-002");
  // Nothing selected: copy leaves the clipboard as it was.
  CHECK(run(s, {"clipboard", "copy"}).empty());
  CHECK(run(s, {"clipboard"}) == "v {xschem version=3.4.8RC file_version=1.3}\nG {}\n");
  CHECK(code_of(s.schematic({"clipboard", "set"})) == "SCHEM-CMD-002");
  CHECK(code_of(s.schematic({"clipboard", "frob"})) == "SCHEM-CMD-001");
}

TEST_CASE("the clipboard works without an open document, except copy", kClipboard) {
  SchemSession s;
  CHECK(run(s, {"clipboard", "set", "N 0 0 10 0 {}"}) == "N 0 0 10 0 {}");
  CHECK(run(s, {"clipboard"}) == "N 0 0 10 0 {}");
  CHECK(code_of(s.schematic({"clipboard", "copy"})) == "SCHEM-CMD-003");
  CHECK(code_of(s.schematic({"paste", "0", "0"})) == "SCHEM-CMD-003");
}

// ---- Label names --------------------------------------------------------------------

// Regression (round-4 open item): `schematic label` stored the net name as
// typed, so a name with `"` or `\` was malformed in the property string and
// read back changed (`a"b` read as `ab`). It is now escaped ("Property
// strings"); braces are escaped by the file writer. The writer does not
// differ by xschem-compat mode; both are checked.
TEST_CASE("label names round-trip through the file", kLabels) {
  for (const bool compat : {true, false}) {
    for (const std::string name : {"a\"b", "a\\b", "x\\", "{x}", "\"q\"", "a b", "c;d", "plain"}) {
      INFO(name << " xschem-compat " << compat);
      SchemSession s;
      s.project().compat.xschem_compat = compat;
      new_schematic(s);
      run(s, {"label", "0", "0", name});
      CHECK(schem::get_token(active(s).instances.back().props, "lab") == name);
      const std::string text = saved(s);
      schem::ReadOptions options;
      options.xschem_compat = compat;
      const auto reread = schem::read_document(text, options);
      REQUIRE(reread.ok());
      REQUIRE(reread.value().instances.size() == 1);
      CHECK(schem::get_token(reread.value().instances[0].props, "lab") == name);
      CHECK(schem::get_token(reread.value().instances[0].props, "name").has_value());
    }
  }
}

// Made ports (`symbol toschematic`) write their names through the same
// escaping as labels.
TEST_CASE("ports made from a symbol keep names with quotes and backslashes", kLabels) {
  SchemSession s;
  s.folder().write("q.sym",
                   "v {xschem version=3.4.8RC file_version=1.3}\nK {type=subcircuit}\n"
                   "B 5 -2.5 -2.5 2.5 2.5 {name=\"a\\\\\\\"b\" dir=in}\n");
  run(s, {"open", s.folder().str("q.sym")});
  const std::string pin = schem::get_token(active(s).boxes[0].props, "name").value_or("<none>");
  REQUIRE(pin.find('"') != std::string::npos);
  run(s, {"symbol", "toschematic"});
  schem::ReadOptions options;
  const auto sch = schem::read_document(s.folder().read("q.sch"), options);
  REQUIRE(sch.ok());
  REQUIRE(sch.value().instances.size() == 1);
  CHECK(schem::get_token(sch.value().instances[0].props, "lab") == pin);
}
