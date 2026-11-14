// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Label layers and label TEXT records (tasks.md 10.5; gds-write "Label
// layers", "Label TEXT records"): TEXT bytes, PRESENTATION, MAG and ANGLE,
// the port label order (Q-MASK-205, our own stable sort), port labels
// without a port layer (Q-MASK-206), MAG and magscale (Q-MASK-221), label
// rectangles and label-layer tiles. Owner: GDSW.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/write/labels_sort.hpp"
#include "hefesto/gds/write/labels_text.hpp"
#include "merge_labels_fixture.hpp"

namespace gds = hefesto::gds;
namespace mask = hefesto::mask;
namespace db = hefesto::db;
namespace diag = hefesto::diag;
namespace compat = hefesto::compat;
using gdsw_test::Bytes;
using gdsw_test::hex;

namespace {

constexpr hefesto::tech::TileType kMetal1 = 5;
const compat::CompatSettings kOn{true, true};
const compat::CompatSettings kOff{false, true};

mask::OutputLayer layer(std::string name, int calma, int datatype, bool with_ops) {
  mask::OutputLayer l;
  l.name = std::move(name);
  l.gds = mask::GdsLayer{calma, datatype};
  if (with_ops) {
    l.ops.push_back(mask::LayerOp{});
  }
  return l;
}

/// Layers: 0 M1 (operators, 49/0), 1 M1PIN (operators, 49/16), 2 M1LBL (no
/// operators, 49/10), 3 BND (labellayer, 63/5), 4 XL (no calma), 5 BIG
/// (calma 300).
mask::OutputStyle style() {
  mask::OutputStyle s;
  s.name = "gdsout";
  s.scale = mask::StyleScale{10, 1, 1};
  s.layers.push_back(layer("M1", 49, 0, true));
  s.layers.push_back(layer("M1PIN", 49, 16, true));
  s.layers.push_back(layer("M1LBL", 49, 10, false));
  mask::OutputLayer bnd = layer("BND", 63, 5, false);
  bnd.label_layer = true;
  s.layers.push_back(bnd);
  mask::OutputLayer xl;
  xl.name = "XL";
  s.layers.push_back(xl);
  s.layers.push_back(layer("BIG", 300, 0, false));
  s.label_layer[kMetal1] = 0;
  return s;
}

db::Label label(std::string text, db::Rect rect) {
  db::Label l;
  l.text = std::move(text);
  l.type = kMetal1;
  l.rect = rect;
  return l;
}

db::Label port(std::string text, std::uint32_t index) {
  db::Label l = label(std::move(text), {0, 0, 0, 0});
  l.port_directions = db::kPortNorth;
  l.port_index = index;
  return l;
}

/// The STRING record texts of a byte stream, in order.
std::vector<std::string> strings(const Bytes& bytes) {
  std::vector<std::string> out;
  std::size_t at = 0;
  while (at + 4 <= bytes.size()) {
    const std::size_t length = (std::to_integer<std::size_t>(bytes[at]) << 8U) | std::to_integer<std::size_t>(bytes[at + 1]);
    REQUIRE(length >= 4);
    if (bytes[at + 2] == std::byte{0x19}) {
      std::string text;
      for (std::size_t i = at + 4; i < at + length && bytes[i] != std::byte{0}; ++i) {
        text.push_back(static_cast<char>(bytes[i]));
      }
      out.push_back(text);
    }
    at += length;
  }
  return out;
}

struct Emit {
  gdsw_test::Output out;
  diag::CollectingSink sink;
  gds::PortLabelWarnings warned;
};

}  // namespace

TEST_CASE("Default-font point label TEXT bytes", "[cap:gds-write][req:Label TEXT records]") {
  const mask::OutputStyle s = style();
  gdsw_test::Output out;
  gds::LabelOutput output(out.writer(), s);
  output.write_scale = 100;
  const std::vector<db::Label> labels{label("VDD", {5, 5, 5, 5})};
  REQUIRE(gds::emit_cell_labels(output, labels).ok());
  CHECK(out.bytes() == hex("0004 0C00 | 0006 0D02 0031 | 0006 1602 0000 | 000C 1003 000001F4 000001F4 |"
                           " 0008 1906 5644 4400 | 0004 1100"));
}

