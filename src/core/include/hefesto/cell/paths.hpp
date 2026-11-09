// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Component-based path operations of capability cell-library-resolution
// ("Path normalization algorithms", "Rebasing paths on working-directory
// change", "Path variable substitution on write") on plain strings, as
// Magic stores cell file paths. Symbolic-link tests and the working
// directory come from a FileHost.
// (Magic ref: database/DBio.c:1729-2362, database/DBio.c:6059-6111)
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cell/host.hpp"

namespace hefesto::cell {

/// "Common splitting rule": components without empty and "." entries.
struct PathComps {
  bool absolute = false;
  std::vector<std::string> comps;
};
[[nodiscard]] PathComps split_path(std::string_view path);
[[nodiscard]] std::string join_path(const PathComps& path);

/// collapse(p): `..` removes the previous component unless that prefix is a
/// symbolic link; "/.." is "/"; an empty result is ".".
[[nodiscard]] std::string collapse_path(std::string_view path, FileHost& host);

/// absComps(p); nullopt for `~`/`$` paths or a non-leading `..`.
[[nodiscard]] std::optional<PathComps> absolute_comps(std::string_view path, FileHost& host);

/// relDir(pdir, cdir); nullopt when it fails.
[[nodiscard]] std::optional<std::string> relative_dir(std::string_view pdir, std::string_view cdir, FileHost& host);

/// absDir(d); nullopt for `~`/`$` paths or an unknown working directory.
[[nodiscard]] std::optional<std::string> absolute_dir(std::string_view dir, FileHost& host);

/// isDirPrefix(pdir, cdir).
[[nodiscard]] bool is_dir_prefix(std::string_view pdir, std::string_view cdir);

/// "Rebasing paths on working-directory change" for one recorded path;
/// returns the path unchanged when it is absolute, `~`/`$`, or climbs
/// above the old directory.
[[nodiscard]] std::string rebase_path(std::string_view path, std::string_view old_dir, std::string_view new_dir);

}  // namespace hefesto::cell
