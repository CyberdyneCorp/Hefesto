// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Input primitives of the .mag reader (mag_input.hpp) and gzip input
// (cell-file-format "Compressed cell input"; the inflated text is capped at
// kMaxInflatedCellBytes, security.md "Gzip bombs").

#include "mag_input.hpp"

#include <zlib.h>

#include <cctype>
#include <cstdint>
#include <limits>
#include <string>

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

namespace {

/// Owns a zlib inflate stream for gzip input (inflateInit2(15 + 32)).
class GzipInflater {
 public:
  GzipInflater() noexcept : ok_(inflateInit2(&zs_, 15 + 32) == Z_OK) {}
  GzipInflater(const GzipInflater&) = delete;
  GzipInflater& operator=(const GzipInflater&) = delete;
  ~GzipInflater() {
    if (ok_) {
      inflateEnd(&zs_);
    }
  }

  [[nodiscard]] bool ok() const noexcept { return ok_; }
  [[nodiscard]] z_stream& stream() noexcept { return zs_; }

 private:
  z_stream zs_{};
  bool ok_ = false;
};

/// `bytes` in the largest unit that divides it: "1 GiB", "1 MiB", "100 bytes".
std::string limit_text(std::uint64_t bytes) {
  constexpr std::uint64_t kGiB = std::uint64_t{1} << 30U;
  constexpr std::uint64_t kMiB = std::uint64_t{1} << 20U;
  if (bytes != 0 && bytes % kGiB == 0) {
    return std::to_string(bytes / kGiB) + " GiB";
  }
  if (bytes != 0 && bytes % kMiB == 0) {
    return std::to_string(bytes / kMiB) + " MiB";
  }
  return std::to_string(bytes) + " bytes";
}

}  // namespace

diag::Outcome<std::string> maybe_decompress(std::string data, std::uint64_t max_output) {
  if (data.size() < 2 || static_cast<unsigned char>(data[0]) != 0x1fU || static_cast<unsigned char>(data[1]) != 0x8bU) {
    return data;
  }
  GzipInflater inflater;
  if (!inflater.ok()) {
    return diag::make(codes::kCompressed, "cannot initialize gzip decoding");
  }
  z_stream& zs = inflater.stream();
  std::string out;
  char buffer[65536];
  zs.next_in = reinterpret_cast<Bytef*>(data.data());
  zs.avail_in = static_cast<uInt>(data.size());
  int rc = Z_OK;
  while (rc == Z_OK) {
    zs.next_out = reinterpret_cast<Bytef*>(buffer);
    zs.avail_out = sizeof buffer;
    rc = inflate(&zs, Z_NO_FLUSH);
    const std::size_t produced = sizeof buffer - zs.avail_out;
    // Checked per chunk, before the text grows: a gzip bomb never holds
    // more than the cap (out.size() never passes max_output).
    if (produced > max_output - out.size()) {
      return diag::make(codes::kCompressed,
                        "compressed cell file inflates past the " + limit_text(max_output) + " limit");
    }
    out.append(buffer, produced);
    if (rc == Z_STREAM_END && zs.avail_in > 0) {
      rc = inflateReset(&zs);  // concatenated gzip members
    }
  }
  if (rc != Z_STREAM_END) {
    return diag::make(codes::kCompressed, "corrupt gzip data");
  }
  return out;
}

}  // namespace hefesto::cell