TEST_CASE("Font label: PRESENTATION, STRANS, MAG and ANGLE", "[cap:gds-write][req:Label TEXT records]") {
  const mask::OutputStyle s = style();  // scalefactor 10, expander 1
  gdsw_test::Output out;
  gds::LabelOutput output(out.writer(), s);
  output.write_scale = 100;
  db::Label l = label("A", {0, 0, 0, 0});
  l.font = 0;
  l.size = 80;
  l.justification = 1;  // N
  l.rotation = 90;
  REQUIRE(gds::emit_label(output, l, gds::plain_label_layers(s, kMetal1)).ok());
  CHECK(out.bytes() == hex("0004 0C00 0006 0D02 0031 0006 1602 0000 | 0006 1701 0009 | 0006 1A01 0000 |"
                           " 000C 1B05 4110000000000000 | 000C 1C05 425A000000000000 |"
                           " 000C 1003 00000000 00000000 | 0006 1906 4100 | 0004 1100"));
}

TEST_CASE("PRESENTATION word per justification and font", "[cap:gds-write][req:Label TEXT records]") {
  const std::vector<std::uint16_t> expected{0x5, 0x9, 0x8, 0x4, 0x0, 0x1, 0x2, 0x6, 0xa};
  for (db::Position j = 0; j < 9; ++j) {
    CHECK(gds::text_presentation(0, j) == expected[static_cast<std::size_t>(j)]);
  }
  CHECK(gds::text_presentation(5, 1) == 0x19);   // font cropped to two bits
  CHECK(gds::text_presentation(3, 0) == 0x35);
  CHECK(gds::text_presentation(1, 42) == 0x10);  // unknown position: font bits only
  CHECK(gds::text_presentation(1, -1) == 0x10);
}

TEST_CASE("MAG ignores magscale with magic-compat ON (Q-MASK-221)", "[cap:gds-write][req:Label TEXT records]") {
  const mask::StyleScale scale{10, 1, 1};
  CHECK(gds::text_magnification(80, scale, 2.0, kOn) == 1.0);
  CHECK(gds::text_magnification(80, scale, 1.0, kOn) == 1.0);
}

TEST_CASE("MAG is divided by magscale with magic-compat OFF (Q-MASK-221)", "[cap:gds-write][req:Label TEXT records]") {
  const mask::StyleScale scale{10, 1, 1};
  CHECK(gds::text_magnification(80, scale, 2.0, kOff) == 0.5);
  CHECK(gds::text_magnification(80, scale, 1.0, kOff) == 1.0);  // the settings agree for magscale 1
  CHECK(gds::text_magnification(80, scale, 0.0, kOff) == 1.0);  // a non-positive magscale is ignored
  // Left-to-right binary64: (size / 800) * S / E.
  const mask::StyleScale odd{3, 7, 1};
  CHECK(gds::text_magnification(100, odd, 1.0, kOff) == (100.0 / 800) * 3.0 / 7.0);
}

TEST_CASE("Port labels follow the plain labels, sorted by port index", "[cap:gds-write][req:Label TEXT records]") {
  mask::OutputStyle s = style();
  s.port_text[kMetal1] = 1;
  s.port_layer[kMetal1] = 1;
  for (const auto& mode : {kOn, kOff}) {
    gdsw_test::Output out;
    gds::LabelOutput output(out.writer(), s);
    output.compat = mode;
    const std::vector<db::Label> labels{port("B", 2), port("A", 1), label("n1", {0, 0, 0, 0})};
    REQUIRE(gds::emit_cell_labels(output, labels).ok());
    CHECK(strings(out.bytes()) == std::vector<std::string>{"n1", "A", "B"});
  }
}

