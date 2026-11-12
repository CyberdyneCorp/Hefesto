// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The path decision of the script sandbox (design D5) and the file system
// the engine commands of a script see (security finding S-02). The Tcl side
// (the filesystem layer and the restricted commands) is in
// tcl_filesystem.cpp and sandbox_commands.cpp.

#include "hefesto/script/tcl/sandbox.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/script/tcl/embedded_library.hpp"
#include "script/tcl/sandbox_runtime.hpp"

namespace hefesto::script {

namespace {

constexpr int kMaxLinkHops = 40;  // the kernel's MAXSYMLINKS
constexpr std::string_view kPathDenied = "SCRIPT-SANDBOX-004";

// Pushes the components of `relative` onto `stack` so that the first one is
// popped first.
void push_components(std::vector<platform::Path>& stack, const platform::Path& relative) {
  const std::vector<platform::Path> parts(relative.begin(), relative.end());
  stack.insert(stack.end(), parts.rbegin(), parts.rend());
}

// `path` (absolute) with `.`/`..` removed and every symbolic link on it
// resolved as the kernel resolves it: a dangling link is followed too, from
// the link's directory (security finding S-04: `weakly_canonical` stops at a
// link whose target does not exist yet, so a relative link moved within a
// root could name a file outside it). Components that do not exist are kept
// as written. Empty when the links loop or cannot be read.
platform::Path resolve(const platform::Path& path) {
  std::vector<platform::Path> stack;
  push_components(stack, path.relative_path());
  platform::Path current = path.root_path();
  int hops = 0;
  while (!stack.empty()) {
    const platform::Path part = std::move(stack.back());
    stack.pop_back();
    if (part.empty() || part == ".") {
      continue;
    }
    if (part == "..") {
      current = current.parent_path();
      continue;
    }
    platform::Path next = current / part;
    std::error_code error;
    if (!std::filesystem::is_symlink(std::filesystem::symlink_status(next, error))) {
      current = std::move(next);
      continue;
    }
    const platform::Path target = std::filesystem::read_symlink(next, error);
    if (error || ++hops > kMaxLinkHops) {
      return {};
    }
    if (target.is_absolute()) {
      current = target.root_path();
    }
    push_components(stack, target.relative_path());
  }
  return current;
}

// True when `path` equals `root` or lies under it, component by component.
bool is_under(const platform::Path& path, const platform::Path& root) {
  auto p = path.begin();
  for (auto r = root.begin(); r != root.end(); ++r, ++p) {
    if (p == path.end() || *p != *r) {
      return false;
    }
  }
  return true;
}

bool under_any(const platform::Path& path, const std::vector<platform::Path>& roots) {
  return !path.empty() &&
         std::any_of(roots.begin(), roots.end(), [&](const platform::Path& root) { return is_under(path, root); });
}

// True when `path` is a directory strictly above one of `roots`.
bool above_any(const platform::Path& path, const std::vector<platform::Path>& roots) {
  return std::any_of(roots.begin(), roots.end(),
                     [&](const platform::Path& root) { return root != path && is_under(root, path); });
}

// Adds `root` (absolute) resolved to `resolved` and as written to `lexical`.
void add_root(const platform::Path& root, std::vector<platform::Path>& resolved,
              std::vector<platform::Path>& lexical) {
  if (!root.is_absolute()) {
    return;
  }
  if (platform::Path normal = resolve(root); !normal.empty()) {
    resolved.push_back(std::move(normal));
  }
  platform::Path written = root.lexically_normal();
  if (!written.has_filename() && written != written.root_path()) {
    written = written.parent_path();  // a trailing separator
  }
  lexical.push_back(std::move(written));
}

}  // namespace

std::vector<std::string> default_removed_commands() {
  return {"exec", "socket", "load", "unload", "exit"};
}

SandboxFilesystem::SandboxFilesystem(SandboxPolicy policy) : policy_(std::move(policy)) {
  for (const auto& root : policy_.granted_roots) {
    add_root(root, roots_, lexical_roots_);
  }
  for (const auto& root : policy_.read_only_roots) {
    add_root(root, read_only_, lexical_roots_);
  }
}

PathDecision SandboxFilesystem::check(std::string_view operation, std::string_view path,
                                      const platform::Path& cwd) const {
  static_cast<void>(operation);  // the decision depends on the path only
  if (path.empty() || path.front() == '~') {
    return PathDecision{false, platform::Path{path}, false};
  }
  const platform::Path requested{path};
  const platform::Path normalized = resolve(requested.is_absolute() ? requested : cwd / requested);
  const bool writable = under_any(normalized, roots_);
  return PathDecision{writable || under_any(normalized, read_only_), normalized, writable};
}

bool SandboxFilesystem::is_root_ancestor(std::string_view normalized) const {
  const platform::Path path = platform::Path{normalized}.lexically_normal();
  // S-10: a root named through a link (macOS /var/... for /private/var/...)
  // is also walked as written.
  return above_any(path, roots_) || above_any(path, read_only_) || above_any(path, lexical_roots_);
}

bool is_embedded_path(std::string_view path) noexcept {
  constexpr std::string_view kTop = "/__hefesto_embedded__";
  static_assert(kVirtualLibraryRoot.substr(0, kTop.size()) == kTop);
  return path.substr(0, kTop.size()) == kTop && (path.size() == kTop.size() || path[kTop.size()] == '/');
}

void set_filesystem_audit(std::function<void(const FilesystemAccess&)> audit) {
  tcl_detail::filesystem_audit() = std::move(audit);
}

// ---- SandboxedFileSystem ------------------------------------------------------------

std::optional<diag::Diagnostic> SandboxedFileSystem::refuse(std::string_view operation, const platform::Path& path,
                                                            bool write) const {
  std::error_code error;
  const platform::Path cwd = std::filesystem::current_path(error);
  const PathDecision decision = SandboxFilesystem{policy_}.check(operation, path.string(), cwd);
  if (decision.allowed && (!write || decision.writable)) {
    return std::nullopt;
  }
  const std::string why = decision.allowed ? "is read-only in the script sandbox"
                                           : "is outside the sandbox's granted roots";
  return diag::make(kPathDenied, std::string{operation} + ": \"" + path.string() + "\" " + why);
}

diag::Outcome<std::string> SandboxedFileSystem::read(const platform::Path& path) {
  if (auto denied = refuse("read", path, false)) {
    return *denied;
  }
  std::error_code error;
  const auto status = std::filesystem::status(path, error);
  if (!error && std::filesystem::exists(status) && !std::filesystem::is_regular_file(status)) {
    // A device (/dev/zero) or FIFO never ends or blocks (S-02).
    return diag::make(kPathDenied, "read: \"" + path.string() + "\" is not a regular file");
  }
  return base_.read(path);
}

diag::Status SandboxedFileSystem::write_atomic(const platform::Path& path, std::string_view data) {
  if (auto denied = refuse("write", path, true)) {
    return *denied;
  }
  return base_.write_atomic(path, data);
}

diag::Status SandboxedFileSystem::append(const platform::Path& path, std::string_view data) {
  if (auto denied = refuse("append", path, true)) {
    return *denied;
  }
  return base_.append(path, data);
}

bool SandboxedFileSystem::exists(const platform::Path& path) {
  return !refuse("exists", path, false) && base_.exists(path);
}

diag::Outcome<std::uint64_t> SandboxedFileSystem::size(const platform::Path& path) {
  if (auto denied = refuse("size", path, false)) {
    return *denied;
  }
  return base_.size(path);
}

diag::Outcome<std::vector<platform::Path>> SandboxedFileSystem::list(const platform::Path& directory) {
  if (auto denied = refuse("list", directory, false)) {
    return *denied;
  }
  return base_.list(directory);
}

diag::Status SandboxedFileSystem::create_directories(const platform::Path& directory) {
  if (auto denied = refuse("mkdir", directory, true)) {
    return *denied;
  }
  return base_.create_directories(directory);
}

diag::Status SandboxedFileSystem::rename(const platform::Path& from, const platform::Path& to) {
  if (auto denied = refuse("rename", from, true)) {
    return *denied;
  }
  if (auto denied = refuse("rename", to, true)) {
    return *denied;
  }
  return base_.rename(from, to);
}

diag::Status SandboxedFileSystem::remove(const platform::Path& path) {
  if (auto denied = refuse("delete", path, true)) {
    return *denied;
  }
  return base_.remove(path);
}

}  // namespace hefesto::script
