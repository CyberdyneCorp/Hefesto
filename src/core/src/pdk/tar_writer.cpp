// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tar and stored-Zstandard writers for offline PDK packs. Zstandard frame
// layout per RFC 8878 sections 3.1.1 (frame header, window descriptor) and
// 3.1.1.2 (block header, Raw_Block).

#include "hefesto/pdk/tar_writer.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "hefesto/pdk/diag_codes.hpp"

namespace hefesto::pdk {

namespace {

constexpr std::size_t kBlock = 512;
constexpr std::uint64_t kMaxUstarSize = 077777777777ULL;  // 11 octal digits

void put_octal(std::string& header, std::size_t offset, std::size_t width, std::uint64_t value) {
  std::string digits(width - 1, '0');
  for (std::size_t i = width - 1; i-- > 0 && value > 0; value /= 8) {
    digits[i] = static_cast<char>('0' + static_cast<char>(value % 8));
  }
  digits.copy(header.data() + offset, width - 1);
}

// "%d path=NAME\n" where %d counts the whole record, its own digits included.
std::string pax_path_record(std::string_view name) {
  const std::string body = " path=" + std::string{name} + "\n";
  std::size_t total = body.size() + 1;
  while (std::to_string(total).size() + body.size() != total) {
    total = std::to_string(total).size() + body.size();
  }
  return std::to_string(total) + body;
}

}  // namespace

diag::Status TarWriter::header(std::string_view name, char type, std::uint64_t size) {
  if (size > kMaxUstarSize) {
    return diag::make(codes::kInstallIo, "pack entry " + std::string{name} + " is larger than 8 GiB");
  }
  std::string block(kBlock, '\0');
  name.substr(0, 100).copy(block.data(), std::min<std::size_t>(name.size(), 100));
  put_octal(block, 100, 8, 0644);
  put_octal(block, 108, 8, 0);
  put_octal(block, 116, 8, 0);
  put_octal(block, 124, 12, size);
  put_octal(block, 136, 12, 0);
  block[156] = type;
  std::string_view{"ustar\0" "00", 8}.copy(block.data() + 257, 8);
  std::fill(block.begin() + 148, block.begin() + 156, ' ');
  unsigned sum = 0;
  for (const char c : block) {
    sum += static_cast<unsigned char>(c);
  }
  put_octal(block, 148, 7, sum);
  out_.write(block.data(), static_cast<std::streamsize>(block.size()));
  return out_ ? diag::Status{diag::Done{}} : diag::make(codes::kInstallIo, "cannot write the pack");
}

void TarWriter::pad(std::uint64_t size) {
  static constexpr std::array<char, kBlock> kZeros{};
  out_.write(kZeros.data(), static_cast<std::streamsize>((kBlock - size % kBlock) % kBlock));
}

diag::Status TarWriter::begin_entry(std::string_view name, std::uint64_t size) {
  if (name.size() > 100) {
    const std::string record = pax_path_record(name);
    if (auto status = header("PaxHeader", 'x', record.size()); !status) {
      return status;
    }
    out_.write(record.data(), static_cast<std::streamsize>(record.size()));
    pad(record.size());
  }
  return header(name, '0', size);
}

diag::Status TarWriter::add_bytes(std::string_view name, std::string_view data) {
  if (auto status = begin_entry(name, data.size()); !status) {
    return status;
  }
  out_.write(data.data(), static_cast<std::streamsize>(data.size()));
  pad(data.size());
  return out_ ? diag::Status{diag::Done{}} : diag::make(codes::kInstallIo, "cannot write the pack");
}

diag::Status TarWriter::add_file(std::string_view name, const platform::Path& source) {
  std::error_code ec;
  const std::uint64_t size = std::filesystem::file_size(source, ec);
  std::ifstream in{source, std::ios::binary};
  if (ec || !in) {
    return diag::make(codes::kInstallIo, "cannot read " + source.string());
  }
  if (auto status = begin_entry(name, size); !status) {
    return status;
  }
  std::vector<char> chunk(64 * 1024);
  std::uint64_t copied = 0;
  while (copied < size && in) {
    in.read(chunk.data(), static_cast<std::streamsize>(std::min<std::uint64_t>(chunk.size(), size - copied)));
    out_.write(chunk.data(), in.gcount());
    copied += static_cast<std::uint64_t>(in.gcount());
  }
  if (copied != size) {
    return diag::make(codes::kInstallIo, source.string() + " changed while it was packed");
  }
  pad(size);
  return out_ ? diag::Status{diag::Done{}} : diag::make(codes::kInstallIo, "cannot write the pack");
}

diag::Status TarWriter::finish() {
  const std::string zeros(2 * kBlock, '\0');
  out_.write(zeros.data(), static_cast<std::streamsize>(zeros.size()));
  out_.flush();
  return out_ ? diag::Status{diag::Done{}} : diag::make(codes::kInstallIo, "cannot write the pack");
}

StoredZstdWriter::StoredZstdWriter(std::ostream& out) : out_(out) {
  // Magic number 0xFD2FB528 (little endian), frame header descriptor 0 (no
  // content size, no checksum, no dictionary, not single segment), window
  // descriptor 0x38: exponent 7, mantissa 0, window 2^17 = 128 KiB.
  static constexpr std::array<char, 6> kHeader = {'\x28', '\xB5', '\x2F', '\xFD', '\x00', '\x38'};
  out_.write(kHeader.data(), static_cast<std::streamsize>(kHeader.size()));
  setp(buffer_.data(), buffer_.data() + buffer_.size());
}

StoredZstdWriter::int_type StoredZstdWriter::overflow(int_type ch) {
  flush_block(false);
  if (traits_type::eq_int_type(ch, traits_type::eof())) {
    return traits_type::not_eof(ch);
  }
  *pptr() = traits_type::to_char_type(ch);
  pbump(1);
  return ch;
}

void StoredZstdWriter::flush_block(bool last) {
  const auto size = static_cast<std::uint32_t>(pptr() - pbase());
  if (size == 0 && !last) {
    return;
  }
  // Block header: bit 0 Last_Block, bits 1-2 Block_Type (0 = Raw), bits 3-23 Block_Size.
  const std::uint32_t word = (size << 3U) | (last ? 1U : 0U);
  const std::array<char, 3> header = {static_cast<char>(word & 0xFFU), static_cast<char>((word >> 8U) & 0xFFU),
                                      static_cast<char>((word >> 16U) & 0xFFU)};
  out_.write(header.data(), static_cast<std::streamsize>(header.size()));
  out_.write(pbase(), static_cast<std::streamsize>(size));
  setp(buffer_.data(), buffer_.data() + buffer_.size());
}

void StoredZstdWriter::finish() {
  if (!finished_) {
    flush_block(true);
    finished_ = true;
  }
}

}  // namespace hefesto::pdk
