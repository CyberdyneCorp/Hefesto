// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS file structure and records (tasks.md 10.2, 10.3; the inputs of
// OASIS-005..015 as hand-assembled unit inputs). Owner: OASFMT.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "codec_fixture.hpp"
#include "hefesto/gds/stream/gzip.hpp"
#include "hefesto/oasis/codec/cblock.hpp"
#include "hefesto/oasis/codec/elements.hpp"

using oasis_test::Decoded;
using oasis_test::File;
using oasis_test::hex;
using oasis_test::pt;
using oasis_test::str;
namespace oasis = hefesto::oasis;
namespace gds = hefesto::gds;
namespace diag = hefesto::diag;

namespace {

/// A file holding `body` after a CELL record by name ("top").
std::vector<std::byte> cell_file(std::string_view body,
                                 oasis::ValidationScheme scheme = oasis::ValidationScheme::crc32) {
  File f;
  f.add("0E").add(str("top")).add(body);
  return f.end(scheme);
}

oasis::Element only_element(const Decoded& d) {
  REQUIRE_FALSE(d.error);
  const auto elements = d.elements();
  REQUIRE(elements.size() == 1);
  return elements.front();
}

std::vector<oasis::Point64> absolute_outline(const oasis::Element& e) {
  std::vector<oasis::Point64> out;
  for (const auto p : oasis::outline(e)) {
    out.push_back(pt(p.x + e.position.x, p.y + e.position.y));
  }
  return out;
}

/// A CBLOCK record holding `raw`.
std::vector<std::byte> cblock(const std::vector<std::byte>& raw) {
  auto data = oasis::deflate_cblock(raw, 6);
  REQUIRE(data.ok());
  std::vector<std::byte> out = hex("22 00");
  oasis::write_unsigned(out, raw.size());
  oasis::write_unsigned(out, data.value().size());
  out.insert(out.end(), data.value().begin(), data.value().end());
  return out;
}

/// A byte sink appending to a vector it does not own.
class VectorSink final : public gds::ByteSink {
 public:
  explicit VectorSink(std::vector<std::byte>& out) : out_(&out) {}
  diag::Status write(std::span<const std::byte> bytes) override {
    out_->insert(out_->end(), bytes.begin(), bytes.end());
    return diag::Done{};
  }
  [[nodiscard]] std::uint64_t position() const noexcept override { return out_->size(); }
  diag::Status finish() override { return diag::Done{}; }

 private:
  std::vector<std::byte>* out_;
};

}  // namespace

// --- File structure and record identifiers ----------------------------------

TEST_CASE("the magic bytes are followed by START", "[cap:oasis-format][req:File structure and magic bytes]") {
  const auto d = oasis_test::decode(File{}.end());
  REQUIRE_FALSE(d.error);
  REQUIRE(d.records.size() == 2);
  CHECK(d.records[0].id == oasis::RecordId::start);
  CHECK(d.records[0].offset == 13);
  CHECK(d.records[1].id == oasis::RecordId::end);
  CHECK(d.diagnostics.empty());
}

TEST_CASE("a GDS file is not an OASIS file", "[cap:oasis-format][req:File structure and magic bytes]") {
  const auto d = oasis_test::decode(hex("00 06 00 02 00 03 00 1C 01 02"));
  REQUIRE(d.error);
  CHECK(d.error->code == "OASIS-CODEC-001");
  CHECK(d.error->text == "not an OASIS file");
  CHECK(d.records.empty());
  CHECK(oasis_test::decode(hex("25 53 45")).error->code == "OASIS-CODEC-001");
}

TEST_CASE("a file cut before END keeps what was decoded", "[cap:oasis-format][req:File structure and magic bytes]") {
  File f;
  f.add("0E").add(str("top")).add("14 63 31 00 90 03 C8 01");
  std::vector<std::byte> bytes = f.end();
  bytes.resize(f.size());
  const auto d = oasis_test::decode(bytes);
  REQUIRE(d.error);
  CHECK(d.error->code == "OASIS-CODEC-002");
  CHECK(d.elements().size() == 1);
  bytes.pop_back();  // inside the RECTANGLE
  CHECK(oasis_test::decode(bytes).error->text == "unexpected end of file");
}

TEST_CASE("data after END is reported and the content kept", "[cap:oasis-format][req:File structure and magic bytes]") {
  std::vector<std::byte> bytes = cell_file("14 63 31 00 90 03 C8 01");
  bytes.push_back(std::byte{0});
  const auto d = oasis_test::decode(bytes);
  REQUIRE_FALSE(d.error);
  CHECK(d.text("OASIS-CODEC-003") == "data after END record");
  CHECK(d.elements().size() == 1);
}

TEST_CASE("a gzip-wrapped file decodes through the gzip source", "[cap:oasis-format][req:File structure and magic bytes]") {
  const std::vector<std::byte> plain = cell_file("14 63 31 00 90 03 C8 01");
  std::vector<std::byte> packed;
  auto sink = gds::make_gzip_sink(std::make_unique<VectorSink>(packed), 6);
  REQUIRE(sink.ok());
  REQUIRE(sink.value()->write(plain).ok());
  REQUIRE(sink.value()->finish().ok());
  auto source = gds::make_gzip_source(packed);
  oasis::Decoder decoder{*source};
  int elements = 0;
  for (auto r = decoder.next(); r.ok() && r.value(); r = decoder.next()) {
    elements += r.value()->element ? 1 : 0;
  }
  CHECK(elements == 1);
  CHECK(decoder.diagnostics().empty());  // the signature covers the decompressed bytes
}

TEST_CASE("record types above 34 stop decoding", "[cap:oasis-format][req:Record identifiers]") {
  File f;
  f.add("23");
  const auto d = oasis_test::decode(f.end());
  REQUIRE(d.error);
  CHECK(d.error->code == "OASIS-CODEC-004");
  CHECK(d.error->text == "invalid record type 35");
  CHECK(d.records.size() == 1);  // START
}

TEST_CASE("PAD, XYABSOLUTE and XYRELATIVE have no fields", "[cap:oasis-format][req:Record identifiers]") {
  const auto d = oasis_test::decode(cell_file("00 10 0F 00"));
  REQUIRE_FALSE(d.error);
  REQUIRE(d.records.size() == 7);
  CHECK(d.records[3].id == oasis::RecordId::xyrelative);
  CHECK(d.records[4].id == oasis::RecordId::xyabsolute);
}

