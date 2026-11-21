// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Embedded symbols, the coordinate system, the orientation transform, the
// canonical write order and the annotated example (capability
// schematic-file-format; Q-SCHEM-2; openspec/changes/m8-schematic tasks.md
// 6.3).
//
// Owner: FORMAT.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "file_test_util.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/file/transform.hpp"

namespace schem = hefesto::schem;
using schem::test::kEmptyHeader;
using schem::test::read_text;
using schem::test::round_trip;

namespace {

constexpr std::string_view kResBlock =
    "[\n"
    "v {xschem version=3.4.8RC file_version=1.3}\n"
    "K {type=resistor}\n"
    "G {}\nV {}\nS {}\nE {}\n"
    "L 4 0 -30 0 30 {}\n"
    "B 5 -2.5 -32.5 2.5 -27.5 {name=P dir=inout}\n"
    "]\n";

std::size_t count(const std::string& text, std::string_view needle) {
  std::size_t n = 0;
  for (auto at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) {
    ++n;
  }
  return n;
}

}  // namespace

TEST_CASE("one embedded copy", "[cap:schematic-file-format][req:Embedded symbols]") {
  const std::string text = std::string{kEmptyHeader} +
                           "C {res.sym} 0 0 0 0 {name=R1}\n"
                           "C {res.sym} 10 0 0 0 {name=R2 embed=true}\n" +
                           std::string{kResBlock} + "C {res.sym} 20 0 0 0 {name=R3 embed=true}\n";
  const auto r = read_text(text);
  REQUIRE(r.doc.embedded.size() == 1);
  CHECK(r.doc.embedded[0].reference == "res.sym");
  CHECK(r.doc.embedded[0].symbol.kind == schem::FileKind::symbol);
  CHECK(r.doc.embedded[0].symbol.globals.k == "type=resistor");
  CHECK(r.doc.embedded[0].symbol.boxes.size() == 1);
  const std::string written = schem::write_document(r.doc);
  CHECK(written == text);
  CHECK(count(written, "[\n") == 1);
}

TEST_CASE("reading a block sets embed=true and skips a second definition", "[cap:schematic-file-format][req:Embedded symbols]") {
  const std::string text = std::string{kEmptyHeader} + "C {res.sym} 0 0 0 0 {name=R1}\n" + std::string{kResBlock} +
                           "C {res.sym} 10 0 0 0 {name=R2}\n[\nK {type=other}\n]\n";
  const auto r = read_text(text);
  REQUIRE(r.doc.instances.size() == 2);
  CHECK(r.doc.instances[0].props == "name=R1\nembed=true");
  CHECK(r.doc.instances[1].props == "name=R2\nembed=true");
  REQUIRE(r.doc.embedded.size() == 1);
  CHECK(r.doc.embedded[0].symbol.globals.k == "type=resistor");
  const std::string written = schem::write_document(r.doc);
  CHECK(count(written, "[\n") == 1);
  CHECK(written.find("C {res.sym} 0 0 0 0 {name=R1\nembed=true}\n[\n") != std::string::npos);
}

TEST_CASE("a block with no preceding instance is skipped with a warning", "[cap:schematic-file-format][req:Embedded symbols]") {
  const auto r = read_text("C {a.sym} 0 0 0 0 {}\nN 0 0 1 1 {}\n[\nK {type=x}\nN 5 5 6 6 {}\n]\nN 2 2 3 3 {}\n");
  CHECK(r.doc.embedded.empty());
  CHECK(r.doc.wires.size() == 2);
  REQUIRE(r.warnings.size() == 1);
  CHECK(r.warnings[0].code == "SCHEM-FILE-004");
}

TEST_CASE("an unclosed block is kept with a warning", "[cap:schematic-file-format][req:Embedded symbols]") {
  const auto r = read_text("C {a.sym} 0 0 0 0 {}\n[\nK {type=x}\nL 4 0 0 1 1 {}\n");
  REQUIRE(r.doc.embedded.size() == 1);
  CHECK(r.doc.embedded[0].symbol.lines.size() == 1);
  REQUIRE(r.warnings.size() == 1);
  CHECK(r.warnings[0].code == "SCHEM-FILE-005");
}

TEST_CASE("the block layout is xschem's in both modes", "[cap:schematic-file-format][req:Embedded symbols]") {
  // Header text and F of the symbol are dropped, polygons follow texts,
  // wires become L 1 after the other layer-1 lines, instances are left out.
  const std::string block_in =
      "[\nv {xschem version=3.4.8RC file_version=1.3\nheader}\nG {g}\nK {type=subcircuit}\nV {v}\nS {s}\nF {f}\nE {e}\n"
      "P 4 2 0 0 1 1 {}\nT {t} 0 0 0 0 0.2 0.2 {}\nN 9 9 8 8 {lab=A}\nL 1 0 0 5 0 {}\nL 0 1 1 2 2 {}\n"
      "C {inner.sym} 0 0 0 0 {}\nA 3 0 0 1 0 360 {}\nB 2 0 0 1 1 {}\n]\n";
  const std::string block_out =
      "[\nv {xschem version=3.4.8RC file_version=1.3}\nK {type=subcircuit}\nG {}\nV {}\nS {}\nE {}\n"
      "L 0 1 1 2 2 {}\nL 1 0 0 5 0 {}\nL 1 8 8 9 9 {lab=A}\nB 2 0 0 1 1 {}\nA 3 0 0 1 0 360 {}\n"
      "T {t} 0 0 0 0 0.2 0.2 {}\nP 4 2 0 0 1 1 {}\n]\n";
  for (const bool compat : {true, false}) {
    const std::string in = std::string{kEmptyHeader} + "C {blk.sch} 0 0 0 0 {embed=true}\n" + block_in;
    CHECK(round_trip(in, schem::FileKind::schematic, compat) ==
          std::string{kEmptyHeader} + "C {blk.sch} 0 0 0 0 {embed=true}\n" + block_out);
  }
}

