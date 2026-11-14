// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Gzip streams of the GDS stream layer (capabilities gds-read "Read command
// and file lookup", gds-write "Compressed output";
// openspec/changes/m5-mask-gds-cif/design.md D3, D10; Q-MASK-223).
//
// The source inflates gzip data as zlib's gzread does: concatenated members
// are read in turn, and bytes after the last member that do not start
// another member are ignored. Its positions count decompressed bytes. A
// seek back restores the nearest inflate checkpoint at or before the target
// (one every few MiB of output, at most kMaxCheckpoints, so memory stays
// bounded for any input) and inflates forward from there; it never seeks
// in the compressed bytes. Inflating streams through a fixed 64 KiB buffer,
// so a highly compressed input never allocates its decompressed size.
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

/// A source over the gzip data `compressed`.
[[nodiscard]] std::unique_ptr<ByteSource> make_gzip_source(std::vector<std::byte> compressed);

/// A sink that compresses into `inner` as one gzip member at deflate level
/// `level` (1-9; GDS-STREAM-005 otherwise). finish() ends the member and
/// finishes `inner`.
[[nodiscard]] diag::Outcome<std::unique_ptr<ByteSink>> make_gzip_sink(std::unique_ptr<ByteSink> inner, int level);

}  // namespace hefesto::gds
