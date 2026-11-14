// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The record reader and writer (tasks.md 9.1): framing, end of stream,
// look-ahead, positions in the stream, the writer's encodings and its
// 64 KB buffer, and malformed input. Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <functional>
#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/gds/stream/io.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "stream_fixture.hpp"

namespace gds = hefesto::gds;
using gds_test::Gds;
using gds_test::hex;

namespace {

gds::Record next(gds::RecordReader& reader) {
  auto r = reader.next();
  REQUIRE(r.ok());
  REQUIRE(r.value().has_value());
  return *r.value();
}

bool at_end(gds::RecordReader& reader) {
  auto r = reader.next();
  REQUIRE(r.ok());
  return !r.value().has_value();
}

std::vector<std::byte> written(const std::function<void(gds::RecordWriter&)>& emit) {
  gds::MemorySink sink;
  gds::RecordWriter writer{sink};
  emit(writer);
  REQUIRE(writer.flush().ok());
  return sink.bytes();
}

}  // namespace

TEST_CASE("records are framed by their length, type and data type",
          "[cap:gds-read][req:Record framing and primitive data types]") {
  Gds g;
  g.str(0x06, "ABC").empty(0x11).i2(0x0d, {49});
  gds::MemorySource source{g.bytes()};
  gds::RecordReader reader{source};
  const gds::Record name = next(reader);
  CHECK(name.type == 0x06);
  CHECK(name.data_type == 6);
  CHECK(name.data == hex("41 42 43 00"));
  CHECK(name.offset == 0);
  const gds::Record endel = next(reader);
  CHECK(endel.type == 0x11);
  CHECK(endel.data.empty());
  CHECK(endel.offset == 8);
  const gds::Record layer = next(reader);
  CHECK(layer.offset == 12);
  CHECK(layer.data == hex("00 31"));
  CHECK(at_end(reader));
  CHECK(at_end(reader));
}

TEST_CASE("the data type code is kept but not interpreted", "[cap:gds-read][req:Record framing and primitive data types]") {
  Gds g;
  g.rec(0x0d, 0x06, hex("00 31"));  // LAYER with a wrong data type code
  gds::MemorySource source{g.bytes()};
  gds::RecordReader reader{source};
  const gds::Record r = next(reader);
  CHECK(r.type == 0x0d);
  CHECK(r.data_type == 6);
  CHECK(r.data == hex("00 31"));
}

TEST_CASE("end of file inside a header is the end of the records",
          "[cap:gds-read][req:Record framing and primitive data types]") {
  Gds g;
  g.empty(0x11).raw(hex("00 04 11"));
  gds::MemorySource source{g.bytes()};
  gds::RecordReader reader{source};
  CHECK(next(reader).type == 0x11);
  CHECK(at_end(reader));
}

TEST_CASE("a record shorter than its header has no data", "[cap:gds-read][req:Record framing and primitive data types]") {
  gds::MemorySource source{hex("00 02 11 00  00 00 07 00  00 04 04 00")};
  gds::RecordReader reader{source};
  const gds::Record a = next(reader);
  CHECK(a.type == 0x11);
  CHECK(a.data.empty());
  CHECK(next(reader).type == 0x07);
  const gds::Record endlib = next(reader);
  CHECK(endlib.type == 0x04);
  CHECK(endlib.offset == 8);
}

TEST_CASE("a record cut off by the end of the stream is an error",
          "[cap:gds-read][req:Record framing and primitive data types]") {
  gds::MemorySource source{hex("00 0C 10 03 00 00 00 01")};
  gds::RecordReader reader{source};
  auto r = reader.next();
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().code == "GDS-STREAM-001");
}

TEST_CASE("one record of look-ahead: peek and unread", "[cap:gds-read][req:Record framing and primitive data types]") {
  Gds g;
  g.empty(0x05).str(0x06, "A").empty(0x07);
  gds::MemorySource source{g.bytes()};
  gds::RecordReader reader{source};
  auto peeked = reader.peek();
  REQUIRE(peeked.ok());
  REQUIRE(peeked.value().has_value());
  CHECK(peeked.value()->type == 0x05);
  CHECK(reader.position() == 0);
  CHECK(next(reader).type == 0x05);
  CHECK(reader.position() == 4);
  const gds::Record name = next(reader);
  CHECK(reader.position() == 10);
  reader.unread(name);
  CHECK(reader.position() == 4);  // the un-read record is next again
  CHECK(next(reader) == name);
  CHECK(next(reader).type == 0x07);
  auto end = reader.peek();
  REQUIRE(end.ok());
  CHECK_FALSE(end.value().has_value());
}

