// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the OASIS reader (capabilities oasis-format,
// oasis-read, oasis-write and gds-write "Read-only OASIS vendor cells";
// hefesto-testing-strategy "Parser fuzzing"; m6-lef-def-oasis design D21).
// The first byte is a mode:
//   bit 0  gzip: rest that starts with the gzip magic is stored as given
//          (the inflater itself is fuzzed); any other rest is compressed
//          here first, so the reader sees the same records behind gzip
//   bit 1  magic-compat OFF
//   bit 2  `oasis readonly yes` (the read-only view path: OASIS_FILE and
//          the content-hash stamps; the writes then re-encode the vendor
//          cells from the stored file)
//   bit 3  `oasis flatten yes` with flatten limit 10
//   bit 4  the decoder runs with the strict-mode checks off
//   bit 5  the OASIS write uses CBLOCKs (compress 1), else none
//   bit 6  the OASIS write with `repetitions no`, `strict no` and
//          `layernames no`
//   bit 7  `oasis circlepoints 8`, else 64
// The rest is the OASIS file. The target runs the decoder over every
// record (name tables, CBLOCKs, repetitions, properties), the END record's
// validation, the content-hash scan, the full read into a scratch session
// with the fixed technology tests/fuzz/data/mask_fuzz.tech, then an OASIS
// write and a GDS write of the top cells into memory (read-then-write
// never crashes).
//
// Resource caps: the decoder's (CBLOCK inflation at most 256 times the
// compressed size, at least 1 GiB; repetitions are never expanded into a
// list, M6-S3/S4; a point list or explicit repetition (types 4/5/6/7/10/11)
// of more than 2^24 entries is skipped without being stored and its element
// dropped, OASIS-CODEC-030, Q-OASIS-14 -- the main memory bound once a
// CBLOCK inflates), the read's approved limits (2 GiB stream, 2^28
// instances, 65530-byte strings, 8191 points; Q-OASIS-41..44), the vendor
// write caps (OASIS-WRITE-010 at 2^24 explicit offsets, GDS-WRITE-055 at
// 2^28 instances). Those caps still let a few bytes of repetition make a
// read of minutes (2^28 instances, a 2 GiB stream), so this target reads
// with kFuzzLimits (2^16 instances, 8 MiB) and skips the writes when the
// decoded elements count more than kMaxWriteInstances instances; at most
// kMaxRecords records are decoded and two top cells are written, skipped
// when a cell spans more than 20 000 interaction squares
// (mask_fuzz_session.hpp, M5-S4).
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZSEC
// workstream.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fuzz_common.hpp"
#include "hefesto/gds/stream/gzip.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/oasis/codec/decoder.hpp"
#include "hefesto/oasis/codec/validate.hpp"
#include "hefesto/oasis/codec/elements.hpp"
#include "hefesto/oasis/read/limits.hpp"
#include "hefesto/oasis/read/read.hpp"
#include "hefesto/oasis/read/stamps.hpp"
#include "hefesto/oasis/write/write.hpp"
#include "mask_fuzz_session.hpp"

