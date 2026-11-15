// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// gds-write "Output scale factors" (Q-MASK-222, 225), "Structure body
// order", "Mask geometry for each structure", "Tile decomposition and
// enumeration order", "Unmerged boundary output" and "Magic write-time side
// effects" (Q-MASK-208).
// Owner: GDSW.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/gds/write/polygons.hpp"
#include "write_fixture.hpp"

namespace gds_write_test {
namespace {

mask::OutputStyle scaled_style(std::int32_t scale_factor, std::int32_t expander, bool angstroms) {
  mask::OutputStyle s;
  s.scale.scale_factor = scale_factor;
  s.scale.expander = expander;
  if (angstroms) {
    s.options |= mask::style_option::angstroms;
  }
  return s;
}

// The XY values of the BOUNDARY elements of `recs`, one vector per element.
std::vector<std::vector<std::int32_t>> boundaries(const std::vector<Rec>& recs) {
  std::vector<std::vector<std::int32_t>> out;
  for (std::size_t i = 0; i < recs.size(); ++i) {
    if (recs[i].record() != gds::RecordType::boundary) {
      continue;
    }
    for (std::size_t k = i; k < recs.size() && recs[k].record() != gds::RecordType::endel; ++k) {
      if (recs[k].record() == gds::RecordType::xy) {
        out.push_back(recs[k].i4());
      }
    }
  }
  return out;
}

TEST_CASE("output scale factors of the spec's styles", "[cap:gds-write][req:Output scale factors]") {
  for (const bool on : {true, false}) {
    const compat::CompatSettings c{on};
    const gds::ScaleResult centimicron = gds::output_scale(scaled_style(10, 1, false), c);
    CHECK(centimicron.scale == gds::OutputScale{100, 10});
    CHECK(centimicron.errors.empty());
    const gds::ScaleResult nanometer = gds::output_scale(scaled_style(5, 10, false), c);
    CHECK(nanometer.scale == gds::OutputScale{5, 1});
    const gds::ScaleResult angstrom = gds::output_scale(scaled_style(25, 100, true), c);
    CHECK(angstrom.scale == gds::OutputScale{25, 1});
    CHECK_FALSE(angstrom.refused);
  }
}

TEST_CASE("an angstrom scale without angstrom units", "[cap:gds-write][req:Output scale factors]") {
  const gds::ScaleResult on = gds::output_scale(scaled_style(25, 100, false), compat::CompatSettings{true});
  CHECK(on.scale == gds::OutputScale{25, 1});
  CHECK_FALSE(on.refused);
  REQUIRE(on.errors.size() == 1);
  CHECK(on.errors.front().code == "GDS-WRITE-005");
  CHECK(on.errors.front().text.find("Please add \"units angstroms\"") != std::string::npos);
  CHECK(on.errors.front().text.find("Output scale units are 0.1 nanometers.") != std::string::npos);

  const gds::ScaleResult off = gds::output_scale(scaled_style(25, 100, false), compat::CompatSettings{false});
  CHECK(off.refused);
  REQUIRE(off.errors.size() == 1);
  CHECK(off.errors.front().text.find("units angstroms") != std::string::npos);
}

TEST_CASE("a mis-scaling style is written with magic-compat ON and refused with it OFF",
          "[cap:gds-write][req:Output scale factors]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& top = f.cell("top");
    f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
    f.style.scale.expander = 100;  // 0.1 nm without `units angstroms`
    const std::string bytes = f.write(top);
    CHECK(f.has_code("GDS-WRITE-005"));
    if (on) {
      CHECK(f.last.ok());
      // Wscale = S = 10, Pscale = 1: the tile (0,0)-(40,20) as it is.
      CHECK(boundaries(decode(bytes)) ==
            std::vector<std::vector<std::int32_t>>{{0, 0, 40, 0, 40, 20, 0, 20, 0, 0}});
    } else {
      CHECK_FALSE(f.last.ok());
      CHECK_FALSE(f.fs.exists("/out.gds"));
    }
  }
}

TEST_CASE("a coordinate outside I4 fails the write", "[cap:gds-write][req:Output scale factors]") {
  // Regression (Q-MASK-222): Magic multiplies in int and overflows.
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& sub = f.cell("sub");
    f.paint(sub, f.m1, db::Rect{0, 0, 2, 2});
    db::CellDef& top = f.cell("top");
    f.place(top, sub, db::translation(30000000, 0), {}, "far");  // 3e9 after Wscale 100
    const std::string bytes = f.write(top);
    CHECK_FALSE(f.last.ok());
    CHECK(f.has_code("GDS-WRITE-004"));
    const std::vector<Rec> recs = decode(bytes);
    CHECK(recs.back().record() == gds::RecordType::endlib);
    CHECK(structure_names(recs) == std::vector<std::string>{"sub", "top"});
  }
}

