// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS encoder (tasks.md 10.4, 10.5): minimal forms, modal omission,
// strict and non-strict tables, CBLOCKs, END with validation, the 419-byte
// reference file, decode(encode(x)) = x over seeded random records and
// encode(decode(f)) = f for hand-assembled minimal files. Owner: OASFMT.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "codec_fixture.hpp"
#include "hefesto/oasis/codec/encoder.hpp"

using oasis_test::Decoded;
using oasis_test::hex;
using oasis_test::pt;
using oasis_test::str;
namespace oasis = hefesto::oasis;
namespace gds = hefesto::gds;
namespace diag = hefesto::diag;

namespace {

/// One cell to encode: its name and elements.
struct Cell {
  std::string name;
  std::vector<oasis::Element> elements;

  friend bool operator==(const Cell&, const Cell&) = default;
};

oasis::Element rectangle(std::uint64_t layer, std::uint64_t datatype, oasis::Point64 at, oasis::Point64 size) {
  oasis::Element e;
  e.kind = oasis::ElementKind::rectangle;
  e.layer = layer;
  e.datatype = datatype;
  e.position = at;
  e.size = size;
  return e;
}

std::vector<std::byte> encode(const std::vector<Cell>& cells, oasis::EncoderOptions options) {
  gds::MemorySink sink;
  oasis::Encoder encoder{sink, options};
  REQUIRE(encoder.begin().ok());
  for (const Cell& cell : cells) {
    REQUIRE(encoder.begin_cell(cell.name).ok());
    for (const auto& e : cell.elements) {
      const auto status = encoder.element(e);
      INFO((status.ok() ? std::string{} : status.error().text));
      REQUIRE(status.ok());
    }
  }
  REQUIRE(encoder.finish().ok());
  return sink.bytes();
}

/// Names given by reference become names.
oasis::Element resolved(oasis::Element e, const oasis::NameTables& names) {
  if (e.reference) {
    const auto& table = e.kind == oasis::ElementKind::placement ? names.cellnames : names.textstrings;
    e.name = table.at(*e.reference);
    e.reference.reset();
  }
  return e;
}

/// The cells of a decoded file, names resolved.
std::vector<Cell> cells_of(const Decoded& d) {
  std::vector<Cell> cells;
  for (const auto& r : d.records) {
    if (r.id == oasis::RecordId::cell_by_name || r.id == oasis::RecordId::cell_by_reference) {
      cells.push_back({r.reference ? d.names.cellnames.at(*r.reference) : r.name, {}});
    } else if (r.element && !cells.empty()) {
      cells.back().elements.push_back(resolved(*r.element, d.names));
    }
  }
  return cells;
}

bool contains(const std::vector<std::byte>& haystack, const std::vector<std::byte>& needle) {
  return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
}

// --- seeded random elements -------------------------------------------------

class Random {
 public:
  explicit Random(std::uint32_t seed) : engine_(seed) {}

  std::int64_t range(std::int64_t lo, std::int64_t hi) {
    return std::uniform_int_distribution<std::int64_t>{lo, hi}(engine_);
  }
  oasis::Point64 point(std::int64_t span) { return {range(-span, span), range(-span, span)}; }
  std::string name(char first) {
    std::string s(1, first);
    for (std::int64_t i = range(0, 6); i > 0; --i) {
      s.push_back(static_cast<char>(range('a', 'z')));
    }
    return s;
  }

  /// Vertices after (0, 0) whose steps all fit `type` (2-5; 0/1 alternate).
  std::vector<oasis::Point64> steps(std::uint8_t type, std::size_t count) {
    std::vector<oasis::Point64> out;
    oasis::Point64 p;
    for (std::size_t i = 0; i < count; ++i) {
      const std::int64_t m = range(1, 500) * (range(0, 1) != 0 ? 1 : -1);
      oasis::Point64 d = point(500);
      if (type <= 1) {
        const bool horizontal = ((i % 2) == 0) != (type == 1);
        d = horizontal ? pt(m, 0) : pt(0, m);
      } else if (type == 2) {
        d = range(0, 1) != 0 ? pt(m, 0) : pt(0, m);
      } else if (type == 3) {
        d = pt(m, range(-1, 1) * m);
      }
      p = pt(p.x + d.x, p.y + d.y);
      out.push_back(p);
    }
    return out;
  }

