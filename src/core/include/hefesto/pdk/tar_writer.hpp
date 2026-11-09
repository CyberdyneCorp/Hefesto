// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Writing tar archives and stored Zstandard frames for offline PDK packs
// (capability hefesto-pdk-manager "Offline PDK packs"). TarWriter writes
// POSIX ustar entries (a pax `path` record for names over 100 bytes);
// StoredZstdWriter frames bytes as a valid Zstandard frame of raw
// (uncompressed) blocks, which every Zstandard decoder reads. The core links
// only the Zstandard decoder, so rebuilt pack assets are stored, not
// compressed.
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <array>
#include <cstdint>
#include <ostream>
#include <streambuf>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::pdk {

class TarWriter {
 public:
  explicit TarWriter(std::ostream& out) : out_(out) {}

  /// A regular file entry with the contents of `source` (mode 0644, mtime 0).
  [[nodiscard]] diag::Status add_file(std::string_view name, const platform::Path& source);
  [[nodiscard]] diag::Status add_bytes(std::string_view name, std::string_view data);
  /// The two zero blocks that end the archive.
  [[nodiscard]] diag::Status finish();

 private:
  [[nodiscard]] diag::Status header(std::string_view name, char type, std::uint64_t size);
  [[nodiscard]] diag::Status begin_entry(std::string_view name, std::uint64_t size);
  void pad(std::uint64_t size);

  std::ostream& out_;
};

/// A streambuf that writes a Zstandard frame of raw blocks to `out`: call
/// finish() once after the last byte (it writes the last block).
class StoredZstdWriter final : public std::streambuf {
 public:
  explicit StoredZstdWriter(std::ostream& out);
  StoredZstdWriter(const StoredZstdWriter&) = delete;
  StoredZstdWriter& operator=(const StoredZstdWriter&) = delete;
  ~StoredZstdWriter() override = default;

  void finish();

 protected:
  int_type overflow(int_type ch) override;

 private:
  void flush_block(bool last);

  static constexpr std::size_t kBlockSize = 128 * 1024;  // the frame's window size
  std::ostream& out_;
  std::array<char, kBlockSize> buffer_{};
  bool finished_ = false;
};

}  // namespace hefesto::pdk
