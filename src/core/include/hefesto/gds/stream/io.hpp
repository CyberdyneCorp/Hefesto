// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Byte sources and sinks of the GDS stream layer (capabilities gds-read,
// gds-write; openspec/changes/m5-mask-gds-cif/design.md D4 "GDS", D10):
// plain files, gzip files and memory. Positions are offsets in the
// decompressed stream, so GDS_START/GDS_END and the Q-MASK-126 seek count
// decompressed bytes for `.gds` and `.gds.gz` alike. Seeking back in a gzip
// source re-inflates from the start or a checkpoint. Inflate reuses the
// cell/mag_input.cpp approach (inflateInit2(15 + 32), concatenated
// members); compressed output is one gzip member at deflate level z
// (Q-MASK-223). All file access goes through platform::FileSystem.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GDSR fills
// in the implementation in src/core/src/gds/stream/ (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::gds {

class ByteSource {
 public:
  ByteSource() = default;
  ByteSource(const ByteSource&) = delete;
  ByteSource& operator=(const ByteSource&) = delete;
  virtual ~ByteSource() = default;

  /// Reads up to out.size() bytes; returns the count (0 at end of stream).
  [[nodiscard]] virtual diag::Outcome<std::size_t> read(std::span<std::byte> out) = 0;
  /// The offset of the next byte in the decompressed stream.
  [[nodiscard]] virtual std::uint64_t position() const noexcept = 0;
  /// Moves to `offset` in the decompressed stream.
  [[nodiscard]] virtual diag::Status seek(std::uint64_t offset) = 0;
};

class ByteSink {
 public:
  ByteSink() = default;
  ByteSink(const ByteSink&) = delete;
  ByteSink& operator=(const ByteSink&) = delete;
  virtual ~ByteSink() = default;

  /// Appends `bytes`.
  [[nodiscard]] virtual diag::Status write(std::span<const std::byte> bytes) = 0;
  /// Bytes written so far (decompressed).
  [[nodiscard]] virtual std::uint64_t position() const noexcept = 0;
  /// Flushes and closes the output; a sink is unusable afterwards.
  [[nodiscard]] virtual diag::Status finish() = 0;
};

/// A source over `bytes` (tests, fuzzing, verbatim copy).
class MemorySource final : public ByteSource {
 public:
  explicit MemorySource(std::vector<std::byte> bytes) : bytes_(std::move(bytes)) {}

  [[nodiscard]] diag::Outcome<std::size_t> read(std::span<std::byte> out) override;
  [[nodiscard]] std::uint64_t position() const noexcept override { return offset_; }
  [[nodiscard]] diag::Status seek(std::uint64_t offset) override;

 private:
  std::vector<std::byte> bytes_;
  std::uint64_t offset_ = 0;
};

/// A sink into memory (tests, fuzzing).
class MemorySink final : public ByteSink {
 public:
  [[nodiscard]] diag::Status write(std::span<const std::byte> bytes) override;
  [[nodiscard]] std::uint64_t position() const noexcept override { return bytes_.size(); }
  [[nodiscard]] diag::Status finish() override { return diag::Done{}; }

  [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }

 private:
  std::vector<std::byte> bytes_;
};

/// Opens `path` for reading; a gzip file (by its magic bytes) is inflated.
[[nodiscard]] diag::Outcome<std::unique_ptr<ByteSource>> open_source(platform::FileSystem& fs,
                                                                      const platform::Path& path);

/// Opens `path` for writing; `compress_level` 0 writes plain bytes, 1-9 one
/// gzip member at that deflate level.
[[nodiscard]] diag::Outcome<std::unique_ptr<ByteSink>> open_sink(platform::FileSystem& fs, const platform::Path& path,
                                                                  int compress_level);

}  // namespace hefesto::gds
