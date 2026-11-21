// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol references to files and back (capability
// schematic-symbols-and-libraries "Symbol references"). `./`, `../` and the
// `.` search-path element resolve against the folder of the containing
// schematic in both modes of `xschem-compat` (Q-SCHEM-30).
//
// Owner: SYMLIB.

#include "hefesto/schem/lib/reference.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <vector>

namespace hefesto::schem {

namespace {

std::vector<std::string_view> components(std::string_view text) {
  std::vector<std::string_view> out;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t slash = std::min(text.find('/', start), text.size());
    out.push_back(text.substr(start, slash - start));
    start = slash + 1;
  }
  return out;
}

bool is_name(std::string_view component) {
  return component != "." && component != "..";
}

std::string join(const std::vector<std::string_view>& parts) {
  std::string out;
  for (const std::string_view part : parts) {
    if (!out.empty()) out.push_back('/');
    out.append(part);
  }
  return out;
}

platform::Path folder_of(const platform::Path& directory, const platform::Path& document_folder) {
  return directory == platform::Path{std::string{kDocumentFolderElement}} ? document_folder : directory;
}

platform::Path joined(const platform::Path& folder, std::string_view reference) {
  return (folder / std::string{reference}).lexically_normal();
}

bool exists_or_folder(platform::FileSystem& fs, const platform::Path& file) {
  return fs.exists(file) || fs.exists(file.parent_path());
}

std::optional<ResolvedReference> by_alias(std::string_view reference, const SearchPath& search_path) {
  const std::size_t slash = reference.find('/');
  if (slash == std::string_view::npos) return std::nullopt;
  const std::string_view first = reference.substr(0, slash);
  for (const LibraryAlias& alias : search_path.aliases()) {
    if (alias.alias == first) return ResolvedReference{joined(alias.folder, reference.substr(slash + 1)), ResolvedVia::alias, {}};
  }
  return std::nullopt;
}

std::optional<ResolvedReference> by_search_path(std::string_view reference, const platform::Path& document_folder,
                                                const SearchPath& search_path, platform::FileSystem& fs) {
  for (const platform::Path& directory : search_path.directories()) {
    const bool dot = directory == platform::Path{std::string{kDocumentFolderElement}};
    const platform::Path folder = folder_of(directory, document_folder);
    const platform::Path file = joined(folder, reference);
    if (!fs.exists(file)) continue;
    return dot ? ResolvedReference{file, ResolvedVia::document_folder, folder}
               : ResolvedReference{file, ResolvedVia::search_path, {}};
  }
  return std::nullopt;
}

// The first component that differs, or the end of `folder` when it is a
// prefix of `file` (both lexically normal).
bool is_prefix(const platform::Path& folder, const platform::Path& file) {
  const auto [mismatch_folder, mismatch_file] = std::mismatch(folder.begin(), folder.end(), file.begin(), file.end());
  if (mismatch_folder == folder.end()) return mismatch_file != file.end();
  // A trailing separator of `folder` is an empty last component.
  return std::next(mismatch_folder) == folder.end() && mismatch_folder->empty() && mismatch_file != file.end();
}

}  // namespace

bool is_web_reference(std::string_view reference) noexcept {
  return reference.starts_with("http://") || reference.starts_with("https://");
}

std::string normalize_reference(std::string_view reference) {
  std::vector<std::string_view> kept;
  const auto parts = components(reference);
  for (std::size_t i = 0; i < parts.size(); ++i) {
    const std::string_view part = parts[i];
    if (part.empty()) continue;
    if (part == ".") {
      if (i == 0) kept.push_back(part);  // the leading `./` of step 5
      continue;
    }
    if (part == ".." && !kept.empty() && is_name(kept.back())) {
      kept.pop_back();
      continue;
    }
    kept.push_back(part);
  }
  return join(kept);
}

ResolvedReference resolve_reference(std::string_view reference, const platform::Path& document_folder,
                                    const SearchPath& search_path, platform::FileSystem& fs) {
  if (reference.empty()) return {};
  if (is_web_reference(reference)) return {{}, ResolvedVia::web, {}};
  if (reference.starts_with('/')) return {platform::Path{std::string{reference}}, ResolvedVia::absolute, {}};
  const std::string normal = normalize_reference(reference);
  if (normal.starts_with("./") || normal.starts_with("../")) {
    const platform::Path file = joined(document_folder, normal);
    if (exists_or_folder(fs, file)) return {file, ResolvedVia::relative, document_folder};
  }
  if (auto aliased = by_alias(normal, search_path)) return *aliased;
  if (auto found = by_search_path(normal, document_folder, search_path, fs)) return *found;
  return {joined(document_folder, normal), ResolvedVia::fallback, {}};
}

std::string shortest_reference(const platform::Path& file, const platform::Path& document_folder,
                               const SearchPath& search_path) {
  const platform::Path normal = file.lexically_normal();
  for (const platform::Path& directory : search_path.directories()) {
    const platform::Path folder = folder_of(directory, document_folder).lexically_normal();
    if (folder.empty() || !is_prefix(folder, normal)) continue;
    const std::string relative = normal.lexically_relative(folder).generic_string();
    for (const LibraryAlias& alias : search_path.aliases()) {
      if (alias.folder.lexically_normal() == folder) return alias.alias + "/" + relative;
    }
    return relative;
  }
  if (!document_folder.empty() && normal.parent_path() == document_folder.lexically_normal()) {
    return normal.filename().generic_string();
  }
  return normal.generic_string();
}

}  // namespace hefesto::schem
