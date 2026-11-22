// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The symbol library search path and the `xschemrc` read as data
// (openspec/changes/m8-schematic design.md D7, "Area contracts" SYMLIB;
// capability schematic-symbols-and-libraries "Library search path",
// "xschemrc as data", Q-SCHEM-23, Q-SCHEM-25).
//
// Owner: SYMLIB.

#pragma once

#include <chrono>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::schem {

struct XschemrcResult;
struct XschemrcInput;

/// The search-path element that stands for the folder of the schematic that
/// contains the instance (Q-SCHEM-30), never the folder Hefesto started in.
inline constexpr std::string_view kDocumentFolderElement = ".";

/// A `<folder> | <alias>` element of XSCHEM_LIBRARY_PATH.
struct LibraryAlias {
  std::string alias;
  platform::Path folder;

  friend bool operator==(const LibraryAlias&, const LibraryAlias&) = default;
};

/// The ordered library directories symbol references are looked up in, and
/// the library aliases. A directory equal to kDocumentFolderElement means
/// the containing schematic's folder; any other relative directory names a
/// folder under it (Q-SCHEM-38 option (a), approved 2026-11-22).
class SearchPath {
 public:
  SearchPath() = default;
  explicit SearchPath(std::vector<platform::Path> directories, std::vector<LibraryAlias> aliases = {})
      : directories_(std::move(directories)), aliases_(std::move(aliases)) {}

  [[nodiscard]] const std::vector<platform::Path>& directories() const noexcept { return directories_; }
  [[nodiscard]] const std::vector<LibraryAlias>& aliases() const noexcept { return aliases_; }

  /// Builds the search path from an XSCHEM_LIBRARY_PATH value ("Library
  /// search path"): split at `:`, `<folder> | <alias>` elements define
  /// aliases (`\|` is an escaped bar) and stay in the path, `#` elements are
  /// dropped, a leading `~` or `~/` is replaced by `env` HOME (no other
  /// expansion: `$NAME` is literal text), each element is cleaned as step 4
  /// of "Symbol references", duplicates are kept, and elements that do not
  /// exist in `fs` are dropped. A relative element other than `.` names a
  /// folder under `document_folder` (Q-SCHEM-38 option (a), approved 2026-11-22): it is kept as
  /// written when that folder exists, and dropped when it does not or when
  /// `document_folder` is empty. An empty result is the document folder.
  [[nodiscard]] static SearchPath parse(std::string_view value, const std::map<std::string, std::string>& env,
                                        platform::FileSystem& fs, const platform::Path& document_folder = {});

  /// Runs `input.rc_file` as data in a fresh sandboxed interpreter with the
  /// synthetic `env()` of Q-SCHEM-23 and returns the search path and the
  /// global variables it set. `xschem_execute_scripts` is read, ignored and
  /// logged (Q-SCHEM-25). The evaluation stops after `input.time_limit`
  /// (SCHEM-LIB-010, Q-SCHEM-32).
  [[nodiscard]] static XschemrcResult from_xschemrc(platform::FileSystem& fs, const XschemrcInput& input);

  /// `front`'s directories and aliases, then this path's (the Hefesto
  /// settings path that is prepended to the `xschemrc` one).
  [[nodiscard]] SearchPath prepended(const SearchPath& front) const;

  friend bool operator==(const SearchPath&, const SearchPath&) = default;

 private:
  std::vector<platform::Path> directories_;
  std::vector<LibraryAlias> aliases_;
};

/// The longest evaluation of the `xschemrc` files of one document (input
/// limit approved 2026-11-21, Q-SCHEM-32).
inline constexpr std::chrono::milliseconds kXschemrcTimeLimit{10'000};

struct XschemrcInput {
  platform::Path rc_file;
  /// The synthetic `env()` entries (Q-SCHEM-23), e.g. PDK_ROOT and PDK.
  /// HOME and PWD default to `document_folder` when absent.
  std::map<std::string, std::string> env;
  /// The folder of the document being opened. Its own `xschemrc`, when it
  /// exists, is evaluated after `rc_file`.
  platform::Path document_folder;
  /// Tcl globals set before evaluation ("xschemrc as data"):
  /// XSCHEM_SHAREDIR, USER_CONF_DIR, PDK_ROOT, PDK.
  std::map<std::string, std::string> variables;
  /// The evaluation of both files stops after this long, in both modes of
  /// `xschem-compat`, with SCHEM-LIB-010; the settings made before the stop
  /// are kept. Tests shorten it.
  std::chrono::milliseconds time_limit = kXschemrcTimeLimit;
};

/// The input for the active PDK `pdk` installed under `pdk_root`: its
/// `<pdk_root>/<pdk>/libs.tech/xschem/xschemrc`, the variables XSCHEM_SHAREDIR,
/// USER_CONF_DIR, PDK_ROOT and PDK, and env(PDK_ROOT), env(PDK), env(HOME)
/// and env(PWD) (HOME and PWD: the document folder).
[[nodiscard]] XschemrcInput pdk_xschemrc_input(const platform::Path& pdk_root, std::string_view pdk,
                                               const platform::Path& sharedir, const platform::Path& user_conf_dir,
                                               const platform::Path& document_folder);

struct XschemrcResult {
  SearchPath search_path;
  /// Global Tcl variables the rc files set that `tcleval` values read
  /// (e.g. SKYWATER_MODELS), by name; scalars only.
  std::map<std::string, std::string> variables;
  /// Errors, the time-limit stop and the Q-SCHEM-25 log line.
  std::vector<diag::Diagnostic> diagnostics;
};

}  // namespace hefesto::schem
