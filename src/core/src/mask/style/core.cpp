// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shared style/variant/scale core (see core.hpp).
//
// Ported from Magic VLSI 8.3.694 cif/CIFtech.c (CIFParseScale, the style
// and variant handling of CIFTechLine) and cif/CIFrdtech.c (the same in
// CIFReadTechLine), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Deviations (docs/decisions/cross-cutting.md X-2): exact registration
// (Q-MASK-10, fix); a variant suffix longer than the style name never
// matches instead of reading before the name (tier 1); a scale with ten or
// more decimal places keeps 10^9 as its denominator instead of overflowing.
//
// Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/mask/style/core.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "mask/style/text.hpp"

namespace hefesto::mask {

namespace {

using style_text::split_commas;
using style_text::starts_with;

bool is_style_line(const std::vector<std::string>& tokens) {
  return tokens.size() == 2 || (tokens.size() == 4 && starts_with(tokens[2], "variant"));
}

/// The pieces of a `variant` line: white space before a comma is removed
/// from the piece it ends.
std::vector<std::string> variant_pieces(std::string_view text) {
  std::vector<std::string> pieces = split_commas(text);
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

std::optional<ScaleValue> parse_scale(std::string_view text) {
  if (text.empty()) {
    return std::nullopt;
  }
  const std::size_t dot = text.find('.');
  if (dot == std::string_view::npos) {
    return ScaleValue{style_text::c_atoi(text), 1};
  }
  const std::string_view decimals = text.substr(dot + 1);
  std::int64_t d = 1;
  for (std::size_t i = 0; i < decimals.size() && d < 1000000000; ++i) {
    d *= 10;
  }
  std::int64_t n = std::int64_t{style_text::c_atoi(text.substr(0, dot))} * d + style_text::c_atoi(decimals);
  const std::int64_t g = style_text::gcf(n, d);  // ReduceFraction
  if (g > 1) {
    n /= g;
    d /= g;
  }
  return ScaleValue{style_text::clamp_int(n), style_text::clamp_int(d)};
}

StyleStateMachine::StyleStateMachine(Options options) : options_(std::move(options)) {
  if (!options_.wanted.empty()) {
    loading_ = options_.wanted;  // a reload: CIFLoadStyle names the style first
  }
}

std::vector<std::string> StyleStateMachine::names() const {
  std::vector<std::string> all = names_;
  all.insert(all.end(), appended_.begin(), appended_.end());
  return all;
}

bool StyleStateMachine::declared(std::string_view name) const {
  const auto equal = [name](const std::string& s) { return s == name; };
  return std::any_of(names_.begin(), names_.end(), equal) || std::any_of(appended_.begin(), appended_.end(), equal);
}

std::optional<std::string> StyleStateMachine::declare_exact(const std::vector<std::string>& tokens) {
  const std::string& base = tokens[1];
  std::vector<std::string> new_names;
  if (tokens.size() == 2) {
    new_names.push_back(base);
  } else {
    for (const std::string& variant : split_commas(tokens[3])) {
      new_names.push_back(base + variant);
    }
  }
  for (const std::string& name : new_names) {
    if (declared(name)) {
      return name;  // already present: nothing is registered
    }
  }
  // Magic's existence test (strncmp over strlen(base)) would drop the names.
  const auto magic = std::find_if(names_.begin(), names_.end(),
                                  [&base](const std::string& s) { return starts_with(s, base); });
  std::vector<std::string>& into = magic == names_.end() ? names_ : appended_;
  if (magic != names_.end()) {
    for (const std::string& name : new_names) {
      shadowed_.push_back(Shadowed{name, *magic});
    }
  }
  into.insert(into.end(), new_names.begin(), new_names.end());
  return new_names.empty() ? std::nullopt : std::optional{new_names.front()};
}

std::optional<std::string> StyleStateMachine::declare(const std::vector<std::string>& tokens) {
  if (options_.registration == Registration::exact) {
    return declare_exact(tokens);
  }
  const std::string& base = tokens[1];
  const auto found =
      std::find_if(names_.begin(), names_.end(), [&base](const std::string& s) { return starts_with(s, base); });
  if (found != names_.end()) {
    return *found;
  }
  if (tokens.size() == 2) {
    names_.push_back(base);
    return base;
  }
  std::optional<std::string> first;
  for (const std::string& variant : split_commas(tokens[3])) {
    names_.push_back(base + variant);
    if (!first) {
      first = names_.back();
    }
  }
  return first;
}

void StyleStateMachine::match_wanted(const std::vector<std::string>& tokens) {
  const std::string& base = tokens[1];
  if (tokens.size() == 2) {
    if (base == loading_) {
      state_ = State::pending;
    }
    return;
  }
  if (!starts_with(loading_, base)) {
    return;
  }
  const std::string_view suffix = std::string_view{loading_}.substr(base.size());
  for (const std::string& variant : split_commas(tokens[3])) {
    if (variant == suffix) {
      state_ = State::pending;
      return;
    }
  }
}

void StyleStateMachine::variant_line(const std::string& list) {
  const std::string& name = loading_;
  for (const std::string& piece : variant_pieces(list)) {
    if (!piece.empty() && piece.front() == '*') {
      state_ = State::pending;
      return;
    }
    const bool fits =
        options_.variant_rule == VariantRule::shorter ? piece.size() < name.size() : piece.size() <= name.size();
    if (fits && std::string_view{name}.substr(name.size() - piece.size()) == piece) {
      state_ = State::pending;
      return;
    }
  }
  state_ = State::suspended;
}

StyleStateMachine::Fed StyleStateMachine::feed(const std::vector<std::string>& tokens) {
  if (tokens.empty()) {
    return {};
  }
  if (tokens[0] == "style") {
    if (!is_style_line(tokens)) {
      return Fed{LineKind::style, true};
    }
    const std::optional<std::string> name = declare(tokens);
    if (loading_.empty()) {
      // The first `style` line of a first load (CIFCurStyle == NULL).
      if (name) {
        loading_ = *name;
        state_ = State::pending;
      }
    } else if (active()) {
      state_ = State::loaded;
    } else if (state_ == State::not_loaded) {
      match_wanted(tokens);
    }
    return Fed{LineKind::style, false};
  }
  if (!active() || !starts_with(tokens[0], "variant")) {
    return {};
  }
  if (tokens.size() != 2) {
    return Fed{LineKind::variant, true};
  }
  variant_line(tokens[1]);
  return Fed{LineKind::variant, false};
}

}  // namespace hefesto::mask
