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

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::schem {

struct XschemrcResult;
struct XschemrcInput;

/// The ordered library directories symbol references are looked up in.
class SearchPath {
 public:
  SearchPath() = default;
  explicit SearchPath(std::vector<platform::Path> directories) : directories_(std::move(directories)) {}

  [[nodiscard]] const std::vector<platform::Path>& directories() const noexcept { return directories_; }

  /// Runs `input.rc_file` as data in a fresh sandboxed interpreter with the
  /// synthetic `env()` of Q-SCHEM-23 and returns the search path and the
  /// variables the inventory lists. `xschem_execute_scripts` is read,
  /// ignored and logged (Q-SCHEM-25).
  [[nodiscard]] static XschemrcResult from_xschemrc(platform::FileSystem& fs, const XschemrcInput& input);

  friend bool operator==(const SearchPath&, const SearchPath&) = default;

 private:
  std::vector<platform::Path> directories_;
};

struct XschemrcInput {
  platform::Path rc_file;
  /// The synthetic `env()` entries (Q-SCHEM-23), e.g. PDK_ROOT and PDK.
  std::map<std::string, std::string> env;
  /// The folder of the document being opened.
  platform::Path document_folder;
};

struct XschemrcResult {
  SearchPath search_path;
  /// Global Tcl variables the rc file set that `tcleval` values read
  /// (e.g. SKYWATER_MODELS), by name.
  std::map<std::string, std::string> variables;
  /// Errors and the Q-SCHEM-25 log line.
  std::vector<diag::Diagnostic> diagnostics;
};

}  // namespace hefesto::schem
