// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// oasis::write: command naming, options, file layout, names, translation,
// modal omission, properties, strict and non-strict files, END and
// validation, determinism, error propagation and the 419-byte reference
// file (tasks.md 12.1-12.3, 12.6). Owner: OASW.

#include <catch2/catch_test_macros.hpp>

#include <atomic>

#include "hefesto/oasis/codec/validate.hpp"
#include "oasis_write_fixture.hpp"

using namespace oasis_write_test;  // NOLINT(google-build-using-namespace)

namespace {

/// The reference cell of "Complete annotated example": top with metal1
/// (0,0)-(4,2), defaults except `compress 0`.
std::string reference_write(OasFixture& f) {
  f.settings.compress = 0;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
  return f.write_oas(top);
}

}  // namespace

TEST_CASE("oasis write of the reference cell gives the 419-byte file",
          "[cap:oasis-write][req:Complete annotated example]") {
  for (const bool on : {true, false}) {
    OasFixture f(on);
    const std::string bytes = reference_write(f);
    INFO("magic-compat " << on);
    CHECK(f.last.ok());
    CHECK(bytes.size() == 419);
    CHECK(bytes == reference_419());
  }
}

TEST_CASE("output file naming appends .oas only to a name without a dot",
          "[cap:oasis-write][req:Write command and output file naming]") {
  CHECK(oasis::output_file_name("out/inv") == "out/inv.oas");
  CHECK(oasis::output_file_name("inv") == "inv.oas");
  CHECK(oasis::output_file_name("lib.v1/inv") == "lib.v1/inv");
  CHECK(oasis::output_file_name("inv.oasis") == "inv.oasis");
  db::CellDef lib{"libs/top", 1};
  CHECK(oasis::default_output_name(lib) == "top");
  CHECK(oasis::output_file_name(oasis::default_output_name(lib)) == "top.oas");
}

TEST_CASE("oasis write creates the named file, never gzip-wrapped",
          "[cap:oasis-write][req:Write command and output file naming]") {
  OasFixture f;
  REQUIRE(f.fs.create_directories("/out").ok());
  db::CellDef& inv = f.cell("inv");
  f.paint(inv, f.m1, db::Rect{0, 0, 4, 2});
  const std::string bytes = f.write_oas(inv, oasis::output_file_name("/out/inv"));
  REQUIRE(f.last.ok());
  CHECK(f.fs.exists("/out/inv.oas"));
  CHECK(bytes.rfind("%SEMI-OASIS\r\n", 0) == 0);
}

TEST_CASE("a write whose prepare phase fails creates no file",
          "[cap:oasis-write][req:Write command and output file naming]") {
  OasFixture f;
  db::CellDef& top = f.cell("top");
  f.install();
  f.ed.mask.output_styles.styles.clear();
  f.ed.mask.output_styles.current.reset();
  oasis::OasisWriteOptions o;
  o.prepare.files = &f.fs;
  f.last = oasis::write(f.ed, top, "/none.oas", o);
  CHECK_FALSE(f.last.ok());
  CHECK(has(f.last, "GDS-WRITE-002"));
  CHECK_FALSE(f.fs.exists("/none.oas"));
}

TEST_CASE("a cancelled write leaves no file", "[cap:oasis-write][req:Write command and output file naming]") {
  OasFixture f;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
  std::atomic<bool> cancel{true};
  mask::Progress progress;
  progress.cancel = &cancel;
  const std::string bytes = f.write_oas(top, "/cancelled.oas", &progress);
  CHECK_FALSE(f.last.ok());
  CHECK(bytes.empty());
  CHECK_FALSE(f.fs.exists("/cancelled.oas"));
  CHECK_FALSE(f.fs.exists("/.cancelled.oas.hefesto-tmp"));
}