TEST_CASE("blocks nested in blocks are listed in the outer document", "[cap:schematic-file-format][req:Embedded symbols]") {
  const auto r = read_text("C {a.sym} 0 0 0 0 {}\n[\nC {b.sym} 0 0 0 0 {}\n[\nK {type=b}\n]\nK {type=a}\n]\n");
  REQUIRE(r.doc.embedded.size() == 2);
  CHECK(r.doc.embedded[0].reference == "a.sym");
  CHECK(r.doc.embedded[0].symbol.globals.k == "type=a");
  CHECK(r.doc.embedded[0].symbol.embedded.empty());
  CHECK(r.doc.embedded[1].reference == "b.sym");
  CHECK(r.doc.embedded[1].symbol.globals.k == "type=b");
  CHECK(r.warnings.empty());
}

TEST_CASE("an instance with embed=true and no definition is written alone",
          "[cap:schematic-file-format][req:Embedded symbols]") {
  schem::Document doc;
  doc.instances.push_back({"x.sym", {0, 0}, 0, 0, "embed=1"});
  CHECK(schem::write_document(doc) == std::string{kEmptyHeader} + "C {x.sym} 0 0 0 0 {embed=1}\n");
}

TEST_CASE("pin centre", "[cap:schematic-file-format][req:Coordinate system]") {
  const auto r = read_text("B 5 -22.5 -2.5 -17.5 2.5 {name=G dir=in}\n", schem::FileKind::symbol);
  REQUIRE(r.doc.boxes.size() == 1);
  CHECK(schem::box_centre(r.doc.boxes[0]) == schem::Point{-20, 0});
}

TEST_CASE("rotated pin position", "[cap:schematic-file-format][req:Orientation transform]") {
  CHECK(schem::transform_point({{100, 200}, 1, 0}, {-20, 0}) == schem::Point{100, 180});
}

TEST_CASE("flipped pin position", "[cap:schematic-file-format][req:Orientation transform]") {
  CHECK(schem::transform_point({{100, 200}, 0, 1}, {-20, 0}) == schem::Point{120, 200});
}

TEST_CASE("the orientation table for every rotation and flip", "[cap:schematic-file-format][req:Orientation transform]") {
  const schem::Point p{3, 7};
  CHECK(schem::transform_point({{0, 0}, 0, 0}, p) == schem::Point{3, 7});
  CHECK(schem::transform_point({{0, 0}, 1, 0}, p) == schem::Point{-7, 3});
  CHECK(schem::transform_point({{0, 0}, 2, 0}, p) == schem::Point{-3, -7});
  CHECK(schem::transform_point({{0, 0}, 3, 0}, p) == schem::Point{7, -3});
  CHECK(schem::transform_point({{0, 0}, 0, 1}, p) == schem::Point{-3, 7});
  CHECK(schem::transform_point({{0, 0}, 1, 1}, p) == schem::Point{-7, -3});
  CHECK(schem::transform_point({{0, 0}, 2, 1}, p) == schem::Point{3, -7});
  CHECK(schem::transform_point({{0, 0}, 3, 1}, p) == schem::Point{7, 3});
}

TEST_CASE("texts and arcs follow the placement", "[cap:schematic-file-format][req:Orientation transform]") {
  using schem::TextOrientation;
  CHECK(schem::transform_text({{0, 0}, 1, 0}, 0, 0) == TextOrientation{1, 0});
  CHECK(schem::transform_text({{0, 0}, 3, 0}, 2, 1) == TextOrientation{1, 1});
  CHECK(schem::transform_text({{0, 0}, 1, 1}, 1, 0) == TextOrientation{0, 1});  // odd text, flipped: r + 2
  CHECK(schem::transform_text({{0, 0}, 1, 1}, 2, 1) == TextOrientation{3, 0});
  CHECK(schem::transform_arc_start({{0, 0}, 0, 0}, 90, 180) == 90);
  CHECK(schem::transform_arc_start({{0, 0}, 1, 0}, 90, 180) == 0);
  CHECK(schem::transform_arc_start({{0, 0}, 3, 0}, 45, 90) == 135);
  CHECK(schem::transform_arc_start({{0, 0}, 0, 1}, 90, 180) == 270);  // fmod(180 - 180 - 90) + 360
  CHECK(schem::transform_arc_start({{0, 0}, 1, 1}, 0, 90) == 0);      // fmod(270 + 180 - 90, 360) = 0
}

