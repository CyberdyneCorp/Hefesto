// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Test doubles of the PDK manager's host mechanisms: an in-memory Fetcher
// (with HTTP-range resume), a pass-through Decompressor (assets are plain
// tar files in tests), a host with real symbolic links and a fixed
// environment, and a small ustar writer.

#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/codes.hpp"
#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/process_runner.hpp"

namespace hefesto::test {

class MapFetcher final : public pdk::Fetcher {
 public:
  std::map<std::string, std::string> documents;  // URL -> body
  std::vector<std::string> downloads;            // URLs downloaded, in order
  std::map<std::string, std::uint64_t> resumed_from;
  std::size_t fail_after_bytes = 0;  // >0: a download stops after this many bytes (connection loss)
  std::vector<std::string> requests;  // URLs of get_text, in order

  diag::Outcome<std::string> get_text(std::string_view url) override {
    requests.emplace_back(url);
    const auto it = documents.find(std::string{url});
    if (it == documents.end()) {
      return diag::make(pdk::codes::kFetchNotFound, "not found (HTTP 404): " + std::string{url});
    }
    return it->second;
  }

  diag::Status download(std::string_view url, const platform::Path& to,
                        const std::function<void(std::uint64_t, std::uint64_t)>& progress) override {
    const auto it = documents.find(std::string{url});
    if (it == documents.end()) {
      return diag::make(pdk::codes::kFetch, "cannot download " + std::string{url});
    }
    if (progress) {
      progress(it->second.size() / 2, 0);  // a fetcher that does not know the total
    }
    downloads.emplace_back(url);
    std::error_code ec;
    const std::uint64_t offset = std::filesystem::exists(to, ec) ? std::filesystem::file_size(to, ec) : 0;
    resumed_from[std::string{url}] = offset;
    std::string rest = offset < it->second.size() ? it->second.substr(offset) : std::string{};
    const bool drop = fail_after_bytes > 0 && rest.size() > fail_after_bytes;
    if (drop) {
      rest.resize(fail_after_bytes);
    }
    std::ofstream out{to, std::ios::binary | std::ios::app};
    out.write(rest.data(), static_cast<std::streamsize>(rest.size()));
    out.close();
    if (drop) {
      fail_after_bytes = 0;
      return diag::make(pdk::codes::kFetch, "connection lost");
    }
    if (progress) {
      progress(it->second.size(), it->second.size());
    }
    return diag::Done{};
  }
};

// Test catalogs publish plain tar files as assets: they are copied. Real
// Zstandard frames (rebuilt pack assets) go to the library decoder.
class CopyDecompressor final : public pdk::Decompressor {
 public:
  diag::Status decompress(const platform::Path& in, const platform::Path& out, std::uint64_t max_bytes) override {
    if (is_zstd(in)) {
      return zstd_->decompress(in, out, max_bytes);
    }
    std::error_code ec;
    if (std::filesystem::file_size(in, ec) > max_bytes) {
      return diag::make(pdk::codes::kUnpackLimit, "too large");
    }
    std::filesystem::copy_file(in, out, std::filesystem::copy_options::overwrite_existing, ec);
    return ec ? diag::Status{diag::make(pdk::codes::kBadArchive, ec.message())} : diag::Status{diag::Done{}};
  }

 private:
  static bool is_zstd(const platform::Path& path) {
    std::ifstream file{path, std::ios::binary};
    std::string magic(4, '\0');
    file.read(magic.data(), 4);
    return file && magic == std::string{"\x28\xB5\x2F\xFD", 4};
  }

  platform::UnsupportedProcessRunner no_processes_;
  std::unique_ptr<pdk::Decompressor> zstd_ = pdk::make_default_decompressor(no_processes_);
};

class FakeHost final : public pdk::InstallHost {
 public:
  std::map<std::string, std::string> env;
  platform::Path home;

  diag::Status make_symlink(const platform::Path& target, const platform::Path& link) override {
    std::error_code ec;
    std::filesystem::create_symlink(target, link, ec);
    return ec ? diag::Status{diag::make(pdk::codes::kInstallIo, ec.message())} : diag::Status{diag::Done{}};
  }
  diag::Outcome<std::optional<platform::Path>> read_symlink(const platform::Path& link) override {
    std::error_code ec;
    if (!std::filesystem::is_symlink(std::filesystem::symlink_status(link, ec))) {
      return std::optional<platform::Path>{};
    }
    return std::optional<platform::Path>{std::filesystem::read_symlink(link, ec)};
  }
  std::optional<std::string> environment(std::string_view name) override {
    const auto it = env.find(std::string{name});
    return it == env.end() ? std::nullopt : std::optional<std::string>{it->second};
  }
  platform::Path home_directory() override { return home; }
};

// A ustar archive built entry by entry.
class TarWriter {
 public:
  TarWriter& file(const std::string& name, const std::string& data) { return entry(name, '0', data); }
  TarWriter& directory(const std::string& name) { return entry(name, '5', ""); }
  TarWriter& symlink(const std::string& name, const std::string& target) { return entry(name, '2', "", target); }
  TarWriter& pax_file(const std::string& long_name, const std::string& data) {
    const std::string record = " path=" + long_name + "\n";
    std::string sized = std::to_string(record.size() + 2) + record;  // length includes its own digits
    if (sized.size() != record.size() + 2) {
      sized = std::to_string(record.size() + 3) + record;
    }
    entry("PaxHeader", 'x', sized);
    return entry("short", '0', data);
  }

  [[nodiscard]] std::string finish() const { return bytes_ + std::string(1024, '\0'); }

 private:
  TarWriter& entry(const std::string& name, char type, const std::string& data, const std::string& link = {}) {
    std::string header(512, '\0');
    name.copy(header.data(), std::min<std::size_t>(name.size(), 100));
    octal(header, 100, 8, 0644);
    octal(header, 108, 8, 0);
    octal(header, 116, 8, 0);
    octal(header, 124, 12, data.size());
    octal(header, 136, 12, 0);
    header[156] = type;
    link.copy(header.data() + 157, std::min<std::size_t>(link.size(), 100));
    std::string("ustar\0" "00", 8).copy(header.data() + 257, 8);
    std::fill(header.begin() + 148, header.begin() + 156, ' ');
    unsigned sum = 0;
    for (const char c : header) {
      sum += static_cast<unsigned char>(c);
    }
    octal(header, 148, 7, sum);
    bytes_ += header + data + std::string((512 - data.size() % 512) % 512, '\0');
    return *this;
  }

  static void octal(std::string& header, std::size_t offset, std::size_t width, std::uint64_t value) {
    std::string digits(width - 1, '0');
    for (std::size_t i = width - 1; i-- > 0 && value > 0; value /= 8) {
      digits[i] = static_cast<char>('0' + value % 8);
    }
    digits.copy(header.data() + offset, width - 1);
  }

  std::string bytes_;
};

}  // namespace hefesto::test