TEST_CASE("Equal port indices come out in reverse label-list order on every host (Q-MASK-205)",
          "[cap:gds-write][req:Label TEXT records]") {
  const std::vector<db::Label> labels{port("x", 1), label("p", {0, 0, 0, 0}), port("y", 1), port("z", 0),
                                      port("w", 1)};
  CHECK(gds::port_label_order(labels) == std::vector<std::size_t>{3, 4, 2, 0});

  mask::OutputStyle s = style();
  s.port_text[kMetal1] = 1;
  s.port_layer[kMetal1] = 1;
  gdsw_test::Output out;
  gds::LabelOutput output(out.writer(), s);
  REQUIRE(gds::emit_cell_labels(output, labels).ok());
  CHECK(strings(out.bytes()) == std::vector<std::string>{"p", "z", "w", "y", "x"});
}

TEST_CASE("Port label without a port layer is dropped with magic-compat ON (Q-MASK-206)",
          "[cap:gds-write][req:Label TEXT records]") {
  mask::OutputStyle s = style();
  s.port_text[kMetal1] = 0;  // `labels metal1` only: port text, no port layer
  Emit e;
  gds::LabelOutput output(e.out.writer(), s);
  output.compat = kOn;
  output.sink = &e.sink;
  output.warned = &e.warned;
  const std::vector<db::Label> labels{port("A", 1), port("B", 2)};
  REQUIRE(gds::emit_cell_labels(output, labels).ok());
  CHECK(e.out.bytes().empty());
  const auto diagnostics = e.sink.snapshot();
  REQUIRE(diagnostics.size() == 1);  // once per type and write
  CHECK(diagnostics[0].code == "GDS-WRITE-020");
  CHECK(diagnostics[0].native_only);
  CHECK(diagnostics[0].text.find("not written") != std::string::npos);
}

TEST_CASE("Port label without a port layer is written on the port-text layer with magic-compat OFF (Q-MASK-206)",
          "[cap:gds-write][req:Label TEXT records]") {
  mask::OutputStyle s = style();
  s.port_text[kMetal1] = 2;  // M1LBL: no operators, so the rectangle is drawn too
  Emit e;
  gds::LabelOutput output(e.out.writer(), s);
  output.compat = kOff;
  output.sink = &e.sink;
  output.warned = &e.warned;
  db::Label a = port("A", 1);
  a.rect = {0, 0, 2, 1};
  const std::vector<db::Label> labels{a, port("B", 2)};
  REQUIRE(gds::emit_cell_labels(output, labels).ok());
  const Bytes bytes = e.out.bytes();
  CHECK(strings(bytes) == std::vector<std::string>{"A", "B"});
  CHECK(bytes == hex("0004 0C00 0006 0D02 0031 0006 1602 000A 000C 1003 00000001 00000000 0006 1906 4100 0004 1100"
                     " 0004 0800 0006 0D02 0031 0006 0E02 000A 002C 1003"
                     " 00000000 00000000 00000002 00000000 00000002 00000001 00000000 00000001 00000000 00000000"
                     " 0004 1100"
                     " 0004 0C00 0006 0D02 0031 0006 1602 000A 000C 1003 00000000 00000000 0006 1906 4200 0004 1100"));
  const auto diagnostics = e.sink.snapshot();
  REQUIRE(diagnostics.size() == 1);
  CHECK(diagnostics[0].text.find("written on their text layer") != std::string::npos);
}

TEST_CASE("Port labels with neither layer are skipped in both settings", "[cap:gds-write][req:Label TEXT records]") {
  const mask::OutputStyle s = style();
  for (const auto& mode : {kOn, kOff}) {
    CHECK_FALSE(gds::port_label_layers(s, kMetal1, mode).has_value());
  }
  CHECK_FALSE(gds::port_label_layers(s, 300, kOff).has_value());
}

