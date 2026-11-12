// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// readspice (capability readspice; Q-SCRIPT-19, Q-SCRIPT-32).

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include "hefesto/tools/readspice.hpp"
#include "hefesto/tools/spice_lines.hpp"
#include "tools_fixture.hpp"

namespace tools = hefesto::tools;
using tools::ContinuationMode;
using tools::test::Session;
using tools::test::texts;

namespace {

std::vector<std::string> logical_lines(std::string_view text, ContinuationMode mode, bool cdl = false) {
  tools::SpiceLineReader reader{text, mode, cdl};
  std::vector<std::string> out;
  while (const auto line = reader.next()) {
    out.push_back(*line);
  }
  return out;
}

std::vector<std::string> pin_candidates(const std::string& line) {
  const tools::SpiceLine parsed = tools::tokenize_spice_line(line);
  return {parsed.tokens.begin() + 2, parsed.tokens.end()};
}

const std::vector<std::pair<std::string, int>> kInvYFirst = {{"Y", 1}, {"A", 2}, {"VDD", 3}, {"VSS", 4}};
const std::vector<std::pair<std::string, int>> kInvOrdered = {{"A", 1}, {"Y", 2}, {"VDD", 3}, {"VSS", 4}};

}  // namespace

// --- Netlist file resolution -------------------------------------------------------------

TEST_CASE("readspice tries the netlist extensions in Magic's order", "[cap:readspice][req:Netlist file resolution]") {
  Session s;
  // "Extension fallback": only cells.spice exists.
  s.write("/work/cells.spice", ".subckt inv A Y\n.ends\n");
  const auto r = s.run("readspice cells");
  REQUIRE(r.ok());
  CHECK(r.value.to_string().empty());
  const auto notes = texts(r, "TOOLS-RSP-001");
  REQUIRE_FALSE(notes.empty());
  CHECK(notes.front() == "Annotating port orders from cells.spice");
  // `.spi` comes before `.spice`; the root drops the last extension only.
  s.write("/work/cells.spi", "");
  CHECK(texts(s.run("readspice cells.x"), "TOOLS-RSP-001").front() == "Annotating port orders from cells.spi");
  CHECK(tools::tcl_file_rootname("dir.v1/cells") == "dir.v1/cells");
  CHECK(tools::tcl_file_rootname("a/b.c.sp") == "a/b.c");
  // An absolute name is read as given.
  s.write("/lib/x.sp", "");
  CHECK(texts(s.run("readspice /lib/x.sp"), "TOOLS-RSP-001").front() == "Annotating port orders from /lib/x.sp");
}

TEST_CASE("readspice returns 1 and reports a missing netlist", "[cap:readspice][req:Netlist file resolution]") {
  Session s;
  // "Missing file".
  const auto r = s.run("readspice nofile.sp");
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == "1");
  CHECK(texts(r, "TOOLS-RSP-002") == std::vector<std::string>{"Error:  Can't read netlist file nofile.sp"});
  CHECK(texts(r, "TOOLS-RSP-001").empty());
  // The procedure takes exactly one argument.
  const auto bad = s.run("readspice a b");
  CHECK(bad.raises);
  CHECK(bad.diagnostics.front().text == "wrong # args: should be \"readspice netfile\"");
}

TEST_CASE("a .cdl netlist, given or found as the last candidate, is read as CDL",
          "[cap:readspice][req:Netlist file resolution]") {
  Session s;
  s.cell_with_labels("inv", kInvOrdered);
  const std::string cdl = ".SUBCKT inv A Y VDD VSS\n*.PININFO A:I Y:O\n.ENDS\n";
  s.write("/work/inv.cdl", cdl);
  REQUIRE(s.run("readspice inv").ok());  // found through the .cdl fallback
  CHECK(s.run("port A class").value.to_string() == "input");
  // A non-CDL file keeps `*.PININFO` as a comment.
  s.cell_with_labels("buf", kInvOrdered);
  s.write("/work/buf.sp", ".SUBCKT buf A Y VDD VSS\n*.PININFO A:I Y:O\n.ENDS\n");
  REQUIRE(s.run("readspice buf.sp").ok());
  CHECK(s.run("port A class").value.to_string() == "default");
}

// --- Line preprocessing ------------------------------------------------------------------

