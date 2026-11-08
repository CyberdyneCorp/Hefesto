// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <system_error>

#include "hefesto/diag/codes.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::platform {

namespace fs = std::filesystem;
using diag::Done;
namespace codes = diag::codes;

namespace {

diag::Diagnostic io_error(std::string_view code, std::string_view what, const Path& path,
                          const std::error_code& ec = {}) {
  std::string text = std::string{what} + " '" + path.generic_string() + "'";
  if (ec) {
    text += ": " + ec.message();
  }
  return diag::make(code, std::move(text));
}

// A random per-process tag, so concurrent processes saving the same file pick
// different temp names (the exclusive create below catches any collision).
std::string process_tag() {
  static const std::string tag = [] {
    std::random_device rd;
    const auto bits = (static_cast<unsigned long long>(rd()) << 32U) ^ rd();
    char buffer[17];
    std::snprintf(buffer, sizeof buffer, "%016llx", bits);
    return std::string{buffer};
  }();
  return tag;
}

Path temp_sibling(const Path& path) {
  static std::atomic<unsigned> counter{0};
  Path tmp = path;
  tmp += ".tmp-" + process_tag() + "-" + std::to_string(counter.fetch_add(1));
  return tmp;
}

struct FileCloser {
  void operator()(std::FILE* f) const noexcept { std::fclose(f); }
};
using UniqueFile = std::unique_ptr<std::FILE, FileCloser>;

enum class NewFile { written, exists, failed };

// Creates `path` exclusively ("x" mode: never opens a file another writer
// created) and writes `data`; a partly written file is removed.
NewFile write_new_file(const Path& path, std::string_view data) {
  UniqueFile file{std::fopen(path.string().c_str(), "wbx")};
  if (!file) {
    std::error_code ec;
    return fs::exists(path, ec) ? NewFile::exists : NewFile::failed;
  }
  const bool written = std::fwrite(data.data(), 1, data.size(), file.get()) == data.size();
  if (std::fclose(file.release()) == 0 && written) {
    return NewFile::written;
  }
  std::error_code ignored;
  fs::remove(path, ignored);
  return NewFile::failed;
}

// Writes `data` to a fresh sibling of `path` that no other writer can share.
std::optional<Path> write_temp_sibling(const Path& path, std::string_view data) {
  constexpr int kAttempts = 8;
  for (int attempt = 0; attempt < kAttempts; ++attempt) {
    const Path tmp = temp_sibling(path);
    switch (write_new_file(tmp, data)) {
      case NewFile::written: return tmp;
      case NewFile::failed: return std::nullopt;
      case NewFile::exists: break;  // name taken by another writer: pick another
    }
  }
  return std::nullopt;
}

bool write_file(const Path& path, std::string_view data, std::ios::openmode mode) {
  std::ofstream out(path, std::ios::binary | mode);
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  out.close();
  return static_cast<bool>(out);
}

}  // namespace

diag::Outcome<std::string> LocalFileSystem::read(const Path& path) {
  std::error_code ec;
  const fs::file_status status = fs::status(path, ec);
  if (status.type() == fs::file_type::not_found) {
    return io_error(codes::kPlatformNotFound, "no such file", path);
  }
  if (ec) {
    return io_error(codes::kPlatformRead, "cannot read", path, ec);
  }
  // An ifstream opens a directory without error and reads nothing from it.
  if (!fs::is_regular_file(status)) {
    return io_error(codes::kPlatformRead, "not a regular file", path);
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return io_error(codes::kPlatformRead, "cannot read", path);
  }
  std::ostringstream buffer;
  if (in.peek() != std::ifstream::traits_type::eof()) {
    buffer << in.rdbuf();  // sets failbit on `buffer` when nothing could be extracted
  }
  if (in.bad() || buffer.fail()) {
    return io_error(codes::kPlatformRead, "cannot read", path);
  }
  return std::move(buffer).str();
}

diag::Status LocalFileSystem::write_atomic(const Path& path, std::string_view data) {
  const std::optional<Path> written = write_temp_sibling(path, data);
  if (!written) {
    return io_error(codes::kPlatformWrite, "cannot write", path);
  }
  const Path& tmp = *written;
  std::error_code ec;
  fs::rename(tmp, path, ec);
  if (ec) {
    std::error_code ignored;
    fs::remove(tmp, ignored);
    return io_error(codes::kPlatformWrite, "cannot replace", path, ec);
  }
  return Done{};
}

diag::Status LocalFileSystem::append(const Path& path, std::string_view data) {
  if (!write_file(path, data, std::ios::app)) {
    return io_error(codes::kPlatformWrite, "cannot append to", path);
  }
  return Done{};
}

bool LocalFileSystem::exists(const Path& path) {
  std::error_code ec;
  return fs::exists(path, ec);
}

diag::Outcome<std::uint64_t> LocalFileSystem::size(const Path& path) {
  std::error_code ec;
  const auto bytes = fs::file_size(path, ec);
  if (ec) {
    return io_error(codes::kPlatformNotFound, "no such file", path, ec);
  }
  return static_cast<std::uint64_t>(bytes);
}

diag::Outcome<std::vector<Path>> LocalFileSystem::list(const Path& directory) {
  std::error_code ec;
  fs::directory_iterator it(directory, ec);
  if (ec) {
    return io_error(codes::kPlatformDirectory, "cannot list", directory, ec);
  }
  std::vector<Path> entries;
  for (const auto& entry : it) {
    entries.push_back(entry.path());
  }
  std::sort(entries.begin(), entries.end());
  return entries;
}

diag::Status LocalFileSystem::create_directories(const Path& directory) {
  std::error_code ec;
  fs::create_directories(directory, ec);
  if (ec) {
    return io_error(codes::kPlatformDirectory, "cannot create directory", directory, ec);
  }
  return Done{};
}

diag::Status LocalFileSystem::rename(const Path& from, const Path& to) {
  std::error_code ec;
  fs::rename(from, to, ec);
  if (ec) {
    return io_error(codes::kPlatformWrite, "cannot rename", from, ec);
  }
  return Done{};
}

diag::Status LocalFileSystem::remove(const Path& path) {
  std::error_code ec;
  fs::remove(path, ec);
  if (ec) {
    return io_error(codes::kPlatformDirectory, "cannot remove", path, ec);
  }
  return Done{};
}

}  // namespace hefesto::platform