// --- START, END, validation ---------------------------------------------------

TEST_CASE("START with the table offsets at END", "[cap:oasis-format][req:START record and table offsets]") {
  const auto d = oasis_test::decode(File{}.end());
  REQUIRE(d.records[0].start);
  const oasis::StartInfo& s = *d.records[0].start;
  CHECK(s.version == "1.0");
  CHECK(s.unit == 1000.0);
  CHECK(s.offsets_at_end);
  REQUIRE(d.records[1].end);
  CHECK(d.records[1].end->tables.has_value());
}

TEST_CASE("START variants: version, unit, offsets in START", "[cap:oasis-format][req:START record and table offsets]") {
  const auto start_file = [](std::string_view start) {
    std::vector<std::byte> bytes = oasis_test::magic();
    const auto rest = hex(start);
    bytes.insert(bytes.end(), rest.begin(), rest.end());
    return oasis_test::decode(bytes);
  };
  const auto v2 = start_file("01 03 32 2E 30 00 E8 07 01");
  REQUIRE(v2.error);
  CHECK(v2.error->code == "OASIS-CODEC-013");
  CHECK(v2.error->text == "unsupported OASIS version 2.0");
  CHECK(v2.records.empty());
  CHECK(start_file("01 03 31 2E 30 00 00 01").error->text == "invalid unit");
  CHECK(start_file("01 03 31 2E 30 01 05 01").error->text == "invalid unit");
  const auto real7 = start_file("01 03 31 2E 30 07 00 00 00 00 00 40 8F 40 01");
  REQUIRE(real7.records.size() == 1);
  CHECK(real7.records[0].start->unit == 1000.0);
  const auto in_start = start_file("01 03 31 2E 30 00 E8 07 00  01 05 00 00 00 00 00 00 00 00 01 00");
  REQUIRE(in_start.records.size() == 1);
  const auto& tables = in_start.records[0].start->tables;
  CHECK(tables[0] == oasis::TableEntry{true, 5});
  CHECK(tables[5] == oasis::TableEntry{true, 0});
  CHECK_FALSE(in_start.records[0].start->offsets_at_end);
  CHECK(start_file("02").error->text == "invalid record type 2: START record expected");
}

namespace {

/// A file whose END record is padded with `padding` content bytes.
std::vector<std::byte> end_with_padding(std::size_t padding, std::uint8_t scheme) {
  File f;
  std::vector<std::byte> bytes = f.end(oasis::ValidationScheme::none);
  bytes.resize(f.size());
  const auto tail = hex("02 00 00 00 00 00 00 00 00 00 00 00 00");
  bytes.insert(bytes.end(), tail.begin(), tail.end());
  oasis::write_unsigned(bytes, padding);
  bytes.insert(bytes.end(), padding, std::byte{0x80});
  bytes.push_back(std::byte{scheme});
  if (scheme == 1 || scheme == 2) {
    const std::uint32_t v = oasis::signature(static_cast<oasis::ValidationScheme>(scheme), bytes);
    for (unsigned i = 0; i < 4; ++i) {
      bytes.push_back(static_cast<std::byte>((v >> (8U * i)) & 0xffU));
    }
  }
  return bytes;
}

}  // namespace

TEST_CASE("the END record is 256 bytes", "[cap:oasis-format][req:END record and padding]") {
  // Offsets at END take 12 bytes: R = 256 - (1 + 12 + 1 + 4) = 238, L = 236.
  const auto good = oasis_test::decode(end_with_padding(236, 1));
  REQUIRE_FALSE(good.error);
  CHECK(good.diagnostics.empty());
  CHECK(good.records.back().end->length == 256);
  for (const std::size_t padding : {std::size_t{235}, std::size_t{237}}) {
    const auto d = oasis_test::decode(end_with_padding(padding, 1));
    REQUIRE_FALSE(d.error);
    CHECK(d.text("OASIS-CODEC-014") == "END record length " + std::to_string(padding + 20));
    CHECK(d.records.back().end->validation.ok());
  }
}

TEST_CASE("validation schemes 0, 1, 2 and an unknown one", "[cap:oasis-format][req:END record and padding]") {
  const auto none = oasis_test::decode(end_with_padding(240, 0));
  CHECK(none.diagnostics.empty());
  CHECK(none.records.back().end->validation.scheme == oasis::ValidationScheme::none);
  const auto sum = oasis_test::decode(end_with_padding(236, 2));
  CHECK(sum.diagnostics.empty());
  CHECK(sum.records.back().end->validation.scheme == oasis::ValidationScheme::checksum32);
  const auto unknown = oasis_test::decode(end_with_padding(240, 3));
  REQUIRE_FALSE(unknown.error);
  CHECK(unknown.text("OASIS-CODEC-014") == "unknown validation scheme 3");
}

TEST_CASE("the writer's padding rule", "[cap:oasis-format][req:END record and padding]") {
  // Offsets in START, no validation: R = 254, length field FC 01 (252).
  File in_start{false};
  const auto bytes = in_start.end(oasis::ValidationScheme::none);
  CHECK(bytes.size() == in_start.size() + 256);
  CHECK(bytes[in_start.size() + 1] == std::byte{0xFC});
  CHECK(bytes[in_start.size() + 2] == std::byte{0x01});
  CHECK(bytes.back() == std::byte{0});
}

TEST_CASE("the CRC32 of the reference file", "[cap:oasis-format][req:Validation signature]") {
  gds::MemorySource source{oasis_test::decode_file("reference_419.oas")};
  auto result = oasis::validate(source);
  REQUIRE(result.ok());
  CHECK(result.value().scheme == oasis::ValidationScheme::crc32);
  CHECK(result.value().stored == 0xEE7E3BD3U);
  CHECK(result.value().computed == 0xEE7E3BD3U);
}

