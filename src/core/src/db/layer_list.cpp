// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The layer-list argument (cmd-paint-erase "Layer list syntax"). Ported
// from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdSubrs.c (CmdParseLayers),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md). Owner: EDIT.

#include "hefesto/db/layer_list.hpp"

#include <array>
#include <string>

#include "db/tech_masks.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/labels.hpp"

namespace hefesto::db {

namespace {

enum class Special : std::uint8_t { dollar, all, errors, labels, subcell, connect };

constexpr std::array<std::string_view, 6> kSpecials = {"$", "*", "errors", "labels", "subcell", "connect"};

bool is_separator(char c) noexcept {
  return c == ',' || c == '+' || c == '-' || c == ' ';
}

constexpr std::string_view kUnrecognized = "Unrecognized layer";
constexpr std::string_view kAmbiguous = "Ambiguous layer";

// The special names listed after a bad one (TxError; the DBTechPrintTypes
// part before them is TxPrintf, see is_bad_layer_name).
std::string special_names() {
  std::string out;
  for (const std::string_view s : kSpecials) {
    out += "\n    " + std::string{s};
  }
  return out;
}

diag::Diagnostic bad_layer(std::string_view why, std::string_view name) {
  return diag::make(codes::kBadLayer, std::string{why} + ": " + std::string{name} + special_names());
}

tech::TypeMask connected_to(const tech::Technology& tech, const tech::TypeMask& set) {
  tech::TypeMask out;
  for (std::size_t t = tech::kFirstUserType; t < tech.type_count(); ++t) {
    if (set.test(static_cast<tech::TileType>(t))) {
      out |= tech.connects_to(static_cast<tech::TileType>(t));
    }
  }
  return out;
}

diag::Outcome<tech::TypeMask> special_mask(Special special, const tech::Technology& tech, const tech::TypeMask& set,
                                           const LayerListContext& context) {
  tech::TypeMask m;
  switch (special) {
    case Special::labels:
      m.set(kLabelsPseudoType);
      return m;
    case Special::subcell:
      m.set(kSubcellPseudoType);
      return m;
    case Special::dollar:
      if (!context.under_pointer) {
        return diag::make(codes::kBadLayer, "Put the cursor in a layout window");
      }
      return *context.under_pointer & (all_but_space_and_drc() | tech::TypeMask::of(tech::kSpace));
    case Special::all:
      m = all_but_space_and_drc();
      m.clear_type(kLabelsPseudoType);
      m.clear_type(kSubcellPseudoType);
      return m;
    case Special::errors:
      m.set(tech::kErrorP);
      m.set(tech::kErrorS);
      m.set(tech::kErrorPS);
      return m;
    case Special::connect:
      break;
  }
  return connected_to(tech, set);
}

// DBTechNameTypes then the specials: the types one name denotes.
diag::Outcome<tech::TypeMask> name_mask(std::string_view name, const tech::Technology& tech,
                                        const tech::TypeMask& set, const LayerListContext& context) {
  for (const auto& [alias, mask] : tech.aliases()) {
    if (alias == name) {
      return mask;
    }
  }
  const auto found = tech.find_type(name);
  if (found.ok()) {
    return tech::TypeMask::of(*found.id);
  }
  if (found.error == tech::LookupError::ambiguous) {
    return bad_layer(kAmbiguous, name);
  }
  const cmd::LookupResult r = cmd::lookup(name, kSpecials);
  if (r.match != cmd::Match::found) {
    return bad_layer(kUnrecognized, name);
  }
  return special_mask(static_cast<Special>(r.index), tech, set, context);
}

// `+` and `-` switch the mode until the next one; other separators keep it.
bool mode_after(char separator, bool adding) noexcept {
  if (separator == '-') {
    return false;
  }
  return separator == '+' ? true : adding;
}

std::size_t name_end(std::string_view text, std::size_t pos) noexcept {
  while (pos < text.size() && !is_separator(text[pos])) {
    ++pos;
  }
  return pos;
}

LayerList as_list(const tech::TypeMask& set) {
  LayerList out;
  out.types = set;
  out.labels = set.test(kLabelsPseudoType);
  out.subcells = set.test(kSubcellPseudoType);
  out.errors = set.test(tech::kErrorP) || set.test(tech::kErrorS) || set.test(tech::kErrorPS);
  return out;
}

}  // namespace

diag::Outcome<LayerList> parse_layer_list(std::string_view text, const tech::Technology& tech,
                                          const LayerListContext& context) {
  tech::TypeMask set;
  bool adding = true;
  std::size_t pos = 0;
  while (pos < text.size()) {
    if (is_separator(text[pos])) {
      adding = mode_after(text[pos], adding);
      ++pos;
      continue;
    }
    const std::size_t end = name_end(text, pos);
    auto m = name_mask(text.substr(pos, end - pos), tech, set, context);
    if (!m.ok()) {
      return m.error();
    }
    set = adding ? (set | m.value()) : (set & ~m.value());
    pos = end;
  }
  return as_list(set);
}

bool is_bad_layer_name(const diag::Diagnostic& d) {
  return d.code == codes::kBadLayer && (d.text.starts_with(kUnrecognized) || d.text.starts_with(kAmbiguous));
}

}  // namespace hefesto::db
