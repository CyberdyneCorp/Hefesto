// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The merged-polygon (task 10.4) and label (task 10.5) emitters reached
// through gds::write: `gds merge yes`, label layers, label TEXT records with
// the `labels` switch, the Q-MASK-206 warning once per write, and the
// string encoding of label text without changing the layout (Q-MASK-208).
// Owner: GDSW.

#include <catch2/catch_test_macros.hpp>

#include "write_fixture.hpp"

namespace gds_write_test {
namespace {

// The XY values of each element of type `type` in `recs`.
std::vector<std::vector<std::int32_t>> element_points(const std::vector<Rec>& recs, gds::RecordType type) {
  std::vector<std::vector<std::int32_t>> out;
  for (std::size_t i = 0; i < recs.size(); ++i) {
    if (recs[i].record() != type) {
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

std::vector<std::string> text_strings(const std::vector<Rec>& recs) {
  std::vector<std::string> out;
  for (const Rec& r : recs) {
    if (r.record() == gds::RecordType::string) {
      out.push_back(r.text());
    }
  }
  return out;
}

db::Label plain_label(std::string text, tech::TileType type, db::Rect rect) {
  db::Label l;
  l.text = std::move(text);
  l.type = type;
  l.rect = rect;
  return l;
}

db::Label port_label(std::string text, tech::TileType type, std::uint32_t index) {
  db::Label l = plain_label(std::move(text), type, {0, 0, 0, 0});
  l.port_directions = db::kPortNorth;
  l.port_index = index;
  return l;
}

std::size_t count_code(const cmd::CommandResult& result, std::string_view code) {
  std::size_t n = 0;
  for (const auto& d : result.diagnostics) {
    n += d.code == code ? 1U : 0U;
  }
  return n;
}

}  // namespace

TEST_CASE("gds merge yes writes the L-shape as one polygon", "[cap:gds-write][req:Merged polygon output]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    f.options.settings.merge = true;
    db::CellDef& top = f.cell("top");
    f.paint(top, f.m1, db::Rect{0, 2, 2, 4});
    f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
    const std::vector<Rec> recs = decode(f.write(top));
    CHECK(f.last.ok());
    const auto polygons = element_points(recs, gds::RecordType::boundary);
    REQUIRE(polygons.size() == 1);
    CHECK(polygons[0] == std::vector<std::int32_t>{200, 400, 0, 400, 0, 0, 400, 0, 400, 200, 200, 200, 200, 400});
  }
}

TEST_CASE("gds merge yes writes an isolated rectangle as the unmerged one",
          "[cap:gds-write][req:Merged polygon output]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 4, 2});
  const std::string plain = f.write(top);
  f.options.settings.merge = true;
  CHECK(f.write(top) == plain);
}

TEST_CASE("cell labels are written after the mask layers unless gds labels is no",
          "[cap:gds-write][req:Label TEXT records]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    f.style.label_layer[f.m1] = 0;
    db::CellDef& top = f.cell("top");
    f.paint(top, f.m1, db::Rect{0, 0, 10, 10});
    top.labels().push_back(plain_label("VDD", f.m1, db::Rect{5, 5, 5, 5}));
    std::vector<Rec> recs = decode(f.write(top));
    CHECK(text_strings(recs) == std::vector<std::string>{"VDD"});
    CHECK(element_points(recs, gds::RecordType::text) == std::vector<std::vector<std::int32_t>>{{500, 500}});
    const std::vector<gds::RecordType> order = types(structure(recs, "top"));
    const auto boundary = std::find(order.begin(), order.end(), gds::RecordType::boundary);
    const auto text = std::find(order.begin(), order.end(), gds::RecordType::text);
    CHECK(boundary < text);

    f.options.settings.labels = false;
    recs = decode(f.write(top));
    CHECK(text_strings(recs).empty());
  }
}

TEST_CASE("label layer tiles are written whatever gds labels says", "[cap:gds-write][req:Label layers]") {
  Fixture f;
  f.add_layer("BND", f.m1, mask::GdsLayer{63, 5});
  f.style.layers.back().label_layer = true;
  f.options.settings.labels = false;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 3, 2});
  const std::vector<Rec> recs = decode(f.write(top));
  CHECK(text_strings(recs) == std::vector<std::string>{"BND"});
  CHECK(element_points(recs, gds::RecordType::text) == std::vector<std::vector<std::int32_t>>{{150, 100}});
}

TEST_CASE("the port-layer warning is given once per write across structures",
          "[cap:gds-write][req:Label TEXT records]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    f.style.port_text[f.m1] = 0;  // `labels metal1` only: no port layer
    db::CellDef& top = f.cell("top");
    db::CellDef& sub = f.cell("sub");
    sub.labels().push_back(port_label("A", f.m1, 1));
    top.labels().push_back(port_label("B", f.m1, 1));
    f.paint(sub, f.m1, db::Rect{0, 0, 2, 2});
    f.place(top, sub);
    const std::vector<Rec> recs = decode(f.write(top));
    CHECK(count_code(f.last, "GDS-WRITE-020") == 1);
    CHECK(text_strings(recs) == (on ? std::vector<std::string>{} : std::vector<std::string>{"A", "B"}));
  }
}

TEST_CASE("a label string is encoded for output and the label keeps its text",
          "[cap:gds-write][req:Magic write-time side effects]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    f.style.label_layer[f.m1] = 0;
    db::CellDef& top = f.cell("top");
    f.paint(top, f.m1, db::Rect{0, 0, 2, 2});
    top.labels().push_back(plain_label("a<1>", f.m1, db::Rect{1, 1, 1, 1}));
    const std::vector<Rec> recs = decode(f.write(top));
    CHECK(text_strings(recs) == std::vector<std::string>{"a_1_"});
    CHECK(top.labels().front().text == "a<1>");
  }
}

TEST_CASE("the emit phase generates with the settings taken at prepare time",
          "[cap:gds-write][req:Write preconditions and subtree loading]") {
  // Regression: emit built its GenerationSource from the editor, so a
  // command between the phases could change compat and `cif unfracture`.
  const auto build = [](Fixture& f) -> db::CellDef& {
    db::CellDef& top = f.cell("top");
    // One '/' triangle in two split tiles and the gap above the lower one.
    const db::TriangleSpec half{db::Diagonal::ne_sw, false};
    REQUIRE(db::paint_type(top, *f.technology, db::Rect{0, 0, 10, 10}, f.m1, half).ok());
    REQUIRE(db::paint_type(top, *f.technology, db::Rect{10, 10, 20, 20}, f.m1, half).ok());
    f.paint(top, f.m1, db::Rect{0, 10, 10, 20});
    db::recompute_bbox(top);
    return top;
  };
  Fixture reference;
  const std::string expected = reference.write(build(reference));
  REQUIRE(reference.last.ok());

  Fixture f;
  db::CellDef& top = build(f);
  f.install();
  auto prepared = gds::prepare_write(f.ed, top, f.options);
  REQUIRE(prepared.ok());
  f.ed.mask.cif.unfracture = true;  // a command between the phases
  f.set_compat(false);
  REQUIRE(gds::emit_write(prepared.value(), "/out.gds").ok());
  CHECK(f.file() == expected);
}

}  // namespace gds_write_test