TEST_CASE("comment and continuation lines in both magic-compat settings",
          "[cap:readspice][req:Line preprocessing]") {
  // "Continuation across a comment": the same in both settings.
  for (const auto mode : {ContinuationMode::magic, ContinuationMode::fixed}) {
    const auto lines = logical_lines(".subckt inv A Y\n* comment\n+ VDD VSS\n", mode);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].empty());  // readspice.tcl starts with lastline ""
    CHECK(lines[1] == ".subckt inv A Y  VDD VSS");
    CHECK(pin_candidates(lines[1]) == std::vector<std::string>{"A", "Y", "", "VDD", "VSS"});
  }
}

TEST_CASE("a continuation joins tokens with magic-compat ON", "[cap:readspice][req:Line preprocessing]") {
  // "Continuation joins tokens with magic-compat ON".
  const auto lines = logical_lines(".subckt inv A Y\n+VDD VSS \n", ContinuationMode::magic);
  REQUIRE(lines.size() == 2);
  CHECK(lines[1] == ".subckt inv A YVDD VSS ");
  CHECK(pin_candidates(lines[1]) == std::vector<std::string>{"A", "YVDD", "VSS", ""});
  // A continuation without a trailing space gets one first.
  CHECK(logical_lines("a b\n+c\n", ContinuationMode::magic)[1] == "a b c");
  // Magic tests the continuation line, so a logical line ending in a space gets a second one.
  CHECK(logical_lines("a b \n+c\n", ContinuationMode::magic)[1] == "a b  c");
}

TEST_CASE("a continuation starts a token with magic-compat OFF", "[cap:readspice][req:Line preprocessing]") {
  // "Continuation starts a token with magic-compat OFF".
  const auto lines = logical_lines(".subckt inv A Y\n+VDD VSS \n", ContinuationMode::fixed);
  REQUIRE(lines.size() == 2);
  CHECK(lines[1] == ".subckt inv A Y VDD VSS ");
  CHECK(pin_candidates(lines[1]) == std::vector<std::string>{"A", "Y", "VDD", "VSS", ""});
  CHECK(logical_lines("a b \n+c\n", ContinuationMode::fixed)[1] == "a b c");
}

TEST_CASE("CDL PININFO comments, line ends and empty tokens", "[cap:readspice][req:Line preprocessing]") {
  // `*.pininfo` (any case) loses its `*` in CDL mode only.
  CHECK(logical_lines("*.PinInfo A:I\n", ContinuationMode::magic, true)[1] == ".PinInfo A:I");
  CHECK(logical_lines("*.PININFO A:I\n", ContinuationMode::magic, false).size() == 1);
  CHECK(logical_lines("*.pininf A:I\n", ContinuationMode::magic, true).size() == 1);
  // \r\n and \r end lines like \n (Tcl's gets); the last line needs no line end.
  CHECK(logical_lines("a\r\nb\rc", ContinuationMode::magic) == std::vector<std::string>{"", "a", "b", "c"});
  // Tokens split on single blanks.
  CHECK(tools::tokenize_spice_line("x  y\tz").tokens == std::vector<std::string>{"x", "", "y", "z"});
  CHECK(tools::tokenize_spice_line("").tokens.empty());
}

TEST_CASE("readspice uses the magic-compat continuation mode", "[cap:readspice][req:Line preprocessing]") {
  for (const bool compat : {true, false}) {
    Session s;
    s.project.compat.magic_compat = compat;
    s.cell_with_labels("inv", kInvYFirst);
    s.write("/work/inv.sp", ".subckt inv A Y\n+VDD VSS \n.ends\n");
    REQUIRE(s.run("readspice inv.sp").ok());
    INFO("magic-compat " << compat);
    // ON: `YVDD` matches nothing, so Y keeps its moved index; OFF: Y is pin 2.
    CHECK(s.index_of("inv", "A") == 1);
    CHECK((s.index_of("inv", "Y") == 2) == !compat);
  }
}

// --- Subcircuit recognition --------------------------------------------------------------