TEST_CASE("a corrupted byte fails validation and the cells stay", "[cap:oasis-format][req:Validation signature]") {
  for (const auto scheme : {oasis::ValidationScheme::crc32, oasis::ValidationScheme::checksum32}) {
    std::vector<std::byte> bytes = cell_file("14 63 31 00 90 03 C8 01", scheme);
    bytes[30] ^= std::byte{0x01};  // the RECTANGLE datatype
    const auto d = oasis_test::decode(bytes);
    REQUIRE_FALSE(d.error);
    CHECK(d.text("OASIS-CODEC-015").starts_with("OASIS validation failed (expected 0x"));
    CHECK(d.elements().size() == 1);
    gds::MemorySource source{bytes};
    auto result = oasis::validate(source);
    REQUIRE(result.ok());
    CHECK_FALSE(result.value().ok());
  }
  gds::MemorySource short_source{hex("25 53 45 4D 49 2D 4F 41 53 49 53 0D 0A 01 03 31 2E 30 00 E8 07 01 02")};
  CHECK_FALSE(oasis::validate(short_source).ok());
}

// --- Names, cells, modal state ------------------------------------------------

TEST_CASE("implicit and explicit name records", "[cap:oasis-format][req:Name records and reference numbers]") {
  File f;
  f.add("03 01 41 03 01 42  06 01 54 07  08 01 50 03  0A 01 53 00  1F 05 02 58 59 04");
  const auto d = oasis_test::decode(f.end());
  REQUIRE_FALSE(d.error);
  CHECK(d.names.cellnames == std::map<std::uint64_t, std::string>{{0, "A"}, {1, "B"}});
  CHECK(d.names.textstrings.at(7) == "T");
  CHECK(d.names.propnames.at(3) == "P");
  CHECK(d.names.propstrings.at(0) == "S");
  CHECK(d.names.xnames.at(4) == oasis::XName{5, "XY"});
  CHECK(d.records[1].reference == 0);
  CHECK(d.records[2].reference == 1);
}

TEST_CASE("mixed numbering and duplicate refnums stop decoding",
          "[cap:oasis-format][req:Name records and reference numbers]") {
  File mixed;
  mixed.add("03 01 41 04 01 42 05");
  const auto m = oasis_test::decode(mixed.end());
  REQUIRE(m.error);
  CHECK(m.error->code == "OASIS-CODEC-016");
  CHECK(m.error->text == "implicit and explicit CELLNAME records mixed");
  File dup;
  dup.add("08 01 41 02 08 01 42 02");
  CHECK(oasis_test::decode(dup.end()).error->text == "duplicate PROPNAME refnum 2");
}

TEST_CASE("a cell name defined twice is an alias", "[cap:oasis-format][req:Name records and reference numbers]") {
  File f;
  f.add("03 01 41 03 01 41");
  const auto d = oasis_test::decode(f.end());
  REQUIRE_FALSE(d.error);
  CHECK(d.text("OASIS-CODEC-017") == "duplicate cell name A");
  CHECK(d.names.cellnames.size() == 2);
}

TEST_CASE("forward references resolve after the scan", "[cap:oasis-format][req:Name records and reference numbers]") {
  File f;
  f.add("0D 00  03 03 74 6F 70");
  const auto d = oasis_test::decode(f.end());
  REQUIRE_FALSE(d.error);
  CHECK(d.records[1].id == oasis::RecordId::cell_by_reference);
  CHECK(d.records[1].reference == 0);
  CHECK(d.names.cellnames.at(*d.records[1].reference) == "top");
  CHECK(d.diagnostics.empty());
  File undefined;
  undefined.add("0D 05");
  CHECK(oasis_test::decode(undefined.end()).text("OASIS-CODEC-018") == "undefined CELLNAME refnum 5");
}

TEST_CASE("a CELL record starts an empty or full cell", "[cap:oasis-format][req:CELL record and cell content]") {
  const auto empty = oasis_test::decode(cell_file(""));
  REQUIRE_FALSE(empty.error);
  CHECK(empty.records[1].id == oasis::RecordId::cell_by_name);
  CHECK(empty.records[1].name == "top");
  CHECK(empty.elements().empty());
  File twice;
  twice.add("0E 03 74 6F 70  03 03 74 6F 70  0D 00");
  CHECK(oasis_test::decode(twice.end()).text("OASIS-CODEC-019") == "cell top defined twice");
}

TEST_CASE("a CELL record resets the modal variables", "[cap:oasis-format][req:CELL record and cell content]") {
  File f;
  f.add("0E").add(str("a")).add("14 63 31 00 0A 0A").add("0E").add(str("b")).add("14 60 0A 0A");
  const auto d = oasis_test::decode(f.end());
  REQUIRE(d.error);
  CHECK(d.error->text == "modal variable layer undefined");
}

TEST_CASE("modal layer, relative coordinates and resets", "[cap:oasis-format][req:Modal variables and reset rules]") {
  // A RECTANGLE sets 49/0, a POLYGON with L=0 D=0 inherits it.
  const auto inherit = oasis_test::decode(cell_file("14 63 31 00 0A 0A  15 20 03 02 50 55"));
  REQUIRE_FALSE(inherit.error);
  REQUIRE(inherit.elements().size() == 2);
  CHECK(inherit.elements()[1].layer == 49);
  CHECK(inherit.elements()[1].datatype == 0);
  // XYRELATIVE: x fields 100, 100, 100 give 100, 200, 300.
  const auto relative = oasis_test::decode(cell_file("10 14 73 31 00 0A 0A C8 01  14 10 C8 01  14 10 C8 01"));
  REQUIRE_FALSE(relative.error);
  const auto e = relative.elements();
  REQUIRE(e.size() == 3);
  CHECK(e[0].position.x == 100);
  CHECK(e[1].position.x == 200);
  CHECK(e[2].position.x == 300);
  const auto undefined = oasis_test::decode(cell_file("14 60 0A 0A"));
  REQUIRE(undefined.error);
  CHECK(undefined.error->code == "OASIS-CODEC-020");
  CHECK(undefined.error->text == "modal variable layer undefined");
}

TEST_CASE("a name record resets the modal variables (Q-OASIS-5)",
          "[cap:oasis-format][req:Modal variables and reset rules]") {
  const auto d = oasis_test::decode(cell_file("14 63 31 00 0A 0A  05 01 41  14 60 0A 0A"));
  REQUIRE(d.error);
  CHECK(d.error->text == "modal variable layer undefined");
  const auto rep = oasis_test::decode(cell_file("14 67 31 00 0A 0A 02 00 05  14 04 00"));
  REQUIRE_FALSE(rep.error);
  CHECK(rep.elements()[1].repetition == rep.elements()[0].repetition);  // type 0 reuses the modal one
  CHECK(oasis_test::decode(cell_file("14 67 31 00 0A 0A 00")).error->text == "modal variable repetition undefined");
}