TEST_CASE("compress 0 writes no CBLOCK; a large cell is compressed at compress 6",
          "[cap:oasis-write][req:Write option settings]") {
  OasFixture f;
  db::CellDef& top = f.cell("top");
  for (int i = 0; i < 400; ++i) {
    f.paint(top, f.m1, db::Rect{i * 10, (i * 7) % 50, i * 10 + 4, (i * 7) % 50 + 3});
  }
  f.settings.compress = 0;
  const Decoded plain = decode_oasis(f.write_oas(top));
  CHECK_FALSE(plain.any_in_cblock());
  f.settings.compress = 6;
  const Decoded packed = decode_oasis(f.write_oas(top));
  CHECK(packed.any_in_cblock());
  CHECK(packed.elements("top") == plain.elements("top"));
}

TEST_CASE("the shared write options act on the GDS element stream", "[cap:oasis-write][req:Write option settings]") {
  OasFixture f;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 10, 10});
  top.labels().push_back(plain_label("VDD", f.m1, db::Rect{5, 5, 5, 5}));
  f.style.label_layer[f.m1] = 0;
  f.settings.compress = 0;
  Decoded d = decode_oasis(f.write_oas(top));
  CHECK(d.count(oasis::RecordId::text) == 1);
  f.settings.labels = false;
  d = decode_oasis(f.write_oas(top));
  CHECK(d.count(oasis::RecordId::text) == 0);
  f.settings.layernames = false;
  d = decode_oasis(f.write_oas(top));
  CHECK(d.names.layernames.empty());
}

TEST_CASE("cells, order and references are gds-write's", "[cap:oasis-write][req:GDS-consistent content]") {
  OasFixture f;
  f.settings.compress = 0;
  db::CellDef& c = f.cell("c");
  db::CellDef& b = f.cell("b");
  db::CellDef& a = f.cell("a");
  db::CellDef& top = f.cell("top");
  f.paint(c, f.m1, db::Rect{0, 0, 2, 2});
  f.place(b, c);
  f.paint(a, f.m1, db::Rect{0, 0, 3, 3});
  f.place(top, b);
  f.place(top, a, db::Transform{1, 0, 20, 0, 1, 0});
  const std::vector<Rec> gds = decode(f.write(top));
  const Decoded d = decode_oasis(f.write_oas(top));
  CHECK(d.cells() == structure_names(gds));
  const std::vector<oasis::Element> placements = d.elements("top");
  REQUIRE(placements.size() == 2);
  for (const oasis::Element& p : placements) {
    CHECK(p.kind == oasis::ElementKind::placement);
    CHECK(p.position == (p.name == "a" ? oasis::Point64{2000, 0} : oasis::Point64{0, 0}));
  }
}

TEST_CASE("Ångström styles write unit 10000", "[cap:oasis-write][req:File layout]") {
  OasFixture f;
  f.style.options |= mask::style_option::angstroms;
  f.settings.compress = 0;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
  const std::string bytes = f.write_oas(top);
  CHECK(bytes.substr(13, 9) == std::string{"\x01\x03\x31\x2E\x30\x00\x90\x4E\x01", 9});
}

TEST_CASE("the rectangle has the GDS XY values", "[cap:oasis-write][req:Units]") {
  OasFixture f;
  const Decoded d = decode_oasis(reference_write(f));
  const std::vector<oasis::Element> e = d.elements("top");
  REQUIRE(e.size() == 1);
  CHECK(e[0].kind == oasis::ElementKind::rectangle);
  CHECK(e[0].position == oasis::Point64{0, 0});
  CHECK(e[0].size == oasis::Point64{400, 200});
}

