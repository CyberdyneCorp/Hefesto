// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tab completion in the Tcl console (see tcl_completion.hpp).

#include "hefesto/shell/tcl_completion.hpp"

#include <algorithm>

namespace hefesto::shell {

namespace {

bool is_space(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/// A character of a command or variable word: Tcl's word separators and
/// substitution characters end it.
bool is_word_char(char c) noexcept {
  return !is_space(c) && std::string_view{";[]{}\"$\\"}.find(c) == std::string_view::npos;
}

/// True when the word starting at `start` is the first word of a command.
bool command_position(std::string_view text, std::size_t start) noexcept {
  std::size_t j = start;
  while (j > 0 && (text[j - 1] == ' ' || text[j - 1] == '\t')) --j;
  return j == 0 || std::string_view{"\n;[{"}.find(text[j - 1]) != std::string_view::npos;
}

TclWordKind word_kind(std::string_view text, std::size_t start) noexcept {
  if (start > 0 && text[start - 1] == '$') return TclWordKind::variable;
  if (start > 1 && text[start - 1] == '{' && text[start - 2] == '$') return TclWordKind::variable;
  return command_position(text, start) ? TclWordKind::command : TclWordKind::none;
}

/// The name as typed: `info commands magic::g*` answers `::magic::gencell`.
std::string as_typed(std::string name, std::string_view prefix) {
  if (!prefix.starts_with("::") && name.starts_with("::")) name.erase(0, 2);
  return name;
}

std::string common_prefix(const std::vector<std::string>& names) {
  std::string common = names.front();
  for (const std::string& name : names) {
    const auto [a, b] = std::ranges::mismatch(common, name);
    common.erase(static_cast<std::size_t>(a - common.begin()));
  }
  return common;
}

}  // namespace

TclCompletionWord tcl_completion_word(std::string_view text, std::size_t cursor) {
  cursor = std::min(cursor, text.size());
  std::size_t start = cursor;
  while (start > 0 && is_word_char(text[start - 1])) --start;
  TclCompletionWord word;
  word.start = start;
  word.prefix = std::string{text.substr(start, cursor - start)};
  word.kind = word.prefix.empty() ? TclWordKind::none : word_kind(text, start);
  return word;
}

std::string tcl_prefix_glob(std::string_view prefix) {
  std::string glob;
  for (const char c : prefix) {
    if (std::string_view{"*?[]\\"}.find(c) != std::string_view::npos) glob += '\\';
    glob += c;
  }
  return glob + "*";
}

ConsoleCompletion complete_tcl_word(std::string_view text, std::size_t cursor, const TclCompletionWord& word,
                                    std::vector<std::string> names) {
  cursor = std::min(cursor, text.size());
  ConsoleCompletion out{std::string{text}, cursor, {}};
  if (word.kind == TclWordKind::none || word.start > cursor) return out;
  for (std::string& name : names) {
    name = as_typed(std::move(name), word.prefix);
    if (name.starts_with(word.prefix)) out.candidates.push_back(std::move(name));
  }
  std::ranges::sort(out.candidates);
  out.candidates.erase(std::ranges::unique(out.candidates).begin(), out.candidates.end());
  if (out.candidates.empty()) return out;
  std::string replacement = common_prefix(out.candidates);
  // A whole command name at the end of a line is followed by its arguments.
  const bool line_end = cursor == text.size() || text[cursor] == '\n';
  if (out.candidates.size() == 1 && word.kind == TclWordKind::command && line_end) replacement += ' ';
  out.text = std::string{text.substr(0, word.start)} + replacement + std::string{text.substr(cursor)};
  out.cursor = word.start + replacement.size();
  return out;
}

std::string completion_listing(const std::vector<std::string>& candidates, std::size_t max) {
  std::string listed = std::to_string(candidates.size()) + " matches:";
  for (std::size_t i = 0; i < candidates.size() && i < max; ++i) listed += (i == 0 ? " " : " · ") + candidates[i];
  if (candidates.size() > max) listed += " …";
  return listed;
}

std::size_t text_offset(std::string_view text, int line, int column) {
  std::size_t offset = 0;
  for (int l = 0; l < line; ++l) {
    const std::size_t newline = text.find('\n', offset);
    if (newline == std::string_view::npos) return text.size();
    offset = newline + 1;
  }
  const std::size_t end = std::min(text.find('\n', offset), text.size());
  return std::min(offset + static_cast<std::size_t>(std::max(column, 0)), end);
}

std::pair<int, int> text_position(std::string_view text, std::size_t offset) {
  offset = std::min(offset, text.size());
  const std::string_view before = text.substr(0, offset);
  const auto line = static_cast<int>(std::ranges::count(before, '\n'));
  const std::size_t newline = before.rfind('\n');
  const std::size_t line_start = newline == std::string_view::npos ? 0 : newline + 1;
  return {line, static_cast<int>(offset - line_start)};
}

}  // namespace hefesto::shell