TEST_CASE("an unloaded subcircuit cell is reported and skipped", "[cap:readspice][req:Subcircuit recognition]") {
  Session s;
  s.cell_with_labels("inv", kInvYFirst);
  s.write("/work/n.sp", ".subckt nand2 A B Y VDD VSS\n.PININFO A:I\n.ends\n");
  const auto r = s.run("readspice n.sp");
  REQUIRE(r.ok());
  // "Unloaded cell".
  CHECK(texts(r, "TOOLS-RSP-001") ==
        std::vector<std::string>{"Annotating port orders from n.sp", "Cell nand2 in netlist has not been loaded."});
  CHECK(texts(r, "TOOLS-RSP-003").empty());  // its PININFO is ignored too
  CHECK(s.index_of("inv", "Y") == 1);
}

TEST_CASE("keywords are case-insensitive; Spectre inline subckt lines; .ends ends the cell",
          "[cap:readspice][req:Subcircuit recognition]") {
  Session s;
  s.cell_with_labels("inv", kInvYFirst);
  s.write("/work/s.scs", "inline subckt inv (A Y VDD VSS)\nends inv\n");
  const auto r = s.run("readspice s.scs");
  REQUIRE(r.ok());
  CHECK(s.index_of("inv", "A") == 1);
  CHECK(s.index_of("inv", "Y") == 2);
  const auto parsed = tools::tokenize_spice_line("Inline SUBCKT inv (A Y)");
  CHECK(parsed.keyword == ".subckt");
  CHECK(parsed.tokens == std::vector<std::string>{"SUBCKT", "inv", "", "A", "Y", ""});
  // After .ENDS, a PININFO line has no subcircuit.
  s.write("/work/e.cdl", ".SUBCKT inv A Y VDD VSS\n.ENDS\n*.PININFO A:I\n");
  const auto ended = s.run("readspice e.cdl");
  CHECK(texts(ended, "TOOLS-RSP-003").empty());
  CHECK(s.run("port A class").value.to_string() == "default");
}

// --- Port renumbering algorithm ----------------------------------------------------------

TEST_CASE("netlist order renumbers the ports", "[cap:readspice][req:Port renumbering algorithm]") {
  Session s;
  s.cell_with_labels("inv", kInvYFirst);
  s.write("/work/inv.sp", ".subckt inv A Y VDD VSS\n.ends\n");
  const auto r = s.run("readspice inv.sp");
  REQUIRE(r.ok());
  // "Reordering ports".
  CHECK(s.index_of("inv", "A") == 1);
  CHECK(s.index_of("inv", "Y") == 2);
  CHECK(s.index_of("inv", "VDD") == 3);
  CHECK(s.index_of("inv", "VSS") == 4);
  CHECK(texts(r, "TOOLS-RSP-001") == std::vector<std::string>{"Annotating port orders from inv.sp",
                                                             "Annotating cell inv",
                                                             "Cell inv port order was modified."});
}

TEST_CASE("already-ordered ports are not reported in either magic-compat setting",
          "[cap:readspice][req:Port renumbering algorithm]") {
  for (const bool compat : {true, false}) {
    Session s;
    s.project.compat.magic_compat = compat;
    s.cell_with_labels("inv", kInvOrdered);
    s.write("/work/inv.sp", ".subckt inv A Y VDD VSS\n.ends\n");
    const auto r = s.run("readspice inv.sp");
    REQUIRE(r.ok());
    // "Already-ordered ports not reported" (Q-SCRIPT-32 fix: golden deviation).
    CHECK(s.index_of("inv", "A") == 1);
    CHECK(s.index_of("inv", "VSS") == 4);
    CHECK(texts(r, "TOOLS-RSP-001") ==
          std::vector<std::string>{"Annotating port orders from inv.sp", "Annotating cell inv"});
  }
}

TEST_CASE("bus brackets, case and bare labels match netlist pins", "[cap:readspice][req:Port renumbering algorithm]") {
  Session s;
  s.project.compat.magic_compat = false;
  // "Bus-bracket mapping" (magic-compat OFF): port D<0> takes the position
  // of D[0]; `clk` matches port CLK; label Q (not a port) becomes a port.
  s.cell_with_labels("reg", {{"CLK", 1}, {"D<0>", 2}, {"Q", 0}});
  s.write("/work/reg.sp", ".subckt reg Q D[0] clk W=1\n.ends\n");
  const auto r = s.run("readspice reg.sp");
  REQUIRE(r.ok());
  CHECK(s.index_of("reg", "Q") == 1);
  CHECK(s.index_of("reg", "D<0>") == 2);
  CHECK(s.index_of("reg", "CLK") == 3);
  CHECK(texts(r, "TOOLS-RSP-001").back() == "Cell reg port order was modified.");
}

