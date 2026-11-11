// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Keyword matching and direction/position names (capability
// cmd-coordinates-and-units, "Keyword matching", "Direction and position
// names"). Ported from Magic VLSI 8.3.694 (fc5daa9e) utils/lookup.c (Lookup)
// and utils/geometry.c (GeoNameToPos), Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md). Owner: EDIT;
// the keyword-terminator rule (Q-SCRIPT-14, keyword_rule.hpp) is BINDING's
// (openspec/changes/m4-scripting/design.md D2).

#include "hefesto/cmd/keywords.hpp"

#include <array>
#include <cctype>
#include <string>

#include "hefesto/diag/codes.hpp"
#include "hefesto/script/binding/keyword_rule.hpp"

namespace hefesto::cmd {

namespace {

// The rule of the thread's innermost KeywordTerminatorScope (Q-SCRIPT-14).
thread_local KeywordTerminator t_terminator = KeywordTerminator::space;

char lower(char c) noexcept {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

// Drops a leading `::magic::` or `magic::` qualifier (Magic's MAGIC_WRAPPER
// branch of Lookup).
std::string_view unqualified(std::string_view word) noexcept {
  if (word.starts_with("::magic::")) {
    return word.substr(9);
  }
  if (word.starts_with("magic::")) {
    return word.substr(7);
  }
  return word;
}

// How `word` matches the leading word of `entry`: 0 = no, 1 = prefix,
// 2 = exact.
int match_entry(std::string_view word, std::string_view entry) noexcept {
  const std::string_view key = keyword_of(entry);
  if (word.size() > key.size()) {
    return 0;
  }
  for (std::size_t i = 0; i < word.size(); ++i) {
    if (lower(word[i]) != lower(key[i])) {
      return 0;
    }
  }
  return word.size() == key.size() ? 2 : 1;
}

struct PositionName {
  std::string_view name;
  Direction direction;
};

// GeoNameToPos's table, in its (alphabetical) order.
constexpr std::array<PositionName, 33> kPositions = {{
    {"bl", Direction::southwest},   {"bottom", Direction::south},   {"br", Direction::southeast},
    {"center", Direction::center},  {"d", Direction::south},        {"dl", Direction::southwest},
    {"down", Direction::south},     {"dr", Direction::southeast},   {"e", Direction::east},
    {"east", Direction::east},      {"left", Direction::west},      {"n", Direction::north},
    {"ne", Direction::northeast},   {"north", Direction::north},    {"northeast", Direction::northeast},
    {"northwest", Direction::northwest}, {"nw", Direction::northwest}, {"right", Direction::east},
    {"s", Direction::south},        {"se", Direction::southeast},   {"south", Direction::south},
    {"southeast", Direction::southeast}, {"southwest", Direction::southwest}, {"sw", Direction::southwest},
    {"tl", Direction::northwest},   {"top", Direction::north},      {"tr", Direction::northeast},
    {"u", Direction::north},        {"ul", Direction::northwest},   {"up", Direction::north},
    {"ur", Direction::northeast},   {"w", Direction::west},         {"west", Direction::west},
}};

std::string legal_names(bool manhattan_only) {
  std::string out = "Legal directions/positions are:\n\t";
  bool first = true;
  for (const PositionName& p : kPositions) {
    if (manhattan_only && !is_manhattan(p.direction)) {
      continue;
    }
    if (!first) {
      out += ',';
    }
    out += p.name;
    first = false;
  }
  return out;
}

diag::Diagnostic direction_error(std::string_view word, std::string_view why, bool manhattan_only) {
  return diag::make(diag::codes::kCmdUsage,
                    "\"" + std::string{word} + "\" " + std::string{why} + "\n" + legal_names(manhattan_only));
}

}  // namespace

KeywordTerminator keyword_terminator() noexcept {
  return t_terminator;
}

bool ends_keyword(char c) noexcept {
  if (t_terminator == KeywordTerminator::space) {
    return c == ' ';
  }
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

std::string_view keyword_of(std::string_view entry) noexcept {
  std::size_t end = 0;
  while (end < entry.size() && !ends_keyword(entry[end])) {
    ++end;
  }
  return entry.substr(0, end);
}

KeywordTerminatorScope::KeywordTerminatorScope(bool magic_compat) noexcept : previous_(t_terminator) {
  t_terminator = magic_compat ? KeywordTerminator::space : KeywordTerminator::whitespace;
}

KeywordTerminatorScope::~KeywordTerminatorScope() {
  t_terminator = previous_;
}

LookupResult lookup(std::string_view word, std::span<const std::string_view> table) {
  const std::string_view w = unqualified(word);
  LookupResult result;
  for (std::size_t i = 0; i < table.size(); ++i) {
    const int m = match_entry(w, table[i]);
    if (m == 2) {
      return LookupResult{Match::found, i};
    }
    if (m == 1) {
      result = result.match == Match::unrecognized ? LookupResult{Match::found, i} : LookupResult{Match::ambiguous, 0};
    }
  }
  return result;
}

diag::Outcome<Direction> parse_direction(std::string_view word, bool manhattan_only) {
  std::array<std::string_view, kPositions.size()> names{};
  for (std::size_t i = 0; i < kPositions.size(); ++i) {
    names.at(i) = kPositions.at(i).name;
  }
  const LookupResult r = lookup(word, names);
  if (r.match == Match::ambiguous) {
    return direction_error(word, "is ambiguous.", manhattan_only);
  }
  if (r.match == Match::unrecognized) {
    return direction_error(word, "is not a valid direction or position.", manhattan_only);
  }
  const Direction d = kPositions.at(r.index).direction;
  if (manhattan_only && !is_manhattan(d)) {
    return direction_error(word, "is not a Manhattan direction or position.", true);
  }
  return d;
}

diag::Outcome<Direction> parse_position(std::string_view word) {
  return parse_direction(word, false);
}

}  // namespace hefesto::cmd
