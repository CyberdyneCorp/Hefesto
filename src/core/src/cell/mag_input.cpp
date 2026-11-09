// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Input primitives of the .mag reader (mag_input.hpp) and gzip input
// (cell-file-format "Compressed cell input").

#include "mag_input.hpp"

#include <zlib.h>

#include <cctype>
#include <limits>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/mag_io.hpp"

namespace hefesto::cell {

namespace detail {

bool is_space(int c) noexcept {
  return c != kEof && std::isspace(c) != 0;
}

bool is_digit(int c) noexcept {
  return c >= '0' && c <= '9';
}

bool MagInput::rest_of_line(std::string& line) {
  line.clear();
  int c = kEof;
  while ((c = getc()) != kEof) {
    if (c != '\r') {
      line += static_cast<char>(c);
    }
    if (c == '\n') {
      break;
    }
  }
  return !line.empty();
}

bool MagInput::fgets(std::string& line) {
  do {
    if (!rest_of_line(line)) {
      return false;
    }
  } while (!line.empty() && (line.front() == '#' || line == "\n"));
  return true;
}

int MagInput::skip_space_then_getc() noexcept {
  int c = kEof;
  while ((c = getc()) != kEof && is_space(c)) {
  }
  return c;
}

bool Scan::lit(std::string_view word) noexcept {
  if (s_.substr(pos_, word.size()) != word) {
    return false;
  }
  pos_ += word.size();
  return true;
}

void Scan::ws() noexcept {
  while (pos_ < s_.size() && is_space(static_cast<unsigned char>(s_[pos_]))) {
    ++pos_;
  }
}

bool Scan::str(std::string& out, std::size_t max) noexcept {
  ws();
  out.clear();
  while (pos_ < s_.size() && out.size() < max && !is_space(static_cast<unsigned char>(s_[pos_]))) {
    out += s_[pos_++];
  }
  return !out.empty();
}

bool Scan::integer64(std::int64_t& out) noexcept {
  ws();
  bool negative = false;
  if (pos_ < s_.size() && (s_[pos_] == '-' || s_[pos_] == '+')) {
    negative = s_[pos_] == '-';
    ++pos_;
  }
  if (pos_ >= s_.size() || !is_digit(static_cast<unsigned char>(s_[pos_]))) {
    return false;
  }
  std::uint64_t value = 0;
  while (pos_ < s_.size() && is_digit(static_cast<unsigned char>(s_[pos_]))) {
    value = value * 10U + static_cast<std::uint64_t>(s_[pos_++] - '0');
  }
  out = negative ? static_cast<std::int64_t>(0U - value) : static_cast<std::int64_t>(value);
  return true;
}

bool Scan::integer(std::int32_t& out) noexcept {
  std::int64_t v = 0;
  if (!integer64(v)) {
    return false;
  }
  out = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(v)));
  return true;
}

bool Scan::chr(char& out) noexcept {
  if (pos_ >= s_.size()) {
    return false;
  }
  out = s_[pos_++];
  return true;
}

bool Scan::rest(std::string& out, std::size_t max) noexcept {
  out.clear();
  while (pos_ < s_.size() && s_[pos_] != '\n' && out.size() < max) {
    out += s_[pos_++];
  }
  return !out.empty();
}

std::size_t count_tokens(std::string_view text) noexcept {
  std::size_t n = 0;
  bool in = false;
  for (const char c : text) {
    const bool sp = is_space(static_cast<unsigned char>(c));
    if (!sp && !in) {
      ++n;
    }
    in = !sp;
  }
  return n;
}

std::string_view chomp(std::string_view line) noexcept {
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
    line.remove_suffix(1);
  }
  return line;
}

}  // namespace detail

diag::Outcome<std::string> maybe_decompress(std::string data) {
  if (data.size() < 2 || static_cast<unsigned char>(data[0]) != 0x1fU || static_cast<unsigned char>(data[1]) != 0x8bU) {
    return data;
  }
  z_stream zs{};
  if (inflateInit2(&zs, 15 + 32) != Z_OK) {
    return diag::make(codes::kCompressed, "cannot initialize gzip decoding");
  }
  std::string out;
  char buffer[65536];
  zs.next_in = reinterpret_cast<Bytef*>(data.data());
  zs.avail_in = static_cast<uInt>(data.size());
  int rc = Z_OK;
  while (rc == Z_OK) {
    zs.next_out = reinterpret_cast<Bytef*>(buffer);
    zs.avail_out = sizeof buffer;
    rc = inflate(&zs, Z_NO_FLUSH);
    out.append(buffer, sizeof buffer - zs.avail_out);
    if (rc == Z_STREAM_END && zs.avail_in > 0) {
      rc = inflateReset(&zs);  // concatenated gzip members
    }
  }
  inflateEnd(&zs);
  if (rc != Z_STREAM_END) {
    return diag::make(codes::kCompressed, "corrupt gzip data");
  }
  return out;
}

}  // namespace hefesto::cell
