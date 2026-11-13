// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (openspec/changes/m5-mask-gds-cif/design.md D3): reading
// and writing records fail with CORE-NOTIMPL-005 until GDSR ports
// CalmaRdio.c and the record writer (tasks.md section 9). Owner: GDSR.

#include "hefesto/gds/stream/records.hpp"

#include <utility>

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::gds {

diag::Outcome<std::optional<Record>> RecordReader::peek() {
  return cmd::not_implemented_diagnostic("gds::RecordReader::peek");
}

diag::Outcome<std::optional<Record>> RecordReader::next() {
  return cmd::not_implemented_diagnostic("gds::RecordReader::next");
}

void RecordReader::unread(Record record) {
  lookahead_ = std::move(record);
}

std::uint64_t RecordReader::position() const noexcept {
  return lookahead_ ? lookahead_->offset : 0;
}

diag::Status RecordReader::seek(std::uint64_t /*offset*/) {
  // The stubs keep the source referenced: GCC 11 ignores [[maybe_unused]] on members (-Werror=attributes)
  // and Clang flags an unread private field.
  static_cast<void>(source_);
  return cmd::not_implemented_status("gds::RecordReader::seek");
}

diag::Status RecordWriter::write(RecordType /*type*/, DataType /*data_type*/, std::span<const std::byte> /*payload*/) {
  return cmd::not_implemented_status("gds::RecordWriter::write");
}

diag::Status RecordWriter::write_empty(RecordType type) {
  return write(type, DataType::none, {});
}

diag::Status RecordWriter::write_int2(RecordType type, std::span<const std::int16_t> /*values*/) {
  return write(type, DataType::int2, {});
}

diag::Status RecordWriter::write_int4(RecordType type, std::span<const std::int32_t> /*values*/) {
  return write(type, DataType::int4, {});
}

diag::Status RecordWriter::write_real8(RecordType type, std::span<const double> /*values*/) {
  return write(type, DataType::real8, {});
}

diag::Status RecordWriter::write_string(RecordType type, std::string_view /*text*/) {
  return write(type, DataType::string, {});
}

diag::Status RecordWriter::flush() {
  static_cast<void>(sink_);  // referenced until GDSR writes to it (see RecordReader::seek)
  buffer_.clear();
  return cmd::not_implemented_status("gds::RecordWriter::flush");
}

}  // namespace hefesto::gds
