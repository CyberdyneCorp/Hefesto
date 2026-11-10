// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The drc / cifoutput style state machine (see style_machine.hpp).
//
// Ported from Magic VLSI 8.3.694 drc/DRCtech.c (DRCTechLine) and
// cif/CIFtech.c (CIFTechLine), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#include "drc/rules/style_machine.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "drc/rules/text_util.hpp"

namespace hefesto::drc::rules_detail {

std::vector<std::string> split_comma_list(std::string_view text) {
  std::vector<std::string> pieces;
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t comma = text.find(',', start);
    if (comma == std::string_view::npos) {
      pieces.emplace_back(text.substr(start));
      break;
    }
    pieces.emplace_back(text.substr(start, comma - start));
    start = comma + 1;
  }
  return pieces;
}

void StyleMachine::begin_reload(std::vector<std::string> list, std::string style) {
  list_ = std::move(list);
  current_ = std::move(style);
  status_ = LoadStatus::not_loaded;
}

std::optional<std::string> StyleMachine::declare(Tokens tokens) {
  const std::string& base = tokens[1];
  // Existence test: break on the first declared name that starts with the
  // given one (strncmp(ds_name, argv[1], strlen(argv[1]))).
  const auto found = std::find_if(list_.begin(), list_.end(),
                                  [&](const std::string& name) { return text::starts_with(name, base); });
  if (found != list_.end()) {
    return *found;
  }
  if (tokens.size() == 2) {
    list_.push_back(base);
    return base;
  }
  std::optional<std::string> first;
  for (const std::string& variant : split_comma_list(tokens[3])) {
    list_.push_back(base + variant);
    if (!first) {
      first = list_.back();
    }
  }
  return first;
}

void StyleMachine::match_reload(Tokens tokens) {
  const std::string& name = *current_;
  if (tokens.size() == 2) {
    if (tokens[1] == name) {
      status_ = LoadStatus::pending;
    }
    return;
  }
  const std::string& base = tokens[1];
  if (!text::starts_with(name, base)) {
    return;
  }
  const std::string_view suffix = std::string_view{name}.substr(base.size());
  for (const std::string& variant : split_comma_list(tokens[3])) {
    if (variant == suffix) {
      status_ = LoadStatus::pending;
      return;
    }
  }
}

bool StyleMachine::style_line(Tokens tokens) {
  const std::size_t argc = tokens.size();
  if (argc != 2 && !(argc == 4 && text::starts_with(tokens[2], "variant"))) {
    return false;
  }
  const std::optional<std::string> declared = declare(tokens);
  if (status_ == LoadStatus::pending || status_ == LoadStatus::suspended) {
    status_ = LoadStatus::loaded;
  } else if (status_ == LoadStatus::not_loaded) {
    if (!current_) {
      // The first style of a first load; an empty variant list declares
      // nothing (Magic would dereference NULL), so nothing is selected.
      if (declared) {
        current_ = declared;
        status_ = LoadStatus::pending;
      }
    } else {
      match_reload(tokens);
    }
  }
  return true;
}

namespace {

/// The `variant` line's list: pieces before a comma lose trailing white space.
std::vector<std::string> variant_pieces(std::string_view text) {
  std::vector<std::string> pieces = split_comma_list(text);
  const bool trailing_comma = !text.empty() && text.back() == ',';
  const std::size_t trimmed = trailing_comma ? pieces.size() : (pieces.empty() ? 0 : pieces.size() - 1);
  for (std::size_t i = 0; i < trimmed; ++i) {
    std::string& piece = pieces[i];
    while (!piece.empty() && std::isspace(static_cast<unsigned char>(piece.back())) != 0) {
      piece.pop_back();
    }
  }
  return pieces;
}

}  // namespace

bool StyleMachine::variant_line(Tokens tokens) {
  if (tokens.size() != 2) {
    return false;
  }
  const std::string& name = current_ ? *current_ : std::string{};
  for (const std::string& piece : variant_pieces(tokens[1])) {
    if (!piece.empty() && piece.front() == '*') {
      status_ = LoadStatus::pending;
      return true;
    }
    // Magic compares the name's last strlen(piece) characters; a piece
    // longer than the name would index before it (X-2 tier 1: no match).
    const bool fits = cif_ ? piece.size() < name.size() : piece.size() <= name.size();
    if (fits && std::string_view{name}.substr(name.size() - piece.size()) == piece) {
      status_ = LoadStatus::pending;
      return true;
    }
  }
  status_ = LoadStatus::suspended;
  return true;
}

bool StyleMachine::admit_rule_line() {
  if (list_.empty()) {
    const std::vector<std::string> implicit{"style", "default"};
    (void)style_line(implicit);
  } else if (list_.size() == 1) {
    // On reload, a single style is always loaded (DRC-CIF extensions).
    status_ = LoadStatus::pending;
  }
  return active();
}

void StyleMachine::ensure_default() {
  if (list_.empty()) {
    list_.emplace_back("default");
    current_ = "default";
    status_ = LoadStatus::loaded;
  }
}

}  // namespace hefesto::drc::rules_detail
