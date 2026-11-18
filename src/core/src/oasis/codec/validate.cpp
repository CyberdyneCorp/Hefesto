// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Validation signatures and CBLOCK compression (capability oasis-format
// "Validation signature", "CBLOCK compression"; oasis-write "CBLOCK
// compression"; design D3, D11, Q-OASIS-2, Q-OASIS-64; security review
// M6-S4):
// - CRC32 is zlib's crc32, CHECKSUM32 the byte sum modulo 2^32, over the
//   file from the first magic byte through the validation-scheme byte;
// - validate() streams the file and keeps only its last 256 bytes, where
//   the END record is;
// - inflate_cblock grows its output as data arrives and stops at the
//   declared size or the cap, so a small CBLOCK never allocates its
//   declared size up front;
// - deflate_cblock uses the pinned parameters: raw DEFLATE (window bits
//   -15), memory level 8, default strategy, one finishing flush.
//
// Owner: OASFMT (design D2).

#include "hefesto/oasis/codec/validate.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <vector>

#include "hefesto/gds/stream/gzip.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/oasis/codec/cblock.hpp"
#include "hefesto/oasis/codec/primitives.hpp"
#include "internal.hpp"

namespace hefesto::oasis {

namespace {

constexpr std::size_t kEndLength = 256;
constexpr std::size_t kChunk = 64 * 1024;
constexpr int kRawWindow = -15;
constexpr int kMemoryLevel = 8;
constexpr std::array<unsigned char, 13> kMagic = {'%', 'S', 'E', 'M', 'I', '-', 'O', 'A', 'S', 'I', 'S', '\r', '\n'};

/// zlib takes 32-bit lengths; feed it in pieces.
std::uint32_t crc(std::uint32_t previous, std::span<const std::byte> bytes) noexcept {
  uLong value = previous;
  while (!bytes.empty()) {
    const std::size_t take = std::min<std::size_t>(bytes.size(), std::numeric_limits<uInt>::max());
    value = ::crc32(value, reinterpret_cast<const Bytef*>(bytes.data()), static_cast<uInt>(take));
    bytes = bytes.subspan(take);
  }
  return static_cast<std::uint32_t>(value);
}

std::uint32_t sum(std::uint32_t previous, std::span<const std::byte> bytes) noexcept {
  for (const std::byte b : bytes) {
    previous += std::to_integer<std::uint32_t>(b);
  }
  return previous;
}

/// The file being validated: the signatures of every byte but the last 256,
/// which are kept.
struct Stream {
  std::vector<std::byte> head;  // the first chunk, for START
  std::vector<std::byte> tail;  // the last (up to) 256 bytes
  std::uint32_t crc = 0;
  std::uint32_t sum = 0;

  void add(std::span<const std::byte> bytes) {
    tail.insert(tail.end(), bytes.begin(), bytes.end());
    if (tail.size() > kEndLength) {
      const auto spill = std::span<const std::byte>{tail}.first(tail.size() - kEndLength);
      crc = oasis::crc(crc, spill);
      sum = oasis::sum(sum, spill);
      tail.erase(tail.begin(), tail.begin() + static_cast<std::ptrdiff_t>(spill.size()));
    }
  }
};

diag::Outcome<Stream> read_stream(gds::ByteSource& source) {
  if (auto sought = source.seek(0); !sought.ok()) {
    return sought.error();
  }
  Stream stream;
  std::vector<std::byte> chunk(kChunk);
  for (;;) {
    auto got = source.read(chunk);
    if (!got.ok()) {
      return got.error();
    }
    if (got.value() == 0) {
      return stream;
    }
    const auto bytes = std::span<const std::byte>{chunk}.first(got.value());
    if (stream.head.size() < kChunk) {
      stream.head.insert(stream.head.end(), bytes.begin(), bytes.end());
    }
    stream.add(bytes);
  }
}

/// Whether START (after the magic) puts the table offsets in END.
bool offsets_at_end(std::span<const std::byte> head) {
  if (head.size() < kMagic.size() ||
      !std::equal(kMagic.begin(), kMagic.end(), head.begin(),
                  [](unsigned char m, std::byte b) { return std::byte{m} == b; })) {
    detail::fail("OASIS-CODEC-001", "not an OASIS file");
  }
  ByteCursor cursor{head, kMagic.size()};
  detail::CursorInput in{cursor};
  if (detail::read_unsigned(in) != 1) {
    detail::fail("OASIS-CODEC-004", "the START record does not follow the magic bytes");
  }
  (void)detail::read_string(in);
  (void)detail::read_real(in);
  const bool at_end = detail::read_unsigned(in) != 0;
  if (!at_end) {
    for (int i = 0; i < 12; ++i) {
      (void)detail::read_unsigned(in);
    }
  }
  return at_end;
}

/// The END record in the last 256 bytes: the scheme and where the
/// covered bytes end in `tail`.
struct EndFields {
  std::uint64_t scheme = 0;
  std::size_t covered = 0;
  std::uint32_t stored = 0;
};

EndFields parse_end(std::span<const std::byte> tail, bool tables_at_end) {
  if (tail.size() != kEndLength || tail[0] != std::byte{2}) {
    detail::fail("OASIS-CODEC-002", "no END record in the last 256 bytes");
  }
  ByteCursor cursor{tail, 1};
  detail::CursorInput in{cursor};
  for (int i = 0; tables_at_end && i < 12; ++i) {
    (void)detail::read_unsigned(in);
  }
  detail::skip_string(in);
  EndFields end{detail::read_unsigned(in), cursor.offset};
  if (end.scheme > 2) {
    detail::fail("OASIS-CODEC-014", "unknown validation scheme " + std::to_string(end.scheme));
  }
  const std::size_t signature_bytes = end.scheme == 0 ? 0 : 4;
  if (cursor.offset + signature_bytes != tail.size()) {
    detail::fail("OASIS-CODEC-002", "no END record in the last 256 bytes");
  }
  for (std::size_t i = 0; i < signature_bytes; ++i) {
    end.stored |= std::to_integer<std::uint32_t>(tail[end.covered + i]) << (8U * i);
  }
  return end;
}

/// Raw inflate of `compressed`, which must hold exactly one DEFLATE stream
/// of `expected` bytes; at most one chunk past `expected` is inflated.
diag::Outcome<std::vector<std::byte>> inflate_raw(std::span<const std::byte> compressed, std::uint64_t expected) {
  z_stream zs{};
  if (inflateInit2(&zs, kRawWindow) != Z_OK) {
    return diag::make("OASIS-CODEC-023", "zlib could not start inflating a CBLOCK");
  }
  std::vector<std::byte> out;
  std::array<std::byte, kChunk> piece{};
  zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));  // zlib's API is not const
  zs.avail_in = static_cast<uInt>(compressed.size());
  int rc = Z_OK;
  while (rc == Z_OK && out.size() <= expected) {
    zs.next_out = reinterpret_cast<Bytef*>(piece.data());
    zs.avail_out = static_cast<uInt>(piece.size());
    rc = inflate(&zs, Z_NO_FLUSH);
    out.insert(out.end(), piece.begin(), piece.begin() + static_cast<std::ptrdiff_t>(piece.size() - zs.avail_out));
  }
  const bool ended = rc == Z_STREAM_END && zs.avail_in == 0;
  inflateEnd(&zs);
  if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR) {
    return diag::make("OASIS-CODEC-023", "damaged DEFLATE data in a CBLOCK");
  }
  if (!ended || out.size() != expected) {
    return diag::make("OASIS-CODEC-023", "CBLOCK size mismatch");
  }
  return out;
}

}  // namespace

