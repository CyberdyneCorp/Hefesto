// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only cells in OASIS output (tasks.md 12.4: the GDS cell transcoder,
// Q-OASIS-63; the OASIS cell re-encoder, Q-OASIS-40) and read-only OASIS
// cells in GDS output (tasks.md 12.5, gds/write/vendor_oasis.cpp,
// Q-OASIS-35). Owner: OASW.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "hefesto/oasis/write/transcode.hpp"
#include "oasis_write_fixture.hpp"

using namespace oasis_write_test;  // NOLINT(google-build-using-namespace)

namespace {

/// Transcodes structure `name` of `library` into a one-cell OASIS file.
struct Transcoded {
  diag::Status status = diag::Done{};
  Decoded decoded;
  diag::CollectingSink sink;
};

void transcode(GdsLibrary& library, std::uint64_t bgnstr, std::string_view name, Transcoded& out) {
  gds::MemorySource source(to_bytes(library.bytes()));
  gds::MemorySink sink;
  oasis::Encoder encoder(sink, oasis::EncoderOptions{true, oasis::ValidationScheme::crc32, 0, 1000.0});
  REQUIRE(encoder.begin().ok());
  out.status = oasis::transcode_gds_cell(source, bgnstr, name, encoder, &out.sink);
  REQUIRE(encoder.finish().ok());
  std::string bytes;
  for (const std::byte b : sink.bytes()) {
    bytes.push_back(static_cast<char>(b));
  }
  out.decoded = decode_oasis(bytes);
}

bool sink_has(diag::CollectingSink& sink, std::string_view code, std::string_view text) {
  for (const diag::Diagnostic& d : sink.snapshot()) {
    if (d.code == code && d.text.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

/// Makes `def` a read-only GDS cell of `file` at [start, end).
void read_only_gds(db::CellDef& def, const std::string& file, std::uint64_t start, std::uint64_t end) {
  def.properties().put("GDS_FILE", file);
  def.properties().put("GDS_START", std::to_string(start));
  def.properties().put("GDS_END", std::to_string(end));
}

void read_only_oasis(db::CellDef& def, const std::string& file, const std::string& cell) {
  def.properties().put("OASIS_FILE", file);
  def.properties().put("OASIS_CELL", cell);
}

void store(OasFixture& f, const std::string& path, const std::string& bytes) {
  REQUIRE(f.fs.write_atomic(path, bytes).ok());
}

oasis::Element circle(std::uint64_t layer, std::uint64_t datatype, std::int64_t x, std::int64_t y, std::int64_t r) {
  oasis::Element e;
  e.kind = oasis::ElementKind::circle;
  e.layer = layer;
  e.datatype = datatype;
  e.position = {x, y};
  e.radius = r;
  return e;
}

oasis::Repetition row(std::uint64_t n, std::int64_t dx) {
  oasis::Repetition r;
  r.type = 2;
  r.columns = n;
  r.column_step = {dx, 0};
  return r;
}

/// The types and LAYER values of the records of structure `name`.
std::vector<Rec> body(const std::string& gds_bytes, std::string_view name) {
  return structure(decode(gds_bytes), name);
}

std::size_t count_type(const std::vector<Rec>& recs, gds::RecordType t) {
  return static_cast<std::size_t>(std::count_if(recs.begin(), recs.end(), [t](const Rec& r) { return r.record() == t; }));
}

}  // namespace

TEST_CASE("a vendor standard cell is transcoded element by element", "[cap:oasis-write][req:Read-only GDS vendor cells]") {
  GdsLibrary lib;
  const std::uint64_t at = lib.begin("sc_inv");
  lib.boundary(68, 20, {0, 0, 100, 0, 100, 50, 0, 50, 0, 0});
  lib.boundary(68, 20, {0, 0, 50, 0, 50, 20, 20, 20, 20, 60, 0, 60, 0, 0});
  lib.path(67, 20, 2, 140, {0, 0, 0, 500});
  lib.text(67, 5, 10, 20, "A");
  (void)lib.end();
  Transcoded t;
  transcode(lib, at, "sc_inv", t);
  REQUIRE(t.status.ok());
  const std::vector<oasis::Element> e = t.decoded.elements("sc_inv");
  REQUIRE(e.size() == 4);
  CHECK(e[0].kind == oasis::ElementKind::rectangle);
  CHECK(e[1].kind == oasis::ElementKind::polygon);
  CHECK(e[2].kind == oasis::ElementKind::path);
  CHECK(e[2].half_width == 70);
  CHECK(e[2].start_extension == 70);
  CHECK(e[2].end_extension == 70);
  CHECK(e[3].kind == oasis::ElementKind::text);
  CHECK(e[3].name == "A");
}

TEST_CASE("an odd-width vendor path fails the write", "[cap:oasis-write][req:Read-only GDS vendor cells]") {
  GdsLibrary lib;
  const std::uint64_t at = lib.begin("sc_buf");
  lib.path(68, 20, 0, 15, {0, 0, 100, 0});
  (void)lib.end();
  Transcoded t;
  transcode(lib, at, "sc_buf", t);
  CHECK_FALSE(t.status.ok());
  CHECK(sink_has(t.sink, "OASIS-WRITE-004", "sc_buf"));
  CHECK(sink_has(t.sink, "OASIS-WRITE-004", "68/20"));
  CHECK(t.decoded.elements("sc_buf").empty());  // not widened or narrowed
}

TEST_CASE("round path ends become CIRCLE caps", "[cap:oasis-write][req:Read-only GDS vendor cells]") {
  GdsLibrary lib;
  const std::uint64_t at = lib.begin("r");
  lib.path(1, 0, 1, 100, {0, 0, 1000, 0});
  (void)lib.end();
  Transcoded t;
  transcode(lib, at, "r", t);
  REQUIRE(t.status.ok());
  const std::vector<oasis::Element> e = t.decoded.elements("r");
  REQUIRE(e.size() == 3);
  CHECK(e[0].kind == oasis::ElementKind::path);
  CHECK(e[0].half_width == 50);
  CHECK(e[0].start_extension == 0);
  CHECK(e[0].end_extension == 0);
  CHECK(e[0].points == std::vector<oasis::Point64>{{1000, 0}});
  CHECK(e[1].kind == oasis::ElementKind::circle);
  CHECK(e[1].radius == 50);
  CHECK(e[1].position == oasis::Point64{0, 0});
  CHECK(e[2].position == oasis::Point64{1000, 0});
}

TEST_CASE("triangles, degenerate boundaries, boxes, nodes and inexact arrays",
          "[cap:oasis-write][req:Read-only GDS vendor cells]") {
  GdsLibrary lib;
  const std::uint64_t at = lib.begin("v");
  lib.boundary(1, 0, {0, 10, 0, 0, 10, 0, 0, 10});  // the lower-left triangle
  lib.boundary(1, 0, {0, 0, 5, 5, 0, 0});           // 2 distinct vertices: dropped
  lib.empty(gds::RecordType::box);
  lib.i2(gds::RecordType::layer, {3});
  lib.i2(gds::RecordType::boxtype, {4});
  lib.i4(gds::RecordType::xy, {0, 0, 10, 0, 10, 20, 0, 20, 0, 0});
  lib.empty(gds::RecordType::endel);
  lib.empty(gds::RecordType::node);
  lib.i2(gds::RecordType::layer, {1});
  lib.i2(gds::RecordType::nodetype, {0});
  lib.i4(gds::RecordType::xy, {0, 0});
  lib.empty(gds::RecordType::endel);
  lib.empty(gds::RecordType::aref);
  lib.str(gds::RecordType::sname, "child");
  lib.i2(gds::RecordType::colrow, {3, 1});
  lib.i4(gds::RecordType::xy, {0, 0, 10, 0, 0, 0});  // a step of 10/3
  lib.empty(gds::RecordType::endel);
  (void)lib.end();
  Transcoded t;
  transcode(lib, at, "v", t);
  REQUIRE(t.status.ok());
  const std::vector<oasis::Element> e = t.decoded.elements("v");
  REQUIRE(e.size() == 3);
  CHECK(e[0].kind == oasis::ElementKind::polygon);
  CHECK(e[0].position == oasis::Point64{0, 10});
  CHECK(e[0].point_list_type == std::uint8_t{3});
  CHECK(e[0].points == std::vector<oasis::Point64>{{0, -10}, {10, -10}});
  CHECK(e[1].kind == oasis::ElementKind::rectangle);
  CHECK(e[1].datatype == 4);
  CHECK(e[1].size == oasis::Point64{10, 20});
  CHECK(e[2].kind == oasis::ElementKind::placement);
  REQUIRE(e[2].repetition);
  CHECK(e[2].repetition->type == 10);
  CHECK(e[2].repetition->offsets == std::vector<oasis::Point64>{{3, 0}, {6, 0}});
  CHECK(sink_has(t.sink, "OASIS-WRITE-005", "fewer than 3 distinct"));
  CHECK(sink_has(t.sink, "OASIS-WRITE-005", "NODE"));
}

// Approved cap (Q-OASIS-70, user decision E3 of 2026-11-20; security.md
// M6-S3, OASIS-WRITE-010): an
// inexact AREF lists every instance in a type 10 repetition, so a 32767 x
// 32767 COLROW would allocate about 17 GB. Above 2^24 instances the array
// is not written and the transcode fails.
TEST_CASE("an inexact AREF above 2^24 instances fails the transcode (approved cap)",
          "[cap:oasis-write][req:Read-only GDS vendor cells]") {
  GdsLibrary lib;
  const std::uint64_t at = lib.begin("v");
  lib.empty(gds::RecordType::aref);
  lib.str(gds::RecordType::sname, "child");
  lib.i2(gds::RecordType::colrow, {4097, 4096});  // 16781312 = 2^24 + 4096 instances
  lib.i4(gds::RecordType::xy, {0, 0, 10000, 0, 0, 10000});  // steps of 10000/4097 and 10000/4096
  lib.empty(gds::RecordType::endel);
  (void)lib.end();
  Transcoded t;
  transcode(lib, at, "v", t);
  CHECK_FALSE(t.status.ok());
  CHECK(sink_has(t.sink, "OASIS-WRITE-010", "an AREF of 4097 x 4096 in cell v has inexact steps"));
  CHECK(t.decoded.elements("v").empty());
}

TEST_CASE("a read-only GDS cell in a write is transcoded without repetition detection",
          "[cap:oasis-write][req:Read-only GDS vendor cells]") {
  OasFixture f;
  f.settings.compress = 0;
  GdsLibrary lib;
  (void)lib.begin("sc_inv");
  const std::uint64_t start = lib.start();
  lib.boundary(49, 0, {0, 0, 100, 0, 100, 100, 0, 100, 0, 0});
  lib.boundary(49, 0, {200, 0, 300, 0, 300, 100, 200, 100, 200, 0});
  lib.path(49, 0, 2, 140, {0, 0, 0, 500});
  const std::uint64_t end = lib.end();
  store(f, "/lib.gds", lib.bytes());
  db::CellDef& inv = f.cell("sc_inv");
  read_only_gds(inv, "/lib.gds", start, end);
  db::CellDef& top = f.cell("top");
  f.place(top, inv);
  const Decoded d = decode_oasis(f.write_oas(top));
  INFO((f.last.diagnostics.empty() ? std::string{} : f.last.diagnostics.front().text));
  REQUIRE(f.last.ok());
  const std::vector<oasis::Element> e = d.elements("sc_inv");
  REQUIRE(e.size() == 3);
  CHECK_FALSE(e[0].repetition.has_value());
  CHECK_FALSE(e[1].repetition.has_value());
  CHECK(e[2].kind == oasis::ElementKind::path);
}

TEST_CASE("a full dump is transcoded structure by structure with gds-write's prefixes",
          "[cap:oasis-write][req:Read-only GDS vendor cells]") {
  for (const bool on : {true, false}) {
    OasFixture f(on);
    f.settings.compress = 0;
    GdsLibrary lib;
    (void)lib.begin("via_x");
    lib.boundary(3, 0, {0, 0, 10, 0, 10, 10, 0, 10, 0, 0});
    (void)lib.end();
    (void)lib.begin("pad");
    lib.empty(gds::RecordType::sref);
    lib.str(gds::RecordType::sname, "via_x");
    lib.i4(gds::RecordType::xy, {0, 0});
    lib.empty(gds::RecordType::endel);
    (void)lib.end();
    store(f, "/lib.gds", lib.bytes());
    db::CellDef& pad = f.cell("pad");
    pad.properties().put("LEFview", std::string{"TRUE"});
    pad.properties().put("GDS_FILE", std::string{"/lib.gds"});
    pad.properties().put("GDS_START", std::string{"0"});
    db::CellDef& top = f.cell("top");
    f.place(top, pad);
    const hefesto::util::LibcRandom before = f.ed.mask.random;
    const std::vector<std::string> gds_names = structure_names(decode(f.write(top)));
    f.ed.mask.random = before;  // the same Q-MASK-211 draws for the OASIS write
    const Decoded d = decode_oasis(f.write_oas(top));
    INFO("magic-compat " << on);
    REQUIRE(f.last.ok());
    CHECK(d.cells() == gds_names);
    CHECK(d.cells().size() == 3);
    const std::vector<oasis::Element> placed = d.elements(d.cells().at(1));
    REQUIRE(placed.size() == 1);
    CHECK(placed[0].name == d.cells().at(0));  // the prefixed via_x
  }
}

TEST_CASE("a read-only GDS cell of another unit is generated from its layout in OASIS output",
          "[cap:oasis-write][req:Read-only GDS vendor cells]") {
  OasFixture f;
  f.settings.compress = 0;
  GdsLibrary lib(1e-8);
  (void)lib.begin("sc_inv");
  const std::uint64_t start = lib.start();
  lib.boundary(68, 20, {0, 0, 100, 0, 100, 100, 0, 100, 0, 0});
  const std::uint64_t end = lib.end();
  store(f, "/lib.gds", lib.bytes());
  db::CellDef& inv = f.cell("sc_inv");
  f.paint(inv, f.m1, db::Rect{0, 0, 1, 1});
  read_only_gds(inv, "/lib.gds", start, end);
  db::CellDef& top = f.cell("top");
  f.place(top, inv);
  const Decoded d = decode_oasis(f.write_oas(top));
  CHECK(has(f.last, "GDS-WRITE-054"));
  const std::vector<oasis::Element> e = d.elements("sc_inv");
  REQUIRE(e.size() == 1);
  CHECK(e[0].layer == 49);  // the layout, not the vendor 68/20
  (void)f.write(top);  // gds write copies it verbatim, whatever its units
  CHECK_FALSE(has(f.last, "GDS-WRITE-054"));
}

TEST_CASE("a read-only OASIS cell is re-encoded with its circles and repetitions",
          "[cap:oasis-write][req:Read-only OASIS vendor cells]") {
  OasFixture f;
  f.settings.compress = 0;
  OasLibrary lib;
  lib.cell("vcell").add(rectangle(68, 20, 0, 0, 10, 10));
  oasis::Element c = circle(1, 0, 100, 100, 50);
  c.repetition = row(3, 200);
  lib.add(c);
  store(f, "/lib.oas", lib.bytes());
  db::CellDef& v = f.cell("vcell");
  read_only_oasis(v, "/lib.oas", "vcell");
  db::CellDef& top = f.cell("top");
  f.place(top, v);
  const Decoded d = decode_oasis(f.write_oas(top));
  INFO((f.last.diagnostics.empty() ? std::string{} : f.last.diagnostics.back().text));
  REQUIRE(f.last.ok());
  const std::vector<oasis::Element> e = d.elements("vcell");
  REQUIRE(e.size() == 2);
  CHECK(e[0] == rectangle(68, 20, 0, 0, 10, 10));
  CHECK(e[1].kind == oasis::ElementKind::circle);
  CHECK(e[1].radius == 50);
  CHECK(e[1].repetition == row(3, 200));
}

TEST_CASE("a read-only OASIS cell keeps its placements and properties",
          "[cap:oasis-write][req:Read-only OASIS vendor cells]") {
  OasFixture f;
  f.settings.compress = 0;
  OasLibrary lib;
  lib.cell("leaf").add(rectangle(1, 0, 0, 0, 5, 5));
  oasis::Element p;
  p.kind = oasis::ElementKind::placement;
  p.name = "leaf";
  p.position = {7, 9};
  lib.cell("vtop").add(p);
  oasis::Property prop;
  prop.name = "S_GDS_PROPERTY";
  prop.standard = true;
  prop.values = {oasis::PropertyValue::of_unsigned(61),
                 oasis::PropertyValue::of_string(oasis::PropertyValue::Kind::b_string, "I0")};
  lib.property(prop);
  store(f, "/lib.oas", lib.bytes());
  db::CellDef& leaf = f.cell("leaf");
  read_only_oasis(leaf, "/lib.oas", "leaf");
  db::CellDef& vtop = f.cell("vtop");
  read_only_oasis(vtop, "/lib.oas", "vtop");
  f.place(vtop, leaf);
  db::CellDef& top = f.cell("top");
  f.place(top, vtop);
  const Decoded d = decode_oasis(f.write_oas(top));
  REQUIRE(f.last.ok());
  const std::vector<oasis::Element> e = d.elements("vtop");
  REQUIRE(e.size() == 1);
  CHECK(e[0].name == "leaf");
  CHECK(e[0].position == oasis::Point64{7, 9});
  CHECK(d.elements("leaf").size() == 1);
}

TEST_CASE("a read-only OASIS cell of another unit is generated from its layout",
          "[cap:oasis-write][req:Read-only OASIS vendor cells]") {
  OasFixture f;
  f.settings.compress = 0;
  OasLibrary lib(2000.0);
  lib.cell("vcell").add(rectangle(68, 20, 0, 0, 10, 10));
  store(f, "/lib.oas", lib.bytes());
  db::CellDef& v = f.cell("vcell");
  f.paint(v, f.m1, db::Rect{0, 0, 1, 1});
  read_only_oasis(v, "/lib.oas", "vcell");
  db::CellDef& top = f.cell("top");
  f.place(top, v);
  const Decoded d = decode_oasis(f.write_oas(top));
  CHECK(has(f.last, "OASIS-WRITE-007"));
  const std::vector<oasis::Element> e = d.elements("vcell");
  REQUIRE(e.size() == 1);
  CHECK(e[0].layer == 49);  // the layout, not the vendor 68/20
}

TEST_CASE("extension records of a re-encoded cell warn when the source has XNAMEs",
          "[cap:oasis-write][req:Read-only OASIS vendor cells]") {
  // The encoder writes no XNAME, so a hand-made file adds one before END.
  OasFixture f;
  f.settings.compress = 0;
  hefesto::gds::MemorySink sink;
  oasis::Encoder encoder(sink, oasis::EncoderOptions{false, oasis::ValidationScheme::none, 0, 1000.0});
  REQUIRE(encoder.begin().ok());
  REQUIRE(encoder.standard_properties(std::nullopt).ok());
  REQUIRE(encoder.begin_cell("x").ok());
  oasis::Element xe;
  xe.kind = oasis::ElementKind::xelement;
  xe.attribute = 3;
  xe.payload = "data";
  REQUIRE(encoder.element(xe).ok());
  std::string bytes;
  for (const std::byte b : sink.bytes()) {
    bytes.push_back(static_cast<char>(b));
  }
  bytes += std::string{"\x1E\x05\x02xn", 5};  // XNAME (implicit), attribute 5, "xn"
  // END: record 2, padding b-string to 256 bytes, scheme 0 (non-strict: no table entries).
  std::string end{"\x02", 1};
  end += static_cast<char>(0xFC);  // 252 padding bytes
  end += static_cast<char>(0x01);
  end += std::string(252, '\0');
  end += '\0';
  REQUIRE(end.size() == 256);
  bytes += end;
  store(f, "/x.oas", bytes);
  db::CellDef& x = f.cell("x");
  read_only_oasis(x, "/x.oas", "x");
  db::CellDef& top = f.cell("top");
  f.place(top, x);
  const Decoded d = decode_oasis(f.write_oas(top));
  CHECK(has(f.last, "OASIS-WRITE-006"));
  const std::vector<oasis::Element> e = d.elements("x");
  REQUIRE(e.size() == 1);
  CHECK(e[0].kind == oasis::ElementKind::xelement);
  CHECK(e[0].payload == "data");
  CHECK(d.names.xnames.empty());  // XNAME records are not copied
}

TEST_CASE("gds write transcodes a read-only OASIS cell", "[cap:gds-write][req:Read-only OASIS vendor cells]") {
  OasFixture f;
  OasLibrary lib;
  oasis::Element t;
  t.kind = oasis::ElementKind::text;
  t.layer = 5;
  t.datatype = 1;
  t.position = {3, 4};
  t.name = "A";
  lib.cell("sc_inv").add(rectangle(68, 20, 0, 0, 10, 10)).add(circle(1, 0, 100, 100, 50)).add(t);
  store(f, "/lib.oas", lib.bytes());
  db::CellDef& inv = f.cell("sc_inv");
  read_only_oasis(inv, "/lib.oas", "sc_inv");
  db::CellDef& top = f.cell("top");
  f.place(top, inv);
  const std::string gds_bytes = f.write(top);
  INFO((f.last.diagnostics.empty() ? std::string{} : f.last.diagnostics.back().text));
  REQUIRE(f.last.ok());
  const std::vector<Rec> recs = body(gds_bytes, "sc_inv");
  std::vector<gds::RecordType> kinds;
  std::vector<std::size_t> xy_points;
  for (const Rec& r : recs) {
    if (r.record() == gds::RecordType::boundary || r.record() == gds::RecordType::text) {
      kinds.push_back(r.record());
    }
    if (r.record() == gds::RecordType::xy) {
      xy_points.push_back(r.i4().size() / 2);
    }
  }
  CHECK(kinds == std::vector<gds::RecordType>{gds::RecordType::boundary, gds::RecordType::boundary,
                                              gds::RecordType::text});
  CHECK(xy_points == std::vector<std::size_t>{5, 65, 1});
}

TEST_CASE("a regular placement repetition becomes an AREF", "[cap:gds-write][req:Read-only OASIS vendor cells]") {
  OasFixture f;
  OasLibrary lib;
  lib.cell("A").add(rectangle(1, 0, 0, 0, 5, 5));
  oasis::Element p;
  p.kind = oasis::ElementKind::placement;
  p.name = "A";
  oasis::Repetition r;
  r.type = 1;
  r.columns = 3;
  r.rows = 2;
  r.column_step = {100, 0};
  r.row_step = {0, 200};
  p.repetition = r;
  lib.cell("V").add(p);
  store(f, "/lib.oas", lib.bytes());
  db::CellDef& a = f.cell("A");
  read_only_oasis(a, "/lib.oas", "A");
  db::CellDef& v = f.cell("V");
  read_only_oasis(v, "/lib.oas", "V");
  f.place(v, a);
  db::CellDef& top = f.cell("top");
  f.place(top, v);
  const std::vector<Rec> recs = body(f.write(top), "V");
  REQUIRE(f.last.ok());
  CHECK(count_type(recs, gds::RecordType::aref) == 1);
  for (const Rec& rec : recs) {
    if (rec.record() == gds::RecordType::colrow) {
      CHECK(rec.i2() == std::vector<std::int16_t>{3, 2});
    }
    if (rec.record() == gds::RecordType::xy) {
      CHECK(rec.i4() == std::vector<std::int32_t>{0, 0, 300, 0, 0, 400});
    }
  }
}

// Approved cap (Q-OASIS-71, user decision E3 of 2026-11-20; security.md
// M6-S3, GDS-WRITE-055): the
// element instances a read-only OASIS cell expands to in GDS output are
// bounded by the oasis-read cap of 2^28 (Q-OASIS-41). The count is checked
// before any instance is written.
TEST_CASE("a vendor OASIS cell above 2^28 element instances fails the cell (approved cap)",
          "[cap:gds-write][req:Read-only OASIS vendor cells]") {
  for (const std::uint64_t rows : {std::uint64_t{1} << 14, (std::uint64_t{1} << 14) + 1}) {
    INFO("rows " << rows);
    OasFixture f;
    OasLibrary lib;
    oasis::Element big = rectangle(1, 0, 0, 0, 1, 1);
    oasis::Repetition r;
    r.type = 1;
    r.columns = std::uint64_t{1} << 14;
    r.rows = rows;
    r.column_step = {2, 0};
    r.row_step = {0, 2};
    big.repetition = r;
    // 2^14 rows: 2^28 instances, and the rectangle before it makes the
    // running count 2^28 + 1. 2^14 + 1 rows: the element alone passes it.
    // Either fails before an instance is expanded.
    lib.cell("vendor").add(rectangle(1, 0, 0, 0, 1, 1)).add(big);
    store(f, "/lib.oas", lib.bytes());
    db::CellDef& v = f.cell("vendor");
    read_only_oasis(v, "/lib.oas", "vendor");
    db::CellDef& top = f.cell("top");
    f.place(top, v);
    const std::string gds_bytes = f.write(top);
    CHECK_FALSE(f.last.ok());
    bool named = false;
    for (const auto& d : f.last.diagnostics) {
      named = named || (d.code == "GDS-WRITE-055" && d.text.find("vendor expands to more than 268435456") !=
                                                         std::string::npos);
    }
    CHECK(named);
    CHECK(structure_names(decode(gds_bytes)).empty());
  }
}

TEST_CASE("layer 32767 is written and 32768 fails the cell", "[cap:gds-write][req:Read-only OASIS vendor cells]") {
  for (const std::uint64_t layer : {std::uint64_t{32767}, std::uint64_t{32768}}) {
    OasFixture f;
    OasLibrary lib;
    lib.cell("vendor").add(rectangle(layer, 0, 0, 0, 10, 10));
    store(f, "/lib.oas", lib.bytes());
    db::CellDef& v = f.cell("vendor");
    read_only_oasis(v, "/lib.oas", "vendor");
    db::CellDef& top = f.cell("top");
    f.place(top, v);
    const std::string gds_bytes = f.write(top);
    if (layer == 32767) {
      REQUIRE(f.last.ok());
      CHECK(contains(gds_bytes, {0x00, 0x06, 0x0D, 0x02, 0x7F, 0xFF}));
    } else {
      CHECK_FALSE(f.last.ok());
      CHECK(has(f.last, "GDS-WRITE-052"));
      const std::vector<std::string> names = structure_names(decode(gds_bytes));
      CHECK(names.empty());  // neither vendor nor its parent
    }
  }
}

TEST_CASE("a polygon above the XY limit fails the cell", "[cap:gds-write][req:Read-only OASIS vendor cells]") {
  OasFixture f;
  OasLibrary lib;
  oasis::Element poly;
  poly.kind = oasis::ElementKind::polygon;
  poly.layer = 1;
  for (std::int64_t i = 1; i < 8191; ++i) {  // 8191 vertices: 8192 XY points with the closing one
    poly.points.push_back({i, (i % 2) * 3 + i});
  }
  lib.cell("big").add(poly);
  store(f, "/lib.oas", lib.bytes());
  db::CellDef& big = f.cell("big");
  read_only_oasis(big, "/lib.oas", "big");
  db::CellDef& top = f.cell("top");
  f.place(top, big);
  (void)f.write(top);
  CHECK_FALSE(f.last.ok());
  bool named = false;
  for (const auto& d : f.last.diagnostics) {
    named = named || (d.code == "GDS-WRITE-052" && d.text.find("big") != std::string::npos &&
                      d.text.find("8192") != std::string::npos);
  }
  CHECK(named);
}

TEST_CASE("a vendor OASIS coordinate outside I4 fails the cell without overflow",
          "[cap:gds-write][req:Read-only OASIS vendor cells]") {
  OasFixture f;
  OasLibrary lib;
  oasis::Repetition r;
  r.type = 9;
  r.columns = 4;
  r.column_step = {std::int64_t{1} << 30, std::int64_t{1} << 30};
  oasis::Element p;
  p.kind = oasis::ElementKind::placement;
  p.name = "leaf";
  p.position = {std::int64_t{1} << 40, 0};
  p.repetition = r;
  lib.cell("leaf").add(rectangle(1, 0, 0, 0, 1, 1));
  lib.cell("far").add(rectangle(1, 0, std::int64_t{1} << 56, 0, 10, 10));
  lib.cell("arr").add(p);
  store(f, "/lib.oas", lib.bytes());
  db::CellDef& leaf = f.cell("leaf");
  read_only_oasis(leaf, "/lib.oas", "leaf");
  for (const std::string name : {"far", "arr"}) {
    db::CellDef& v = f.cell(name);
    read_only_oasis(v, "/lib.oas", name);
    if (name == "arr") {
      f.place(v, leaf);
    }
    db::CellDef& top = f.cell("top_" + name);
    f.place(top, v);
    (void)f.write(top);
    INFO(name);
    CHECK_FALSE(f.last.ok());
    CHECK(has(f.last, "GDS-WRITE-004"));
  }
}

TEST_CASE("an OASIS file that is missing follows the unopenable-file rules",
          "[cap:gds-write][req:Read-only OASIS vendor cells]") {
  OasFixture f;
  db::CellDef& v = f.cell("vendor");
  read_only_oasis(v, "/nowhere.oas", "vendor");
  db::CellDef& top = f.cell("top");
  f.place(top, v);
  (void)f.write(top);
  CHECK_FALSE(f.last.ok());
  CHECK(has(f.last, "GDS-WRITE-041"));
  f.options.settings.allow_undefined = true;
  (void)f.write(top);
  CHECK(f.last.ok());
}
