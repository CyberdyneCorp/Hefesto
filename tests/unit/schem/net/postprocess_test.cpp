// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Element line post-processing and line breaking (capability
// schematic-netlisting "Element line post-processing" with Q-SCHEM-18 and
// the Q-SCHEM-22 branch point, "Line breaking" Q-SCHEM-20; tasks.md 8.3).
//
// Owner: NETLIST.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <vector>

#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/net/postprocess.hpp"

namespace schem = hefesto::schem;

namespace {

schem::NetlistChunk element(std::string text, std::string prefix = {}) {
  return schem::NetlistChunk{std::move(text), std::move(prefix), false};
}

std::string finish(const std::vector<schem::NetlistChunk>& chunks, bool compat = true) {
  return schem::finish_netlist(chunks, compat);
}

std::string block(const std::string& code) {
  return std::string{schem::kBeginUserCode} + "\n" + code + std::string{schem::kEndUserCode} + "\n";
}

/// A line of `count` fields of 10 characters separated by single spaces.
std::string ten_char_fields(int count) {
  std::string line;
  for (int i = 0; i < count; ++i) {
    std::string field = "f" + std::to_string(i);
    field.resize(10, 'x');
    line += (i == 0 ? "" : " ") + field;
  }
  return line;
}

}  // namespace

TEST_CASE("Vector instance on a bus", "[cap:schematic-netlisting][req:Element line post-processing]") {
  CHECK(finish({element("R1[1],R1[0] ?1 A[1],A[0] ?1 B 1k\n")}) == "R1[1] A[1] B 1k\nR1[0] A[0] B 1k\n");
}

TEST_CASE("sky130 transistor with prefix", "[cap:schematic-netlisting][req:Element line post-processing]") {
  CHECK(finish({element("M1 ?1 D ?1 G ?1 S ?1 B sky130_fd_pr__nfet_01v8 L=0.15 W=1\n+ nf=1 ad=0.29\n", "X")}) ==
        "XM1 D G S B sky130_fd_pr__nfet_01v8 L=0.15 W=1 nf=1 ad=0.29\n");
}

TEST_CASE("Current save", "[cap:schematic-netlisting][req:Element line post-processing]") {
  // vsource.sym `@name @pinlist @value @savecurrent` with savecurrent=true.
  CHECK(finish({element("V1 ?1 A ?1 0  3 \n.save I( ?1 V1 )\n")}) == "V1 A 0 3\n.save i(v1)\n");
}

TEST_CASE("Trailing m=1 in user code", "[cap:schematic-netlisting][req:Element line post-processing]") {
  const std::vector<schem::NetlistChunk> chunks{element("R1 ?1 A ?1 B  1k m=1\n"),
                                                element(block(".param m=1\n"))};
  CHECK(finish(chunks, true) == "R1 A B 1k\n" + block(".param\n"));
  CHECK(finish(chunks, false) == "R1 A B 1k\n" + block(".param m=1\n"));
}

TEST_CASE("post-processing: the Q-SCHEM-22 branch point", "[cap:schematic-netlisting][req:Element line post-processing]") {
  CHECK(schem::strips_trailing_m1("R1 a b m=1", false, false));
  CHECK_FALSE(schem::strips_trailing_m1("* R1 a b m=1", false, false));
  CHECK_FALSE(schem::strips_trailing_m1(".param m=1", false, false));
  CHECK_FALSE(schem::strips_trailing_m1("R1 a b m=1", true, false));
  CHECK(schem::strips_trailing_m1(".param m=1", true, true));
  CHECK(schem::remove_trailing_m1("R1 a b M = 1  ") == "R1 a b");
  CHECK(schem::remove_trailing_m1("R1 a b m=10") == "R1 a b m=10");
  CHECK(schem::remove_trailing_m1("R1 a bm=1") == "R1 a bm=1");
  // Comments, header code and .param lines with xschem-compat OFF keep it.
  const std::vector<schem::NetlistChunk> chunks{element("* note m=1\n"), element(".param p=2 m=1\n"),
                                                schem::NetlistChunk{".option m=1\n", {}, true}};
  CHECK(finish(chunks, false) == "* note m=1\n.param p=2 m=1\n.option m=1\n");
  CHECK(finish(chunks, true) == "* note\n.param p=2\n.option\n");
}

TEST_CASE("Trailing m=1 in a testbench, xschem-compat ON",
          "[cap:schematic-netlisting][req:Element line post-processing]") {
  // The element texts of SCHEM-NL-01 (tests/golden/SCHEM/cases/SCHEM-NL-01).
  const std::vector<schem::NetlistChunk> chunks{
      element("** sch_path: /work/input/SC_Test.sch\n"), element("**.subckt SC_Test\n"),
      element("V1 ?1 VIN ?1 0  5 \n"), element("R1 ?1 VIN ?1 VOUT  1k m=1\n"), element("C1 ?1 VOUT ?1 0  1n m=1\n"),
      element(block("\n.param m=1\n.tran 1n 1u\n.save all\n\n")), element("**.ends\n"), element(".end\n")};
  CHECK(finish(chunks) ==
        "** sch_path: /work/input/SC_Test.sch\n**.subckt SC_Test\nV1 VIN 0 5\nR1 VIN VOUT 1k\nC1 VOUT 0 1n\n"
        "**** begin user architecture code\n\n.param\n.tran 1n 1u\n.save all\n\n**** end user architecture code\n"
        "**.ends\n.end\n");
}