// --- Elements -------------------------------------------------------------------

TEST_CASE("PLACEMENT 17: quarter turns and flip", "[cap:oasis-format][req:PLACEMENT records]") {
  const auto e = only_element(oasis_test::decode(cell_file("11 C7 03  03 01 41 03 01 42 03 01 43 03 01 44")));
  CHECK(e.kind == oasis::ElementKind::placement);
  CHECK(e.reference == 3);
  CHECK(e.angle == 270.0);
  CHECK(e.flip);
  CHECK(e.position == pt(0, 0));
  CHECK(e.magnification == 1.0);
  for (unsigned aa = 0; aa < 4; ++aa) {
    std::vector<std::byte> body = hex("11");
    body.push_back(static_cast<std::byte>(0x80U | (aa << 1U)));
    body.push_back(std::byte{1});
    body.push_back(std::byte{'X'});
    File f;
    f.add("0E").add(str("top")).add(body);
    CHECK(only_element(oasis_test::decode(f.end())).angle == 90.0 * aa);
  }
}

TEST_CASE("PLACEMENT 18: magnification and angle", "[cap:oasis-format][req:PLACEMENT records]") {
  const auto e = only_element(oasis_test::decode(cell_file("12 C4 02 00 02  03 01 41 03 01 42 03 01 43")));
  CHECK(e.reference == 2);
  CHECK(e.magnification == 2.0);
  CHECK(e.angle == 0.0);
  const auto angled = only_element(oasis_test::decode(cell_file("12 86 01 41 02 02 00 2D")));  // mag 0.5, angle 45
  CHECK(angled.name == "A");
  CHECK(angled.magnification == 0.5);
  CHECK(angled.angle == 45.0);
  CHECK(only_element(oasis_test::decode(cell_file("12 82 01 41 00 C2 03"))).angle == 450.0);
  // D3(b) of the user decisions of 2026-11-19: a magnification of 0 or
  // less drops the placement with a warning.
  for (const char* m : {"01 01", "00 00"}) {  // -1, 0
    const auto negative = oasis_test::decode(cell_file(std::string{"12 84 01 41 "} + m));
    REQUIRE_FALSE(negative.error);
    CHECK(negative.elements().empty());
    CHECK(negative.has("OASIS-CODEC-029"));
    CHECK_FALSE(negative.has("OASIS-CODEC-021"));
    CHECK(diag::make("OASIS-CODEC-029", "").severity == diag::Severity::warning);
  }
  CHECK(oasis_test::decode(cell_file("11 00")).error->text == "modal variable placement-cell undefined");
}

TEST_CASE("TEXT by refnum", "[cap:oasis-format][req:TEXT record]") {
  const auto e = only_element(oasis_test::decode(cell_file("13 7B 00 31 00 E8 07 D0 0F  05 03 56 44 44")));
  CHECK(e.kind == oasis::ElementKind::text);
  CHECK(e.reference == 0);
  CHECK(e.layer == 49);
  CHECK(e.datatype == 0);
  CHECK(e.position == pt(500, 1000));
  const auto inline_text = oasis_test::decode(cell_file("13 C3 02 68 69 05 00"));
  CHECK(inline_text.text("OASIS-CODEC-022") == "TEXT info byte with bit 7 set");
  CHECK(only_element(inline_text).name == "hi");
}

TEST_CASE("RECTANGLE: plain and square", "[cap:oasis-format][req:RECTANGLE record]") {
  const auto plain = only_element(oasis_test::decode(cell_file("14 63 31 00 90 03 C8 01")));
  CHECK(plain.layer == 49);
  CHECK(plain.size == pt(400, 200));
  CHECK(absolute_outline(plain) == std::vector{pt(0, 0), pt(400, 0), pt(400, 200), pt(0, 200)});
  const auto square = oasis_test::decode(cell_file("14 DB 05 00 0A 28 50  14 00"));
  REQUIRE_FALSE(square.error);
  const auto e = square.elements();
  CHECK(e[0].layer == 5);
  CHECK(e[0].position == pt(20, 40));
  CHECK(e[0].size == pt(10, 10));
  CHECK(e[1].size == pt(10, 10));  // geometry-h became 10 (Q-OASIS-3)
  const auto both = oasis_test::decode(cell_file("14 E3 05 00 0A 07"));
  CHECK(both.has("OASIS-CODEC-022"));
  CHECK(only_element(both).size == pt(10, 10));
  CHECK(only_element(oasis_test::decode(cell_file("14 63 05 00 00 00"))).size == pt(0, 0));  // empty is legal
}

TEST_CASE("POLYGON point lists and closure", "[cap:oasis-format][req:POLYGON record]") {
  const auto rect = only_element(oasis_test::decode(cell_file("15 3B 31 00 00 02 50 3C 14 28")));
  CHECK(absolute_outline(rect) == std::vector{pt(10, 20), pt(50, 20), pt(50, 50), pt(10, 50)});
  CHECK(rect.point_list_type == 0);
  const auto l_shape = only_element(oasis_test::decode(cell_file("15 23 31 00 01 04 28 14 15 28")));
  CHECK(oasis::outline(l_shape) == std::vector{pt(0, 0), pt(0, 20), pt(10, 20), pt(10, 10), pt(30, 10), pt(30, 0)});
  const auto triangle = only_element(oasis_test::decode(cell_file("15 23 31 00 03 02 50 55")));
  CHECK(oasis::outline(triangle) == std::vector{pt(0, 0), pt(10, 0), pt(0, 10)});
  const auto odd = oasis_test::decode(cell_file("15 23 31 00 00 03 50 3C 51"));
  CHECK(odd.text("OASIS-CODEC-022") == "type 0/1 point list with odd count");
  CHECK(oasis::outline(only_element(odd)).size() == 5);
  const auto two = oasis_test::decode(cell_file("15 23 31 00 02 01 28"));
  CHECK(two.elements().empty());
  CHECK(two.text("OASIS-CODEC-021") == "polygon with fewer than 3 vertices");
  const auto empty = oasis_test::decode(cell_file("15 23 31 00 02 00"));
  CHECK(empty.elements().empty());
  CHECK(empty.has("OASIS-CODEC-009"));
  CHECK(oasis_test::decode(cell_file("15 23 31 00 06 01 00")).error->code == "OASIS-CODEC-009");
}