TEST_CASE("post-order CELLNAME refnums", "[cap:oasis-write][req:Reference numbers and name tables]") {
  OasFixture f;
  f.settings.compress = 0;
  db::CellDef& c = f.cell("c");
  db::CellDef& b = f.cell("b");
  db::CellDef& a = f.cell("a");
  db::CellDef& top = f.cell("top");
  f.paint(c, f.m1, db::Rect{0, 0, 1, 1});
  f.place(b, c);
  f.place(top, b);
  f.paint(a, f.m1, db::Rect{0, 0, 1, 1});
  f.place(top, a, db::Transform{1, 0, 10, 0, 1, 0});
  const std::vector<std::string> order = structure_names(decode(f.write(top)));
  const Decoded d = decode_oasis(f.write_oas(top));
  REQUIRE(order.size() == 4);
  REQUIRE(d.names.cellnames.size() == 4);
  for (std::uint64_t i = 0; i < 4; ++i) {
    CHECK(d.names.cellnames.at(i) == order[i]);  // written in gds-write's post-order, each before any reference
  }
  // S_GDS_PROPERTY is PROPNAME 4, defined because references are written.
  CHECK(d.names.propnames.at(4) == "S_GDS_PROPERTY");
  CHECK(d.names.propstrings.at(0) == "top");
}

TEST_CASE("cell names follow gds-write's mapping", "[cap:oasis-write][req:Cell names]") {
  OasFixture f;
  f.settings.compress = 0;
  db::CellDef& nand = f.cell("nand-2");
  f.paint(nand, f.m1, db::Rect{0, 0, 1, 1});
  db::CellDef& ab = f.cell("a_b");
  f.paint(ab, f.m1, db::Rect{0, 0, 1, 1});
  db::CellDef& dash = f.cell("a-b");
  f.paint(dash, f.m1, db::Rect{0, 0, 1, 1});
  db::CellDef& top = f.cell("top");
  f.place(top, nand);
  f.place(top, dash, db::Transform{1, 0, 10, 0, 1, 0});
  f.place(top, ab, db::Transform{1, 0, 20, 0, 1, 0});
  const Decoded d = decode_oasis(f.write_oas(top));
  const std::vector<std::string> cells = d.cells();
  CHECK(std::find(cells.begin(), cells.end(), "nand_2") != cells.end());
  CHECK(std::find(cells.begin(), cells.end(), "a_b") != cells.end());
  CHECK(std::count_if(cells.begin(), cells.end(), [](const std::string& s) { return s.rfind("XXXXX", 0) == 0; }) == 1);
}

TEST_CASE("a name with a space is illegal for OASIS", "[cap:oasis-write][req:Cell names]") {
  OasFixture f;
  f.style.options |= mask::style_option::permissive_labels;
  f.settings.compress = 0;
  db::CellDef& spaced = f.cell("a b");
  f.paint(spaced, f.m1, db::Rect{0, 0, 1, 1});
  db::CellDef& top = f.cell("top");
  f.place(top, spaced);
  const Decoded d = decode_oasis(f.write_oas(top));
  const std::vector<std::string> cells = d.cells();
  REQUIRE(cells.size() == 2);
  CHECK(cells[0].rfind("XXXXX", 0) == 0);
  CHECK(d.elements("top").at(0).name == cells[0]);  // the placement uses the same name
}

TEST_CASE("label strings are gds-write's STRING bytes", "[cap:oasis-write][req:String mapping]") {
  OasFixture f;
  f.settings.compress = 0;
  f.style.label_layer[f.m1] = 0;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 10, 10});
  top.labels().push_back(plain_label("A<0>", f.m1, db::Rect{1, 1, 1, 1}));
  top.labels().push_back(plain_label("B\x01" "C", f.m1, db::Rect{2, 2, 2, 2}));
  const Decoded d = decode_oasis(f.write_oas(top));
  std::vector<std::string> texts;
  for (const oasis::Element& e : d.elements("top")) {
    if (e.kind == oasis::ElementKind::text) {
      texts.push_back(e.name);
    }
  }
  CHECK(texts == std::vector<std::string>{"A_0_", "B_C"});
}

TEST_CASE("an unmerged rectangle is a RECTANGLE record", "[cap:oasis-write][req:Boundary translation]") {
  OasFixture f;
  const std::string bytes = reference_write(f);
  CHECK(bytes.substr(39, 8) == std::string{"\x14\x63\x31\x00\x90\x03\xC8\x01", 8});
}

