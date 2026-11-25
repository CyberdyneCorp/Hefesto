// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Format recognition on open (capability oasis-command "Format recognition
// on open"; openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D15): the first
// bytes of the (already gzip-inflated) stream decide the format, whatever
// the file name says:
// - OASIS: the 13 magic bytes `%SEMI-OASIS\r\n` (oasis-format "File
//   structure and magic bytes");
// - GDS: a HEADER record first, the 4 bytes 00 06 00 02 (record length 6,
//   type HEADER, two-byte integer data);
// - a Magic cell: the first line is `magic` (DBCellRead's first check).
//
// Owner: OASCMD (design D2).

#include "hefesto/oasis/command/format.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#include "hefesto/gds/stream/io.hpp"

namespace hefesto::oasis {

namespace {

constexpr std::array<unsigned char, 4> kGdsHeader{0x00, 0x06, 0x00, 0x02};
constexpr std::string_view kMagicLine = "magic";

/// Reads up to `out.size()` bytes; returns how many were read.
std::size_t read_prefix(gds::ByteSource& source, std::span<std::byte> out) {
  std::size_t filled = 0;
  while (filled < out.size()) {
    const auto got = source.read(out.subspan(filled));
    if (!got.ok() || got.value() == 0) {
      break;
    }
    filled += got.value();
  }
  return filled;
}

bool is_magic_cell(std::string_view prefix) {
  if (!prefix.starts_with(kMagicLine)) {
    return false;
  }
  const std::string_view rest = prefix.substr(kMagicLine.size());
  return rest.empty() || rest.front() == '\n' || rest.front() == '\r';
}

bool is_gds(std::string_view prefix) {
  if (prefix.size() < kGdsHeader.size()) {
    return false;
  }
  for (std::size_t i = 0; i < kGdsHeader.size(); ++i) {
    if (static_cast<unsigned char>(prefix[i]) != kGdsHeader[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

FileFormat sniff_format(gds::ByteSource& source) {
  std::array<std::byte, kOasisMagic.size()> buffer{};
  const std::size_t size = read_prefix(source, buffer);
  const std::string_view prefix{reinterpret_cast<const char*>(buffer.data()), size};
  if (prefix == kOasisMagic) {
    return FileFormat::oasis;
  }
  if (is_gds(prefix)) {
    return FileFormat::gds;
  }
  if (is_magic_cell(prefix)) {
    return FileFormat::magic;
  }
  return FileFormat::unknown;
}

}  // namespace hefesto::oasis