TEST_CASE("PATH half-width and extensions (Q-OASIS-11)", "[cap:oasis-format][req:PATH record]") {
  const auto d = oasis_test::decode(cell_file("16 E3 31 00 05 0A 00 01 C8 01  16 60 08 00 01 C8 01"));
  REQUIRE_FALSE(d.error);
  const auto e = d.elements();
  CHECK(e[0].points == std::vector{pt(100, 0)});
  CHECK(e[0].half_width == 5);
  CHECK(e[0].start_extension == 5);
  CHECK(e[0].end_extension == 5);
  CHECK(e[1].half_width == 8);
  CHECK(e[1].start_extension == 5);  // the numeric modal value, not "half-width"
  const auto explicit_ext = only_element(oasis_test::decode(cell_file("16 E3 31 00 05 0F 05 0E 00 01 C8 01")));
  CHECK(explicit_ext.start_extension == -2);
  CHECK(explicit_ext.end_extension == 7);
  const auto flush = only_element(oasis_test::decode(cell_file("16 E3 31 00 05 05 00 01 C8 01")));
  CHECK(flush.start_extension == 0);
  CHECK(oasis_test::decode(cell_file("16 63 31 00 05 00 01 C8 01")).error->text ==
        "modal variable path-start-extension undefined");
}

TEST_CASE("TRAPEZOID vertices", "[cap:oasis-format][req:TRAPEZOID records]") {
  const auto h = only_element(oasis_test::decode(cell_file("17 63 31 00 64 14 28 29")));
  CHECK(oasis::outline(h) == std::vector{pt(20, 20), pt(80, 20), pt(100, 0), pt(0, 0)});
  const auto v = only_element(oasis_test::decode(cell_file("17 E3 31 00 0A 64 28 00")));
  CHECK(oasis::outline(v) == std::vector{pt(0, 20), pt(0, 100), pt(10, 100), pt(10, 0)});
  const auto a_only = only_element(oasis_test::decode(cell_file("18 63 31 00 64 14 28")));
  CHECK(a_only.delta_b == 0);
  const auto b_only = only_element(oasis_test::decode(cell_file("19 63 31 00 64 14 29")));
  CHECK(b_only.delta_a == 0);
  CHECK(b_only.delta_b == -20);
  // a = w turns the trapezoid into a triangle; zero size leaves fewer than 3 vertices.
  CHECK(oasis::outline(only_element(oasis_test::decode(cell_file("18 63 31 00 64 14 C8 01")))).size() == 3);
  const auto flat = oasis_test::decode(cell_file("17 63 31 00 00 00 00 00"));
  CHECK(flat.elements().empty());
  CHECK(flat.has("OASIS-CODEC-022"));
}

TEST_CASE("CTRAPEZOID: all 26 types (Q-OASIS-4)", "[cap:oasis-format][req:CTRAPEZOID record]") {
  struct Case {
    std::uint8_t type;
    std::int64_t w;
    std::int64_t h;
    std::vector<oasis::Point64> vertices;
  };
  const std::vector<Case> cases = {
      {0, 30, 10, {pt(0, 0), pt(0, 10), pt(20, 10), pt(30, 0)}},
      {1, 30, 10, {pt(0, 0), pt(0, 10), pt(30, 10), pt(20, 0)}},
      {2, 30, 10, {pt(0, 0), pt(10, 10), pt(30, 10), pt(30, 0)}},
      {3, 30, 10, {pt(10, 0), pt(0, 10), pt(30, 10), pt(30, 0)}},
      {4, 30, 10, {pt(0, 0), pt(10, 10), pt(20, 10), pt(30, 0)}},
      {5, 30, 10, {pt(10, 0), pt(0, 10), pt(30, 10), pt(20, 0)}},
      {6, 30, 10, {pt(0, 0), pt(10, 10), pt(30, 10), pt(20, 0)}},
      {7, 30, 10, {pt(10, 0), pt(0, 10), pt(20, 10), pt(30, 0)}},
      {8, 10, 30, {pt(0, 0), pt(0, 30), pt(10, 20), pt(10, 0)}},
      {9, 10, 30, {pt(0, 0), pt(0, 20), pt(10, 30), pt(10, 0)}},
      {10, 10, 30, {pt(0, 0), pt(0, 30), pt(10, 30), pt(10, 10)}},
      {11, 10, 30, {pt(0, 10), pt(0, 30), pt(10, 30), pt(10, 0)}},
      {12, 10, 30, {pt(0, 0), pt(0, 30), pt(10, 20), pt(10, 10)}},
      {13, 10, 30, {pt(0, 10), pt(0, 20), pt(10, 30), pt(10, 0)}},
      {14, 10, 30, {pt(0, 0), pt(0, 20), pt(10, 30), pt(10, 10)}},
      {15, 10, 30, {pt(0, 10), pt(0, 30), pt(10, 20), pt(10, 0)}},
      {16, 10, 0, {pt(0, 0), pt(0, 10), pt(10, 0)}},
      {17, 10, 0, {pt(0, 0), pt(0, 10), pt(10, 10)}},
      {18, 10, 0, {pt(0, 0), pt(10, 10), pt(10, 0)}},
      {19, 10, 0, {pt(0, 10), pt(10, 10), pt(10, 0)}},
      {20, 0, 10, {pt(0, 0), pt(10, 10), pt(20, 0)}},
      {21, 0, 10, {pt(0, 10), pt(20, 10), pt(10, 0)}},
      {22, 10, 0, {pt(0, 0), pt(0, 20), pt(10, 10)}},
      {23, 10, 0, {pt(10, 0), pt(0, 10), pt(10, 20)}},
      {24, 30, 10, {pt(0, 0), pt(0, 10), pt(30, 10), pt(30, 0)}},
      {25, 10, 0, {pt(0, 0), pt(0, 10), pt(10, 10), pt(10, 0)}},
  };
  for (const Case& c : cases) {
    CAPTURE(c.type);
    std::vector<std::byte> body = hex("1A");
    const bool uses_w = oasis::ctrapezoid_uses_width(c.type);
    const bool uses_h = oasis::ctrapezoid_uses_height(c.type);
    body.push_back(static_cast<std::byte>(0x83U | (uses_w ? 0x40U : 0U) | (uses_h ? 0x20U : 0U)));
    body.insert(body.end(), {std::byte{49}, std::byte{0}, std::byte{c.type}});
    if (uses_w) {
      oasis::write_unsigned(body, static_cast<std::uint64_t>(c.w));
    }
    if (uses_h) {
      oasis::write_unsigned(body, static_cast<std::uint64_t>(c.h));
    }
    File f;
    f.add("0E").add(str("top")).add(body);
    const auto e = only_element(oasis_test::decode(f.end()));
    CHECK(e.ctrapezoid_type == c.type);
    CHECK(oasis::outline(e) == c.vertices);
  }
}