TEST_CASE("post-processing: notations of element text", "[cap:schematic-netlisting][req:Element line post-processing]") {
  CHECK(finish({element("X1 ##literal ?1 A PARAM:w=1\n")}) == "X1 literal A w=1\n");
  // `#<p>#<rest>` gets `<p>` after every comma and a ?1 marker before it.
  CHECK(finish({element("X1[1],X1[0] #n#a,b c\n")}) == "X1[1] na c\nX1[0] nb c\n");
  // `#<p>#?<w>` puts the prefix before each item of the next field.
  CHECK(finish({element("X1 #p#?2 a,b c\n")}) == "X1 pa pb c\n");
  // A width marker with a key, and w = -1 meaning the item count.
  CHECK(finish({element("X1[1],X1[0] k=?1 u,v ?-1 a,b\n")}) == "X1[1] k=u a b\nX1[0] k=v a b\n");
  CHECK(finish({element("X1[1],X1[0] ?1 a,b,c\n")}) == "X1[1] a\nX1[0] b\n");
  // An element whose name list is empty (a .model line with no third
  // field has no item to repeat its name for) is an empty line.
  CHECK(finish({element(".model nmos\n")}) == "\n");
  // .model lines repeat their name per item of the third field.
  CHECK(finish({element(".model ?1 a,b nmos\n")}) == ".model a nmos\n.model b nmos\n");
}

TEST_CASE("post-processing: renames", "[cap:schematic-netlisting][req:Element line post-processing]") {
  CHECK(finish({element("G1 ?1 a ?1 b ?1 c ?1 d VALUE=2\n")}) == "G1 a b c d cur=2\n");
  CHECK(finish({element("E1 ?1 a ?1 b value=2 value=3\n")}) == "E1 a b vol=2 value=3\n");
  CHECK(finish({element("r1 ?1 a ?1 b value=2\n")}) == "r1 a b r=2\n");
  CHECK(finish({element("C1 ?1 a ?1 b Value=2\n")}) == "C1 a b c=2\n");
  CHECK(finish({element("D1 ?1 a ?1 b dmod PERI = 3\n")}) == "D1 a b dmod PJ= 3\n");
  CHECK(finish({element("d1 ?1 a ?1 b dmod PERI=3\n")}) == "d1 a b dmod PERI=3\n");  // `D` only
}

TEST_CASE("post-processing: parametric vector names", "[cap:schematic-netlisting][req:Element line post-processing]") {
  const std::vector<schem::NetlistChunk> chunks{element("RR[hi:lo] ?1 a ?1 b 1k\n"),
                                                element(block(".PARAM hi = 1 lo=0\n"))};
  CHECK(finish(chunks) == "RR[1] a b 1k\nRR[0] a b 1k\n" + block(".PARAM hi = 1 lo=0\n"));
  // No `.param` value, or a one-letter base the pattern does not match: kept.
  CHECK(finish({element("RR[hi:lo] ?1 a 1k\n")}) == "RR[hi:lo] a 1k\n");
  CHECK(finish({element("R[hi:lo] ?1 a 1k\n"), element(".param hi=1 lo=0\n")}) == "R[hi:lo] a 1k\n.param hi=1 lo=0\n");
}

TEST_CASE("post-processing: save lines", "[cap:schematic-netlisting][req:Element line post-processing]") {
  CHECK(finish({element(".SAVE ?2 V( a,b )\n")}) == ".save v(a)\n.save v(b)\n");
  CHECK(finish({element(".print ?1 I( r1 )\n")}) == ".print i(r1)\n");
  CHECK(finish({element(".save all\n")}) == ".save all\n");
}

TEST_CASE("post-processing: subcircuit lines", "[cap:schematic-netlisting][req:Element line post-processing]") {
  CHECK(finish({element(".subckt inv A[1],A[0] Y m=1 W=1\n")}) == ".subckt inv A[1] A[0] Y  W=1\n");
  CHECK(finish({element("**.subckt T A[1],A[0]\n")}) == "**.subckt T A[1],A[0]\n");
}

TEST_CASE("post-processing: a vector element past kMaxBusBytes is written unexpanded",
          "[cap:schematic-netlisting][req:Element line post-processing]") {
  // A hostile width marker: 2^20 names, each repeating a field of 2^20
  // items, would be terabytes of text.
  std::string items;
  for (int i = 0; i < 1000; ++i) items += (i == 0 ? "" : ",") + std::string(60, 'n');
  const std::string names = schem::join_bits(schem::expand_bus("R[1048575:0]"), ",");
  std::vector<hefesto::diag::Diagnostic> warnings;
  const auto lines = schem::postprocess(std::vector{element(names + " ?1048576 " + items + "\n")}, true, &warnings);
  REQUIRE(lines.size() == 1);
  REQUIRE(warnings.size() == 1);
  CHECK(warnings.front().code == "SCHEM-NET-006");
}

