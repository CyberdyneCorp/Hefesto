// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Statement helpers of the LEF and DEF readers: LookupFull
// (utils/lookupfull.c:52-75), the `%f` number parse, LefEndStatement
// (lefRead.c:1826-1835), LefParseEndStatement (lefRead.c:401-445) and
// LefSkipSection (lefRead.c:468-505) (capability lef-read "LEF/DEF
// tokenizer", "END statement matching"; design D6).
//
// Owner: LEFR (design D2).

#include "hefesto/lef/lex/statements.hpp"

#include <array>
#include <cstdlib>

namespace hefesto::lef {

namespace {

constexpr std::string_view kEndCode = "LEF-LEX-003";
constexpr std::array<std::string_view, 2> kEndKeys{"END", "ENDEXT"};

char upper(char c) noexcept {
  return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

}  // namespace

bool keyword_is(std::string_view token, std::string_view keyword) noexcept {
  if (token.size() != keyword.size()) {
    return false;
  }
  for (std::size_t i = 0; i < token.size(); ++i) {
    if (upper(token[i]) != upper(keyword[i])) {
      return false;
    }
  }
  return true;
}

std::optional<std::size_t> lookup_full(std::string_view token, std::span<const std::string_view> table) noexcept {
  for (std::size_t i = 0; i < table.size(); ++i) {
    if (keyword_is(token, table[i])) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<float> parse_number(std::string_view token) {
  const std::string text{token};
  char* end = nullptr;
  const float value = std::strtof(text.c_str(), &end);
  if (end == text.c_str()) {
    return std::nullopt;
  }
  return value;
}

std::string truncate_name(std::string_view name) {
  return std::string{name.substr(0, 127)};
}

std::size_t skip_statement(Lexer& lexer) {
  std::size_t skipped = 0;
  while (const auto token = lexer.take()) {
    if (token->text.starts_with(';')) {
      break;
    }
    ++skipped;
  }
  return skipped;
}

EndMatch parse_end(Lexer& lexer, std::optional<std::string_view> name, const MessageTarget& messages) {
  const auto token = lexer.take(name ? EolMode::ignore : EolMode::report);
  if (!token) {
    messages.report(Dialect::lef, diag::Severity::error, kEndCode, "Bad file read while looking for END statement");
    return EndMatch::not_matched;
  }
  if (token->end_of_line && !name) {
    return EndMatch::closed;
  }
  if (name && keyword_is(token->text, *name)) {
    return EndMatch::closed;
  }
  return keyword_is(token->text, "END") ? EndMatch::end_end : EndMatch::not_matched;
}

void skip_section(Lexer& lexer, std::optional<std::string_view> section, const MessageTarget& messages) {
  while (const auto token = lexer.take()) {
    const auto key = lookup_full(token->text, kEndKeys);
    if (key == 0U) {
      EndMatch result = EndMatch::end_end;
      while (result == EndMatch::end_end) {
        result = parse_end(lexer, section, messages);
      }
      if (result == EndMatch::closed) {
        return;
      }
    } else if (key == 1U && section == std::string_view{"BEGINEXT"}) {
      return;
    }
  }
  // Magic prints a null section name as "(null)".
  messages.report(Dialect::lef, diag::Severity::error, kEndCode,
                  "Section " + std::string{section.value_or("(null)")} + " has no END record!");
}

}  // namespace hefesto::lef