TEST_CASE("CTRAPEZOID modal width and height become the bounding box",
          "[cap:oasis-format][req:CTRAPEZOID record]") {
  const auto t0 = only_element(oasis_test::decode(cell_file("1A E3 31 00 00 1E 0A")));
  CHECK(oasis::outline(t0) == std::vector{pt(0, 0), pt(0, 10), pt(20, 10), pt(30, 0)});
  // Type 25 with no h and geometry-h undefined; a RECTANGLE then reuses w = h = 8.
  const auto d = oasis_test::decode(cell_file("1A C3 31 00 19 08  14 00"));
  REQUIRE_FALSE(d.error);
  CHECK(oasis::outline(d.elements()[0]) == std::vector{pt(0, 0), pt(0, 8), pt(8, 8), pt(8, 0)});
  CHECK(d.elements()[1].size == pt(8, 8));
  // Type 20 (h only): the box is 2h wide.
  const auto t20 = oasis_test::decode(cell_file("1A A3 31 00 14 05  14 00"));
  CHECK(t20.elements()[1].size == pt(10, 5));
  const auto bad = oasis_test::decode(cell_file("1A C3 31 00 1A 08"));
  CHECK(bad.elements().empty());
  CHECK(bad.text("OASIS-CODEC-021") == "invalid CTRAPEZOID type 26");
}

TEST_CASE("CIRCLE fields", "[cap:oasis-format][req:CIRCLE record]") {
  const auto e = only_element(oasis_test::decode(cell_file("1B 3B 02 00 0A 64 64")));
  CHECK(e.kind == oasis::ElementKind::circle);
  CHECK(e.layer == 2);
  CHECK(e.radius == 10);
  CHECK(e.position == pt(50, 50));
  CHECK(only_element(oasis_test::decode(cell_file("1B 3B 02 00 00 64 64"))).radius == 0);
  CHECK(oasis_test::decode(cell_file("1B 1B 02 00 64 64")).error->text == "modal variable circle-radius undefined");
}

// --- PROPERTY, standard properties, extensions, PAD ---------------------------------

TEST_CASE("a standard file property after START", "[cap:oasis-format][req:PROPERTY records]") {
  File f;
  f.add("1C 17 00 08 04  07").add(str("S_MAX_SIGNED_INTEGER_WIDTH"));
  const auto d = oasis_test::decode(f.end());
  REQUIRE_FALSE(d.error);
  const oasis::Record& r = d.records[1];
  REQUIRE(r.property);
  CHECK(r.target == oasis::PropertyTarget::file);
  CHECK(r.property->standard);
  CHECK(r.property->name_reference == 0);
  CHECK(r.property->values == std::vector{oasis::PropertyValue::of_unsigned(4)});
  CHECK(d.diagnostics.empty());
}

TEST_CASE("PROPERTY repeat, value list reuse and counts", "[cap:oasis-format][req:PROPERTY records]") {
  const auto d = oasis_test::decode(
      cell_file("14 63 31 00 0A 0A  1C 14 01 50 08 07  14 10 28  1D  14 10 50  1C 0C 01 51  1C F4 01 52 0F"
                " 08 00 08 01 08 02 08 03 08 04 08 05 08 06 08 07 08 08 08 09 08 0A 08 0B 08 0C 08 0D 08 0E"));
  REQUIRE_FALSE(d.error);
  std::vector<oasis::Property> props;
  for (const auto& r : d.records) {
    if (r.property) {
      CHECK(r.target == oasis::PropertyTarget::element);
      props.push_back(*r.property);
    }
  }
  REQUIRE(props.size() == 4);
  CHECK(props[0].name == "P");
  CHECK(props[1] == props[0]);  // record 29 repeats it on the second rectangle
  CHECK(props[2].name == "Q");
  CHECK(props[2].values == props[0].values);  // V = 1 reuses the value list
  CHECK(props[3].values.size() == 15);  // UUUU = 15 and an explicit count
  CHECK(props[3].values[14].unsigned_value == 14);
}

TEST_CASE("every PROPERTY value type", "[cap:oasis-format][req:PROPERTY records]") {
  const auto d = oasis_test::decode(cell_file("14 63 31 00 0A 0A  1C F4 01 50 0B"
                                              " 00 05  01 05  02 04  03 04  04 03 02  05 03 02  06 00 00 C0 3F"
                                              " 07 00 00 00 00 00 00 F8 3F  08 07  09 03  0A 01 61"));
  REQUIRE_FALSE(d.error);
  REQUIRE(d.records[3].property);
  const auto& values = d.records[3].property->values;
  REQUIRE(values.size() == 11);
  CHECK(values[0].real == 5.0);
  CHECK(values[1].real == -5.0);
  CHECK(values[2].real == 0.25);
  CHECK(values[4].real == 1.5);
  CHECK(values[8].unsigned_value == 7);
  CHECK(values[9].signed_value == -1);
  CHECK(values[10].kind == oasis::PropertyValue::Kind::a_string);
  const auto refs = oasis_test::decode(cell_file("14 63 31 00 0A 0A  1C 54 01 50 0B 01 62 0C 01 63 0D 00 0E 00 0F 00"
                                                 "  09 01 73"));
  REQUIRE_FALSE(refs.error);
  const auto& r = refs.records[3].property->values;
  CHECK(r[1].kind == oasis::PropertyValue::Kind::n_string);
  CHECK(r[2].kind == oasis::PropertyValue::Kind::a_reference);
  CHECK(r[4].kind == oasis::PropertyValue::Kind::n_reference);
  CHECK(oasis_test::decode(cell_file("14 63 31 00 0A 0A  1C 14 01 50 10")).error->code == "OASIS-CODEC-028");
}