TEST_CASE("references precede paint, and a layer without a GDS number is skipped",
          "[cap:gds-write][req:Structure body order]") {
  Fixture f;
  f.add_layer("XL", f.m2, std::nullopt);
  f.add_layer("BIG", f.m2, mask::GdsLayer{300, 0});
  f.add_layer("M2", f.m2, mask::GdsLayer{50, 0});
  db::CellDef& inv = f.cell("inv");
  f.paint(inv, f.m1, db::Rect{0, 0, 2, 2});
  db::CellDef& top = f.cell("top");
  f.place(top, inv);
  f.paint(top, f.m1, db::Rect{10, 0, 14, 2});
  f.paint(top, f.m2, db::Rect{10, 0, 14, 2});
  const std::vector<Rec> body = structure(decode(f.write(top)), "top");
  REQUIRE(f.last.ok());
  const std::vector<gds::RecordType> t = types(body);
  const auto sref = std::find(t.begin(), t.end(), gds::RecordType::sref);
  const auto boundary = std::find(t.begin(), t.end(), gds::RecordType::boundary);
  REQUIRE(sref != t.end());
  REQUIRE(boundary != t.end());
  CHECK(sref < boundary);
  std::vector<std::int16_t> layers;
  for (const Rec& r : body) {
    if (r.record() == gds::RecordType::layer) {
      layers.push_back(r.i2().front());
    }
  }
  CHECK(layers == std::vector<std::int16_t>{49, 50});  // XL (no calma) and BIG (300) are not written
  CHECK(body.back().record() == gds::RecordType::endstr);
}

TEST_CASE("the generation area includes a FIXED_BBOX", "[cap:gds-write][req:Mask geometry for each structure]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 10, 10});
  top.properties().put("FIXED_BBOX", db::DimensionList{{-20, -20, 30, 30}});
  auto prepared = gds::prepare_write(f.ed, top, f.options);
  REQUIRE(prepared.ok());
  CHECK(prepared.value().hierarchy.fixed_bbox);
  CHECK(mask::generation_area(top, 2, prepared.value().hierarchy.fixed_bbox) == db::Rect{-20, -20, 30, 30});
}

TEST_CASE("the generation switches follow the cif settings", "[cap:gds-write][req:Mask geometry for each structure]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.ed.mask.cif.hier = false;
  f.ed.mask.cif.array = false;
  auto prepared = gds::prepare_write(f.ed, top, f.options);
  REQUIRE(prepared.ok());
  CHECK_FALSE(prepared.value().hierarchy.subcells);
  CHECK_FALSE(prepared.value().hierarchy.arrays);
  // A CIFhier property forces both on for that cell (and its subtree).
  top.properties().put("CIFhier", std::string{"TRUE"});
  const mask::HierarchyOptions forced =
      mask::hierarchy_options(top, prepared.value().hierarchy, true, f.compat);
  CHECK(forced.subcells);
  CHECK(forced.arrays);
}

TEST_CASE("an L-shape is written as its tiles in enumeration order",
          "[cap:gds-write][req:Tile decomposition and enumeration order]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
  f.paint(top, f.m1, db::Rect{0, 2, 2, 4});
  const std::vector<Rec> recs = decode(f.write(top));
  REQUIRE(f.last.ok());
  CHECK(boundaries(recs) == std::vector<std::vector<std::int32_t>>{{0, 200, 200, 200, 200, 400, 0, 400, 0, 200},
                                                                    {0, 0, 400, 0, 400, 200, 0, 200, 0, 0}});
}