TEST_CASE("Long device line", "[cap:schematic-netlisting][req:Line breaking]") {
  const std::string line = ten_char_fields(30);
  const std::string out = schem::break_lines(std::vector<schem::ProcessedLine>{{line, false}});
  std::string expected = ten_char_fields(11) + "\n+";
  std::string second;
  for (int i = 11; i < 23; ++i) second += " " + ten_char_fields(i + 1).substr(static_cast<std::size_t>(i) * 11);
  expected += second + "\n+";
  for (int i = 23; i < 30; ++i) expected += " " + ten_char_fields(i + 1).substr(static_cast<std::size_t>(i) * 11);
  CHECK(out == expected + "\n");
}

TEST_CASE("line breaking: trailing blanks, comments and lines never wrapped", "[cap:schematic-netlisting][req:Line breaking]") {
  const std::string long_line = ten_char_fields(20);
  const auto broken = [](std::vector<schem::ProcessedLine> lines) { return schem::break_lines(lines); };
  CHECK(broken({{"R1 a b 1k \t", false}, {"   ", false}}) == "R1 a b 1k\n\n");
  CHECK(broken({{"* " + long_line, false}}).find("\n*+ ") != std::string::npos);
  CHECK(broken({{".include " + long_line, false}}) == ".include " + long_line + "\n");
  CHECK(broken({{"x.LIB " + long_line, false}}) == "x.LIB " + long_line + "\n");
  CHECK(broken({{"** sch_path: " + long_line, false}}) == "** sch_path: " + long_line + "\n");
  CHECK(broken({{long_line, true}}) == long_line + "\n");  // architecture code
  CHECK(broken({{long_line, false}}).find("\n+ ") != std::string::npos);
}

TEST_CASE("line breaking: the quote rule carries its state across lines", "[cap:schematic-netlisting][req:Line breaking]") {
  // A quote opened on one line stays open: the next line is read by the
  // quote rule and not wrapped inside the quote.
  const std::string spaced = ten_char_fields(20);
  const std::string out =
      schem::break_lines(std::vector<schem::ProcessedLine>{{"B1 a b v='1+", false}, {spaced + "'", false}});
  CHECK(out == "B1 a b v='1+\n" + spaced + "'\n");
  // Outside quotes the line wraps before the blank that passes 130.
  const std::string quoted = "B1 a b {x} " + spaced;
  const std::string wrapped = schem::break_lines(std::vector<schem::ProcessedLine>{{quoted, false}});
  CHECK(wrapped.find("\n+ ") != std::string::npos);
  CHECK(wrapped.find('\n') > 130);
}

// Regression (review of PR #80, point 1): the field after a width marker
// was split into its items once per name, so 200,000 names with a field of
// 200,000 items took about 4 x 10^10 steps although the text written is a
// few megabytes. The items are now split once per element.
TEST_CASE("post-processing: a marker's items are split once, not once per name",
          "[cap:schematic-netlisting][req:Element line post-processing]") {
  const std::string names = schem::join_bits(schem::expand_bus("R[199999:0]"), ",");
  const std::string items = schem::join_bits(schem::expand_bus("n[199999:0]"), ",");
  const auto start = std::chrono::steady_clock::now();
  const auto lines = schem::postprocess(std::vector{element(names + " ?1 " + items + " 1k\n")}, true, nullptr);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  REQUIRE(lines.size() == 200000);
  CHECK(lines.front().text == "R[199999] n[199999] 1k ");
  CHECK(lines.back().text == "R[0] n[0] 1k ");
  CHECK(elapsed < std::chrono::seconds{10});
}

// Regression (review of PR #80, point 8): each `PARAM:` was erased in
// place, shifting the rest of the field, so a field of 400,000 of them took
// about 5 x 10^11 byte moves. They are now removed in one pass, still
// without searching the text left behind again.
TEST_CASE("post-processing: PARAM: is removed in one pass",
          "[cap:schematic-netlisting][req:Element line post-processing]") {
  std::string field;
  for (int i = 0; i < 400000; ++i) field += "PARAM:";
  const auto start = std::chrono::steady_clock::now();
  const auto lines = schem::postprocess(std::vector{element("X1 ?1 a " + field + "w=1\n")}, true, nullptr);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  REQUIRE(lines.size() == 1);
  CHECK(lines.front().text == "X1 a w=1 ");
  CHECK(elapsed < std::chrono::seconds{10});
  CHECK(finish({element("X1 ?1 a PAPARAM:RAM:w=1\n")}) == "X1 a PARAM:w=1\n");
}
