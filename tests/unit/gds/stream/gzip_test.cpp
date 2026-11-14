// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Byte sources and sinks (tasks.md 9.1): plain and gzip files through
// platform::FileSystem, concatenated gzip members, decompressed offsets,
// seeking back by re-inflating from a checkpoint, and corrupt or cut-off
// compressed input (Q-MASK-223). Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/gds/stream/gzip.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "hefesto/platform/file_system.hpp"
#include "stream_fixture.hpp"

namespace gds = hefesto::gds;
namespace platform = hefesto::platform;
using gds_test::Bytes;

namespace {

/// Compressible but not trivial bytes.
Bytes pattern(std::size_t size, std::uint32_t seed) {
  Bytes out(size);
  std::uint32_t state = seed;
  for (std::size_t i = 0; i < size; ++i) {
    state = state * 1103515245U + 12345U;
    out[i] = static_cast<std::byte>(i % 7 == 0 ? (state >> 24U) : (i / 64) & 0xffU);
  }
  return out;
}

Bytes gzip(const Bytes& plain, int level = 6) {
  auto inner = std::make_unique<gds::MemorySink>();
  gds::MemorySink* memory = inner.get();
  auto sink = gds::make_gzip_sink(std::move(inner), level);
  REQUIRE(sink.ok());
  REQUIRE(sink.value()->write(plain).ok());
  CHECK(sink.value()->position() == plain.size());
  REQUIRE(sink.value()->finish().ok());
  return memory->bytes();
}

Bytes read_all(gds::ByteSource& source, std::size_t chunk = 1000) {
  Bytes out;
  std::vector<std::byte> buffer(chunk);
  for (;;) {
    auto got = source.read(buffer);
    REQUIRE(got.ok());
    if (got.value() == 0) {
      return out;
    }
    out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(got.value()));
  }
}

Bytes read_n(gds::ByteSource& source, std::size_t n) {
  Bytes out(n);
  auto got = source.read(out);
  REQUIRE(got.ok());
  out.resize(got.value());
  return out;
}

Bytes slice(const Bytes& b, std::size_t from, std::size_t n) {
  return Bytes(b.begin() + static_cast<std::ptrdiff_t>(from), b.begin() + static_cast<std::ptrdiff_t>(from + n));
}

Bytes concat(Bytes a, const Bytes& b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

Bytes as_bytes(const std::string& s) {
  return Bytes(reinterpret_cast<const std::byte*>(s.data()), reinterpret_cast<const std::byte*>(s.data()) + s.size());
}

}  // namespace

TEST_CASE("compressed and plain outputs agree after decompression", "[cap:gds-write][req:Compressed output]") {
  const Bytes plain = pattern(200000, 1);
  for (const int level : {1, 6, 9}) {
    INFO(level);
    const Bytes compressed = gzip(plain, level);
    CHECK(gds::is_gzip(compressed));
    CHECK(compressed.size() < plain.size());
    auto source = gds::make_gzip_source(compressed);
    CHECK(read_all(*source) == plain);
    CHECK(source->position() == plain.size());
  }
  for (const int level : {-1, 0, 10}) {
    auto bad = gds::make_gzip_sink(std::make_unique<gds::MemorySink>(), level);
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().code == "GDS-STREAM-005");
  }
}

TEST_CASE("files are written plain or as one gzip member and read back by content",
          "[cap:gds-write][req:Compressed output]") {
  platform::MemoryFileSystem fs;
  const Bytes plain = gds_test::Gds{}.library("lib").empty(0x04).bytes();
  for (const int level : {0, 9}) {
    INFO(level);
    const platform::Path path = level == 0 ? "out.gds" : "out.gds.gz";
    auto sink = gds::open_sink(fs, path, level);
    REQUIRE(sink.ok());
    REQUIRE(sink.value()->write(plain).ok());
    CHECK_FALSE(fs.exists(path));  // written atomically when finished
    REQUIRE(sink.value()->finish().ok());
    const auto stored = fs.read(path);
    REQUIRE(stored.ok());
    CHECK(gds::is_gzip(as_bytes(stored.value())) == (level != 0));
    auto source = gds::open_source(fs, path);
    REQUIRE(source.ok());
    CHECK(read_all(*source.value()) == plain);
  }
  CHECK_FALSE(gds::open_source(fs, "missing.gds").ok());
}

TEST_CASE("concatenated gzip members read as one stream; trailing bytes are ignored",
          "[cap:gds-read][req:Record framing and primitive data types]") {
  const Bytes a = pattern(5000, 2);
  const Bytes b = pattern(7000, 3);
  auto source = gds::make_gzip_source(concat(concat(gzip(a), gzip(b)), gds_test::hex("00 11 22")));
  CHECK(read_all(*source, 333) == concat(a, b));
  auto empty = gds::make_gzip_source({});
  CHECK(read_all(*empty).empty());
}