TEST_CASE("Label rectangle on a labels-only layer", "[cap:gds-write][req:Label TEXT records]") {
  mask::OutputStyle s = style();
  s.label_layer[kMetal1] = 2;  // M1LBL, 49/10, no operators
  gdsw_test::Output out;
  gds::LabelOutput output(out.writer(), s);
  output.write_scale = 100;
  const std::vector<db::Label> labels{label("L", {0, 0, 2, 1}), label("P", {3, 3, 3, 3})};
  REQUIRE(gds::emit_cell_labels(output, labels).ok());
  CHECK(out.bytes() == hex("0004 0C00 0006 0D02 0031 0006 1602 000A 000C 1003 00000064 00000032 0006 1906 4C00 0004 1100"
                           " 0004 0800 0006 0D02 0031 0006 0E02 000A 002C 1003"
                           " 00000000 00000000 000000C8 00000000 000000C8 00000064 00000000 00000064 00000000 00000000"
                           " 0004 1100"
                           " 0004 0C00 0006 0D02 0031 0006 1602 000A 000C 1003 0000012C 0000012C 0006 1906 5000 0004 1100"));
}

TEST_CASE("A label whose text layer has no valid GDS number is not written",
          "[cap:gds-write][req:Label TEXT records]") {
  mask::OutputStyle s = style();
  gdsw_test::Output out;
  gds::LabelOutput output(out.writer(), s);
  const db::Label l = label("X", {0, 0, 1, 1});
  REQUIRE(gds::emit_label(output, l, {4, 4}).ok());  // XL: no calma statement
  REQUIRE(gds::emit_label(output, l, {5, 5}).ok());  // BIG: calma 300
  REQUIRE(gds::emit_label(output, l, {mask::kNoLayer, 2}).ok());
  REQUIRE(gds::emit_label(output, l, {99, 99}).ok());
  CHECK(out.bytes().empty());
}

TEST_CASE("Label strings go through the string record hook", "[cap:gds-write][req:Label TEXT records]") {
  const mask::OutputStyle s = style();
  gdsw_test::Output out;
  gds::LabelOutput output(out.writer(), s);
  std::vector<std::string> seen;
  output.string_record = [&seen](gds::RecordWriter& w, gds::RecordType type, std::string_view text) {
    seen.emplace_back(text);
    return w.write_string(type, "Z");
  };
  const std::vector<db::Label> labels{label("vdd!", {0, 0, 0, 0})};
  REQUIRE(gds::emit_cell_labels(output, labels).ok());
  CHECK(seen == std::vector<std::string>{"vdd!"});
  CHECK(strings(out.bytes()) == std::vector<std::string>{"Z"});
  CHECK(labels[0].text == "vdd!");  // Q-MASK-208: the label is untouched
}

TEST_CASE("Label coordinates outside I4 fail the write", "[cap:gds-write][req:Label TEXT records]") {
  const mask::OutputStyle s = style();
  gdsw_test::Output out;
  gds::LabelOutput output(out.writer(), s);
  output.write_scale = std::int64_t{1} << 40;
  const std::vector<db::Label> labels{label("A", {1000, 1000, 1000, 1000})};
  const auto status = gds::emit_cell_labels(output, labels);
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == "GDS-WRITE-004");
  output.write_scale = std::int64_t{1} << 62;
  REQUIRE_FALSE(gds::emit_cell_labels(output, labels).ok());
}

TEST_CASE("A label outside I4 writes nothing and names the cell and the value",
          "[cap:gds-write][req:Output scale factors]") {
  // Regression: TEXT, LAYER and TEXTTYPE were written before the centre
  // was range-checked, and the error named neither the cell nor the value.
  const mask::OutputStyle s = style();
  gdsw_test::Output out;
  gds::LabelOutput output(out.writer(), s);
  output.cell = "top";
  output.write_scale = std::int64_t{1} << 62;
  const std::vector<db::Label> labels{label("A", {1000, 1000, 1000, 1000})};
  const auto status = gds::emit_cell_labels(output, labels);
  REQUIRE_FALSE(status.ok());
  CHECK(out.bytes().empty());
  // (1000 + 1000) * 2^62 / 2
  CHECK(status.error().text.find("value 4611686018427387904000 in cell top") != std::string::npos);
}

