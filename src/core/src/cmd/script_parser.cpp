// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M0 command-script reader (rules in hefesto/cmd/script.hpp).

#include <string_view>

#include "hefesto/cmd/script.hpp"
#include "hefesto/diag/codes.hpp"

namespace hefesto::cmd {

namespace {

namespace codes = diag::codes;

constexpr bool is_blank(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r';
}

constexpr bool is_separator(char c) noexcept {
  return c == ';' || c == '\n';
}

// The character a backslash escape stands for (outside braces).
constexpr char unescape(char c) noexcept {
  switch (c) {
    case 'n':
      return '\n';
    case 't':
      return '\t';
    case 'r':
      return '\r';
    default:
      return c;
  }
}

class Parser {
 public:
  Parser(std::string_view text, std::string_view source_name) : text_(text), source_name_(source_name) {}

  ParsedScript run() {
    while (!result_.error && skip_to_command()) {
      if (peek() == '#') {
        skip_comment();
        continue;
      }
      parse_command();
    }
    return std::move(result_);
  }

 private:
  [[nodiscard]] bool at_end() const noexcept { return pos_ >= text_.size(); }
  [[nodiscard]] char peek(std::size_t ahead = 0) const noexcept {
    return pos_ + ahead < text_.size() ? text_[pos_ + ahead] : '\0';
  }
  [[nodiscard]] bool at_line_continuation() const noexcept {
    return peek() == '\\' && pos_ + 1 < text_.size() && peek(1) == '\n';
  }

  // Consumes one character, counting lines.
  char advance() noexcept {
    const char c = text_[pos_++];
    if (c == '\n') {
      ++line_;
    }
    return c;
  }

  // Skips blanks, separators and line continuations; false at end of text.
  bool skip_to_command() {
    while (!at_end() && (is_blank(peek()) || is_separator(peek()) || at_line_continuation())) {
      skip_one();
    }
    return !at_end();
  }

  // Skips blanks and line continuations within a command.
  void skip_blanks() {
    while (!at_end() && (is_blank(peek()) || at_line_continuation())) {
      skip_one();
    }
  }

  // Consumes one character, or both characters of a line continuation.
  void skip_one() {
    if (at_line_continuation()) {
      advance();
    }
    advance();
  }

  void skip_comment() {
    while (!at_end() && peek() != '\n') {
      advance();
    }
  }

  void parse_command() {
    ParsedCommand command;
    command.line = line_;
    while (!result_.error) {
      skip_blanks();
      if (at_end() || is_separator(peek())) {
        break;
      }
      command.words.push_back(parse_word());
    }
    if (!result_.error && !command.words.empty()) {
      result_.commands.push_back(std::move(command));
    }
  }

  std::string parse_word() {
    if (peek() == '"') {
      return finish_group(parse_quoted());
    }
    if (peek() == '{') {
      return finish_group(parse_braced());
    }
    return parse_bare();
  }

  // After a closing quote or brace the word must end.
  std::string finish_group(std::string word) {
    if (!result_.error && !at_end() && !is_blank(peek()) && !is_separator(peek()) && !at_line_continuation()) {
      fail(codes::kCmdParseExtra, "extra characters after close-quote or close-brace", line_);
    }
    return word;
  }

  std::string parse_bare() {
    std::string word;
    while (!at_end() && !is_blank(peek()) && !is_separator(peek()) && !at_line_continuation()) {
      const char c = advance();
      if (c == '\\' && !at_end()) {
        word += unescape(advance());
      } else {
        word += c;
      }
    }
    return word;
  }

  std::string parse_quoted() {
    const std::uint32_t start = line_;
    advance();  // opening quote
    std::string word;
    while (!at_end() && peek() != '"') {
      if (at_line_continuation()) {
        advance();
        advance();
        word += ' ';
        continue;
      }
      const char c = advance();
      word += (c == '\\' && !at_end()) ? unescape(advance()) : c;
    }
    if (at_end()) {
      fail(codes::kCmdParseQuote, "missing \"", start);
      return word;
    }
    advance();  // closing quote
    return word;
  }

  std::string parse_braced() {
    const std::uint32_t start = line_;
    advance();  // opening brace
    std::string word;
    int depth = 1;
    while (!at_end()) {
      const char c = advance();
      if (c == '\\' && !at_end()) {
        word += c;
        word += advance();
        continue;
      }
      depth += c == '{' ? 1 : (c == '}' ? -1 : 0);
      if (depth == 0) {
        return word;
      }
      word += c;
    }
    fail(codes::kCmdParseBrace, "missing close-brace", start);
    return word;
  }

  void fail(std::string_view code, std::string text, std::uint32_t line) {
    if (result_.error) {
      return;
    }
    diag::Location location;
    if (!source_name_.empty()) {
      location = diag::SourceLocation{std::string{source_name_}, line};
    }
    result_.error = diag::make(code, std::move(text), std::move(location));
  }

  std::string_view text_;
  std::string_view source_name_;
  std::size_t pos_ = 0;
  std::uint32_t line_ = 1;
  ParsedScript result_;
};

constexpr std::string_view kWordSpecials = " \t\r\n;\"{}\\";

}  // namespace

ParsedScript parse_script(std::string_view text, std::string_view source_name) {
  return Parser{text, source_name}.run();
}

std::string quote_word(std::string_view word) {
  const bool plain = !word.empty() && word.front() != '#' && word.find_first_of(kWordSpecials) == std::string_view::npos;
  if (plain) {
    return std::string{word};
  }
  std::string out = "\"";
  for (const char c : word) {
    switch (c) {
      case '\n':
        out += "\\n";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\r':
        out += "\\r";
        break;
      case '"':
      case '\\':
        out += '\\';
        out += c;
        break;
      default:
        out += c;
    }
  }
  out += '"';
  return out;
}

}  // namespace hefesto::cmd
