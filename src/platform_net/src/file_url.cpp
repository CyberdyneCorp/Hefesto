// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "file_url.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#include "hefesto/diag/codes.hpp"
#include "hefesto/pdk/diag_codes.hpp"

namespace hefesto::platform_net::detail {

namespace {

namespace stdfs = std::filesystem;

constexpr std::string_view kScheme = "file://";
constexpr std::string_view kLocalhost = "localhost";

int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::optional<std::string> percent_decode(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '%') {
      out += text[i];
      continue;
    }
    const int high = i + 2 < text.size() ? hex_value(text[i + 1]) : -1;
    const int low = i + 2 < text.size() ? hex_value(text[i + 2]) : -1;
    if (high < 0 || low < 0) return std::nullopt;
    out += static_cast<char>(high * 16 + low);
    i += 2;
  }
  return out;
}

diag::Diagnostic fetch_error(std::string_view url, const std::string& why) {
  return diag::make(pdk::codes::kFetch, "cannot download " + std::string{url} + ": " + why);
}

std::uint64_t size_or_zero(const platform::Path& path) {
  std::error_code ec;
  const std::uint64_t size = stdfs::file_size(path, ec);
  return ec ? 0 : size;
}

// Appends `in` (positioned at `from`) to `out`, reporting each chunk.
bool append_rest(std::ifstream& in, std::ofstream& out, std::uint64_t from, std::uint64_t total,
                 const std::function<void(std::uint64_t, std::uint64_t)>& progress) {
  std::array<char, 64 * 1024> buffer{};
  std::uint64_t written = from;
  while (in) {
    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto got = in.gcount();
    if (got <= 0) break;
    out.write(buffer.data(), got);
    if (!out) return false;
    written += static_cast<std::uint64_t>(got);
    if (progress) progress(written, total);
  }
  out.flush();
  return static_cast<bool>(out);
}

}  // namespace

std::optional<platform::Path> file_url_path(std::string_view url) {
  if (!url.starts_with(kScheme)) return std::nullopt;
  std::string_view rest = url.substr(kScheme.size());
  if (rest.starts_with(kLocalhost)) rest.remove_prefix(kLocalhost.size());
  if (!rest.starts_with('/')) return std::nullopt;  // another host
  auto decoded = percent_decode(rest);
  if (!decoded) return std::nullopt;
  return platform::Path{*decoded};
}

diag::Outcome<std::string> read_file_url(std::string_view url) {
  const auto path = file_url_path(url);
  if (!path) return fetch_error(url, "unsupported file URL");
  std::error_code ec;
  if (!stdfs::is_regular_file(*path, ec)) {
    return diag::make(pdk::codes::kFetchNotFound, "not found: " + std::string{url});
  }
  std::ifstream in{*path, std::ios::binary};
  std::ostringstream text;
  text << in.rdbuf();
  if (!in) return fetch_error(url, "cannot read " + path->string());
  return text.str();
}

diag::Status copy_file_url(std::string_view url, const platform::Path& to,
                           const std::function<void(std::uint64_t, std::uint64_t)>& progress) {
  const auto path = file_url_path(url);
  if (!path) return fetch_error(url, "unsupported file URL");
  std::ifstream in{*path, std::ios::binary};
  if (!in) return fetch_error(url, "not found");
  const std::uint64_t total = size_or_zero(*path);
  std::uint64_t offset = size_or_zero(to);
  if (offset > total) offset = 0;  // not a prefix of this file: start over
  std::ofstream out{to, std::ios::binary | (offset > 0 ? std::ios::app : std::ios::trunc)};
  in.seekg(static_cast<std::streamoff>(offset));
  if (!out || !in) return fetch_error(url, "cannot write " + to.string());
  if (!append_rest(in, out, offset, total, progress)) return fetch_error(url, "cannot write " + to.string());
  return diag::Done{};
}

}  // namespace hefesto::platform_net::detail