  oasis::Repetition repetition() {
    oasis::Repetition r;
    r.type = static_cast<std::uint8_t>(range(1, 11));
    const auto count = static_cast<std::uint64_t>(range(2, 5));
    switch (r.type) {
      case 1:
        r.columns = count;
        r.rows = static_cast<std::uint64_t>(range(2, 4));
        r.column_step = pt(range(0, 900), 0);
        r.row_step = pt(0, range(0, 900));
        break;
      case 2:
        r.columns = count;
        r.column_step = pt(range(0, 900), 0);
        break;
      case 3:
        r.rows = count;
        r.row_step = pt(0, range(0, 900));
        break;
      case 8:
        r.columns = count;
        r.rows = static_cast<std::uint64_t>(range(2, 4));
        r.column_step = point(900);
        r.row_step = point(900);
        break;
      case 9:
        r.columns = count;
        r.column_step = point(900);
        break;
      default:
        list(r, count);
        break;
    }
    return r;
  }

  oasis::Element element(oasis::ElementKind kind) {
    oasis::Element e;
    e.kind = kind;
    e.layer = static_cast<std::uint64_t>(range(0, 3));
    e.datatype = static_cast<std::uint64_t>(range(0, 1));
    e.position = point(5000);
    if (range(0, 3) == 0) {
      e.repetition = repetition();
    }
    fill(e);
    return e;
  }

 private:
  void list(oasis::Repetition& r, std::uint64_t count) {
    const bool gridded = r.type == 5 || r.type == 7 || r.type == 11;
    r.grid = gridded ? static_cast<std::uint64_t>(range(1, 9)) : 0;
    const std::int64_t g = gridded ? static_cast<std::int64_t>(r.grid) : 1;
    oasis::Point64 p;
    for (std::uint64_t i = 1; i < count; ++i) {
      const std::int64_t s = range(0, 300) * g;
      oasis::Point64 step = r.type <= 5 ? pt(s, 0) : pt(0, s);
      if (r.type >= 10) {
        step = pt(range(-300, 300) * g, range(-300, 300) * g);
      }
      p = pt(p.x + step.x, p.y + step.y);
      r.offsets.push_back(p);
    }
  }