TEST_CASE("Label layer tile TEXT bytes", "[cap:gds-write][req:Label layers]") {
  const mask::OutputStyle s = style();
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{0, 0, 15, 10}});
  gdsw_test::Output out;
  gds::LabelOutput output(out.writer(), s);
  output.paint_scale = 1;
  REQUIRE(gds::emit_label_layer(output, 3, plane).ok());
  CHECK(out.bytes() == hex("0004 0C00 | 0006 0D02 003F | 0006 1602 0005 | 000C 1003 00000007 00000005 |"
                           " 0008 1906 424E 4400 | 0004 1100"));
}

TEST_CASE("Label layer centres truncate toward zero; split tiles are ignored", "[cap:gds-write][req:Label layers]") {
  const mask::OutputStyle s = style();
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{-15, -10, 0, -1}});
  REQUIRE(plane.paint_triangle({40, 40, 50, 50}, db::Diagonal::nw_se, false).ok());
  gdsw_test::Output out;
  gds::LabelOutput output(out.writer(), s);
  output.paint_scale = 1;
  REQUIRE(gds::emit_label_layer(output, 3, plane).ok());
  CHECK(out.bytes() == hex("0004 0C00 0006 0D02 003F 0006 1602 0005 000C 1003 FFFFFFF9 FFFFFFFB"
                           " 0008 1906 424E 4400 0004 1100"));
}

TEST_CASE("Label layer tiles are clipped to the clip area", "[cap:gds-write][req:Label layers]") {
  const mask::OutputStyle s = style();
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{0, 0, 20, 10}, {100, 100, 110, 110}});
  gdsw_test::Output out;
  gds::LabelOutput output(out.writer(), s);
  output.paint_scale = 2;
  REQUIRE(gds::emit_label_layer(output, 3, plane, db::Rect{10, 0, 50, 50}).ok());
  // Clipped tile (10,0)-(20,10): ((10 + 20) * 2 / 2, (0 + 10) * 2 / 2).
  CHECK(out.bytes() == hex("0004 0C00 0006 0D02 003F 0006 1602 0005 000C 1003 0000001E 0000000A"
                           " 0008 1906 424E 4400 0004 1100"));
}

TEST_CASE("A label layer without a valid GDS number writes nothing", "[cap:gds-write][req:Label layers]") {
  const mask::OutputStyle s = style();
  mask::MaskPlane plane;
  gdsw_test::paint(plane, {{0, 0, 15, 10}});
  gdsw_test::Output out;
  gds::LabelOutput output(out.writer(), s);
  REQUIRE(gds::emit_label_layer(output, 4, plane).ok());
  REQUIRE(gds::emit_label_layer(output, 5, plane).ok());
  REQUIRE(gds::emit_label_layer(output, 99, plane).ok());
  CHECK(out.bytes().empty());
}

TEST_CASE("The stable merge sort keeps equal keys in input order like std::stable_sort",
          "[cap:gds-write][req:Label TEXT records]") {
  std::mt19937 rng(205);
  for (std::size_t n = 0; n < 300; n += 7) {
    std::vector<std::pair<int, std::size_t>> items;
    std::uniform_int_distribution<int> key(0, 5);
    for (std::size_t i = 0; i < n; ++i) {
      items.emplace_back(key(rng), i);
    }
    auto expected = items;
    const auto by_key = [](const auto& a, const auto& b) { return a.first < b.first; };
    std::stable_sort(expected.begin(), expected.end(), by_key);
    gds::stable_merge_sort(items, by_key);
    CHECK(items == expected);
  }
}