TEST_CASE("a member boundary at the edge of the inflate feed window reads on",
          "[cap:gds-read][req:Record framing and primitive data types]") {
  const Bytes a = pattern(5000, 4);
  const Bytes b = pattern(7000, 5);
  const Bytes first = gzip(a);
  const Bytes both = concat(first, gzip(b));
  // A window ending 1 byte into the second member hides its magic number
  // from the bytes handed to inflate.
  for (std::size_t feed = first.size() - 1; feed <= first.size() + 3; ++feed) {
    INFO("feed window " << feed);
    auto source = gds::make_gzip_source(both, feed);
    CHECK(read_all(*source, 333) == concat(a, b));
  }
  auto tiny = gds::make_gzip_source(both, 1);
  CHECK(read_all(*tiny) == concat(a, b));
}

TEST_CASE("positions count decompressed bytes and seeks re-inflate",
          "[cap:gds-read][req:Record framing and primitive data types]") {
  // Large enough for several inflate checkpoints (one every 4 MiB of output).
  const Bytes plain = pattern(std::size_t{18} << 20U, 4);
  auto source = gds::make_gzip_source(gzip(plain, 1));
  CHECK(read_n(*source, 100) == slice(plain, 0, 100));
  CHECK(source->position() == 100);
  // Forward past several checkpoints.
  const std::uint64_t far = (std::uint64_t{17} << 20U) + 12345;
  REQUIRE(source->seek(far).ok());
  CHECK(source->position() == far);
  CHECK(read_n(*source, 64) == slice(plain, far, 64));
  // Back to a checkpointed region, to the start, and within the current chunk.
  for (const std::uint64_t target : {std::uint64_t{9} << 20U, std::uint64_t{(9U << 20U) + 70000}, std::uint64_t{5},
                                     std::uint64_t{(13U << 20U) + 1}, std::uint64_t{(13U << 20U) + 3}}) {
    INFO(target);
    REQUIRE(source->seek(target).ok());
    CHECK(source->position() == target);
    CHECK(read_n(*source, 4096) == slice(plain, target, 4096));
  }
  // To the very end, and past it.
  REQUIRE(source->seek(plain.size()).ok());
  CHECK(read_n(*source, 10).empty());
  auto past = source->seek(plain.size() + 1);
  REQUIRE_FALSE(past.ok());
  CHECK(past.error().code == "GDS-STREAM-003");
  REQUIRE(source->seek(1).ok());
  CHECK(read_n(*source, 3) == slice(plain, 1, 3));
}

TEST_CASE("a record reader over gzip reports decompressed offsets",
          "[cap:gds-read][req:Record framing and primitive data types]") {
  gds_test::Gds g;
  g.library("lib").i2(0x05, {124, 1, 1, 0, 0, 0, 124, 1, 1, 0, 0, 0}).str(0x06, "A").empty(0x07).empty(0x04);
  auto source = gds::make_gzip_source(gzip(g.bytes()));
  gds::RecordReader reader{*source};
  std::vector<std::uint64_t> offsets;
  for (;;) {
    auto r = reader.next();
    REQUIRE(r.ok());
    if (!r.value()) {
      break;
    }
    offsets.push_back(r.value()->offset);
  }
  // HEADER 6, BGNLIB 28, LIBNAME 8, UNITS 20, BGNSTR 28, STRNAME 6, ENDSTR 4, ENDLIB 4.
  CHECK(offsets == std::vector<std::uint64_t>{0, 6, 34, 42, 62, 90, 96, 100});
  REQUIRE(reader.seek(90).ok());
  auto name = reader.next();
  REQUIRE(name.ok());
  CHECK(name.value()->type == 0x06);
}

TEST_CASE("corrupt or cut-off gzip input is an error, not a crash",
          "[cap:gds-read][req:Record framing and primitive data types]") {
  const Bytes plain = pattern(100000, 5);
  const Bytes good = gzip(plain);
  SECTION("cut off inside the member") {
    auto source = gds::make_gzip_source(slice(good, 0, good.size() / 2));
    std::vector<std::byte> buffer(plain.size());
    auto got = source->read(buffer);
    REQUIRE_FALSE(got.ok());
    CHECK(got.error().code == "GDS-STREAM-002");
  }
  SECTION("damaged deflate data") {
    Bytes bad = good;
    REQUIRE(bad.size() > 60);
    for (std::size_t i = 20; i < 60; ++i) {
      bad.at(i) = std::byte{0xff};
    }
    auto source = gds::make_gzip_source(bad);
    std::vector<std::byte> buffer(plain.size());
    auto got = source->read(buffer);
    REQUIRE_FALSE(got.ok());
    CHECK(got.error().code == "GDS-STREAM-002");
  }
  SECTION("only the magic number") {
    auto source = gds::make_gzip_source(gds_test::hex("1f 8b"));
    std::vector<std::byte> buffer(10);
    CHECK_FALSE(source->read(buffer).ok());
  }
}