TEST_CASE("layers regrouped", "[cap:schematic-file-format][req:Canonical write order]") {
  const auto written = round_trip(
      "L 4 0 0 1 1 {a}\nB 5 0 0 1 1 {}\nL 3 0 0 1 1 {}\nT {t} 0 0 0 0 1 1 {}\nN 0 0 1 1 {}\nL 4 0 0 1 1 {b}\n");
  CHECK(written == std::string{kEmptyHeader} +
                       "L 3 0 0 1 1 {}\nL 4 0 0 1 1 {a}\nL 4 0 0 1 1 {b}\nB 5 0 0 1 1 {}\nT {t} 0 0 0 0 1 1 {}\n"
                       "N 0 0 1 1 {}\n");
}

TEST_CASE("every record kind in canonical order", "[cap:schematic-file-format][req:Canonical write order]") {
  const auto written = round_trip(
      "C {c.sym} 1 2 1 0 {name=x}\nN 1 1 2 2 {}\nT {t} 1 2 3 1 0.5 0.25 {}\nP 7 2 0 0 1 1 {}\nP 6 1 5 5 {}\n"
      "A 9 0 0 1 0 90 {}\nA 2 0 0 1 0 90 {}\nB 8 0 0 1 1 {}\nB 6 0 0 1 1 {}\nL 21 0 0 1 1 {}\nL 0 0 0 1 1 {}\n"
      "S {.option temp=27}\nv {xschem version=1 file_version=1.3}\n");
  CHECK(written ==
        "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {.option temp=27}\nF {}\nE {}\n"
        "L 0 0 0 1 1 {}\nL 21 0 0 1 1 {}\nB 6 0 0 1 1 {}\nB 8 0 0 1 1 {}\nA 2 0 0 1 0 90 {}\nA 9 0 0 1 0 90 {}\n"
        "P 6 1 5 5 {}\nP 7 2 0 0 1 1 {}\nT {t} 1 2 3 1 0.5 0.25 {}\nN 1 1 2 2 {}\nC {c.sym} 1 2 1 0 {name=x}\n");
}

TEST_CASE("the annotated symbol and schematic", "[cap:schematic-file-format][req:Complete annotated example]") {
  const std::string symbol =
      "v {xschem version=3.4.8RC file_version=1.3}\n"
      "G {}\n"
      "K {type=regulator\n"
      "format=\"x@name @pinlist r@symname\"\n"
      "verilog_format=\"assign @#2 = @#0 ;\"\n"
      "template=\"name=U1 footprint=TO220\"}\n"
      "V {}\nS {}\nF {}\nE {}\n"
      "L 4 -60 0 -50 0 {}\n"
      "L 4 -50 -20 50 -20 {}\n"
      "B 5 -62.5 -2.5 -57.5 2.5 {name=IN dir=in pinnumber=1}\n"
      "B 5 -2.5 27.5 2.5 32.5 {name=GND dir=inout pinnumber=2}\n"
      "B 5 57.5 -2.5 62.5 2.5 {name=OUT dir=out pinnumber=3}\n"
      "T {@name} -17.5 -15 0 0 0.2 0.2 {}\n"
      "T {@symname} -17.5 0 0 0 0.2 0.2 {}\n";
  const auto sym = read_text(symbol, schem::FileKind::symbol);
  CHECK(schem::write_document(sym.doc) == symbol);
  CHECK(schem::parse_properties(sym.doc.globals.k).size() == 4);
  CHECK(schem::get_token(sym.doc.globals.k, "format") == "x@name @pinlist r@symname");
  CHECK(schem::get_token(sym.doc.globals.k, "template") == "name=U1 footprint=TO220");

  const std::string schematic =
      "v {xschem version=3.4.8RC file_version=1.3}\n"
      "G {}\nK {}\nV {}\n"
      "S {.option temp=27}\n"
      "F {}\nE {}\n"
      "N 0 0 40 0 {lab=VIN}\n"
      "C {lab_pin.sym} 0 0 0 0 {name=p1 lab=VIN}\n"
      "C {7805.sym} 100 0 0 0 {name=U1 footprint=TO220}\n";
  const auto sch = read_text(schematic);
  CHECK(schem::write_document(sch.doc) == schematic);
  CHECK(sch.doc.globals.s == ".option temp=27");
  REQUIRE(sch.doc.instances.size() == 2);
  const auto& u1 = sch.doc.instances[1];
  const schem::Placement placement{u1.origin, u1.rotation, u1.flip};
  REQUIRE(sym.doc.boxes.size() == 3);
  CHECK(schem::transform_point(placement, schem::box_centre(sym.doc.boxes[0])) == schem::Point{40, 0});
  CHECK(schem::transform_point(placement, schem::box_centre(sym.doc.boxes[1])) == schem::Point{100, 30});
  CHECK(schem::transform_point(placement, schem::box_centre(sym.doc.boxes[2])) == schem::Point{160, 0});
}
