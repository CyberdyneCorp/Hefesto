// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cell-path rebasing behind the binding's `cd` (scripting-api
// "Directory change rebases cell paths"; DBRebaseCellPaths / dbRebasePathFunc
// of Magic 8.3.694 (fc5daa9e) database/DBio.c:2245-2340, Copyright (C)
// 1985, 1990 Regents of the University of California, see
// THIRD_PARTY_NOTICES.md).

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "binding_internal.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::script::detail {

namespace {

// dbPathCompsSplit: the components of a path without the root and `.`.
std::vector<std::string> components(std::string_view path) {
  std::vector<std::string> parts;
  for (const auto& part : std::filesystem::path{std::string{path}}) {
    const std::string text = part.string();
    if (!text.empty() && text != "/" && text != ".") {
      parts.push_back(text);
    }
  }
  return parts;
}

// dbRebasePathFunc for one recorded path; nullopt when it is left alone
// (absolute, `~` or `$` paths, or more leading `..` than the old directory
// has components). Leading `..` are resolved against the old directory; the
// result steps up out of the new directory to the common part and down to
// the file.
std::optional<std::string> rebased(const std::string& file, const std::vector<std::string>& old_dir,
                                   const std::vector<std::string>& new_dir) {
  if (file.empty() || file.front() == '/' || file.front() == '~' || file.front() == '$') {
    return std::nullopt;
  }
  const std::vector<std::string> parts = components(file);
  std::size_t up = 0;
  while (up < parts.size() && parts[up] == "..") {
    ++up;
  }
  if (up > old_dir.size()) {
    return std::nullopt;
  }
  const std::size_t base = old_dir.size() - up;
  std::size_t common = 0;
  while (common < base && common < new_dir.size() && old_dir[common] == new_dir[common]) {
    ++common;
  }
  std::filesystem::path out;
  for (std::size_t i = common; i < new_dir.size(); ++i) {
    out /= "..";
  }
  for (std::size_t i = common; i < base; ++i) {
    out /= old_dir[i];
  }
  for (std::size_t i = up; i < parts.size(); ++i) {
    out /= parts[i];
  }
  return out.string();
}

}  // namespace

void rebase_cell_paths(editor::EditorState& editor, std::string_view old_dir, std::string_view new_dir) {
  const std::vector<std::string> from = components(old_dir);
  const std::vector<std::string> to = components(new_dir);
  editor.cells.for_each([&](db::CellDef& def) {
    if (auto path = rebased(def.file_path().string(), from, to)) {
      def.set_file_path(std::move(*path));
    }
  });
}

}  // namespace hefesto::script::detail
