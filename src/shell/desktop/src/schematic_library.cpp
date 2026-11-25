// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The symbol library browser (schematic_library.hpp). Owner: SHELL.

#include "hefesto/shell/schematic_library.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <utility>

#include "hefesto/app/startup.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/shell/assets.hpp"
#include "hefesto/shell/manager_views.hpp"

namespace hefesto::shell {

namespace {

std::string lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

// `dir` normalised without a trailing separator ("/a/b/" is "/a/b").
platform::Path normal_dir(const platform::Path& dir) {
  const platform::Path normal = dir.lexically_normal();
  return normal.has_filename() ? normal : normal.parent_path();
}

bool same_dir(const platform::Path& a, const platform::Path& b) {
  return normal_dir(a) == normal_dir(b);
}

std::string folder_label(const platform::Path& dir) {
  const platform::Path normal = normal_dir(dir);
  const platform::Path name = normal.filename();
  return name.empty() ? normal.generic_string() : name.generic_string();
}

}  // namespace

platform::Path system_library_dir(const platform::Path& install_root) {
  if (install_root.empty()) return {};
  return install_root / "assets" / "schematic" / "xschem_library" / "devices";
}

platform::Path desktop_install_root(platform::FileSystem& fs, const platform::Path& executable_dir,
                                    const std::optional<std::string>& cad_root_env) {
  // A relative folder would resolve against the working directory, whose
  // `assets/` must never become the system library or a sandbox root.
  const platform::Path exe_dir = executable_dir.is_absolute() ? executable_dir : platform::Path{};
  return app::resolve_install_root(cad_root_env, app::install_root_for_executable(fs, exe_dir));
}

std::optional<platform::Path> desktop_asset_dir(const platform::Path& override_dir,
                                               const platform::Path& executable_dir,
                                               const platform::Path& source_dir) {
  // A relative folder would name `assets/` under the working directory.
  const platform::Path exe_dir = executable_dir.is_absolute() ? executable_dir : platform::Path{};
  return find_asset_dir(asset_search_dirs(override_dir, exe_dir, source_dir));
}

std::vector<platform::Path> parse_path_list(std::string_view output) {
  auto words = pcell::split_tcl_list(output);
  std::vector<platform::Path> dirs;
  if (!words) return dirs;
  for (const std::string& word : words.value()) dirs.emplace_back(word);
  return dirs;
}

std::vector<LibraryFolder> library_folders(const std::vector<platform::Path>& search_path,
                                           const platform::Path& install_root) {
  std::vector<LibraryFolder> folders;
  for (const platform::Path& dir : search_path) folders.push_back(LibraryFolder{dir, folder_label(dir), false});
  const platform::Path system = system_library_dir(install_root);
  if (system.empty()) return folders;
  const auto listed = std::find_if(folders.begin(), folders.end(),
                                   [&system](const LibraryFolder& f) { return same_dir(f.dir, system); });
  if (listed != folders.end()) {
    listed->system = true;
  } else {
    folders.push_back(LibraryFolder{system, folder_label(system), true});
  }
  return folders;
}

void SymbolLibraryBrowser::load(platform::FileSystem& fs, std::vector<LibraryFolder> folders) {
  folders_ = std::move(folders);
  entries_.clear();
  for (std::size_t i = 0; i < folders_.size(); ++i) {
    auto files = fs.list(folders_[i].dir);
    if (!files) continue;
    std::vector<std::string> names;
    for (const platform::Path& file : files.value()) {
      if (file.extension() == ".sym") names.push_back(file.filename().generic_string());
    }
    std::sort(names.begin(), names.end());
    for (std::string& name : names) entries_.push_back(LibraryEntry{std::move(name), i});
  }
  refilter();
}

void SymbolLibraryBrowser::set_filter(std::string text) {
  filter_ = std::move(text);
  refilter();
}

void SymbolLibraryBrowser::refilter() {
  const std::string needle = lower(filter_);
  visible_.clear();
  std::copy_if(entries_.begin(), entries_.end(), std::back_inserter(visible_),
               [&needle](const LibraryEntry& e) { return lower(e.name).find(needle) != std::string::npos; });
}

std::string SymbolLibraryBrowser::title() const {
  return folders_.empty() ? std::string{"Symbols"} : "Symbols · " + folders_.front().label;
}

std::optional<std::string> SymbolLibraryBrowser::place_command(std::size_t row, schem::Point at) const {
  if (row >= visible_.size()) return std::nullopt;
  return command_line({"schematic", "place", visible_[row].name, coordinate_text(at.x), coordinate_text(at.y)});
}

std::string coordinate_text(double value) {
  std::array<char, 32> buffer{};
  const auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  return ec == std::errc{} ? std::string(buffer.data(), end) : std::string{"0"};
}

}  // namespace hefesto::shell
