// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Gzip streams of the GDS stream layer (capabilities gds-read "Read command
// and file lookup", gds-write "Compressed output";
// openspec/changes/archive/2026-11-24-m5-mask-gds-cif/design.md D3, D10; Q-MASK-223).
//
// The source inflates gzip data as zlib's gzread does: concatenated members
// are read in turn, and bytes after the last member that do not start
// another member are ignored. Its positions count decompressed bytes. A
// seek back restores the nearest inflate checkpoint at or before the target
// (one every few MiB of output, at most kMaxCheckpoints, so memory stays
// bounded for any input) and inflates forward from there; it never seeks
// in the compressed bytes. Inflating streams through a fixed 64 KiB buffer,
// so a highly compressed input never allocates its decompressed size.
// The decompressed stream is capped (inflate_cap: 256 times the compressed
// size, at least 1 GiB), so a gzip bomb ends the read with GDS-STREAM-006
// instead of keeping it busy for hours (hardening, design R9; Magic has no
// cap).
//
// The sink writes one gzip member at deflate level 1-9 to an inner sink.
// The compressed bytes depend on zlib and are informative; conformance
// compares the decompressed bytes (Q-MASK-223).
//
// Owner: GDSR (design D2).

#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "hefesto/gds/stream/io.hpp"

namespace hefesto::gds {

/// True if `bytes` start with the gzip magic number (1f 8b).
[[nodiscard]] bool is_gzip(std::span<const std::byte> bytes) noexcept;

/// The most inflate checkpoints a gzip source keeps.
inline constexpr std::size_t kMaxCheckpoints = 256;

/// The input bytes handed to one inflate call (zlib counts in 32 bits).
inline constexpr std::size_t kMaxGzipFeed = std::size_t{1} << 30U;

/// The inflate size cap: a gzip source yields at most
/// max(kMinInflateCap, kMaxInflateRatio * compressed size) decompressed
/// bytes; reading past it fails with GDS-STREAM-006. Deflate compresses at
/// most about 1032:1, and GDS data compresses about 3:1 to 20:1.
inline constexpr std::uint64_t kMinInflateCap = std::uint64_t{1} << 30U;
inline constexpr std::uint64_t kMaxInflateRatio = 256;
[[nodiscard]] std::uint64_t inflate_cap(std::uint64_t compressed_size) noexcept;

/// A source over the gzip data `compressed`, inflated `max_feed` input
/// bytes at a time (smaller in tests, to reach the window edges), yielding
/// at most `max_output` decompressed bytes (0: inflate_cap of the
/// compressed size).
[[nodiscard]] std::unique_ptr<ByteSource> make_gzip_source(std::vector<std::byte> compressed,
                                                           std::size_t max_feed = kMaxGzipFeed,
                                                           std::uint64_t max_output = 0);

/// A sink that compresses into `inner` as one gzip member at deflate level
/// `level` (1-9; GDS-STREAM-005 otherwise). finish() ends the member and
/// finishes `inner`.
[[nodiscard]] diag::Outcome<std::unique_ptr<ByteSink>> make_gzip_sink(std::unique_ptr<ByteSink> inner, int level);

}  // namespace hefesto::gds
