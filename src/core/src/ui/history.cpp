// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Command history with GNU Readline history expansion (ui-command-dispatch
// "Command history"). Owner: DISPATCH.

#include "hefesto/ui/history.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <optional>
#include <utility>

#include "hefesto/ui/diag_codes.hpp"

namespace hefesto::ui {

namespace {

diag::Diagnostic history_error(std::string text) {
  return diag::make(codes::kHistory, std::move(text));
}

bool is_blank(char c) {
  return c == ' ' || c == '\t' || c == '\n';
}

std::vector<std::string> words_of(std::string_view line) {
  std::vector<std::string> words;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && is_blank(line[i])) {
      ++i;
    }
    const std::size_t start = i;
    while (i < line.size() && !is_blank(line[i])) {
      ++i;
    }
    if (i > start) {
      words.emplace_back(line.substr(start, i - start));
    }
  }
  return words;
}

std::string join(const std::vector<std::string>& words, std::size_t first, std::size_t last) {
  std::string out;
  for (std::size_t i = first; i <= last && i < words.size(); ++i) {
    if (!out.empty()) {
      out += ' ';
    }
    out += words[i];
  }
  return out;
}

std::optional<std::size_t> read_number(std::string_view text, std::size_t& pos) {
  std::size_t value = 0;
  const auto [end, ec] = std::from_chars(text.data() + pos, text.data() + text.size(), value);
  if (ec != std::errc{} || end == text.data() + pos) {
    return std::nullopt;
  }
  pos = static_cast<std::size_t>(end - text.data());
  return value;
}

// Parses one history reference starting at the `!` at `pos` of `line` and
// produces its text. `typed` is the expansion of the line so far (`!#`).
class Reference {
 public:
  Reference(const std::vector<std::string>& lines, std::string_view line, std::size_t pos, std::string_view typed)
      : lines_(lines), line_(line), pos_(pos + 1), typed_(typed) {}

  diag::Outcome<std::string> expand(bool& print_only) {
    auto event = read_event();
    if (!event.ok()) {
      return event.error();
    }
    auto words = read_words(event.value());
    if (!words.ok()) {
      return words.error();
    }
    return apply_modifiers(std::move(words.value()), print_only);
  }

  [[nodiscard]] std::size_t end() const noexcept { return pos_; }

 private:
  [[nodiscard]] char peek(std::size_t ahead = 0) const {
    return pos_ + ahead < line_.size() ? line_[pos_ + ahead] : '\0';
  }

  diag::Outcome<std::string> entry(std::size_t index_from_one, std::string_view spec) const {
    if (index_from_one == 0 || index_from_one > lines_.size()) {
      return history_error(std::string{spec} + ": event not found");
    }
    return lines_[index_from_one - 1];
  }

