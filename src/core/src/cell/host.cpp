// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// File hosts (cell/host.hpp): the local disk through std::filesystem and an
// in-memory host for sessions on a MemoryFileSystem.

#include "hefesto/cell/host.hpp"

#include <cstdio>
#include <filesystem>
#include <system_error>

namespace hefesto::cell {

namespace {

namespace fs = std::filesystem;

constexpr auto kAnyWrite = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;

class LocalFileHost final : public FileHost {
 public:
  bool is_symlink(const platform::Path& path) override {
    std::error_code ec;
    return fs::is_symlink(fs::symlink_status(path, ec));
  }
  bool is_directory(const platform::Path& path) override {
    std::error_code ec;
    return fs::is_directory(path, ec);
  }
  std::optional<bool> same_file(const platform::Path& a, const platform::Path& b) override {
    std::error_code ec;
    const bool same = fs::equivalent(a, b, ec);
    if (ec) {
      return std::nullopt;
    }
    return same;
  }
  platform::Path current_directory() override {
    std::error_code ec;
    platform::Path cwd = fs::current_path(ec);
    return ec ? platform::Path{} : cwd;
  }
  bool writable(const platform::Path& path) override {
    std::error_code ec;
    const fs::file_status st = fs::symlink_status(path, ec);
    if (ec || !fs::is_regular_file(st) || (st.permissions() & kAnyWrite) == fs::perms::none) {
      return false;
    }
    std::FILE* f = std::fopen(path.string().c_str(), "r+b");
    if (f == nullptr) {
      return false;
    }
    std::fclose(f);
    return true;
  }
  std::optional<std::uint32_t> permissions(const platform::Path& path) override {
    std::error_code ec;
    const fs::file_status st = fs::status(path, ec);
    if (ec || !fs::exists(st)) {
      return std::nullopt;
    }
    return static_cast<std::uint32_t>(st.permissions() & fs::perms::mask);
  }
  bool set_permissions(const platform::Path& path, std::uint32_t mode) override {
    std::error_code ec;
    fs::permissions(path, static_cast<fs::perms>(mode) & fs::perms::mask, fs::perm_options::replace, ec);
    return !ec;
  }
  std::optional<std::string> git_head(const platform::Path& /*directory*/) override { return std::nullopt; }
};

class MemoryFileHost final : public FileHost {
 public:
  MemoryFileHost(platform::FileSystem& files, platform::Path cwd) : files_(files), cwd_(std::move(cwd)) {}

  bool is_symlink(const platform::Path& /*path*/) override { return false; }
  bool is_directory(const platform::Path& path) override { return files_.list(path).ok(); }
  std::optional<bool> same_file(const platform::Path& a, const platform::Path& b) override {
    if (!files_.exists(a) || !files_.exists(b)) {
      return std::nullopt;
    }
    return a.lexically_normal() == b.lexically_normal();
  }
  platform::Path current_directory() override { return cwd_; }
  bool writable(const platform::Path& path) override { return files_.exists(path); }
  std::optional<std::uint32_t> permissions(const platform::Path& path) override {
    if (!files_.exists(path)) {
      return std::nullopt;
    }
    return 0644U;
  }
  bool set_permissions(const platform::Path& /*path*/, std::uint32_t /*mode*/) override { return true; }
  std::optional<std::string> git_head(const platform::Path& /*directory*/) override { return std::nullopt; }

 private:
  platform::FileSystem& files_;
  platform::Path cwd_;
};

}  // namespace

std::unique_ptr<FileHost> make_local_file_host() {
  return std::make_unique<LocalFileHost>();
}

std::unique_ptr<FileHost> make_memory_file_host(platform::FileSystem& fs, platform::Path current_directory) {
  return std::make_unique<MemoryFileHost>(fs, std::move(current_directory));
}

}  // namespace hefesto::cell
