// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The decoder's record stream (see input.hpp). Owner: OASFMT (design D2).

#include "input.hpp"

#include <algorithm>
#include <span>
#include <utility>

#include "hefesto/gds/stream/io.hpp"
#include "hefesto/oasis/codec/validate.hpp"

namespace hefesto::oasis::detail {

namespace {

constexpr std::size_t kBufferSize = 64 * 1024;

}  // namespace

RecordInput::RecordInput(gds::ByteSource& source)
    : source_(&source), buffer_(kBufferSize), buffer_start_(source.position()) {}

void RecordInput::feed() {
  const auto consumed = std::span<const std::byte>{buffer_}.subspan(fed_, next_ - fed_);
  crc_ = update_signature(ValidationScheme::crc32, crc_, consumed);
  sum_ = update_signature(ValidationScheme::checksum32, sum_, consumed);
  fed_ = next_;
}

bool RecordInput::fill() {
  if (eof_) {
    return false;
  }
  feed();
  buffer_start_ += end_;
  next_ = 0;
  end_ = 0;
  fed_ = 0;
  auto got = source_->read(buffer_);
  if (!got.ok()) {
    throw CodecError{got.error()};
  }
  end_ = got.value();
  eof_ = end_ == 0;
  return !eof_;
}

std::uint8_t RecordInput::get() {
  if (in_cblock()) {
    return std::to_integer<std::uint8_t>(cblock_[cblock_next_++]);
  }
  if (next_ == end_ && !fill()) {
    fail("OASIS-CODEC-002", "unexpected end of file");
  }
  return std::to_integer<std::uint8_t>(buffer_[next_++]);
}

bool RecordInput::at_end() {
  return !in_cblock() && next_ == end_ && !fill();
}

std::vector<std::byte> RecordInput::read_file_bytes(std::uint64_t count) {
  std::vector<std::byte> out;
  while (count > 0) {
    if (next_ == end_ && !fill()) {
      fail("OASIS-CODEC-002", "unexpected end of file");
    }
    const std::size_t take = static_cast<std::size_t>(std::min<std::uint64_t>(count, end_ - next_));
    out.insert(out.end(), buffer_.begin() + static_cast<std::ptrdiff_t>(next_),
               buffer_.begin() + static_cast<std::ptrdiff_t>(next_ + take));
    next_ += take;
    count -= take;
  }
  return out;
}

void RecordInput::push_cblock(std::vector<std::byte> bytes, std::uint64_t offset) {
  cblock_ = std::move(bytes);
  cblock_next_ = 0;
  cblock_offset_ = offset;
}

std::uint32_t RecordInput::crc32() {
  feed();
  return crc_;
}

std::uint32_t RecordInput::checksum32() {
  feed();
  return sum_;
}

}  // namespace hefesto::oasis::detail