  void fill(oasis::Element& e) {
    switch (e.kind) {
      case oasis::ElementKind::placement:
        e.name = name('C');
        e.flip = range(0, 1) != 0;
        e.angle = std::vector{0.0, 90.0, 180.0, 270.0, 45.0}[static_cast<std::size_t>(range(0, 4))];
        e.magnification = std::vector{1.0, 0.5, 2.0}[static_cast<std::size_t>(range(0, 2))];
        e.layer = 0;
        e.datatype = 0;
        break;
      case oasis::ElementKind::text:
        e.name = name('t') + " x";
        break;
      case oasis::ElementKind::rectangle:
        e.size = pt(range(0, 900), range(0, 900));
        break;
      case oasis::ElementKind::polygon: {
        const auto type = static_cast<std::uint8_t>(range(0, 5));
        e.point_list_type = type;
        if (type <= 1) {
          e.points = steps(type, static_cast<std::size_t>(range(1, 4)) * 2);
          const auto last = e.points.back();
          e.points.push_back(type == 0 ? pt(0, last.y) : pt(last.x, 0));  // the synthesized vertex
        } else {
          e.points = steps(type, static_cast<std::size_t>(range(2, 8)));
        }
        break;
      }
      case oasis::ElementKind::path:
        e.point_list_type = static_cast<std::uint8_t>(range(0, 5));
        e.points = steps(*e.point_list_type, static_cast<std::size_t>(range(1, 6)));
        e.half_width = range(0, 50);
        e.start_extension = std::vector<std::int64_t>{0, e.half_width, -7}[static_cast<std::size_t>(range(0, 2))];
        e.end_extension = range(-20, 20);
        break;
      case oasis::ElementKind::trapezoid:
        e.size = pt(range(40, 900), range(40, 900));
        e.delta_a = range(-10, 10);
        e.delta_b = range(-10, 10);
        e.vertical = range(0, 1) != 0;
        break;
      case oasis::ElementKind::ctrapezoid:
        e.ctrapezoid_type = static_cast<std::uint8_t>(range(0, 25));
        // w odd below 200, h above 400: no type degenerates (w != h, 2w != h).
        e.size = pt(oasis::ctrapezoid_uses_width(e.ctrapezoid_type) ? range(0, 99) * 2 + 1 : 0,
                    oasis::ctrapezoid_uses_height(e.ctrapezoid_type) ? range(401, 900) : 0);
        break;
      case oasis::ElementKind::circle:
        e.radius = range(0, 900);
        break;
      case oasis::ElementKind::xgeometry:
        e.attribute = static_cast<std::uint64_t>(range(0, 9));
        e.payload = std::string{"\x00\xff", 2} + name('g');
        break;
      case oasis::ElementKind::xelement:
        e = oasis::Element{};
        e.kind = oasis::ElementKind::xelement;
        e.attribute = static_cast<std::uint64_t>(range(0, 9));
        e.payload = name('x');
        break;
    }
  }

  std::mt19937 engine_;
};

}  // namespace

TEST_CASE("the encoder writes the 419-byte reference file", "[cap:oasis-format][req:Complete annotated example]") {
  gds::MemorySink sink;
  oasis::Encoder encoder{sink, oasis::EncoderOptions{true, oasis::ValidationScheme::crc32, 0, 1000.0}};
  REQUIRE(encoder.begin().ok());
  REQUIRE(encoder.standard_properties("top").ok());
  REQUIRE(encoder.layer_name("M1", 49, 0).ok());
  REQUIRE(encoder.begin_cell("top").ok());
  REQUIRE(encoder.element(rectangle(49, 0, pt(0, 0), pt(400, 200))).ok());
  REQUIRE(encoder.finish().ok());
  CHECK(sink.bytes() == oasis_test::decode_file("reference_419.oas"));
}

TEST_CASE("re-encoding the decoded reference file gives it back", "[cap:oasis-format][req:Complete annotated example]") {
  const std::vector<std::byte> bytes = oasis_test::decode_file("reference_419.oas");
  const Decoded d = oasis_test::decode(bytes);
  REQUIRE_FALSE(d.error);
  gds::MemorySink sink;
  oasis::Encoder encoder{sink, oasis::EncoderOptions{true, oasis::ValidationScheme::crc32, 0, 1000.0}};
  REQUIRE(encoder.begin().ok());
  for (const auto& r : d.records) {
    if (r.property && r.target == oasis::PropertyTarget::file) {
      oasis::Property p = *r.property;
      p.name = d.names.propnames.at(*p.name_reference);
      p.name_reference.reset();
      for (auto& v : p.values) {
        if (v.kind == oasis::PropertyValue::Kind::n_reference) {
          v = oasis::PropertyValue::of_string(oasis::PropertyValue::Kind::n_string, d.names.propstrings.at(v.reference));
        }
      }
      REQUIRE(encoder.file_property(p).ok());
    }
  }
  for (const auto& layer : d.names.layernames) {
    REQUIRE(encoder.layer_name(layer.name, layer.layers.low, layer.datatypes.low).ok());
  }
  for (const Cell& cell : cells_of(d)) {
    REQUIRE(encoder.begin_cell(cell.name).ok());
    for (const auto& e : cell.elements) {
      REQUIRE(encoder.element(e).ok());
    }
  }
  REQUIRE(encoder.finish().ok());
  CHECK(sink.bytes() == bytes);
}

