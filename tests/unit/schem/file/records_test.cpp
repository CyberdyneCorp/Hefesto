// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The record grammar, brace strings, numeric fields and the graphic, text,
// wire and instance records of `.sch`/`.sym` files (capability
// schematic-file-format; openspec/changes/m8-schematic tasks.md 6.1).
//
// Owner: FORMAT.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <string>

#include "file_test_util.hpp"
#include "hefesto/schem/file/read_write.hpp"

namespace schem = hefesto::schem;
using schem::test::codes;
using schem::test::kEmptyHeader;
using schem::test::read_text;
using schem::test::round_trip;

TEST_CASE("CR-LF file reads like LF and is written with LF only",
          "[cap:schematic-file-format][req:File kinds and character handling]") {
  const auto r = read_text("K {type=nmos\r\nformat=\"@name\"}\r\n", schem::FileKind::symbol);
  CHECK(r.doc.globals.k == "type=nmos\nformat=\"@name\"");
  const std::string written = schem::write_document(r.doc);
  CHECK(written.find('\r') == std::string::npos);
  CHECK(written.find("K {type=nmos\nformat=\"@name\"}\n") != std::string::npos);
}

TEST_CASE("non-ASCII bytes are kept in brace strings",
          "[cap:schematic-file-format][req:File kinds and character handling]") {
  const std::string text = "T {\xc3\xa9t\xc3\xa9 \xe2\x84\xa6} 0 0 0 0 0.4 0.4 {}\n";
  const auto r = read_text(text);
  REQUIRE(r.doc.texts.size() == 1);
  CHECK(r.doc.texts[0].text == "\xc3\xa9t\xc3\xa9 \xe2\x84\xa6");
  CHECK(schem::write_document(r.doc) == std::string{kEmptyHeader} + text);
}

TEST_CASE("unknown record with a two-line brace string is skipped", "[cap:schematic-file-format][req:Record grammar]") {
  const auto r = read_text("Z 1 2 {a\nb} 3\nN 0 0 10 0 {}\n");
  REQUIRE(r.doc.wires.size() == 1);
  CHECK(r.doc.wires[0].p1 == schem::Point{0, 0});
  CHECK(r.doc.wires[0].p2 == schem::Point{10, 0});
  CHECK(r.warnings.empty());
}

TEST_CASE("comments, repeated globals and trailing text on a line", "[cap:schematic-file-format][req:Record grammar]") {
  const auto r = read_text("# comment {\nS {first}\nS {second} trailing {x\nN 0 0 1 1 {} more\n  \t\nG {g}");
  CHECK(r.doc.globals.s == "second");  // the last one read wins
  CHECK(r.doc.globals.g == "g");
  CHECK(r.doc.wires.size() == 1);
}

TEST_CASE("records may appear in any order and fields may span lines", "[cap:schematic-file-format][req:Record grammar]") {
  const auto r = read_text("N 0 0\n 10 0 {lab=A}\nv {xschem version=3.4.8RC file_version=1.3}\nL 4\n0 0 1 1\n{}\n");
  REQUIRE(r.doc.wires.size() == 1);
  CHECK(r.doc.wires[0].props == "lab=A");
  REQUIRE(r.doc.lines.size() == 1);
  CHECK(r.doc.file_version == "1.3");
}

TEST_CASE("escaped braces and backslashes round-trip", "[cap:schematic-file-format][req:Brace strings]") {
  const std::string value = "drc=\"f {@name\\}\"";
  CHECK(schem::brace_string(value) == "{drc=\"f \\{@name\\\\\\}\"}");
  const auto r = read_text("K " + schem::brace_string(value) + "\n", schem::FileKind::symbol);
  CHECK(r.doc.globals.k == value);
}

TEST_CASE("double-escaped quote in a PDK template", "[cap:schematic-file-format][req:Brace strings]") {
  const auto r = read_text("K {ad=\\\\\"expr('0.29')\\\\\"}\n", schem::FileKind::symbol);
  CHECK(r.doc.globals.k == "ad=\\\"expr('0.29')\\\"");
}

TEST_CASE("bytes before the opening brace are skipped and empty strings are written as {}",
          "[cap:schematic-file-format][req:Brace strings]") {
  const auto r = read_text("S junk{a\\b\\\\c}\nE {}\n");
  CHECK(r.doc.globals.s == "ab\\c");
  CHECK(r.doc.globals.e.empty());
  CHECK(schem::brace_string("") == "{}");
  CHECK(schem::brace_string("a\nb;c") == "{a\nb;c}");
}