TEST_CASE("D[0] never matches port D<0> with magic-compat ON", "[cap:readspice][req:Port renumbering algorithm]") {
  // Regression (golden SCRIPT-RSP-03, Q-SCRIPT-47): Magic's `string map
  // {[ < ] >]}` turns D[0] into D<0>], so D<0> keeps the index it was moved
  // to in step 2; the angle-to-square mapping still finds d[1] from d<1>.
  Session s;
  s.cell_with_labels("reg", {{"CLK", 1}, {"D<0>", 2}, {"Q", 3}, {"d[1]", 4}});
  s.write("/work/reg.sp", ".subckt reg Q D[0] CLK d<1>\n.ends\n");
  const auto r = s.run("readspice reg.sp");
  REQUIRE(r.ok());
  CHECK(s.index_of("reg", "Q") == 1);
  CHECK(s.index_of("reg", "CLK") == 2);
  CHECK(s.index_of("reg", "d[1]") == 3);
  CHECK(s.index_of("reg", "D<0>") > 4);  // not renumbered: still out of the way
}

TEST_CASE("layout ports absent from the netlist keep high indices", "[cap:readspice][req:Port renumbering algorithm]") {
  Session s;
  // "Extra layout port": BODY moves above the netlist pins.
  s.cell_with_labels("inv", {{"A", 1}, {"Y", 2}, {"VDD", 3}, {"VSS", 4}, {"BODY", 5}});
  s.write("/work/inv.sp", ".subckt inv A Y VDD VSS\n.ends\n");
  REQUIRE(s.run("readspice inv.sp").ok());
  CHECK(s.index_of("inv", "A") == 1);
  CHECK(s.index_of("inv", "VSS") == 4);
  CHECK(s.index_of("inv", "BODY") > 4);
  // A pin that names neither a port nor a label is skipped without a port.
  s.cell_with_labels("buf", kInvOrdered);
  s.write("/work/buf.sp", ".subckt buf NOPE A Y VDD VSS\n.ends\n");
  const auto r = s.run("readspice buf.sp");
  REQUIRE(r.ok());
  CHECK(s.index_of("buf", "A") == 1);
}

// --- Port direction from PININFO ---------------------------------------------------------

TEST_CASE("CDL PININFO sets the class of default-class ports", "[cap:readspice][req:Port direction from PININFO]") {
  Session s;
  s.cell_with_labels("inv", kInvOrdered);
  s.write("/work/inv.cdl", ".SUBCKT inv A Y VDD VSS\n*.PININFO A:I Y:O VDD:B VSS:B\n.ENDS\n");
  const auto r = s.run("readspice inv.cdl");
  REQUIRE(r.ok());
  // "CDL pin directions".
  CHECK(s.run("port A class").value.to_string() == "input");
  CHECK(s.run("port Y class").value.to_string() == "output");
  // `inout` is reported under its first name, `bidirectional`.
  CHECK(s.run("port VDD class").value.to_string() == "bidirectional");
  CHECK(s.run("port VSS class").value.to_string() == "bidirectional");
}

TEST_CASE("PININFO ignores other letters and set classes, and reports unknown pins",
          "[cap:readspice][req:Port direction from PININFO]") {
  Session s;
  s.cell_with_labels("inv", kInvOrdered);
  s.ok("port Y class output");
  s.write("/work/inv.cdl", ".SUBCKT inv A Y VDD VSS\n*.PININFO A:i Y:I VDD:X Z:I :O\n.ENDS\n");
  const auto r = s.run("readspice inv.cdl");
  REQUIRE(r.ok());
  // Q-SCRIPT-19: literal, case-sensitive letters (`switch`).
  CHECK(s.run("port A class").value.to_string() == "default");
  CHECK(s.run("port Y class").value.to_string() == "output");  // not default: kept
  CHECK(s.run("port VDD class").value.to_string() == "default");
  CHECK(texts(r, "TOOLS-RSP-003") == std::vector<std::string>{".PININFO error:  Pin Z not found."});
}
