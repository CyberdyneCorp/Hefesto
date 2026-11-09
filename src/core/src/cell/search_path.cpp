// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Search paths, cell file lookup, use-path writing and library discovery
// (cell-library-resolution). Ported from Magic 8.3.694 utils/path.c
// (PaExpand, nextName, PaLockZOpen) and database/DBio.c (dbReadOpen,
// DBPathSubstitute, dbWriteCellFunc, dbSearchForTech, DBAddStandardCellPaths);
// Magic is (c) the Regents of the University of California, distributed
// under its permissive license (THIRD_PARTY_NOTICES.md).

#include "hefesto/cell/search_path.hpp"

#include <algorithm>
#include <cctype>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/paths.hpp"

namespace hefesto::cell {

namespace {

constexpr std::string_view kSuffix = ".mag";
constexpr int kMaxDepth = 10;

bool is_space(char c) {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool ends_entry(char c) {
  return is_space(c) || c == '/' || c == ':';
}

void warn(diag::DiagnosticSink& sink, std::string_view code, std::string text) {
  sink.emit(diag::make(code, std::move(text)));
}

/// The text after a `~` or `$` prefix up to '/', ':', whitespace or end.
std::size_t name_end(std::string_view s, std::size_t from) {
  std::size_t i = from;
  while (i < s.size() && !ends_entry(s[i])) {
    ++i;
  }
  return i;
}

std::optional<std::string> expand_tilde(std::string_view s, const PathVariables& vars) {
  const std::size_t end = name_end(s, 1);
  const auto home = vars.home(s.substr(1, end - 1));
  if (!home) {
    return std::nullopt;
  }
  return *home + std::string{s.substr(end)};
}

std::optional<std::string> expand_dollar(std::string_view s, const PathVariables& vars) {
  const std::size_t end = name_end(s, 1);
  std::string name;
  for (char c : s.substr(1, end - 1)) {
    if (c != '{' && c != '}') {
      name += c;
    }
  }
  const auto value = vars.get(name);
  if (!value) {
    return std::nullopt;
  }
  return *value + std::string{s.substr(end)};
}

std::string strip_dot_slash(std::string_view s) {
  while (!s.empty() && s.front() == '.') {
    if (s.size() > 1 && s[1] == '/') {
      s.remove_prefix(2);
      continue;
    }
    if (s.size() == 1) {
      s.remove_prefix(1);
    }
    break;
  }
  return std::string{s};
}

bool opens_directly(std::string_view f) {
  return f.starts_with("/") || f == "." || f == ".." || f.starts_with("./") || f.starts_with("../");
}

std::string candidate_in(const std::string& dir, const std::string& file) {
  if (dir.empty() || dir.back() == '/') {
    return dir + file;
  }
  return dir + "/" + file;
}

std::optional<FoundFile> search_list(LookupContext& ctx, const std::string& file,
                                     const std::vector<std::string>& entries) {
  for (const std::string& entry : entries) {
    for (const std::string& piece : split_search_path(entry)) {
      const auto dir = expand_entry(piece, ctx.vars);
      if (!dir) {
        continue;  // unknown user: nothing tried for this entry
      }
      if (auto found = open_candidate(ctx, candidate_in(*dir, file))) {
        found->via_search_path = true;
        return found;
      }
    }
  }
  return std::nullopt;
}

std::string strip_mag(std::string_view recorded) {
  const std::size_t slash = recorded.rfind('/');
  const std::size_t base = slash == std::string_view::npos ? 0 : slash + 1;
  const std::size_t dot = recorded.rfind('.');
  if (dot != std::string_view::npos && dot >= base && recorded.substr(dot) == kSuffix) {
    return std::string{recorded.substr(0, dot)};
  }
  return std::string{recorded};
}

std::optional<FoundFile> direct(LookupContext& ctx, const std::string& stem) {
  if (auto f = search_file(ctx, stem + ".mag", {"."}, nullptr)) {
    return f;
  }
  return search_file(ctx, stem + ".mag.gz", {"."}, nullptr);
}

std::optional<FoundFile> through_paths(LookupContext& ctx, std::string_view name) {
  const std::string n{name};
  if (auto f = search_file(ctx, n + ".mag", ctx.paths.path, &ctx.paths.library)) {
    return f;
  }
  return search_file(ctx, n + ".mag.gz", ctx.paths.path, &ctx.paths.library);  // Q-CELL-18: the name
}

std::string dir_of(std::string_view file) {
  const std::size_t slash = file.rfind('/');
  return slash == std::string_view::npos ? std::string{} : std::string{file.substr(0, slash)};
}

/// Step 2 of "Writing instance paths": cf with the leading components it
/// shares with pf stripped, cut at cf's last '/'; nullopt for none.
std::optional<std::string> stripped_child_dir(std::string_view cf, std::string_view pf) {
  const std::size_t last = cf.rfind('/');
  if (last == std::string_view::npos) {
    return std::nullopt;
  }
  std::size_t start = 0;
  for (std::size_t slash = cf.find('/'); slash != std::string_view::npos; slash = cf.find('/', start)) {
    const std::size_t len = slash + 1;
    if (pf.substr(0, len) != cf.substr(0, len)) {
      break;
    }
    start = slash + 1;
  }
  if (!cf.empty() && cf.front() == '/' && start == 1) {
    start = 0;  // only the root was shared: keep the absolute path
  }
  if (start >= last) {
    return std::nullopt;
  }
  return std::string{cf.substr(start, last - start)};
}

bool is_rooted_form(std::string_view s) {
  return !s.empty() && (s.front() == '/' || s.front() == '~' || s.front() == '$');
}

std::string parent_dir_for_write(std::string_view pf) {
  const std::size_t slash = pf.rfind('/');
  if (slash == std::string_view::npos) {
    return {};
  }
  if (slash == 0) {
    return "/";
  }
  return std::string{pf.substr(0, slash)};
}

bool ends_with_mag(const platform::Path& p) {
  const std::string name = p.filename().string();
  return name.size() > kSuffix.size() && name.ends_with(kSuffix);
}

void discover_in(platform::FileSystem& fs, FileHost& host, const platform::Path& dir, int depth,
                 std::vector<std::string>& out) {
  if (depth > kMaxDepth) {
    return;
  }
  auto entries = fs.list(dir);
  if (!entries.ok()) {
    return;
  }
  bool added = false;
  for (const platform::Path& entry : entries.value()) {
    if (host.is_directory(entry)) {
      if (!host.is_symlink(entry)) {
        discover_in(fs, host, entry, depth + 1, out);
      }
    } else if (!added && ends_with_mag(entry)) {
      out.push_back(dir.generic_string());
      added = true;
    }
  }
}

std::optional<platform::Path> tech_in(platform::FileSystem& fs, FileHost& host, const platform::Path& dir,
                                      std::string_view tech, int depth) {
  if (depth > kMaxDepth) {
    return std::nullopt;
  }
  auto entries = fs.list(dir);
  if (!entries.ok()) {
    return std::nullopt;
  }
  std::vector<platform::Path> order = entries.value();
  std::stable_partition(order.begin(), order.end(),
                        [&](const platform::Path& p) { return p.filename().string() == tech; });
  const std::string file = std::string{tech} + ".tech";
  for (const platform::Path& entry : order) {
    const bool is_dir = host.is_directory(entry);
    if (!is_dir && entry.filename().string() == file) {
      return dir;
    }
    if (is_dir && !host.is_symlink(entry)) {
      if (auto found = tech_in(fs, host, entry, tech, depth + 1)) {
        return found;
      }
    }
  }
  return std::nullopt;
}

}  // namespace

std::optional<std::string> PathVariables::get(std::string_view name) const {
  if (variable) {
    if (auto v = variable(name)) {
      return v;
    }
  }
  if (name == "CAD_ROOT" && !install_root.empty()) {
    return install_root.generic_string();
  }
  return std::nullopt;
}

std::optional<std::string> PathVariables::home(std::string_view user) const {
  if (home_of) {
    return home_of(user);
  }
  if (user.empty() && variable) {
    return variable("HOME");
  }
  return std::nullopt;
}

std::vector<std::string> split_search_path(std::string_view text) {
  std::vector<std::string> entries;
  std::string current;
  for (const char c : text) {
    if (c == ':' || is_space(c)) {
      if (!current.empty()) {
        entries.push_back(std::move(current));
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) {
    entries.push_back(std::move(current));
  }
  return entries;
}

std::string join_search_path(const std::vector<std::string>& entries) {
  std::string out;
  for (const std::string& e : entries) {
    if (!out.empty()) {
      out += ' ';
    }
    out += e;
  }
  return out;
}

std::optional<std::string> expand_entry(std::string_view entry, const PathVariables& vars) {
  if (entry.starts_with("~")) {
    return expand_tilde(entry, vars);
  }
  if (entry.starts_with("$")) {
    if (auto expanded = expand_dollar(entry, vars)) {
      return expanded;
    }
    return std::string{entry};  // undefined: kept literally
  }
  return strip_dot_slash(entry);
}

std::optional<FoundFile> open_candidate(LookupContext& ctx, const std::string& name) {
  const std::string gz = name + ".gz";
  if (ctx.fs.exists(name)) {
    if (!name.ends_with(".gz") && ctx.fs.exists(gz)) {
      warn(ctx.sink, codes::kPathFallback,
           "Both " + name + " and " + gz + " exist; reading " + name + " (the compressed file is ignored).");
    }
    return FoundFile{name, name, false};
  }
  if (!name.ends_with(".gz") && ctx.fs.exists(gz)) {
    return FoundFile{gz, name, false};
  }
  return std::nullopt;
}

std::optional<FoundFile> search_file(LookupContext& ctx, const std::string& file,
                                     const std::vector<std::string>& path,
                                     const std::vector<std::string>* library) {
  if (file.empty()) {
    return std::nullopt;
  }
  if (file.front() == '~' || file.front() == '$') {
    const auto expanded = file.front() == '~' ? expand_tilde(file, ctx.vars) : expand_dollar(file, ctx.vars);
    if (!expanded) {
      return file.front() == '$' ? open_candidate(ctx, file) : std::nullopt;
    }
    return open_candidate(ctx, *expanded);
  }
  if (opens_directly(file)) {
    return open_candidate(ctx, file);
  }
  if (auto found = search_list(ctx, file, path)) {
    return found;
  }
  if (library == nullptr) {
    return std::nullopt;
  }
  return search_list(ctx, file, *library);
}

std::optional<FoundFile> find_cell_file(LookupContext& ctx, std::string_view name, std::string_view recorded,
                                        bool dereference) {
  if (recorded.empty()) {
    return through_paths(ctx, name);
  }
  const std::string stem = strip_mag(recorded);
  if (!dereference) {
    if (auto f = direct(ctx, stem)) {
      return f;
    }
  }
  if (auto f = through_paths(ctx, name)) {
    if (!dereference) {
      warn(ctx.sink, codes::kPathFallback,
           "Warning:  Parent cell lists instance of \"" + std::string{name} + "\" at bad file path " +
               std::string{recorded} + ".");
      warn(ctx.sink, codes::kPathFallback, "The cell exists in the search paths at " + f->recorded + ".");
      warn(ctx.sink, codes::kPathFallback, "The discovered version will be used.");
    }
    return f;
  }
  if (!dereference) {
    return std::nullopt;
  }
  auto f = direct(ctx, stem);
  if (f) {
    warn(ctx.sink, codes::kPathFallback,
         "Warning:  Dereferenced cell \"" + std::string{name} + "\" not found in search paths;  using original "
                                                                "location " +
             std::string{recorded} + ".");
  }
  return f;
}

std::string substitute_path(std::string_view s, std::string_view child_file, const PathVariables& vars,
                            bool magic_compat) {
  for (const char* var : {"PDK_PATH", "PDKPATH", "PDK_ROOT", "PDKROOT"}) {
    const auto value = vars.get(var);
    if (!value || !s.starts_with(*value)) {
      continue;
    }
    const std::string_view rest = s.substr(value->size());
    if (!rest.empty() && rest.front() != '/') {
      continue;  // Q-CELL-20: a path prefix, not a character prefix
    }
    return "$" + std::string{var} + std::string{rest};
  }
  if (s.empty() || s.front() != '/') {
    return std::string{s};
  }
  const auto home = vars.home();
  const std::string_view tested = magic_compat ? child_file : s;  // Q-CELL-21
  if (home && !home->empty() && tested.starts_with(*home) && tested.size() > home->size() &&
      tested[home->size()] == '/') {
    return "~" + std::string{tested.substr(home->size())};
  }
  return std::string{s};
}

std::optional<std::string> use_path_for_write(const UsePathRequest& request, const PathVariables& vars,
                                              FileHost& host) {
  const std::string_view cf = request.child_file;
  if (request.already_written || cf.empty()) {
    return std::nullopt;
  }
  std::optional<std::string> path;
  if (const auto stripped = stripped_child_dir(cf, request.parent_file); stripped && !stripped->empty()) {
    path = substitute_path(*stripped, dir_of(cf), vars, request.magic_compat);
  }
  if (path && is_rooted_form(*path)) {
    return path;
  }
  const std::string cdir = dir_of(cf);
  const std::string pdir = parent_dir_for_write(request.parent_file);
  if (is_dir_prefix(pdir, cdir)) {
    return path;
  }
  const auto rel = relative_dir(pdir.empty() ? "." : pdir, cdir.empty() ? "." : cdir, host);
  if (rel) {
    return rel->empty() ? std::nullopt : rel;
  }
  if (const auto abs = absolute_dir(cdir, host)) {
    return substitute_path(*abs, dir_of(cf), vars, request.magic_compat);
  }
  return path;
}

std::vector<std::string> discover_library_paths(platform::FileSystem& fs, FileHost& host,
                                                const platform::Path& root) {
  std::vector<std::string> out;
  discover_in(fs, host, root, 0, out);
  return out;
}

std::optional<platform::Path> find_technology_dir(platform::FileSystem& fs, FileHost& host,
                                                  const platform::Path& root, std::string_view tech) {
  return tech_in(fs, host, root, tech, 0);
}

std::vector<platform::Path> technology_search_roots(const PathVariables& vars) {
  std::vector<platform::Path> roots;
  const auto add = [&roots](const std::string& root) {
    if (!root.empty() && std::find(roots.begin(), roots.end(), platform::Path{root}) == roots.end()) {
      roots.emplace_back(root);
    }
  };
  for (const std::string_view name : {"PDK_PATH", "PDKPATH", "PDK_ROOT", "PDKROOT"}) {
    add(vars.get(name).value_or(""));
  }
  add("/usr/local/share/pdk");
  return roots;
}

std::optional<platform::Path> locate_technology_dir(platform::FileSystem& fs, FileHost& host,
                                                    const PathVariables& vars, std::string_view tech) {
  for (const platform::Path& root : technology_search_roots(vars)) {
    if (auto dir = find_technology_dir(fs, host, root, tech)) {
      return dir;
    }
  }
  return std::nullopt;
}

std::optional<std::string> libs_ref_root(std::string_view tech_dir) {
  const std::size_t at = tech_dir.find("libs.tech");
  if (at == std::string_view::npos) {
    return std::nullopt;
  }
  return std::string{tech_dir.substr(0, at)} + "libs.ref";
}

}  // namespace hefesto::cell