TEST_CASE("a single rectangle and a triangle as unmerged boundaries",
          "[cap:gds-write][req:Unmerged boundary output]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
  f.options.settings.merge = false;
  const std::vector<Rec> recs = decode(f.write(top));
  REQUIRE(f.last.ok());
  const std::vector<Rec> body = structure(recs, "top");
  REQUIRE(body.size() == 8);
  CHECK(types(body) == std::vector<gds::RecordType>{gds::RecordType::bgnstr, gds::RecordType::strname,
                                                    gds::RecordType::boundary, gds::RecordType::layer,
                                                    gds::RecordType::datatype, gds::RecordType::xy,
                                                    gds::RecordType::endel, gds::RecordType::endstr});
  CHECK(body[3].i2() == std::vector<std::int16_t>{49});
  CHECK(body[4].i2() == std::vector<std::int16_t>{0});
  CHECK(body[5].i4() == std::vector<std::int32_t>{0, 0, 400, 0, 400, 200, 0, 200, 0, 0});

  // A split tile (0,0)-(10,10) with a `\` diagonal, solid on its left side.
  mask::MaskPlane plane;
  REQUIRE(plane.paint_triangle(db::Rect{0, 0, 10, 10}, db::Diagonal::nw_se, false).ok());
  gds::MemorySink sink;
  gds::RecordWriter records{sink};
  const gds::StructureContext context{&top, &f.style, &f.options.settings, f.compat, gds::OutputScale{1, 1}};
  gds::ElementWriter out{records, context, nullptr};
  REQUIRE(gds::emit_tile_boundaries(plane, mask::GdsLayer{49, 0}, out).ok());
  REQUIRE(records.flush().ok());
  std::string bytes;
  for (const std::byte b : sink.bytes()) {
    bytes.push_back(static_cast<char>(b));
  }
  const std::vector<Rec> tri = decode(bytes);
  REQUIRE(tri.size() == 5);
  CHECK(tri[3].data.size() == 32);  // XY length 36
  CHECK(tri[3].i4() == std::vector<std::int32_t>{0, 10, 0, 0, 10, 0, 0, 10});
}

TEST_CASE("the four triangle point orders", "[cap:gds-write][req:Unmerged boundary output]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  struct Case {
    db::Diagonal diagonal;
    bool right;
    std::vector<std::int32_t> xy;
  };
  const std::vector<Case> cases{
      {db::Diagonal::ne_sw, false, {0, 0, 0, 10, 10, 10, 0, 0}},
      {db::Diagonal::nw_se, false, {0, 10, 0, 0, 10, 0, 0, 10}},
      {db::Diagonal::ne_sw, true, {0, 0, 10, 0, 10, 10, 0, 0}},
      {db::Diagonal::nw_se, true, {0, 10, 10, 10, 10, 0, 0, 10}},
  };
  for (const Case& c : cases) {
    mask::MaskPlane plane;
    REQUIRE(plane.paint_triangle(db::Rect{0, 0, 10, 10}, c.diagonal, c.right).ok());
    gds::MemorySink sink;
    gds::RecordWriter records{sink};
    const gds::StructureContext context{&top, &f.style, &f.options.settings, f.compat, gds::OutputScale{1, 1}};
    gds::ElementWriter out{records, context, nullptr};
    REQUIRE(gds::emit_tile_boundaries(plane, mask::GdsLayer{49, 0}, out).ok());
    REQUIRE(records.flush().ok());
    std::string bytes;
    for (const std::byte b : sink.bytes()) {
      bytes.push_back(static_cast<char>(b));
    }
    const std::vector<Rec> recs = decode(bytes);
    REQUIRE(recs.size() == 5);
    CHECK(recs[3].i4() == c.xy);
  }
}

TEST_CASE("a write does not change the instance names it maps",
          "[cap:gds-write][req:Magic write-time side effects]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& sub = f.cell("sub");
    db::CellDef& top = f.cell("top");
    db::CellUse& use = f.place(top, sub, {}, {}, "a<1>");
    const std::vector<Rec> recs = decode(f.write(top));
    REQUIRE(f.last.ok());
    CHECK(use_names(structure(recs, "top")) == std::vector<std::string>{"a_1_"});
    CHECK(use.id == "a<1>");
  }
}

}  // namespace
}  // namespace gds_write_test