TEST_CASE("PROPERTY attachment targets (Q-OASIS-6)", "[cap:oasis-format][req:PROPERTY records]") {
  const auto d = oasis_test::decode(cell_file("1C 04 01 41  11 80 01 42  10  1C 04 01 43  03 01 44  1C 04 01 45"));
  REQUIRE_FALSE(d.error);
  std::vector<oasis::PropertyTarget> targets;
  for (const auto& r : d.records) {
    if (r.property) {
      targets.push_back(r.target);
    }
  }
  CHECK(targets == std::vector{oasis::PropertyTarget::cell, oasis::PropertyTarget::cell,
                               oasis::PropertyTarget::name_record});
  CHECK(oasis_test::decode(cell_file("1D")).error->text == "modal variable last-property-name undefined");
  CHECK(oasis_test::decode(cell_file("1C 08")).error->text == "modal variable last-property-name undefined");
}

TEST_CASE("GDS property on a placement", "[cap:oasis-format][req:Standard properties]") {
  File f;
  f.add("0E").add(str("top")).add("11 80 01 42  1C 25").add(str("S_GDS_PROPERTY")).add("08 3D 0B 02 75 31");
  const auto d = oasis_test::decode(f.end());
  REQUIRE_FALSE(d.error);
  const oasis::Property& p = *d.records[3].property;
  CHECK(d.records[3].target == oasis::PropertyTarget::element);
  CHECK(oasis::standard_property(p.name) == oasis::StandardProperty::gds_property);
  CHECK(p.values[0].unsigned_value == 61);
  CHECK(p.values[1].string == "u1");
  CHECK(d.diagnostics.empty());
}

TEST_CASE("an unknown standard property is kept with a warning", "[cap:oasis-format][req:Standard properties]") {
  File f;
  f.add("1C 05").add(str("S_UNKNOWN"));
  const auto d = oasis_test::decode(f.end());
  REQUIRE_FALSE(d.error);
  CHECK(d.text("OASIS-CODEC-025") == "unknown standard property S_UNKNOWN");
  REQUIRE(d.records[1].property);
  CHECK(oasis::standard_property_name(oasis::StandardProperty::cell_offset) == "S_CELL_OFFSET");
}

TEST_CASE("XNAME, XELEMENT and XGEOMETRY", "[cap:oasis-format][req:Extension records]") {
  const auto d = oasis_test::decode(cell_file("1F 01 02 61 62 00  1F 02 00 07  20 03 02 FF 00  21 07 09 07 00 01 7A 02 00 0A"
                                              "  14 62 00 0A 0A"));
  REQUIRE_FALSE(d.error);
  CHECK(d.names.xnames.at(0) == oasis::XName{1, "ab"});
  CHECK(d.names.xnames.at(7) == oasis::XName{2, ""});
  File implicit;
  implicit.add("1E 04 01 71  1E 05 00");
  const auto i = oasis_test::decode(implicit.end());
  REQUIRE_FALSE(i.error);
  CHECK(i.names.xnames.at(1) == oasis::XName{5, ""});
  const auto e = d.elements();
  REQUIRE(e.size() == 3);
  CHECK(e[0].kind == oasis::ElementKind::xelement);
  CHECK(e[0].attribute == 3);
  CHECK(e[0].payload == std::string{"\xff\x00", 2});
  CHECK(e[1].kind == oasis::ElementKind::xgeometry);
  CHECK(e[1].layer == 7);
  CHECK(e[1].payload == "z");
  CHECK(e[1].repetition.has_value());
  CHECK(e[2].layer == 7);  // the RECTANGLE takes the modal layer of the XGEOMETRY
}

TEST_CASE("a cell body inside a CBLOCK", "[cap:oasis-format][req:CBLOCK compression]") {
  File f;
  f.add("0E").add(str("top")).add(cblock(hex("14 63 31 00 0A 0A  14 10 28")));
  const auto d = oasis_test::decode(f.end());
  REQUIRE_FALSE(d.error);
  const auto e = d.elements();
  REQUIRE(e.size() == 2);
  CHECK(e[1].position == pt(20, 0));
  CHECK(d.records[2].id == oasis::RecordId::cblock);
  CHECK(d.records[3].in_cblock);
  CHECK(d.records[3].offset == d.records[2].offset);
  CHECK(d.records[4].cblock_position == 6);
}

TEST_CASE("a record may span the end of a CBLOCK (Q-OASIS-9)", "[cap:oasis-format][req:CBLOCK compression]") {
  File f;
  f.add("0E").add(str("top")).add(cblock(hex("14 63 31 00 0A"))).add("0A");
  f.add(cblock(hex("13 43 03 61"))).add("62 63 00 00");
  const auto d = oasis_test::decode(f.end());
  REQUIRE_FALSE(d.error);
  const auto e = d.elements();
  REQUIRE(e.size() == 2);
  CHECK(e[0].size == pt(10, 10));
  CHECK(e[1].name == "abc");  // the string continues after the CBLOCK
}

TEST_CASE("CBLOCK errors stop decoding", "[cap:oasis-format][req:CBLOCK compression]") {
  const auto type1 = oasis_test::decode(cell_file("22 01 00 00"));
  REQUIRE(type1.error);
  CHECK(type1.error->code == "OASIS-CODEC-023");
  CHECK(type1.error->text == "unsupported CBLOCK compression type 1");
  std::vector<std::byte> wrong = cblock(hex("14 63 31 00 0A 0A"));
  wrong[2] = std::byte{7};  // declared uncompressed size
  File mismatch;
  mismatch.add("0E").add(str("top")).add(wrong);
  CHECK(oasis_test::decode(mismatch.end()).error->text == "CBLOCK size mismatch");
  File nested;
  nested.add("0E").add(str("top")).add(cblock(hex("22 00 00 00")));
  CHECK(oasis_test::decode(nested.end()).error->text == "record type 34 inside a CBLOCK");
  File ending;
  ending.add(cblock(hex("02")));
  CHECK(oasis_test::decode(ending.end()).error->text == "record type 2 inside a CBLOCK");
  // 1 MiB of zeros deflates to about 1 KiB, so 256 x compressed is below it.
  File capped;
  capped.add("0E").add(str("top")).add(cblock(std::vector<std::byte>(std::size_t{1} << 20, std::byte{0})));
  const auto over = oasis_test::decode(capped.end(), oasis::DecoderOptions{true, 1024});
  REQUIRE(over.error);
  CHECK(over.error->code == "OASIS-CODEC-023");
  CHECK(over.error->text.starts_with("CBLOCK of 1048576 bytes is over the cap of "));
  CHECK_FALSE(oasis::inflate_cblock(hex("FF FF FF"), 10).ok());
}

