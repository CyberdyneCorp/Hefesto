// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The decoder's record stream (capability oasis-format "CBLOCK compression",
// "Validation signature"; design D11): the file bytes, buffered from a
// gds::ByteSource, with the running CRC32 and CHECKSUM32 of every file byte
// consumed, and at most one inflated CBLOCK in front of them. A record that
// starts in a CBLOCK continues into the file bytes after it (Q-OASIS-9).
//
// Owner: OASFMT (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "internal.hpp"

namespace hefesto::gds {
class ByteSource;
}

namespace hefesto::oasis::detail {

class RecordInput final : public Input {
 public:
  explicit RecordInput(gds::ByteSource& source);

  std::uint8_t get() override;

  /// Whether inflated CBLOCK bytes remain.
  [[nodiscard]] bool in_cblock() const noexcept { return cblock_next_ < cblock_.size(); }
  /// The file offset of the next file byte.
  [[nodiscard]] std::uint64_t file_position() const noexcept { return buffer_start_ + next_; }
  /// The file offset of the CBLOCK being read, and the next byte's position in it.
  [[nodiscard]] std::uint64_t cblock_offset() const noexcept { return cblock_offset_; }
  [[nodiscard]] std::uint64_t cblock_position() const noexcept { return cblock_next_; }
  /// Whether no byte remains (no CBLOCK bytes and the end of the file).
  [[nodiscard]] bool at_end();

  /// Reads `count` file bytes (never from the CBLOCK), growing the result
  /// as the bytes arrive.
  [[nodiscard]] std::vector<std::byte> read_file_bytes(std::uint64_t count);
  /// Puts inflated bytes in front of the file bytes; `offset` is the
  /// CBLOCK's file offset.
  void push_cblock(std::vector<std::byte> bytes, std::uint64_t offset);

  /// The CRC32 and CHECKSUM32 of every file byte consumed so far.
  [[nodiscard]] std::uint32_t crc32();
  [[nodiscard]] std::uint32_t checksum32();

 private:
  /// Refills the buffer; false at the end of the file.
  bool fill();
  /// Feeds the consumed, not yet summed buffer bytes to the signatures.
  void feed();

  gds::ByteSource* source_;
  std::vector<std::byte> buffer_;
  std::size_t next_ = 0;
  std::size_t end_ = 0;
  std::uint64_t buffer_start_ = 0;
  std::size_t fed_ = 0;
  std::uint32_t crc_ = 0;
  std::uint32_t sum_ = 0;
  bool eof_ = false;
  std::vector<std::byte> cblock_;
  std::size_t cblock_next_ = 0;
  std::uint64_t cblock_offset_ = 0;
};

}  // namespace hefesto::oasis::detail