TEST_CASE("end of file inside a brace string makes the field absent", "[cap:schematic-file-format][req:Brace strings]") {
  const auto r = read_text("N 0 0 10 0 {lab=A\\}");
  REQUIRE(r.doc.wires.size() == 1);
  CHECK(r.doc.wires[0].props.empty());
  CHECK(codes(r.warnings) == std::vector<std::string>{"SCHEM-FILE-003"});
  CHECK(r.warnings[0].text == "EOF reached, malformed {...} string input, missing close brace");
}

TEST_CASE("re-formatting a hand-edited coordinate", "[cap:schematic-file-format][req:Numeric fields]") {
  CHECK(round_trip("N 10.50 -20.0 30 -20 {}\n") == std::string{kEmptyHeader} + "N 10.5 -20 30 -20 {}\n");
}

TEST_CASE("numbers are printed with %.16g", "[cap:schematic-file-format][req:Numeric fields]") {
  schem::Document doc;
  doc.wires.push_back({{10.0, 0.1}, {1e-5, -0.0}, ""});
  doc.wires.push_back({{0.30000000000000004, 123456789012345678.0}, {1e300, -2.5e-300}, ""});
  doc.wires.push_back({{std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()},
                       {std::numeric_limits<double>::quiet_NaN(), 1}, ""});
  CHECK(schem::write_document(doc) == std::string{kEmptyHeader} +
                                          "N 10 0.1 1e-05 -0 {}\n"
                                          "N 0.3 1.234567890123457e+17 1e+300 -2.5e-300 {}\n"
                                          "N inf -inf nan 1 {}\n");
}

TEST_CASE("numbers are read with strtod syntax", "[cap:schematic-file-format][req:Numeric fields]") {
  constexpr double kInf = std::numeric_limits<double>::infinity();
  const auto r = read_text("N +1.5e1 .5 0x10 -INF {}\nN 1e400 -1e400 1e-400 5. {}\nN NaN 2 3 4 {}\n");
  REQUIRE(r.doc.wires.size() == 3);
  CHECK(r.doc.wires[0].p1 == schem::Point{15, 0.5});
  CHECK(r.doc.wires[0].p2 == schem::Point{16, -kInf});
  CHECK(r.doc.wires[1].p1 == schem::Point{0, 5});  // endpoints ordered: x2 < x1
  CHECK_FALSE(std::signbit(r.doc.wires[1].p1.x));
  CHECK(r.doc.wires[1].p2 == schem::Point{kInf, -kInf});
  CHECK(std::isnan(r.doc.wires[2].p1.x));
  CHECK(r.warnings.empty());
}

TEST_CASE("integer fields read as decimal integers", "[cap:schematic-file-format][req:Numeric fields]") {
  // `4.5` reads layer 4, then `.5` as the first coordinate, as C's fscanf.
  const auto r = read_text("L 4.5 1 2 3 {}\nL 99999999999 0 0 1 1 {}\n");
  REQUIRE(r.doc.lines.size() == 1);
  CHECK(r.doc.lines[0].layer == 4);
  CHECK(r.doc.lines[0].p1 == schem::Point{0.5, 1});
  CHECK(r.doc.lines[0].p2 == schem::Point{2, 3});
  CHECK(codes(r.warnings) == std::vector<std::string>{"SCHEM-FILE-002"});
}

TEST_CASE("out-of-range layer is skipped with a warning", "[cap:schematic-file-format][req:Layers]") {
  const auto r = read_text("L 30 0 0 10 10 {}\nL 4 0 0 10 10 {}\nB -1 0 0 1 1 {}\nA 22 0 0 1 0 360 {}\nP 21 2 0 0 1 1 {}\n");
  CHECK(r.doc.lines.size() == 1);
  CHECK(r.doc.boxes.empty());
  CHECK(r.doc.arcs.empty());
  CHECK(r.doc.polygons.size() == 1);
  REQUIRE(r.warnings.size() == 3);
  CHECK(r.warnings[0].code == "SCHEM-FILE-002");
  CHECK(r.warnings[0].text == "wrong or missing layer number for LINE object, ignoring");
  CHECK(r.warnings[1].text == "wrong or missing layer number for xRECT object, ignoring");
  CHECK(r.warnings[2].text == "wrong or missing layer number for ARC object, ignoring");
}

