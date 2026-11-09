// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Path normalization algorithms (cell-library-resolution "Path
// normalization algorithms", "Rebasing paths on working-directory change").
// Ported from Magic 8.3.694 database/DBio.c (dbPathComps*, dbCollapsePath,
// dbAbsoluteComps, dbIsDirPrefix, dbRelativeDir, dbAbsoluteDir,
// dbRebasePathFunc); Magic is (c) the Regents of the University of
// California, distributed under its permissive license (THIRD_PARTY_NOTICES.md).

#include "hefesto/cell/paths.hpp"

#include <algorithm>

namespace hefesto::cell {

namespace {

std::string join_range(const std::vector<std::string>& comps, std::size_t start, std::size_t end, bool absolute) {
  std::string out = absolute ? "/" : "";
  for (std::size_t i = start; i < end; ++i) {
    if (i > start) {
      out += '/';
    }
    out += comps[i];
  }
  return out;
}

bool starts_tilde_or_dollar(std::string_view p) {
  return !p.empty() && (p.front() == '~' || p.front() == '$');
}

void split_into(PathComps& pc, std::string_view path) {
  if (pc.comps.empty() && !path.empty() && path.front() == '/') {
    pc.absolute = true;
  }
  std::size_t p = 0;
  while (p < path.size()) {
    while (p < path.size() && path[p] == '/') {
      ++p;
    }
    if (p >= path.size()) {
      break;
    }
    std::size_t q = p;
    while (q < path.size() && path[q] != '/') {
      ++q;
    }
    const std::string_view comp = path.substr(p, q - p);
    if (comp != ".") {
      pc.comps.emplace_back(comp);
    }
    p = q;
  }
}

}  // namespace

PathComps split_path(std::string_view path) {
  PathComps pc;
  split_into(pc, path);
  return pc;
}

std::string join_path(const PathComps& path) {
  return join_range(path.comps, 0, path.comps.size(), path.absolute);
}

std::string collapse_path(std::string_view path, FileHost& host) {
  if (path.empty()) {
    return {};
  }
  const bool absolute = path.front() == '/';
  const PathComps pc = split_path(path);
  std::vector<std::string> out;
  for (const std::string& comp : pc.comps) {
    if (comp == ".." && !out.empty() && out.back() != "..") {
      if (!host.is_symlink(join_range(out, 0, out.size(), absolute))) {
        out.pop_back();
        continue;
      }
    } else if (comp == ".." && absolute && out.empty()) {
      continue;  // "/.." is "/"
    }
    out.push_back(comp);
  }
  std::string result = join_range(out, 0, out.size(), absolute);
  return result.empty() ? std::string{"."} : result;
}

std::optional<PathComps> absolute_comps(std::string_view path, FileHost& host) {
  if (starts_tilde_or_dollar(path)) {
    return std::nullopt;
  }
  PathComps pc;
  if (path.empty() || path.front() != '/') {
    const platform::Path cwd = host.current_directory();
    if (cwd.empty()) {
      return std::nullopt;
    }
    split_into(pc, cwd.generic_string());
  }
  pc.absolute = true;
  const bool rooted = !path.empty() && path.front() == '/';
  bool leading = true;
  for (const std::string& comp : split_path(path).comps) {
    if (comp != "..") {
      leading = false;
      pc.comps.push_back(comp);
      continue;
    }
    if (!leading || rooted || pc.comps.empty()) {
      return std::nullopt;
    }
    pc.comps.pop_back();
  }
  return pc;
}

bool is_dir_prefix(std::string_view pdir, std::string_view cdir) {
  const PathComps p = split_path(pdir);
  const PathComps c = split_path(cdir);
  if (p.absolute != c.absolute || p.comps.size() > c.comps.size()) {
    return false;
  }
  return std::equal(p.comps.begin(), p.comps.end(), c.comps.begin());
}

std::optional<std::string> relative_dir(std::string_view pdir, std::string_view cdir, FileHost& host) {
  const auto p = absolute_comps(pdir, host);
  const auto c = absolute_comps(cdir, host);
  if (!p || !c) {
    return std::nullopt;
  }
  std::size_t m = 0;
  while (m < p->comps.size() && m < c->comps.size() && p->comps[m] == c->comps[m]) {
    ++m;
  }
  if (m == 0) {
    return std::nullopt;
  }
  for (std::size_t i = m; i < p->comps.size(); ++i) {
    if (host.is_symlink(join_range(p->comps, 0, i + 1, true))) {
      return std::nullopt;
    }
  }
  std::vector<std::string> out(p->comps.size() - m, "..");
  out.insert(out.end(), c->comps.begin() + static_cast<std::ptrdiff_t>(m), c->comps.end());
  return join_range(out, 0, out.size(), false);
}

std::optional<std::string> absolute_dir(std::string_view dir, FileHost& host) {
  if (starts_tilde_or_dollar(dir)) {
    return std::nullopt;
  }
  if (!dir.empty() && dir.front() == '/') {
    return std::string{dir};
  }
  const platform::Path cwd = host.current_directory();
  if (cwd.empty()) {
    return std::nullopt;
  }
  std::string joined = cwd.generic_string();
  if (!dir.empty()) {
    joined += '/';
    joined += dir;
  }
  return collapse_path(joined, host);
}

std::string rebase_path(std::string_view path, std::string_view old_dir, std::string_view new_dir) {
  if (path.empty() || path.front() == '/' || starts_tilde_or_dollar(path) || old_dir == new_dir) {
    return std::string{path};
  }
  const PathComps r = split_path(path);
  const PathComps old_c = split_path(old_dir);
  const PathComps new_c = split_path(new_dir);
  std::size_t k = 0;
  while (k < r.comps.size() && r.comps[k] == "..") {
    ++k;
  }
  if (k > old_c.comps.size()) {
    return std::string{path};
  }
  const std::size_t base = old_c.comps.size() - k;
  std::size_t m = 0;
  while (m < base && m < new_c.comps.size() && old_c.comps[m] == new_c.comps[m]) {
    ++m;
  }
  std::vector<std::string> out(new_c.comps.size() - m, "..");
  out.insert(out.end(), old_c.comps.begin() + static_cast<std::ptrdiff_t>(m),
             old_c.comps.begin() + static_cast<std::ptrdiff_t>(base));
  out.insert(out.end(), r.comps.begin() + static_cast<std::ptrdiff_t>(k), r.comps.end());
  return join_range(out, 0, out.size(), false);
}

}  // namespace hefesto::cell