  diag::Outcome<std::string> read_event() {
    const char c = peek();
    if (c == '!') {
      ++pos_;
      return entry(lines_.size(), "!!");
    }
    if (c == '#') {
      ++pos_;
      return std::string{typed_};
    }
    if (c == ':' || c == '^' || c == '$' || c == '*' || c == '%') {
      return entry(lines_.size(), "!");
    }
    if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
      const auto n = read_number(line_, pos_);
      return entry(n.value_or(0), "!" + std::to_string(n.value_or(0)));
    }
    if (c == '-') {
      ++pos_;
      const auto n = read_number(line_, pos_).value_or(0);
      const std::string spec = "!-" + std::to_string(n);
      return n == 0 || n > lines_.size() ? diag::Outcome<std::string>{history_error(spec + ": event not found")}
                                         : entry(lines_.size() + 1 - n, spec);
    }
    return c == '?' ? search_containing() : search_prefix();
  }

  diag::Outcome<std::string> search_prefix() {
    const std::size_t start = pos_;
    while (pos_ < line_.size() && !is_blank(line_[pos_]) && line_[pos_] != ':') {
      ++pos_;
    }
    const std::string_view prefix = line_.substr(start, pos_ - start);
    for (auto it = lines_.rbegin(); it != lines_.rend(); ++it) {
      if (std::string_view{*it}.starts_with(prefix)) {
        return *it;
      }
    }
    return history_error("!" + std::string{prefix} + ": event not found");
  }

  diag::Outcome<std::string> search_containing() {
    const std::size_t start = ++pos_;
    const std::size_t close = line_.find('?', start);
    const std::size_t stop = close == std::string_view::npos ? line_.size() : close;
    const std::string_view needle = line_.substr(start, stop - start);
    pos_ = close == std::string_view::npos ? line_.size() : close + 1;
    for (auto it = lines_.rbegin(); it != lines_.rend(); ++it) {
      if (it->find(needle) != std::string::npos) {
        return *it;
      }
    }
    return history_error("!?" + std::string{needle} + ": event not found");
  }

  // The word designator after the event, if any.
  diag::Outcome<std::string> read_words(const std::string& event) {
    const char c = peek();
    const bool colon = c == ':' && is_word_start(peek(1));
    if (!colon && c != '^' && c != '$' && c != '*' && c != '%') {
      return event;
    }
    if (colon) {
      ++pos_;
    }
    const std::vector<std::string> words = words_of(event);
    const std::size_t start = pos_;
    auto range = read_range(words.empty() ? 0 : words.size() - 1);
    if (!range) {
      return history_error(std::string{line_.substr(start, pos_ - start)} + ": bad word specifier");
    }
    if (range->first > range->second || range->second >= words.size()) {
      return range->first == words.size() && peek_was_star_ ? std::string{}
                                                            : diag::Outcome<std::string>{history_error(
                                                                  std::string{line_.substr(start, pos_ - start)} +
                                                                  ": bad word specifier")};
    }
    return join(words, range->first, range->second);
  }

  static bool is_word_start(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0 || c == '^' || c == '$' || c == '*' || c == '-' ||
           c == '%';
  }

  // `n`, `^`, `$`, `x-y`, `x-`, `-y`, `*`, `x*`.
  std::optional<std::pair<std::size_t, std::size_t>> read_range(std::size_t last) {
    peek_was_star_ = false;
    if (peek() == '*') {
      ++pos_;
      peek_was_star_ = true;
      return std::pair<std::size_t, std::size_t>{1, last};
    }
    const auto first = read_index(last, 0);
    if (!first) {
      return std::nullopt;
    }
    if (peek() == '*') {
      ++pos_;
      peek_was_star_ = true;
      return std::pair<std::size_t, std::size_t>{*first, last};
    }
    if (peek() != '-') {
      return std::pair<std::size_t, std::size_t>{*first, *first};
    }
    ++pos_;
    const auto second = read_index(last, last > 0 ? last - 1 : 0);
    return std::pair<std::size_t, std::size_t>{*first, second.value_or(last > 0 ? last - 1 : 0)};
  }

  std::optional<std::size_t> read_index(std::size_t last, std::size_t missing) {
    const char c = peek();
    if (c == '^') {
      ++pos_;
      return 1;
    }
    if (c == '$') {
      ++pos_;
      return last;
    }
    if (c == '-') {
      return 0;  // `-y` starts at word 0
    }
    if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
      return read_number(line_, pos_);
    }
    return missing == 0 && c != '\0' && !is_blank(c) && c != ':' ? std::nullopt : std::optional<std::size_t>{missing};
  }

  diag::Outcome<std::string> apply_modifiers(std::string text, bool& print_only) {
    while (peek() == ':') {
      const char m = peek(1);
      pos_ += 2;
      if (m == 'p') {
        print_only = true;
      } else if (m == 'h' || m == 't' || m == 'r' || m == 'e') {
        text = path_modifier(text, m);
      } else if (m == 's' || m == 'g') {
        if (m == 'g' && peek() == 's') {
          ++pos_;
        }
        auto replaced = substitute(text, m == 'g');
        if (!replaced.ok()) {
          return replaced.error();
        }
        text = std::move(replaced.value());
      } else {
        return history_error(std::string{":"} + m + ": unrecognized history modifier");
      }
    }
    return text;
  }

  static std::string path_modifier(const std::string& text, char m) {
    const std::size_t slash = text.rfind('/');
    const std::size_t dot = text.rfind('.');
    const bool has_dot = dot != std::string::npos && (slash == std::string::npos || dot > slash);
    switch (m) {
      case 'h':
        return slash == std::string::npos ? text : text.substr(0, slash);
      case 't':
        return slash == std::string::npos ? text : text.substr(slash + 1);
      case 'r':
        return has_dot ? text.substr(0, dot) : text;
      default:
        return has_dot ? text.substr(dot + 1) : std::string{};
    }
  }

  // `s/old/new/` (any delimiter); `&` in new stands for old.
  diag::Outcome<std::string> substitute(const std::string& text, bool global) {
    const char delim = peek();
    if (delim == '\0') {
      return history_error("substitution failed");
    }
    ++pos_;
    const std::string old_text = read_until(delim);
    std::string new_text = read_until(delim);
    for (std::size_t at = new_text.find('&'); at != std::string::npos; at = new_text.find('&', at + old_text.size())) {
      new_text.replace(at, 1, old_text);
    }
    return replace_text(text, old_text, new_text, global);
  }

  std::string read_until(char delim) {
    std::string out;
    while (pos_ < line_.size() && line_[pos_] != delim) {
      if (line_[pos_] == '\\' && pos_ + 1 < line_.size() && line_[pos_ + 1] == delim) {
        ++pos_;
      }
      out += line_[pos_++];
    }
    if (pos_ < line_.size()) {
      ++pos_;  // the closing delimiter
    }
    return out;
  }

  static diag::Outcome<std::string> replace_text(std::string text, const std::string& old_text,
                                                 const std::string& new_text, bool global) {
    std::size_t at = old_text.empty() ? std::string::npos : text.find(old_text);
    if (at == std::string::npos) {
      return history_error("substitution failed");
    }
    while (at != std::string::npos) {
      text.replace(at, old_text.size(), new_text);
      at = global ? text.find(old_text, at + new_text.size()) : std::string::npos;
    }
    return text;
  }

  const std::vector<std::string>& lines_;
  std::string_view line_;
  std::size_t pos_;
  std::string_view typed_;
  bool peek_was_star_ = false;
};

