// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The path decision of the script sandbox (design D5). The Tcl side (the
// filesystem layer and the restricted commands) is in tcl_filesystem.cpp
// and sandbox_commands.cpp.

#include "hefesto/script/tcl/sandbox.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

#include "hefesto/script/tcl/embedded_library.hpp"
#include "script/tcl/sandbox_runtime.hpp"

namespace hefesto::script {

namespace {

// `path` with `.`/`..` removed, the symbolic links of its existing prefix
// resolved, and no trailing separator.
platform::Path resolve(const platform::Path& path) {
  std::error_code error;
  platform::Path resolved = std::filesystem::weakly_canonical(path, error);
  if (error) {
    resolved = path;
  }
  resolved = resolved.lexically_normal();
  if (!resolved.has_filename() && resolved.has_parent_path() && resolved != resolved.root_path()) {
    resolved = resolved.parent_path();
  }
  return resolved;
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

}  // namespace

std::vector<std::string> default_removed_commands() {
  return {"exec", "socket", "load", "unload", "exit"};
}

SandboxFilesystem::SandboxFilesystem(SandboxPolicy policy) : policy_(std::move(policy)) {
  for (const auto& root : policy_.granted_roots) {
    if (root.is_absolute()) {
      roots_.push_back(resolve(root));
    }
  }
}

PathDecision SandboxFilesystem::check(std::string_view operation, std::string_view path,
                                      const platform::Path& cwd) const {
  static_cast<void>(operation);  // the decision depends on the path only
  if (path.empty() || path.front() == '~') {
    return PathDecision{false, platform::Path{path}};
  }
  const platform::Path requested{path};
  const platform::Path normalized = resolve(requested.is_absolute() ? requested : cwd / requested);
  const bool allowed = std::any_of(roots_.begin(), roots_.end(),
                                   [&](const platform::Path& root) { return is_under(normalized, root); });
  return PathDecision{allowed, normalized};
}

bool SandboxFilesystem::is_root_ancestor(std::string_view normalized) const {
  const platform::Path path = platform::Path{normalized}.lexically_normal();
  return std::any_of(roots_.begin(), roots_.end(), [&](const platform::Path& root) {
    return root != path && is_under(root, path);
  });
}

bool is_embedded_path(std::string_view path) noexcept {
  constexpr std::string_view kTop = "/__hefesto_embedded__";
  static_assert(kVirtualLibraryRoot.substr(0, kTop.size()) == kTop);
  return path.substr(0, kTop.size()) == kTop && (path.size() == kTop.size() || path[kTop.size()] == '/');
}

void set_filesystem_audit(std::function<void(const FilesystemAccess&)> audit) {
  tcl_detail::filesystem_audit() = std::move(audit);
}

}  // namespace hefesto::script
