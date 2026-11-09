// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cell search paths and file lookup (capability cell-library-resolution:
// "Search path syntax and expansion", "Cell file lookup order", "Writing
// instance paths", "Path variable substitution on write", "Standard-cell
// library path discovery", "Automatic technology discovery" search).
// (Magic ref: utils/path.c, database/DBio.c:223-391, database/DBio.c:1447-1662,
// database/DBio.c:6059-6243)
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cell/host.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::cell {

/// Resolves `$NAME` / `${NAME}` and `~user` during expansion. Hosts bind it
/// to the session's path-variable table (Q-CELL-17: seeded from the
/// environment and PDK startup scripts on the desktop).
struct PathVariables {
  std::function<std::optional<std::string>(std::string_view name)> variable;
  /// Home directory of `user`; "" asks for the current user (HOME).
  std::function<std::optional<std::string>(std::string_view user)> home_of;
  platform::Path install_root;  // CAD_ROOT fallback

  [[nodiscard]] std::optional<std::string> get(std::string_view name) const;
  [[nodiscard]] std::optional<std::string> home(std::string_view user = {}) const;
};

/// The two cell search paths, Path and the library path, plus the system
/// path (startup files and fonts; `path sys`).
struct SearchPaths {
  std::vector<std::string> path{"."};  // unexpanded entries, in order
  std::vector<std::string> library;    // unexpanded entries, in order
  std::vector<std::string> system;     // unexpanded entries, in order
};

/// Splits a search-path string on runs of spaces, tabs or colons.
[[nodiscard]] std::vector<std::string> split_search_path(std::string_view text);
/// The entries joined with single spaces (the `path` command output).
[[nodiscard]] std::string join_search_path(const std::vector<std::string>& entries);

/// Expands one entry (`~`, `~user`, `$NAME`, `${NAME}`, leading `./`);
/// std::nullopt when it is unusable (unknown user).
[[nodiscard]] std::optional<std::string> expand_entry(std::string_view entry, const PathVariables& vars);

/// A located cell file.
struct FoundFile {
  platform::Path path;           // the file opened (may end in .gz)
  std::string recorded;          // the candidate name to record (Magic's realName)
  bool via_search_path = false;  // found through Path or the library path
};

/// What find_cell_file needs.
struct LookupContext {
  platform::FileSystem& fs;
  const SearchPaths& paths;
  const PathVariables& vars;
  diag::DiagnosticSink& sink;
};

/// Opens one candidate name with the compressed-counterpart rule (Q-CELL-16):
/// `name` if it exists (warning when `name.gz` exists too), else `name.gz`.
[[nodiscard]] std::optional<FoundFile> open_candidate(LookupContext& ctx, const std::string& name);

/// Searches `file` (extension included) through a search path, then the
/// library path, as PaLockZOpen does: names starting with `/`, `./`, `../`
/// (or exactly `.`/`..`) are opened directly, `~`/`$` names are expanded
/// and opened directly.
[[nodiscard]] std::optional<FoundFile> search_file(LookupContext& ctx, const std::string& file,
                                                   const std::vector<std::string>& path,
                                                   const std::vector<std::string>* library);

/// "Cell file lookup order" for cell `name` with recorded path `recorded`
/// (empty when none). Emits the bad-path and dereference warnings.
[[nodiscard]] std::optional<FoundFile> find_cell_file(LookupContext& ctx, std::string_view name,
                                                      std::string_view recorded, bool dereference);

/// "Path variable substitution on write": subst(s). `child_file` is the
/// child's recorded path (Magic tests it for the HOME rule, Q-CELL-21;
/// `magic_compat` false tests `s`).
[[nodiscard]] std::string substitute_path(std::string_view s, std::string_view child_file, const PathVariables& vars,
                                          bool magic_compat);

/// Inputs of "Writing instance paths" for one use line.
struct UsePathRequest {
  std::string_view child_file;   // cf: the child's recorded path ("" = unset)
  std::string_view parent_file;  // pf: the parent's recorded path, or its name
  bool already_written = false;  // the child got a path earlier in this file
  bool magic_compat = true;
};

/// "Writing instance paths": the path token, or nullopt for none.
[[nodiscard]] std::optional<std::string> use_path_for_write(const UsePathRequest& request, const PathVariables& vars,
                                                            FileHost& host);

/// "Standard-cell library path discovery": the directories at depth <= 10
/// below `root` that directly contain a `.mag` file, in Magic's order (a
/// directory is appended when its first `.mag` entry is met; entries are
/// visited sorted by name, Q-CELL-40).
[[nodiscard]] std::vector<std::string> discover_library_paths(platform::FileSystem& fs, FileHost& host,
                                                              const platform::Path& root);

/// "Automatic technology discovery" step 2: the directory below `root`
/// holding `<tech>.tech` (depth <= 10; an entry named exactly `tech` first,
/// then the others sorted by name, Q-CELL-40); nullopt when absent.
[[nodiscard]] std::optional<platform::Path> find_technology_dir(platform::FileSystem& fs, FileHost& host,
                                                                const platform::Path& root, std::string_view tech);

/// Step 6: a found `.../libs.tech/...` directory truncated to end at
/// `libs.ref`; nullopt when it does not contain `libs.tech`.
[[nodiscard]] std::optional<std::string> libs_ref_root(std::string_view tech_dir);

}  // namespace hefesto::cell