// `!` stays literal before a blank, `=`, `(` or the end of the line.
bool starts_reference(std::string_view line, std::size_t i) {
  if (i + 1 >= line.size()) {
    return false;
  }
  const char next = line[i + 1];
  return !is_blank(next) && next != '=' && next != '(';
}

// `^old^new^` at the start of the line: `!!:s^old^new^`.
std::string quick_substitution(std::string_view line) {
  return "!!:s" + std::string{line};
}

}  // namespace

void CommandHistory::add(std::string line) {
  if (!line.empty()) {
    lines_.push_back(std::move(line));
  }
}

diag::Outcome<HistoryExpansion> CommandHistory::expand(std::string_view line) const {
  std::string rewritten;
  if (!line.empty() && line.front() == '^') {
    rewritten = quick_substitution(line);
    line = rewritten;
  }
  HistoryExpansion result;
  bool quoted = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '\\' && i + 1 < line.size()) {
      result.line += c;
      result.line += line[++i];
      continue;
    }
    if (c == '\'') {
      quoted = !quoted;
    }
    if (c != '!' || quoted || !starts_reference(line, i)) {
      result.line += c;
      continue;
    }
    Reference ref{lines_, line, i, result.line};
    auto text = ref.expand(result.print_only);
    if (!text.ok()) {
      return text.error();
    }
    result.line += text.value();
    i = ref.end() - 1;
  }
  return result;
}

std::string CommandHistory::listing(std::size_t count, bool numbers, bool reverse) const {
  const std::size_t n = count == 0 ? lines_.size() : std::min(count, lines_.size());
  std::vector<std::size_t> indices;
  for (std::size_t i = lines_.size() - n; i < lines_.size(); ++i) {
    indices.push_back(i);
  }
  if (reverse) {
    std::reverse(indices.begin(), indices.end());
  }
  std::string out;
  for (const std::size_t i : indices) {
    if (numbers) {
      std::array<char, 24> buf{};
      std::snprintf(buf.data(), buf.size(), "\t%4zu ", i + 1);
      out += buf.data();
    }
    out += lines_[i];
    out += '\n';
  }
  if (!out.empty()) {
    out.pop_back();
  }
  return out;
}

}  // namespace hefesto::ui