std::uint32_t update_signature(ValidationScheme scheme, std::uint32_t previous,
                               std::span<const std::byte> bytes) noexcept {
  switch (scheme) {
    case ValidationScheme::crc32:
      return crc(previous, bytes);
    case ValidationScheme::checksum32:
      return sum(previous, bytes);
    case ValidationScheme::none:
      break;
  }
  return 0;
}

std::uint32_t signature(ValidationScheme scheme, std::span<const std::byte> bytes) noexcept {
  return update_signature(scheme, 0, bytes);
}

diag::Outcome<ValidationResult> validate(gds::ByteSource& source) {
  auto stream = read_stream(source);
  if (!stream.ok()) {
    return stream.error();
  }
  Stream& s = stream.value();
  try {
    const EndFields end = parse_end(s.tail, offsets_at_end(s.head));
    const auto covered = std::span<const std::byte>{s.tail}.first(end.covered);
    ValidationResult result{static_cast<ValidationScheme>(end.scheme), end.stored, 0};
    const std::uint32_t prefix = result.scheme == ValidationScheme::crc32 ? s.crc : s.sum;
    result.computed = update_signature(result.scheme, prefix, covered);
    return result;
  } catch (const detail::CodecError& error) {
    return error.diagnostic;
  }
}

diag::Outcome<std::vector<std::byte>> inflate_cblock(std::span<const std::byte> compressed,
                                                     std::uint64_t uncompressed_size, std::uint64_t cap) {
  const std::uint64_t limit = cap != 0 ? cap : gds::inflate_cap(compressed.size());
  if (uncompressed_size > limit) {
    return diag::make("OASIS-CODEC-023", "CBLOCK of " + std::to_string(uncompressed_size) +
                                             " bytes is over the cap of " + std::to_string(limit) + " bytes");
  }
  if (compressed.size() > std::numeric_limits<uInt>::max()) {
    return diag::make("OASIS-CODEC-023", "CBLOCK of " + std::to_string(compressed.size()) +
                                             " compressed bytes is too large");
  }
  return inflate_raw(compressed, uncompressed_size);
}

diag::Outcome<std::vector<std::byte>> deflate_cblock(std::span<const std::byte> raw, int level) {
  if (level < 1 || level > 9) {
    return diag::make("OASIS-CODEC-027", "compression level " + std::to_string(level) + " is not 1 to 9");
  }
  if (raw.size() > std::numeric_limits<uInt>::max() / 2) {
    return diag::make("OASIS-CODEC-027", "CBLOCK chunk too large to compress");
  }
  z_stream zs{};
  if (deflateInit2(&zs, level, Z_DEFLATED, kRawWindow, kMemoryLevel, Z_DEFAULT_STRATEGY) != Z_OK) {
    return diag::make("OASIS-CODEC-027", "zlib could not start compressing a CBLOCK");
  }
  std::vector<std::byte> out(deflateBound(&zs, uLong{raw.size()}));
  zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(raw.data()));  // zlib's API is not const
  zs.avail_in = static_cast<uInt>(raw.size());
  zs.next_out = reinterpret_cast<Bytef*>(out.data());
  zs.avail_out = static_cast<uInt>(out.size());
  const int rc = deflate(&zs, Z_FINISH);
  out.resize(out.size() - zs.avail_out);
  deflateEnd(&zs);
  if (rc != Z_STREAM_END) {
    return diag::make("OASIS-CODEC-027", "zlib failed while compressing a CBLOCK");
  }
  return out;
}

}  // namespace hefesto::oasis