TEST_CASE("line endpoints normalised", "[cap:schematic-file-format][req:Line, box, arc and polygon records]") {
  CHECK(round_trip("L 4 20 0 10 5 {}\nL 4 0 10 0 -10 {}\n") ==
        std::string{kEmptyHeader} + "L 4 10 5 20 0 {}\nL 4 0 -10 0 10 {}\n");
}

TEST_CASE("box corners normalised", "[cap:schematic-file-format][req:Line, box, arc and polygon records]") {
  CHECK(round_trip("B 5 2.5 2.5 -2.5 -2.5 {name=A dir=in}\n") ==
        std::string{kEmptyHeader} + "B 5 -2.5 -2.5 2.5 2.5 {name=A dir=in}\n");
  CHECK(round_trip("B 5 -1 3 1 -3 {}\n") == std::string{kEmptyHeader} + "B 5 -1 -3 1 3 {}\n");
}

TEST_CASE("arcs are stored as read and polygons keep their points",
          "[cap:schematic-file-format][req:Line, box, arc and polygon records]") {
  const auto r = read_text("A 4 0 0 15 450 -720 {fill=true}\nP 4 4 -30 -20 -40 0 -30 20 -30 -20 {bezier=true}\nP 9 0 {}\n");
  REQUIRE(r.doc.arcs.size() == 1);
  CHECK(r.doc.arcs[0].start_angle == 450);
  CHECK(r.doc.arcs[0].sweep_angle == -720);
  REQUIRE(r.doc.polygons.size() == 2);
  CHECK(r.doc.polygons[0].points.size() == 4);
  CHECK(r.doc.polygons[0].points.front() == r.doc.polygons[0].points.back());  // closed
  CHECK(r.doc.polygons[1].points.empty());
  CHECK(schem::write_document(r.doc) == std::string{kEmptyHeader} +
                                            "A 4 0 0 15 450 -720 {fill=true}\n"
                                            "P 4 4 -30 -20 -40 0 -30 20 -30 -20 {bezier=true}\n"
                                            "P 9 0 {}\n");
}

TEST_CASE("a polygon with fewer pairs than its count is skipped",
          "[cap:schematic-file-format][req:Line, box, arc and polygon records]") {
  const auto r = read_text("P 4 3 0 0 1 1 {}\nP 4 -1 {}\nP 4 2000000000 0 0\nN 0 0 1 0 {}\nN 5 5 6 6 {}\n");
  CHECK(r.doc.polygons.empty());
  // The count-2e9 polygon is cut at `N` of the next line, which goes with it.
  REQUIRE(r.doc.wires.size() == 1);
  CHECK(r.doc.wires[0].p1 == schem::Point{5, 5});
  CHECK(codes(r.warnings) == std::vector<std::string>{"SCHEM-FILE-001", "SCHEM-FILE-001", "SCHEM-FILE-001"});
  CHECK(r.warnings[0].text == "missing fields for POLYGON object, ignoring");
}

TEST_CASE("multi-line text written back byte-identically", "[cap:schematic-file-format][req:Text records]") {
  const std::string record = "T {line one\nline two} 100 -50 0 0 0.4 0.4 {layer=4}\n";
  const auto r = read_text(record);
  REQUIRE(r.doc.texts.size() == 1);
  CHECK(r.doc.texts[0].text == "line one\nline two");
  CHECK(r.doc.texts[0].x_scale == 0.4);
  CHECK(schem::write_document(r.doc) == std::string{kEmptyHeader} + record);
}

TEST_CASE("a text with a missing numeric field is skipped", "[cap:schematic-file-format][req:Text records]") {
  const auto r = read_text("T {a} 1 2 0 0 0.4\nN 0 0 1 0 {}\nT {b} 1 2 x 0 0.4 0.4 {}\nN 0 0 2 0 {}\n");
  CHECK(r.doc.texts.empty());
  REQUIRE(r.doc.wires.size() == 1);
  CHECK(r.doc.wires[0].p2.x == 2);
  CHECK(r.warnings.size() == 2);
  CHECK(r.warnings[0].text == "missing fields for TEXT object, ignoring");
}

TEST_CASE("wire with stored net name", "[cap:schematic-file-format][req:Wire records]") {
  const auto r = read_text("N 890 -110 890 -130 {lab=ANALOG_GND}\n");
  REQUIRE(r.doc.wires.size() == 1);
  CHECK(r.doc.wires[0].p1 == schem::Point{890, -130});
  CHECK(r.doc.wires[0].p2 == schem::Point{890, -110});
  CHECK(r.doc.wires[0].props == "lab=ANALOG_GND");
}

