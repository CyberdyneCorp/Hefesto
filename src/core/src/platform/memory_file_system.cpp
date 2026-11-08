// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <algorithm>

#include "hefesto/diag/codes.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::platform {

using diag::Done;
namespace codes = diag::codes;

namespace {

// Lexically normal, without a trailing separator ("/a/" and "/a" are one
// directory, as on the local file system).
Path normal(const Path& path) {
  Path p = path.lexically_normal();
  if (!p.has_filename() && p.has_relative_path()) {
    p = p.parent_path();
  }
  return p;
}

diag::Diagnostic not_found(const Path& path) {
  return diag::make(codes::kPlatformNotFound, "no such file '" + path.generic_string() + "'");
}

// True when `child` is strictly below `parent`.
bool is_below(const Path& child, const Path& parent) {
  const auto [p, c] = std::mismatch(parent.begin(), parent.end(), child.begin(), child.end());
  return p == parent.end() && c != child.end();
}

}  // namespace

bool MemoryFileSystem::is_directory_locked(const Path& path) const {
  if (std::find(directories_.begin(), directories_.end(), path) != directories_.end()) {
    return true;
  }
  return std::any_of(files_.begin(), files_.end(), [&](const auto& kv) { return is_below(kv.first, path); });
}

diag::Outcome<std::string> MemoryFileSystem::read(const Path& path) {
  const std::scoped_lock lock{mutex_};
  const auto it = files_.find(normal(path));
  if (it == files_.end()) {
    return not_found(path);
  }
  return it->second;
}

diag::Status MemoryFileSystem::write_atomic(const Path& path, std::string_view data) {
  const std::scoped_lock lock{mutex_};
  files_[normal(path)] = std::string{data};
  return Done{};
}

diag::Status MemoryFileSystem::append(const Path& path, std::string_view data) {
  const std::scoped_lock lock{mutex_};
  files_[normal(path)] += data;
  return Done{};
}

bool MemoryFileSystem::exists(const Path& path) {
  const std::scoped_lock lock{mutex_};
  const Path p = normal(path);
  return files_.contains(p) || is_directory_locked(p);
}

diag::Outcome<std::uint64_t> MemoryFileSystem::size(const Path& path) {
  const std::scoped_lock lock{mutex_};
  const auto it = files_.find(normal(path));
  if (it == files_.end()) {
    return not_found(path);
  }
  return static_cast<std::uint64_t>(it->second.size());
}

diag::Outcome<std::vector<Path>> MemoryFileSystem::list(const Path& directory) {
  const std::scoped_lock lock{mutex_};
  const Path dir = normal(directory);
  if (!is_directory_locked(dir)) {
    return diag::make(codes::kPlatformDirectory, "cannot list '" + directory.generic_string() + "'");
  }
  std::vector<Path> entries;
  auto add_child = [&](const Path& p) {
    if (!is_below(p, dir)) {
      return;
    }
    const Path child = dir / *std::next(p.begin(), std::distance(dir.begin(), dir.end()));
    if (std::find(entries.begin(), entries.end(), child) == entries.end()) {
      entries.push_back(child);
    }
  };
  for (const auto& kv : files_) {
    add_child(kv.first);
  }
  for (const auto& d : directories_) {
    add_child(d);
  }
  std::sort(entries.begin(), entries.end());
  return entries;
}

diag::Status MemoryFileSystem::create_directories(const Path& directory) {
  const std::scoped_lock lock{mutex_};
  const Path dir = normal(directory);
  if (!is_directory_locked(dir)) {
    directories_.push_back(dir);
  }
  return Done{};
}

diag::Status MemoryFileSystem::rename(const Path& from, const Path& to) {
  const std::scoped_lock lock{mutex_};
  auto node = files_.extract(normal(from));
  if (node.empty()) {
    return not_found(from);
  }
  files_[normal(to)] = std::move(node.mapped());
  return Done{};
}

diag::Status MemoryFileSystem::remove(const Path& path) {
  const std::scoped_lock lock{mutex_};
  files_.erase(normal(path));
  return Done{};
}

}  // namespace hefesto::platform
