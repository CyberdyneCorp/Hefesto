// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDSII record reader and writer (see hefesto/gds/stream/records.hpp).
// Ported from Magic 8.3.694 calma/calmaInt.h (READRH, UNREADRH),
// calma/CalmaRdio.c (calmaSkipBytes) and calma/CalmaWrite.c (calmaOutRH,
// calmaOutI2, calmaOutI4, calmaOutR8); Magic is (c) the Regents of the
// University of California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).
//
// Framing (gds-read "Record framing and primitive data types"): end of
// file inside a header is the end of the records; a record with L < 4 has
// no payload; a payload cut off by the end of the stream is an error
// (GDS-STREAM-001). The payload is at most 65531 bytes, so one record
// never allocates more than that. Owner: GDSR.

#include "hefesto/gds/stream/records.hpp"

#include <array>
#include <string>
#include <utility>

#include "hefesto/gds/stream/encode.hpp"

namespace hefesto::gds {

namespace {

constexpr std::size_t kHeaderLength = 4;
constexpr std::size_t kMaxPayload = 0xffff - kHeaderLength;
constexpr std::size_t kWriteBuffer = 64 * 1024;

/// Reads until `out` is full or the stream ends; returns the count.
diag::Outcome<std::size_t> read_fully(ByteSource& source, std::span<std::byte> out) {
  std::size_t done = 0;
  while (done < out.size()) {
    auto got = source.read(out.subspan(done));
    if (!got.ok()) {
      return got.error();
    }
    if (got.value() == 0) {
      break;
    }
    done += got.value();
  }
  return done;
}

diag::Diagnostic truncated(std::uint64_t offset, std::size_t want, std::size_t got) {
  return diag::make("GDS-STREAM-001", "Unexpected end of file inside the record at byte position " +
                                          std::to_string(offset) + ": expected " + std::to_string(want) +
                                          " data bytes, found " + std::to_string(got) + ".");
}

void put_u16(std::vector<std::byte>& out, std::uint32_t v) {
  out.push_back(static_cast<std::byte>((v >> 8U) & 0xffU));
  out.push_back(static_cast<std::byte>(v & 0xffU));
}

void put_u32(std::vector<std::byte>& out, std::uint32_t v) {
  put_u16(out, v >> 16U);
  put_u16(out, v & 0xffffU);
}

}  // namespace

diag::Outcome<std::optional<Record>> RecordReader::peek() {
  if (!lookahead_) {
    auto record = next();
    if (!record.ok() || !record.value()) {
      return record;
    }
    lookahead_ = std::move(*record.value());
  }
  return std::optional<Record>{*lookahead_};
}

diag::Outcome<std::optional<Record>> RecordReader::next() {
  if (lookahead_) {
    std::optional<Record> record = std::move(lookahead_);
    lookahead_.reset();
    return record;
  }
  Record record;
  record.offset = source_.position();
  std::array<std::byte, kHeaderLength> header{};
  auto got = read_fully(source_, header);
  if (!got.ok()) {
    return got.error();
  }
  if (got.value() < header.size()) {
    return std::optional<Record>{};  // end of file inside a header: no more records
  }
  const std::size_t length = (std::to_integer<std::size_t>(header[0]) << 8U) | std::to_integer<std::size_t>(header[1]);
  record.type = std::to_integer<std::uint8_t>(header[2]);
  record.data_type = std::to_integer<std::uint8_t>(header[3]);
  if (length > kHeaderLength) {
    record.data.resize(length - kHeaderLength);
    auto data = read_fully(source_, record.data);
    if (!data.ok()) {
      return data.error();
    }
    if (data.value() < record.data.size()) {
      return truncated(record.offset, record.data.size(), data.value());
    }
  }
  return std::optional<Record>{std::move(record)};
}

void RecordReader::unread(Record record) {
  lookahead_ = std::move(record);
}

std::uint64_t RecordReader::position() const noexcept {
  return lookahead_ ? lookahead_->offset : source_.position();
}

diag::Status RecordReader::seek(std::uint64_t offset) {
  lookahead_.reset();
  return source_.seek(offset);
}

diag::Status RecordWriter::write(RecordType type, DataType data_type, std::span<const std::byte> payload) {
  if (payload.size() > kMaxPayload) {
    return diag::make("GDS-STREAM-004", "A GDS record of type " + std::to_string(static_cast<int>(type)) + " needs " +
                                            std::to_string(payload.size()) +
                                            " data bytes; a record holds at most 65531.");
  }
  if (buffer_.size() + kHeaderLength + payload.size() > kWriteBuffer) {
    if (auto status = flush(); !status.ok()) {
      return status;
    }
  }
  put_u16(buffer_, static_cast<std::uint32_t>(payload.size() + kHeaderLength));
  buffer_.push_back(static_cast<std::byte>(type));
  buffer_.push_back(static_cast<std::byte>(data_type));
  buffer_.insert(buffer_.end(), payload.begin(), payload.end());
  return diag::Done{};
}

diag::Status RecordWriter::write_empty(RecordType type) {
  return write(type, DataType::none, {});
}

diag::Status RecordWriter::write_int2(RecordType type, std::span<const std::int16_t> values) {
  std::vector<std::byte> payload;
  payload.reserve(values.size() * 2);
  for (const std::int16_t v : values) {
    put_u16(payload, static_cast<std::uint16_t>(v));
  }
  return write(type, DataType::int2, payload);
}

diag::Status RecordWriter::write_int4(RecordType type, std::span<const std::int32_t> values) {
  std::vector<std::byte> payload;
  payload.reserve(values.size() * 4);
  for (const std::int32_t v : values) {
    put_u32(payload, static_cast<std::uint32_t>(v));
  }
  return write(type, DataType::int4, payload);
}

diag::Status RecordWriter::write_real8(RecordType type, std::span<const double> values) {
  std::vector<std::byte> payload;
  payload.reserve(values.size() * 8);
  for (const double v : values) {
    const auto bytes = encode_real8(v);
    payload.insert(payload.end(), bytes.begin(), bytes.end());
  }
  return write(type, DataType::real8, payload);
}

diag::Status RecordWriter::write_string(RecordType type, std::string_view text) {
  std::vector<std::byte> payload;
  payload.reserve(text.size() + 1);
  for (const char c : text) {
    payload.push_back(static_cast<std::byte>(c));
  }
  if ((payload.size() & 1U) != 0) {
    payload.push_back(std::byte{0});
  }
  return write(type, DataType::string, payload);
}

diag::Status RecordWriter::flush() {
  if (buffer_.empty()) {
    return diag::Done{};
  }
  auto status = sink_.write(buffer_);
  buffer_.clear();
  return status;
}

}  // namespace hefesto::gds