TEST_CASE("a merged L-shape is a type 0 POLYGON", "[cap:oasis-write][req:Boundary translation]") {
  OasFixture f;
  f.settings.compress = 0;
  f.settings.merge = true;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
  f.paint(top, f.m1, db::Rect{0, 2, 2, 4});
  const Decoded d = decode_oasis(f.write_oas(top));
  const std::vector<oasis::Element> e = d.elements("top");
  REQUIRE(e.size() == 1);
  CHECK(e[0].kind == oasis::ElementKind::polygon);
  CHECK(e[0].point_list_type == std::uint8_t{0});
  CHECK(e[0].position == oasis::Point64{200, 400});
  CHECK(e[0].points == std::vector<oasis::Point64>{{-200, 0}, {-200, -400}, {200, -400}, {200, -200}, {0, -200}});
}

TEST_CASE("the second rectangle on a layer omits its modal fields", "[cap:oasis-write][req:Modal field omission]") {
  OasFixture f;
  f.settings.compress = 0;
  f.settings.repetitions = false;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
  f.paint(top, f.m1, db::Rect{10, 0, 14, 2});
  const std::string bytes = f.write_oas(top);
  CHECK(contains(bytes, {0x14, 0x63, 0x31, 0x00, 0x90, 0x03, 0xC8, 0x01, 0x14, 0x10, 0xD0, 0x0F}));
}

TEST_CASE("a point label is a TEXT record; font labels are counted", "[cap:oasis-write][req:Text translation]") {
  OasFixture f;
  f.settings.compress = 0;
  f.style.label_layer[f.m1] = 0;
  db::CellDef& top = f.cell("top");
  top.labels().push_back(plain_label("VDD", f.m1, db::Rect{5, 5, 5, 5}));
  std::string bytes = f.write_oas(top);
  CHECK(contains(bytes, {0x13, 0x7B, 0x00, 0x31, 0x00, 0xE8, 0x07, 0xE8, 0x07}));
  CHECK_FALSE(has(f.last, "OASIS-WRITE-002"));
  db::Label font = plain_label("F", f.m1, db::Rect{1, 1, 1, 1});
  font.font = 0;
  font.size = 8;
  top.labels().push_back(font);
  bytes = f.write_oas(top);
  CHECK(has(f.last, "OASIS-WRITE-002"));
  CHECK(decode_oasis(bytes).count(oasis::RecordId::text) == 2);
}

TEST_CASE("a label rectangle follows its TEXT", "[cap:oasis-write][req:Text translation]") {
  OasFixture f;
  f.settings.compress = 0;
  mask::OutputLayer lbl;
  lbl.name = "M1LBL";
  lbl.gds = mask::GdsLayer{49, 10};
  f.style.layers.push_back(lbl);
  f.style.label_layer[f.m1] = 1;
  db::CellDef& top = f.cell("top");
  top.labels().push_back(plain_label("L", f.m1, db::Rect{0, 0, 2, 1}));
  const Decoded d = decode_oasis(f.write_oas(top));
  const std::vector<oasis::Element> e = d.elements("top");
  REQUIRE(e.size() == 2);
  CHECK(e[0].kind == oasis::ElementKind::text);
  CHECK(e[0].position == oasis::Point64{100, 50});
  CHECK(e[1].kind == oasis::ElementKind::rectangle);
  CHECK(e[1].layer == 49);
  CHECK(e[1].datatype == 10);
  CHECK(e[1].size == oasis::Point64{200, 100});
}

TEST_CASE("a rotated SREF and its use-name property", "[cap:oasis-write][req:Placements]") {
  OasFixture f;
  f.settings.compress = 0;
  db::CellDef& sub = f.cell("sub");
  f.paint(sub, f.m1, db::Rect{0, 0, 1, 1});
  db::CellDef& top = f.cell("top");
  f.place(top, sub, db::Transform{0, 1, 10, -1, 0, 20}, {}, std::string{"sub_0"});
  const std::string bytes = f.write_oas(top);
  // sub is CELLNAME 0 here (written first), so the reference is refnum 0.
  CHECK(contains(bytes, {0x11, 0xF6, 0x00, 0xD0, 0x0F, 0xA0, 0x1F}));
  CHECK(contains(bytes, {0x1C, 0x27, 0x04, 0x08, 0x3D, 0x0E, 0x01}));
}

