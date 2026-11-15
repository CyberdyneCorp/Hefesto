// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the GDS reader (capability gds-read;
// hefesto-testing-strategy "Parser fuzzing"; m5-mask-gds-cif design D21,
// R9). The first byte is a mode:
//   bit 0  gzip: the rest is read through the gzip source. Rest that starts
//          with the gzip magic is inflated as given (the inflater itself is
//          fuzzed); any other rest is compressed here first, so the reader
//          sees the same records behind the gzip layer.
//   bit 1  a tiny inflate feed window (7 bytes), to reach the window edges
//   bit 2  magic-compat OFF
//   bit 3  `gds readonly yes` (the read-only view path: GDS_FILE/START/END
//          and the content-hash stamps)
//   bit 4  `gds ordering yes` (post-order look-ahead and repositioning)
//   bit 5  `gds flatten yes` with flatten limit 10
//   bit 6  `gds merge yes` for the write
//   bit 7  `gds compress 1` for the write
// The rest is the GDS stream. The target runs the record reader to the end,
// the content-hash scanner over the bytes and over the source, a seek back
// into the source, the full read into a scratch session with the fixed
// technology tests/fuzz/data/mask_fuzz.tech, then a GDS write of the top
// cells into memory (read-then-write never crashes).
//
// Resource caps: the reader's own (GDS-READ-011: look-ahead and
// magnification depth, array copies, flatten paints; the inflate size cap
// GDS-STREAM-006); the write's output style has no cut arrays, and at most
// two top cells are written.
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZ workstream.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "fuzz_common.hpp"
#include "hefesto/gds/read/read.hpp"
#include "hefesto/gds/read/stamps.hpp"
#include "hefesto/gds/stream/gzip.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "mask_fuzz_session.hpp"

namespace {

namespace gds = hefesto::gds;

constexpr std::uint8_t kGzip = 0x01;
constexpr std::uint8_t kTinyFeed = 0x02;
constexpr std::uint8_t kCompatOff = 0x04;
constexpr std::uint8_t kReadOnly = 0x08;
constexpr std::uint8_t kOrdering = 0x10;
constexpr std::uint8_t kFlatten = 0x20;
constexpr std::uint8_t kMerge = 0x40;
constexpr std::uint8_t kCompress = 0x80;
constexpr std::size_t kTinyFeedBytes = 7;
constexpr std::size_t kMaxRecords = std::size_t{1} << 20U;
constexpr std::size_t kMaxTopWrites = 2;
constexpr const char* kInputPath = "/in.gds";

std::vector<std::byte> bytes_of(std::string_view text) {
  std::vector<std::byte> out(text.size());
  std::transform(text.begin(), text.end(), out.begin(), [](char c) { return static_cast<std::byte>(c); });
  return out;
}

/// `plain` as one gzip member (deflate level 1).
std::vector<std::byte> compress(std::span<const std::byte> plain) {
  auto memory = std::make_unique<gds::MemorySink>();
  gds::MemorySink* inner = memory.get();
  auto sink = gds::make_gzip_sink(std::move(memory), 1);
  if (!sink.ok() || !sink.value()->write(plain).ok() || !sink.value()->finish().ok()) {
    return {};
  }
  return inner->bytes();
}

/// The stream as stored in the file: plain, or gzip data.
std::vector<std::byte> stored_bytes(std::uint8_t mode, std::string_view rest) {
  std::vector<std::byte> bytes = bytes_of(rest);
  if ((mode & kGzip) == 0 || gds::is_gzip(bytes)) {
    return bytes;
  }
  return compress(bytes);
}

std::unique_ptr<gds::ByteSource> open(std::uint8_t mode, const std::vector<std::byte>& stored) {
  if ((mode & kGzip) == 0) {
    return std::make_unique<gds::MemorySource>(stored);
  }
  return gds::make_gzip_source(stored, (mode & kTinyFeed) != 0 ? kTinyFeedBytes : gds::kMaxGzipFeed);
}

/// Every record to the end (or the first error), then a seek back to the
/// middle and one more record.
void scan_records(gds::ByteSource& source) {
  gds::RecordReader reader{source};
  for (std::size_t n = 0; n < kMaxRecords; ++n) {
    auto record = reader.next();
    if (!record.ok() || !record.value()) {
      break;
    }
  }
  if (reader.seek(reader.position() / 2).ok()) {
    (void)reader.next();
  }
}

void scan(std::uint8_t mode, const std::vector<std::byte>& stored) {
  if ((mode & kGzip) == 0) {
    (void)gds::content_stamps(std::span<const std::byte>{stored});
  }
  if (auto source = open(mode, stored)) {
    scan_records(*source);
  }
  if (auto source = open(mode, stored)) {
    (void)gds::content_stamps(*source);
  }
}

void read_then_write(std::uint8_t mode, const std::vector<std::byte>& stored) {
  hefesto::fuzz::MaskFuzzSession session{(mode & kCompatOff) == 0};
  if (!session.files.write_atomic(kInputPath, std::string_view{reinterpret_cast<const char*>(stored.data()),  // NOLINT
                                                               stored.size()})
           .ok()) {
    return;
  }
  gds::GdsReadOptions options;
  options.settings = session.editor.mask.gds;
  options.settings.readonly = (mode & kReadOnly) != 0;
  options.settings.ordering = (mode & kOrdering) != 0;
  options.settings.flatten = (mode & kFlatten) != 0;
  options.files = &session.files;
  options.gds_file = kInputPath;
  options.time_zone = "UTC0";
  options.typed_name = "in";
  auto source = open(mode, stored);
  (void)gds::read(session.editor, *source, options);
  session.editor.mask.gds.readonly = options.settings.readonly;
  session.write_tops(kMaxTopWrites, (mode & kMerge) != 0, (mode & kCompress) != 0 ? 1 : 0);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, rest] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  const std::vector<std::byte> stored = stored_bytes(mode, rest);
  scan(mode, stored);
  read_then_write(mode, stored);
  return 0;
}
