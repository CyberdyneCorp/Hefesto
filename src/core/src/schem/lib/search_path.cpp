// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The library search path built from an XSCHEM_LIBRARY_PATH value
// (capability schematic-symbols-and-libraries "Library search path",
// Q-SCHEM-30). SearchPath::from_xschemrc is in xschemrc.cpp.
//
// Owner: SYMLIB.

#include "hefesto/schem/lib/search_path.hpp"

#include <cctype>

namespace hefesto::schem {

namespace {

using Env = std::map<std::string, std::string>;

std::string_view trim(std::string_view text) {
  const std::size_t first = text.find_first_not_of(" \t");
  if (first == std::string_view::npos) return {};
  const std::size_t last = text.find_last_not_of(" \t");
  return text.substr(first, last - first + 1);
}

bool is_name_char(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// The `$NAME` or `${NAME}` reference at `text[at]` (a `$`): its name and
// length; an empty name when there is no reference.
std::pair<std::string_view, std::size_t> variable_at(std::string_view text, std::size_t at) {
  if (at + 1 < text.size() && text[at + 1] == '{') {
    const std::size_t close = text.find('}', at + 2);
    if (close == std::string_view::npos) return {{}, 0};
    return {text.substr(at + 2, close - at - 2), close - at + 1};
  }
  std::size_t end = at + 1;
  while (end < text.size() && is_name_char(text[end])) ++end;
  return {text.substr(at + 1, end - at - 1), end - at};
}

// `~` at the start and `$NAME` / `${NAME}` references, from `env`. A
// reference to an undefined name is left as written.
std::string expand(std::string_view element, const Env& env) {
  std::string out;
  std::size_t at = 0;
  if (element.starts_with('~')) {
    if (const auto home = env.find("HOME"); home != env.end()) {
      out = home->second;
      at = 1;
    }
  }
  while (at < element.size()) {
    const std::size_t dollar = element.find('$', at);
    out.append(element.substr(at, dollar - at));
    if (dollar == std::string_view::npos) break;
    const auto [name, length] = variable_at(element, dollar);
    const auto value = name.empty() ? env.end() : env.find(std::string{name});
    if (value == env.end()) {
      out.push_back('$');
      at = dollar + 1;
      continue;
    }
    out.append(value->second);
    at = dollar + length;
  }
  return out;
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

SearchPath SearchPath::parse(std::string_view value, const Env& env, platform::FileSystem& fs) {
  std::vector<platform::Path> directories;
  std::vector<LibraryAlias> aliases;
  for (const std::string_view raw : split_elements(value)) {
    const std::string_view element = trim(raw);
    if (element.empty() || element.starts_with('#')) continue;
    auto [folder_text, alias] = split_alias(element);
    const std::string folder = expand(folder_text, env);
    if (folder == kDocumentFolderElement) {
      directories.emplace_back(folder);
      continue;
    }
    if (folder.empty() || !fs.exists(folder)) continue;
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