namespace {

namespace gds = hefesto::gds;
namespace oasis = hefesto::oasis;

constexpr std::uint8_t kGzip = 0x01;
constexpr std::uint8_t kCompatOff = 0x02;
constexpr std::uint8_t kReadOnly = 0x04;
constexpr std::uint8_t kFlatten = 0x08;
constexpr std::uint8_t kLax = 0x10;
constexpr std::uint8_t kCblocks = 0x20;
constexpr std::uint8_t kPlainWrite = 0x40;
constexpr std::uint8_t kFewCirclePoints = 0x80;
constexpr std::size_t kMaxRecords = std::size_t{1} << 20U;
constexpr std::size_t kMaxTopWrites = 2;
constexpr std::uint64_t kMaxWriteInstances = std::uint64_t{1} << 16U;
constexpr oasis::ReadLimits kFuzzLimits{std::uint64_t{8} << 20U, std::uint64_t{1} << 16U};
constexpr const char* kInputPath = "/in.oas";

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

/// The file as stored: plain, or gzip data.
std::vector<std::byte> stored_bytes(std::uint8_t mode, std::string_view rest) {
  std::vector<std::byte> bytes = bytes_of(rest);
  if ((mode & kGzip) == 0 || gds::is_gzip(bytes)) {
    return bytes;
  }
  return compress(bytes);
}

std::unique_ptr<gds::ByteSource> open(const std::vector<std::byte>& stored) {
  if (!gds::is_gzip(stored)) {
    return std::make_unique<gds::MemorySource>(stored);
  }
  return gds::make_gzip_source(stored);
}

/// The instances of one decoded record, saturating.
std::uint64_t instances_of(const oasis::Record& record) {
  if (!record.element || !record.element->repetition) {
    return 1;
  }
  return oasis::instance_count(*record.element->repetition);
}

/// Every record to END (or the first error), with the name tables kept;
/// the element instances counted, saturating.
std::uint64_t decode(std::uint8_t mode, const std::vector<std::byte>& stored) {
  std::uint64_t instances = 0;
  auto source = open(stored);
  if (!source) {
    return instances;
  }
  oasis::DecoderOptions options;
  options.check_strict = (mode & kLax) == 0;
  oasis::Decoder decoder{*source, options};
  for (std::size_t n = 0; n < kMaxRecords; ++n) {
    auto record = decoder.next();
    if (!record.ok() || !record.value()) {
      break;
    }
    const std::uint64_t more = instances_of(*record.value());
    instances = more > kMaxWriteInstances - std::min(instances, kMaxWriteInstances) ? kMaxWriteInstances + 1
                                                                                   : instances + more;
  }
  (void)decoder.names();
  return instances;
}

/// The decode, validation and content-hash scans; the instances counted.
std::uint64_t scan(std::uint8_t mode, const std::vector<std::byte>& stored) {
  const std::uint64_t instances = decode(mode, stored);
  if (auto source = open(stored)) {
    (void)oasis::validate(*source);
  }
  if (auto source = open(stored)) {
    (void)oasis::content_stamps(*source);
  }
  return instances;
}

/// An OASIS write of each top cell (at most kMaxTopWrites) into memory.
void write_oasis(hefesto::fuzz::MaskFuzzSession& session, std::uint8_t mode) {
  if (!session.write_bounded()) {
    return;
  }
  oasis::OasisWriteOptions options;
  options.settings = session.editor.mask.oasis;
  options.settings.compress = (mode & kCblocks) != 0 ? 1 : 0;
  if ((mode & kPlainWrite) != 0) {
    options.settings.repetitions = false;
    options.settings.strict = false;
    options.settings.layernames = false;
  }
  options.prepare = oasis::to_gds_write_options(options.settings, session.editor.mask.stream_read);
  options.prepare.typed_name = "out";
  options.prepare.files = &session.files;
  options.prepare.time_zone = "UTC0";
  options.prepare.now = 0;
  for (hefesto::db::CellDef* top : session.top_cells(kMaxTopWrites)) {
    (void)oasis::write(session.editor, *top, "/out.oas", options);
  }
}

void read_then_write(std::uint8_t mode, const std::vector<std::byte>& stored, bool write) {
  hefesto::fuzz::MaskFuzzSession session{(mode & kCompatOff) == 0};
  if (!session.files.write_atomic(kInputPath, std::string_view{reinterpret_cast<const char*>(stored.data()),  // NOLINT
                                                               stored.size()})
           .ok()) {
    return;
  }
  oasis::OasisReadOptions options;
  options.stream_read = session.editor.mask.stream_read;
  options.settings = session.editor.mask.oasis;
  options.stream_read.readonly = (mode & kReadOnly) != 0;
  options.stream_read.flatten = (mode & kFlatten) != 0;
  options.stream_read.flatten_limit = 10;
  options.settings.circlepoints = (mode & kFewCirclePoints) != 0 ? 8 : 64;
  options.files = &session.files;
  options.oasis_file = kInputPath;
  options.typed_name = "in";
  (void)oasis::read(session.editor, kInputPath, options, kFuzzLimits);
  if (!write) {
    return;
  }
  session.editor.mask.stream_read.readonly = options.stream_read.readonly;
  write_oasis(session, mode);
  session.write_tops(kMaxTopWrites, false, 0);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, rest] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  const std::vector<std::byte> stored = stored_bytes(mode, rest);
  const std::uint64_t instances = scan(mode, stored);
  read_then_write(mode, stored, instances <= kMaxWriteInstances);
  return 0;
}