TEST_CASE("encode(decode(f)) = f for a hand-assembled minimal non-strict file",
          "[cap:oasis-format][req:Modal variables and reset rules]") {
  oasis_test::File f{false};
  f.add("0E").add(str("top"));
  f.add("14 63 31 00 90 03 C8 01  14 10 D0 0F  15 20 00 02 50 3C  16 E0 05 0A 00 01 C8 01  17 60 64 14 28 29");
  f.add("1A E0 00 1E 0A  1B 20 0A  13 43 02 68 69 05 00  11 80 03 73 75 62");
  const std::vector<std::byte> bytes = f.end();
  const Decoded d = oasis_test::decode(bytes);
  REQUIRE_FALSE(d.error);
  CHECK(d.diagnostics.empty());
  const auto cells = cells_of(d);
  REQUIRE(cells.size() == 1);
  CHECK(cells[0].elements.size() == 9);
  CHECK(encode(cells, oasis::EncoderOptions{false, oasis::ValidationScheme::crc32, 0, 1000.0}) == bytes);
}

TEST_CASE("decode(encode(x)) = x over seeded random records", "[cap:oasis-format][req:Modal variables and reset rules]") {
  constexpr std::array<oasis::ElementKind, 10> kinds = {
      oasis::ElementKind::placement, oasis::ElementKind::text,       oasis::ElementKind::rectangle,
      oasis::ElementKind::polygon,   oasis::ElementKind::path,       oasis::ElementKind::trapezoid,
      oasis::ElementKind::ctrapezoid, oasis::ElementKind::circle,    oasis::ElementKind::xgeometry,
      oasis::ElementKind::xelement};
  for (std::uint32_t seed = 1; seed <= 20; ++seed) {
    CAPTURE(seed);
    Random random{seed};
    std::vector<Cell> cells;
    for (int c = 0; c < 3; ++c) {
      Cell cell{"cell" + std::to_string(c), {}};
      for (int i = 0; i < 60; ++i) {
        cell.elements.push_back(random.element(kinds[static_cast<std::size_t>(random.range(0, 9))]));
      }
      cells.push_back(std::move(cell));
    }
    for (const bool strict : {true, false}) {
      for (const int compress : {0, 6}) {
        const auto bytes = encode(cells, oasis::EncoderOptions{strict, oasis::ValidationScheme::crc32, compress, 1000.0});
        const Decoded d = oasis_test::decode(bytes);
        REQUIRE_FALSE(d.error);
        CHECK(d.diagnostics.empty());
        CHECK(cells_of(d) == cells);
      }
    }
  }
}

TEST_CASE("a field equal to its modal value is omitted", "[cap:oasis-format][req:Modal variables and reset rules]") {
  const auto bytes = encode({{"top", {rectangle(49, 0, pt(0, 0), pt(400, 200)), rectangle(49, 0, pt(1000, 0), pt(400, 200))}}},
                            oasis::EncoderOptions{true, oasis::ValidationScheme::crc32, 0, 1000.0});
  CHECK(contains(bytes, hex("0D 00 14 63 31 00 90 03 C8 01 14 10 D0 0F 07")));
}

// Regression (OASIS-062, E4): an XGEOMETRY after a RECTANGLE on the same layer
// at the same point was written with info 09 (datatype and x omitted), which
// gdstk 1.0.2 crashes on; it is now written in full form.
TEST_CASE("XGEOMETRY is written in full form", "[cap:oasis-write][req:Modal field omission]") {
  oasis::Element xg;
  xg.kind = oasis::ElementKind::xgeometry;
  xg.layer = 9;
  xg.datatype = 0;
  xg.attribute = 3;
  xg.payload = "ab";
  const std::vector<Cell> cells = {{"top", {rectangle(9, 0, pt(0, 0), pt(10, 20)), xg, rectangle(9, 0, pt(0, 0), pt(10, 20))}}};
  const auto bytes = encode(cells, oasis::EncoderOptions{true, oasis::ValidationScheme::crc32, 0, 1000.0});
  // 21 1B: attribute 3, layer 9, datatype 0, "ab", x 0, y 0; then the second
  // rectangle with every field modal (info 00): the modal state was updated.
  CHECK(contains(bytes, hex("21 1B 03 09 00 02 61 62 00 00 14 00")));
  const Decoded d = oasis_test::decode(bytes);
  REQUIRE_FALSE(d.error);
  CHECK(cells_of(d) == cells);
}