TEST_CASE("an array use is a type 1 placement repetition", "[cap:oasis-write][req:Array placements]") {
  OasFixture f;
  f.settings.compress = 0;
  db::CellDef& sub = f.cell("sub");
  f.paint(sub, f.m1, db::Rect{0, 0, 1, 1});
  db::CellDef& top = f.cell("top");
  f.place(top, sub, {}, db::ArrayInfo{0, 2, 0, 1, 10, 20});
  const std::string bytes = f.write_oas(top);
  CHECK(contains(bytes, {0x11, 0xC8, 0x00, 0x01, 0x01, 0x00, 0xE8, 0x07, 0xD0, 0x0F}));
}

TEST_CASE("a rotated array with negative pitch is a type 9 repetition", "[cap:oasis-write][req:Array placements]") {
  OasFixture f;
  f.settings.compress = 0;
  db::CellDef& sub = f.cell("sub");
  f.paint(sub, f.m1, db::Rect{0, 0, 1, 1});
  db::CellDef& top = f.cell("top");
  f.place(top, sub, db::Transform{-1, 0, 0, 0, -1, 0}, db::ArrayInfo{0, 1, 0, 0, 10, 0});
  const Decoded d = decode_oasis(f.write_oas(top));
  const std::vector<oasis::Element> e = d.elements("top");
  REQUIRE(e.size() == 1);
  REQUIRE(e[0].repetition);
  CHECK(e[0].repetition->type == 9);
  CHECK(e[0].repetition->columns == 2);
  CHECK(e[0].repetition->column_step == oasis::Point64{-1000, 0});
}

TEST_CASE("reference properties are S_GDS_PROPERTY b-strings", "[cap:oasis-write][req:Reference properties]") {
  for (const bool strict : {true, false}) {
    OasFixture f;
    f.settings.compress = 0;
    f.settings.strict = strict;
    db::CellDef& sub = f.cell("sub");
    f.paint(sub, f.m1, db::Rect{0, 0, 1, 1});
    db::CellDef& top = f.cell("top");
    f.place(top, sub, {}, {}, std::string{"u1"});
    const Decoded d = decode_oasis(f.write_oas(top));
    std::vector<oasis::Property> properties;
    for (const oasis::Record& r : d.records) {
      if (r.property && r.target == oasis::PropertyTarget::element) {
        properties.push_back(*r.property);
      }
    }
    REQUIRE(properties.size() == 1);
    REQUIRE(properties[0].values.size() == 2);
    CHECK(properties[0].values[0].unsigned_value == 61);
    CHECK(properties[0].values[1].kind ==
          (strict ? oasis::PropertyValue::Kind::b_reference : oasis::PropertyValue::Kind::b_string));
  }
}

TEST_CASE("non-strict files name cells inline", "[cap:oasis-write][req:Strict and non-strict modes]") {
  OasFixture f;
  f.settings.compress = 0;
  f.settings.strict = false;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
  const std::string bytes = f.write_oas(top);
  CHECK(contains(bytes, {0x0E, 0x03, 0x74, 0x6F, 0x70}));
  CHECK(bytes.substr(13, 22) == std::string{"\x01\x03\x31\x2E\x30\x00\xE8\x07\x00" "\x00\x00\x00\x00\x00\x00"
                                            "\x00\x00\x00\x00\x00\x00\x1C",
                                            22});
  const Decoded d = decode_oasis(bytes);
  CHECK(d.count(oasis::RecordId::cellname_implicit) == 0);
  CHECK(d.names.layernames.size() == 2);  // inline LAYERNAMEs after the file properties
}

