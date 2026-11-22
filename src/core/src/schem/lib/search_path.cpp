// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The library search path built from an XSCHEM_LIBRARY_PATH value
// (capability schematic-symbols-and-libraries "Library search path",
// Q-SCHEM-30). SearchPath::from_xschemrc is in xschemrc.cpp.
//
// Owner: SYMLIB.

#include "hefesto/schem/lib/search_path.hpp"

#include "hefesto/schem/lib/reference.hpp"

namespace hefesto::schem {

namespace {

using Env = std::map<std::string, std::string>;

std::string_view trim(std::string_view text) {
  const std::size_t first = text.find_first_not_of(" \t");
  if (first == std::string_view::npos) return {};
  const std::size_t last = text.find_last_not_of(" \t");
  return text.substr(first, last - first + 1);
}

// A leading `~` alone or `~/` replaced by env(HOME); `~user` and every
// other text are kept as written ("Library search path": no other
// expansion is done, `$NAME` stays literal).
std::string expand_home(std::string_view element, const Env& env) {
  const bool home = element == "~" || element.starts_with("~/");
  const auto value = env.find("HOME");
  if (!home || value == env.end()) return std::string{element};
  return value->second + std::string{element.substr(1)};
}

// Step 4 of "Symbol references" applied to a folder: runs of `/` collapsed,
// `/./` and `dir/../` removed, a trailing `/` or `/.` removed; a leading `/`
// is kept.
std::string clean(std::string_view folder) {
  if (!folder.starts_with('/')) return normalize_reference(folder);
  std::string cleaned = "/" + normalize_reference(folder.substr(1));
  return cleaned == "/." ? std::string{"/"} : cleaned;
}

// Splits `<folder> | <alias>` at the first unescaped bar; `\|` is a bar.
// The alias is empty when there is no bar.
std::pair<std::string, std::string> split_alias(std::string_view element) {
  std::string folder;
  for (std::size_t i = 0; i < element.size(); ++i) {
    if (element[i] == '\\' && i + 1 < element.size() && element[i + 1] == '|') {
      folder.push_back('|');
      ++i;
    } else if (element[i] == '|') {
      return {std::string{trim(folder)}, std::string{trim(element.substr(i + 1))}};
    } else {
      folder.push_back(element[i]);
    }
  }
  return {std::string{trim(folder)}, {}};
}

std::vector<std::string_view> split_elements(std::string_view value) {
  std::vector<std::string_view> elements;
  std::size_t start = 0;
  while (start <= value.size()) {
    const std::size_t colon = value.find(':', start);
    const std::size_t end = colon == std::string_view::npos ? value.size() : colon;
    elements.push_back(value.substr(start, end - start));
    start = end + 1;
  }
  return elements;
}

}  // namespace

namespace {

// Whether a cleaned element names an existing folder; `.` always does, and
// any other relative element is looked up under `document_folder`.
bool element_exists(const std::string& folder, platform::FileSystem& fs, const platform::Path& document_folder) {
  if (folder == kDocumentFolderElement) return true;
  if (platform::Path{folder}.is_absolute()) return fs.exists(folder);
  return !document_folder.empty() && fs.exists((document_folder / folder).lexically_normal());
}

}  // namespace

SearchPath SearchPath::parse(std::string_view value, const Env& env, platform::FileSystem& fs,
                             const platform::Path& document_folder) {
  std::vector<platform::Path> directories;
  std::vector<LibraryAlias> aliases;
  for (const std::string_view raw : split_elements(value)) {
    const std::string_view element = trim(raw);
    if (element.empty() || element.starts_with('#')) continue;
    auto [folder_text, alias] = split_alias(element);
    const std::string folder = clean(expand_home(folder_text, env));
    // A relative element is kept as written: references resolve it against
    // the containing schematic's folder (reference.cpp).
    if (folder.empty() || !element_exists(folder, fs, document_folder)) continue;
    directories.emplace_back(folder);
    if (!alias.empty()) aliases.push_back({std::move(alias), folder});
  }
  if (directories.empty()) directories.emplace_back(std::string{kDocumentFolderElement});
  return SearchPath{std::move(directories), std::move(aliases)};
}

SearchPath SearchPath::prepended(const SearchPath& front) const {
  std::vector<platform::Path> directories = front.directories_;
  directories.insert(directories.end(), directories_.begin(), directories_.end());
  std::vector<LibraryAlias> aliases = front.aliases_;
  aliases.insert(aliases.end(), aliases_.begin(), aliases_.end());
  return SearchPath{std::move(directories), std::move(aliases)};
}

XschemrcInput pdk_xschemrc_input(const platform::Path& pdk_root, std::string_view pdk, const platform::Path& sharedir,
                                 const platform::Path& user_conf_dir, const platform::Path& document_folder) {
  XschemrcInput input;
  input.rc_file = pdk_root / std::string{pdk} / "libs.tech" / "xschem" / "xschemrc";
  input.document_folder = document_folder;
  const std::string root = pdk_root.generic_string();
  input.env = {{"PDK_ROOT", root},
               {"PDK", std::string{pdk}},
               {"HOME", document_folder.generic_string()},
               {"PWD", document_folder.generic_string()}};
  input.variables = {{"XSCHEM_SHAREDIR", sharedir.generic_string()},
                     {"USER_CONF_DIR", user_conf_dir.generic_string()},
                     {"PDK_ROOT", root},
                     {"PDK", std::string{pdk}}};
  return input;
}

}  // namespace hefesto::schem