TEST_CASE("the encoder's placement and property forms", "[cap:oasis-format][req:PLACEMENT records]") {
  gds::MemorySink sink;
  oasis::Encoder encoder{sink, oasis::EncoderOptions{true, oasis::ValidationScheme::crc32, 0, 1000.0}};
  REQUIRE(encoder.begin().ok());
  REQUIRE(encoder.standard_properties("top").ok());
  REQUIRE(encoder.begin_cell("top").ok());
  oasis::Element sref;
  sref.kind = oasis::ElementKind::placement;
  sref.name = "sub";
  sref.angle = 270.0;
  sref.position = pt(1000, 2000);
  REQUIRE(encoder.element(sref).ok());
  REQUIRE(encoder.property(oasis::Property{"S_GDS_PROPERTY", std::nullopt, true,
                                           {oasis::PropertyValue::of_unsigned(61),
                                            oasis::PropertyValue::of_string(oasis::PropertyValue::Kind::b_string, "sub_0")}})
              .ok());
  REQUIRE(encoder.finish().ok());
  CHECK(contains(sink.bytes(), hex("11 F6 01 D0 0F A0 1F 1C 27 04 08 3D 0E 01")));
  const Decoded d = oasis_test::decode(sink.bytes());
  REQUIRE_FALSE(d.error);
  CHECK(d.diagnostics.empty());  // a strict file written by the encoder reads back clean

  oasis::Element aref;
  aref.kind = oasis::ElementKind::placement;
  aref.name = "sub";
  aref.repetition = oasis::Repetition{1, 3, 2, pt(1000, 0), pt(0, 2000), {}, 0};
  const auto array = encode({{"sub", {}}, {"top", {aref}}}, oasis::EncoderOptions{true, oasis::ValidationScheme::crc32, 0, 1000.0});
  CHECK(contains(array, hex("0D 01 11 C8 00 01 01 00 E8 07 D0 0F")));
}

TEST_CASE("non-strict files name cells inline and need no tables", "[cap:oasis-format][req:Strict mode]") {
  gds::MemorySink sink;
  oasis::Encoder encoder{sink, oasis::EncoderOptions{false, oasis::ValidationScheme::none, 0, 1000.0}};
  REQUIRE(encoder.begin().ok());
  REQUIRE(encoder.layer_name("M1", 49, 0).ok());
  REQUIRE(encoder.layer_name("M1", 49, 0).ok());  // an identical pair is written once
  REQUIRE(encoder.begin_cell("top").ok());
  REQUIRE(encoder.finish().ok());
  const auto& bytes = sink.bytes();
  CHECK(contains(bytes, hex("01 03 31 2E 30 00 E8 07 00 00 00 00 00 00 00 00 00 00 00 00 00")));
  CHECK(contains(bytes, hex("0B 02 4D 31 03 31 03 00 0C 02 4D 31 03 31 03 00 0E 03 74 6F 70 02")));
  CHECK(bytes.size() == 13 + 21 + 16 + 5 + 256);
  CHECK(bytes.back() == std::byte{0});  // validation none
  CHECK(oasis_test::decode(bytes).diagnostics.empty());
}