TEST_CASE("seeking drops the look-ahead and repositions in the stream",
          "[cap:gds-read][req:Record framing and primitive data types]") {
  Gds g;
  g.empty(0x05).str(0x06, "A").empty(0x07);
  gds::MemorySource source{g.bytes()};
  gds::RecordReader reader{source};
  REQUIRE(reader.peek().ok());
  REQUIRE(reader.seek(4).ok());
  CHECK(next(reader).type == 0x06);
  REQUIRE(reader.seek(0).ok());
  CHECK(next(reader).type == 0x05);
  auto past = reader.seek(1000);
  REQUIRE_FALSE(past.ok());
  CHECK(past.error().code == "GDS-STREAM-003");
}

TEST_CASE("the writer encodes headers and big-endian values", "[cap:gds-write][req:Record encoding]") {
  CHECK(written([](gds::RecordWriter& w) { REQUIRE(w.write_empty(gds::RecordType::endel).ok()); }) ==
        hex("00 04 11 00"));
  CHECK(written([](gds::RecordWriter& w) {
          const std::array<std::int16_t, 1> v{49};
          REQUIRE(w.write_int2(gds::RecordType::layer, v).ok());
        }) == hex("00 06 0D 02 00 31"));
  CHECK(written([](gds::RecordWriter& w) {
          const std::array<std::int32_t, 1> v{-200};
          REQUIRE(w.write_int4(gds::RecordType::xy, v).ok());
        }) == hex("00 08 10 03 FF FF FF 38"));
  CHECK(written([](gds::RecordWriter& w) {
          const std::array<double, 2> v{0.001, 1e-9};
          REQUIRE(w.write_real8(gds::RecordType::units, v).ok());
        }) == hex("00 14 03 05 3E 41 89 37 4B C6 A7 F0 39 44 B8 2F A0 9B 5A 54"));
  CHECK(written([](gds::RecordWriter& w) { REQUIRE(w.write_string(gds::RecordType::strname, "top").ok()); }) ==
        hex("00 08 06 06 74 6F 70 00"));
  CHECK(written([](gds::RecordWriter& w) {
          REQUIRE(w.write(gds::RecordType::strans, gds::DataType::bitarray, hex("80 00")).ok());
        }) == hex("00 06 1A 01 80 00"));
}

TEST_CASE("the writer buffers 64 KB and flushes in order", "[cap:gds-write][req:Record encoding]") {
  gds::MemorySink sink;
  gds::RecordWriter writer{sink};
  const std::vector<std::byte> payload(60000, std::byte{0x41});
  REQUIRE(writer.write(gds::RecordType::string, gds::DataType::string, payload).ok());
  CHECK(sink.bytes().empty());  // still buffered
  REQUIRE(writer.write(gds::RecordType::string, gds::DataType::string, payload).ok());
  CHECK(sink.bytes().size() == 60004);  // the first record went out to make room
  REQUIRE(writer.flush().ok());
  REQUIRE(sink.bytes().size() == 120008);
  gds::MemorySource source{sink.bytes()};
  gds::RecordReader reader{source};
  CHECK(next(reader).data == payload);
  CHECK(next(reader).data == payload);
  CHECK(at_end(reader));
}

TEST_CASE("a payload over 65531 bytes is refused", "[cap:gds-write][req:Record encoding]") {
  gds::MemorySink sink;
  gds::RecordWriter writer{sink};
  const std::vector<std::byte> largest(65531, std::byte{0});
  REQUIRE(writer.write(gds::RecordType::xy, gds::DataType::int4, largest).ok());
  const std::vector<std::byte> too_large(65532, std::byte{0});
  auto status = writer.write(gds::RecordType::xy, gds::DataType::int4, too_large);
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == "GDS-STREAM-004");
  REQUIRE(writer.flush().ok());
  CHECK(sink.bytes().size() == 65535);
  CHECK(sink.bytes()[0] == std::byte{0xff});
  CHECK(sink.bytes()[1] == std::byte{0xff});
}

TEST_CASE("every byte sequence reads without crashing", "[cap:gds-read][req:Record framing and primitive data types]") {
  // A small deterministic sweep over malformed streams: random lengths, types and truncation.
  std::uint32_t state = 12345;
  for (int round = 0; round < 500; ++round) {
    std::vector<std::byte> bytes;
    const std::size_t size = (state = state * 1103515245U + 12345U) % 200;
    for (std::size_t i = 0; i < size; ++i) {
      bytes.push_back(static_cast<std::byte>((state = state * 1103515245U + 12345U) >> 16U));
    }
    gds::MemorySource source{bytes};
    gds::RecordReader reader{source};
    for (int i = 0; i < 1000; ++i) {
      auto r = reader.next();
      if (!r.ok() || !r.value()) {
        break;
      }
      CHECK(r.value()->data.size() <= 65531);
    }
  }
}