TEST_CASE("instance read and written byte-identically", "[cap:schematic-file-format][req:Component instance records]") {
  const std::string record = "C {capa.sym} 890 -160 0 0 {name=C4 m=1 value=10u device=\"tantalium capacitor\"}\n";
  const auto r = read_text(std::string{kEmptyHeader} + record);
  REQUIRE(r.doc.instances.size() == 1);
  CHECK(r.doc.instances[0].symbol == "capa.sym");
  CHECK(r.doc.instances[0].origin == schem::Point{890, -160});
  CHECK(schem::write_document(r.doc) == std::string{kEmptyHeader} + record);
}

TEST_CASE("an instance with missing fields is skipped; an unknown symbol is kept",
          "[cap:schematic-file-format][req:Component instance records]") {
  const auto r = read_text("v {xschem version=3.4.8RC file_version=1.3}\nC {res.sym} 0 0 0\nC {nowhere/x.sym} 1 2 3 1 {name=X1}\n");
  CHECK(r.doc.instances.empty());  // the second line went with the first record
  CHECK(r.warnings.size() == 1);
  CHECK(r.warnings[0].text == "missing fields for INSTANCE object, ignoring");
  const auto kept = read_text("v {xschem version=3.4.8RC file_version=1.3}\nC {nowhere/x.sym} 1 2 3 1 {name=X1}\n");
  REQUIRE(kept.doc.instances.size() == 1);
  CHECK(kept.doc.instances[0].rotation == 3);
  CHECK(kept.doc.instances[0].flip == 1);
  CHECK(schem::write_document(kept.doc) == std::string{kEmptyHeader} + "C {nowhere/x.sym} 1 2 3 1 {name=X1}\n");
}

TEST_CASE("truncated wire takes the next line with it", "[cap:schematic-file-format][req:Malformed input]") {
  const auto r = read_text("N 0 0 10\nN 0 10 20 10 {}\nN 0 0 10 0 {}\n");
  REQUIRE(r.doc.wires.size() == 1);
  CHECK(r.doc.wires[0].p2 == schem::Point{10, 0});
  CHECK(codes(r.warnings) == std::vector<std::string>{"SCHEM-FILE-001"});
  CHECK(r.warnings[0].text == "missing fields for WIRE object, ignoring");
}

TEST_CASE("unterminated property at the end of the file", "[cap:schematic-file-format][req:Malformed input]") {
  const auto r = read_text("N 0 0 10 0 {lab=A\n");
  REQUIRE(r.doc.wires.size() == 1);
  CHECK(r.doc.wires[0].props.empty());
  CHECK(codes(r.warnings) == std::vector<std::string>{"SCHEM-FILE-003"});
  CHECK(schem::write_document(r.doc) == std::string{kEmptyHeader} + "N 0 0 10 0 {}\n");
}

TEST_CASE("malformed records are reported with their line", "[cap:schematic-file-format][req:Malformed input]") {
  const auto r = read_text("\n\nN 1 x\nL 99 0 0 1 1 {}\n");
  REQUIRE(r.warnings.size() == 2);
  const auto* first = std::get_if<hefesto::diag::SourceLocation>(&r.warnings[0].location);
  REQUIRE(first != nullptr);
  CHECK(first->line == 3);
  const auto* second = std::get_if<hefesto::diag::SourceLocation>(&r.warnings[1].location);
  REQUIRE(second != nullptr);
  CHECK(second->line == 4);
}

TEST_CASE("hostile input is read in bounded work", "[cap:schematic-file-format][req:Malformed input]") {
  // Deep nesting of embedded blocks, a huge polygon count and stray braces.
  std::string text;
  for (int i = 0; i < 20000; ++i) {
    text += "C {s" + std::to_string(i) + "} 0 0 0 0 {}\n[\n";
  }
  text += "P 1 2147483647 0 0 {\n}}}}{{{\n";
  const auto r = read_text(text);
  CHECK(r.doc.instances.size() == 1);
  CHECK(r.doc.embedded.size() == 20000);
  CHECK(r.doc.embedded.back().symbol.instances.empty());
  CHECK(r.doc.embedded.front().symbol.instances.size() == 1);
  CHECK_FALSE(r.warnings.empty());
}