TEST_CASE("CBLOCKs: tiny bodies stay plain, large ones are chunked", "[cap:oasis-format][req:CBLOCK compression]") {
  // Non-strict, so no name table is compressed either.
  const oasis::EncoderOptions compressed{false, oasis::ValidationScheme::crc32, 6, 1000.0};
  const auto tiny = oasis_test::decode(encode({{"top", {rectangle(1, 0, pt(0, 0), pt(4, 2))}}}, compressed));
  CHECK(std::none_of(tiny.records.begin(), tiny.records.end(),
                     [](const oasis::Record& r) { return r.id == oasis::RecordId::cblock; }));
  Cell big{"big", {}};
  for (std::int64_t i = 0; i < 200000; ++i) {
    big.elements.push_back(rectangle(1, 0, pt(i * 7, (i % 13) * 5), pt(3 + i % 5, 2)));
  }
  const auto plain_bytes = encode({big}, oasis::EncoderOptions{true, oasis::ValidationScheme::crc32, 0, 1000.0});
  REQUIRE(plain_bytes.size() > 1048576);
  const auto packed_bytes = encode({big}, oasis::EncoderOptions{true, oasis::ValidationScheme::crc32, 9, 1000.0});
  CHECK(packed_bytes.size() < plain_bytes.size());
  const Decoded packed = oasis_test::decode(packed_bytes);
  REQUIRE_FALSE(packed.error);
  CHECK(packed.diagnostics.empty());
  CHECK(std::count_if(packed.records.begin(), packed.records.end(),
                      [](const oasis::Record& r) { return r.id == oasis::RecordId::cblock; }) >= 2);
  CHECK(cells_of(packed) == cells_of(oasis_test::decode(plain_bytes)));
}

TEST_CASE("END: padding, table offsets and validation", "[cap:oasis-format][req:END record and padding]") {
  for (const auto scheme : {oasis::ValidationScheme::none, oasis::ValidationScheme::crc32,
                            oasis::ValidationScheme::checksum32}) {
    for (const bool strict : {true, false}) {
      const auto bytes = encode({{"top", {rectangle(1, 0, pt(0, 0), pt(4, 2))}}},
                                oasis::EncoderOptions{strict, scheme, 6, 1000.0});
      const Decoded d = oasis_test::decode(bytes);
      REQUIRE_FALSE(d.error);
      CHECK(d.diagnostics.empty());
      CHECK(d.records.back().end->length == 256);
      CHECK(d.records.back().offset == bytes.size() - 256);
      gds::MemorySource source{bytes};
      auto v = oasis::validate(source);
      REQUIRE(v.ok());
      CHECK(v.value().ok());
      CHECK(v.value().scheme == scheme);
    }
  }
}

TEST_CASE("the encoder rejects what it cannot write", "[cap:oasis-format][req:Repetitions]") {
  gds::MemorySink sink;
  oasis::Encoder encoder{sink};
  CHECK(encoder.element(rectangle(1, 0, pt(0, 0), pt(1, 1))).error().code == "OASIS-CODEC-026");
  REQUIRE(encoder.begin().ok());
  CHECK_FALSE(encoder.begin().ok());
  CHECK(encoder.begin_cell("my cell").error().code == "OASIS-CODEC-026");
  REQUIRE(encoder.begin_cell("top").ok());
  oasis::Element bad = rectangle(1, 0, pt(0, 0), pt(1, 1));
  bad.repetition = oasis::Repetition{1, 1, 1, pt(1, 0), pt(0, 1), {}, 0};  // one column: not type 1
  CHECK(encoder.element(bad).error().code == "OASIS-CODEC-026");
  bad.repetition = oasis::Repetition{4, 1, 1, {}, {}, {pt(5, 0), pt(3, 0)}, 0};  // decreasing x
  CHECK_FALSE(encoder.element(bad).ok());
  oasis::Element far = rectangle(1, 0, pt(std::int64_t{1} << 60, 0), pt(1, 1));
  CHECK_FALSE(encoder.element(far).ok());
  oasis::Element negative = rectangle(1, 0, pt(0, 0), pt(-1, 1));
  CHECK_FALSE(encoder.element(negative).ok());
  CHECK(encoder.layer_name("M1", 1, 0).error().code == "OASIS-CODEC-026");  // after the first cell
  CHECK(encoder.begin_cell("top").error().text == "OASIS encoder: cell top written twice");
}