TEST_CASE("regression: the CBLOCK cap is 256 x compressed size, at least the limit",
          "[cap:oasis-format][req:CBLOCK compression]") {
  // Design D11. The cap was the fixed inflate_limit, so a CBLOCK above it
  // was refused even when it inflated to less than 256 x its compressed
  // size. A TEXTSTRING of 4093 pseudo-random printable bytes (4096 bytes
  // with its header) deflates to well over 4096 / 256 bytes.
  std::vector<std::byte> record = hex("05 FD 1F");
  std::uint32_t x = 12345;
  for (int i = 0; i < 4093; ++i) {
    x = x * 1103515245U + 12345U;
    record.push_back(static_cast<std::byte>(0x21 + (x >> 16) % 94));
  }
  REQUIRE(record.size() == 4096);
  File f;
  f.add(cblock(record)).add("0E").add(str("top"));
  const auto d = oasis_test::decode(f.end(), oasis::DecoderOptions{true, 1024});
  CHECK_FALSE(d.error);
}

TEST_CASE("an inflate bomb is refused before inflating", "[cap:oasis-format][req:CBLOCK compression]") {
  // A CBLOCK that declares 2^40 bytes is refused from its header alone.
  std::vector<std::byte> header = hex("22 00");
  oasis::write_unsigned(header, std::uint64_t{1} << 40);
  oasis::write_unsigned(header, 1);
  header.push_back(std::byte{0});
  File f;
  f.add("0E").add(str("top")).add(header);
  const auto bomb = oasis_test::decode(f.end());
  REQUIRE(bomb.error);
  CHECK(bomb.error->code == "OASIS-CODEC-023");
  // inflate_cblock's default cap is gds::inflate_cap of the compressed size.
  CHECK_FALSE(oasis::inflate_cblock(hex("03 00"), std::uint64_t{1} << 40).ok());
}

TEST_CASE("strict mode violations warn and keep the content", "[cap:oasis-format][req:Strict mode]") {
  // TEXTSTRING strict at offset 0 and an inline TEXT string.
  File f;
  f.add("0E").add(str("top")).add("13 43 02 68 69 05 00");
  const auto d = oasis_test::decode(f.end(oasis::ValidationScheme::crc32, "00 00 01 00 00 00 00 00 00 00 00 00"));
  REQUIRE_FALSE(d.error);
  CHECK(d.text("OASIS-CODEC-024") == "strict mode violation: inline TEXT string with a strict TEXTSTRING table (1 times)");
  CHECK(d.elements().size() == 1);
  // CELLNAME strict at the wrong offset, split by a PROPNAME, and a CELL by name.
  File g;
  g.add("0E").add(str("top")).add("03 01 41  07 01 50  03 01 42");
  const auto s = oasis_test::decode(g.end(oasis::ValidationScheme::crc32, "01 05 00 00 00 00 00 00 00 00 00 00"));
  std::vector<std::string> texts;
  for (const auto& diagnostic : s.diagnostics) {
    if (diagnostic.code == "OASIS-CODEC-024") {
      texts.push_back(diagnostic.text);
    }
  }
  CHECK(texts == std::vector<std::string>{"strict mode violation: CELLNAME table does not start at its offset 5",
                                          "strict mode violation: CELLNAME records are not contiguous",
                                          "strict mode violation: CELL record by name with a strict CELLNAME table (1 times)"});
  const auto unchecked = oasis_test::decode(g.end(oasis::ValidationScheme::crc32, "01 05 00 00 00 00 00 00 00 00 00 00"),
                                            oasis::DecoderOptions{false});
  CHECK_FALSE(unchecked.has("OASIS-CODEC-024"));
}

TEST_CASE("a PAD is transparent for PROPERTY attachment", "[cap:oasis-format][req:PAD record]") {
  const auto d = oasis_test::decode(cell_file("14 63 31 00 0A 0A  00  1C 04 01 50"));
  REQUIRE_FALSE(d.error);
  CHECK(d.records[3].id == oasis::RecordId::pad);
  CHECK(d.records[4].target == oasis::PropertyTarget::element);
}

TEST_CASE("the reference file decodes", "[cap:oasis-format][req:Complete annotated example]") {
  const std::vector<std::byte> bytes = oasis_test::decode_file("reference_419.oas");
  CHECK(bytes.size() == 419);
  const auto d = oasis_test::decode(bytes);
  REQUIRE_FALSE(d.error);
  CHECK(d.diagnostics.empty());  // valid CRC32, strict tables respected, no unknown names
  CHECK(d.records[0].start->unit == 1000.0);
  const auto e = only_element(d);
  CHECK(e.layer == 49);
  CHECK(e.datatype == 0);
  CHECK(absolute_outline(e) == std::vector{pt(0, 0), pt(400, 0), pt(400, 200), pt(0, 200)});
  REQUIRE(d.records[4].reference);
  CHECK(d.names.cellnames.at(*d.records[4].reference) == "top");
  CHECK(d.names.propnames.at(0) == "S_MAX_SIGNED_INTEGER_WIDTH");
  CHECK(d.names.propnames.at(2) == "S_TOP_CELL");
  CHECK(d.names.propstrings.at(0) == "top");
  REQUIRE(d.names.layernames.size() == 2);
  CHECK(d.names.layernames[0] == oasis::LayerName{"M1", false, {3, 49, 49}, {3, 0, 0}});
  CHECK(d.names.layernames[1].text);
  const oasis::Record& end = d.records.back();
  CHECK(end.offset == 163);
  CHECK(end.end->length == 256);
  CHECK(end.end->validation == oasis::ValidationResult{oasis::ValidationScheme::crc32, 0xEE7E3BD3U, 0xEE7E3BD3U});
  CHECK(end.end->tables->at(0) == oasis::TableEntry{true, 153});
  CHECK(end.end->tables->at(2) == oasis::TableEntry{true, 47});
}