TEST_CASE("library mode omits S_TOP_CELL and the root", "[cap:oasis-write][req:Standard properties]") {
  OasFixture f;
  f.settings.compress = 0;
  f.settings.library = true;
  db::CellDef& sub = f.cell("sub");
  f.paint(sub, f.m1, db::Rect{0, 0, 1, 1});
  db::CellDef& top = f.cell("top");
  f.place(top, sub);
  const Decoded d = decode_oasis(f.write_oas(top));
  CHECK(d.cells() == std::vector<std::string>{"sub"});
  for (const oasis::Record& r : d.records) {
    if (r.property && r.target == oasis::PropertyTarget::file) {
      CHECK(r.property->name_reference != std::optional<std::uint64_t>{2});
    }
  }
  CHECK(d.names.propnames.at(2) == "S_TOP_CELL");
}

TEST_CASE("each mask layer gets a LAYERNAME pair", "[cap:oasis-write][req:Layer names]") {
  OasFixture f;
  const std::string bytes = reference_write(f);
  CHECK(contains(bytes, {0x0B, 0x02, 0x4D, 0x31, 0x03, 0x31, 0x03, 0x00, 0x0C, 0x02, 0x4D, 0x31, 0x03, 0x31, 0x03,
                         0x00}));
}

TEST_CASE("validation none ends the file with a zero scheme byte", "[cap:oasis-write][req:END record and validation]") {
  OasFixture f;
  f.settings.validation = oasis::ValidationScheme::none;
  const std::string bytes = reference_write(f);
  REQUIRE(bytes.size() == 419);
  CHECK(bytes.back() == '\0');
  CHECK(bytes[bytes.size() - 256] == '\x02');
  f.settings.validation = oasis::ValidationScheme::checksum32;
  const std::string summed = f.write_oas(f.ed.cells.find("top") != nullptr ? *f.ed.cells.find("top") : f.cell("top"));
  hefesto::gds::MemorySource source(to_bytes(summed));
  const auto checked = oasis::validate(source);
  REQUIRE(checked.ok());
  CHECK(checked.value().scheme == oasis::ValidationScheme::checksum32);
  CHECK(checked.value().ok());
}

TEST_CASE("two writes of the same layout are byte-identical", "[cap:oasis-write][req:Determinism]") {
  OasFixture f;
  f.settings.compress = 9;
  db::CellDef& sub = f.cell("sub");
  db::CellDef& top = f.cell("top");
  for (int i = 0; i < 50; ++i) {
    f.paint(sub, f.m1, db::Rect{i * 3, 0, i * 3 + 2, 2});
  }
  f.place(top, sub, {}, db::ArrayInfo{0, 3, 0, 2, 200, 10});
  top.labels().push_back(plain_label("OUT", f.m1, db::Rect{1, 1, 1, 1}));
  const std::string first = f.write_oas(top);
  f.options.now = kNow + 1000;
  const std::string second = f.write_oas(top);
  CHECK(first == second);
}

TEST_CASE("a disallowed abstract view fails the write but leaves a valid file",
          "[cap:oasis-write][req:Write result and error propagation]") {
  OasFixture f;
  f.settings.compress = 0;
  db::CellDef& macro = f.cell("macro");
  f.paint(macro, f.m1, db::Rect{0, 0, 1, 1});
  macro.properties().put("LEFview", std::string{"TRUE"});
  db::CellDef& top = f.cell("top");
  f.place(top, macro);
  const std::string bytes = f.write_oas(top);
  CHECK_FALSE(f.last.ok());
  REQUIRE(bytes.size() >= 256);
  const Decoded d = decode_oasis(bytes);
  CHECK(d.cells().empty());
  CHECK(d.count(oasis::RecordId::end) == 1);
  hefesto::gds::MemorySource source(to_bytes(bytes));
  const auto checked = oasis::validate(source);
  REQUIRE(checked.ok());
  CHECK(checked.value().ok());
}
