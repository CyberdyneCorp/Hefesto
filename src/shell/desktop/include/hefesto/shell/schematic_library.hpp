// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The symbol library browser of the desktop schematic editor (board
// docs/design/mockups/Schematic.dc.html, "Symbols" panel with "Search
// symbols"; capability schematic-symbols-and-libraries "Library search
// path", "System library"; schematic-editing "Placing instances";
// openspec/changes/m8-schematic design D8, D15):
//
// - the folders are the session's search path (`schematic path list`), then
//   the system library `<install root>/assets/schematic/xschem_library/devices`
//   when the search path does not already hold it;
// - each folder lists its `.sym` files by name; the search field filters
//   them (case-insensitive substring of the file name);
// - placing a row issues `schematic place <reference> <x> <y>`: the
//   reference is the file name (the system library is reachable as `<name>`
//   and `devices/<name>`; the search path finds the others by name).
//
// It also holds the desktop app's install root (M8-D10, approved
// 2026-11-23): CAD_ROOT when set, else the install root holding the
// executable (app::install_root_for_executable).
//
// Raven-free and unit-tested in tests/unit/shell/schematic_library_test.cpp.
// Owner: SHELL (openspec/changes/m8-schematic design D3, D15; task 13.2).

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/document.hpp"

namespace hefesto::shell {

/// `<install_root>/assets/schematic/xschem_library/devices` (design D8;
/// `${XSCHEM_SHAREDIR}/xschem_library/devices`); empty for an empty root.
[[nodiscard]] platform::Path system_library_dir(const platform::Path& install_root);

/// The desktop app's installation root (M8-D10): `cad_root_env` when set
/// and non-empty, else the install root of the executable `program`
/// (argv[0], made canonical as the asset loader does), else empty.
[[nodiscard]] platform::Path desktop_install_root(platform::FileSystem& fs, const platform::Path& program,
                                                  const std::optional<std::string>& cad_root_env);

/// The folders of `schematic path list`'s result (a Tcl list); empty when
/// the text is not a list.
[[nodiscard]] std::vector<platform::Path> parse_path_list(std::string_view output);

struct LibraryFolder {
  platform::Path dir;
  std::string label;    // the folder's name ("devices", "sky130_fd_pr")
  bool system = false;  // the shipped system library
};

/// The search path in order, then the system library unless already in it.
[[nodiscard]] std::vector<LibraryFolder> library_folders(const std::vector<platform::Path>& search_path,
                                                         const platform::Path& install_root);

struct LibraryEntry {
  std::string name;  // the file name ("res.sym")
  std::size_t folder = 0;
};

class SymbolLibraryBrowser {
 public:
  /// Lists the `.sym` files of each folder (a folder that cannot be listed
  /// is shown empty), sorted by name within the folder.
  void load(platform::FileSystem& fs, std::vector<LibraryFolder> folders);
  /// The search field; empty shows every symbol.
  void set_filter(std::string text);
  [[nodiscard]] const std::string& filter() const noexcept { return filter_; }

  [[nodiscard]] const std::vector<LibraryFolder>& folders() const noexcept { return folders_; }
  /// The rows shown: every entry whose name holds the filter.
  [[nodiscard]] const std::vector<LibraryEntry>& visible() const noexcept { return visible_; }
  /// The panel title: "Symbols · <first folder's label>", "Symbols" without folders.
  [[nodiscard]] std::string title() const;
  /// The search field's placeholder ("Search symbols").
  [[nodiscard]] static std::string_view placeholder() noexcept { return "Search symbols"; }

  /// The command that places visible row `row` at `at`; nullopt for a row
  /// out of range.
  [[nodiscard]] std::optional<std::string> place_command(std::size_t row, schem::Point at) const;

 private:
  void refilter();

  std::vector<LibraryFolder> folders_;
  std::vector<LibraryEntry> entries_;
  std::vector<LibraryEntry> visible_;
  std::string filter_;
};

/// A coordinate as the command reads it back exactly (shortest round trip).
[[nodiscard]] std::string coordinate_text(double value);

}  // namespace hefesto::shell
