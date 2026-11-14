// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The session options of the `cif` command (capability cif-format-io "cif
// limit and rescale options", "cif output-format toggles", "cif drccheck
// option", "cif warning option", "cif polygon and unfracture options";
// mask-hierarchical-generation "Hierarchy and array output switches").
// Ported from Magic 8.3.694 (fc5daa9e) commands/CmdCD.c CmdCif (ARRAY and
// HIER with `write`, AREALABELS, CIF_DRC_CHECK, IDCELL, CIF_LIMIT, PREFIX,
// RESCALE, WARNING, POLYGONS, UNFRACTURE), Copyright (C) 1985, 1990 Regents
// of the University of California (see THIRD_PARTY_NOTICES.md). Queries
// answer as Magic's Tcl build does (Tcl_SetObjResult).
// Owner: CIF.

#include <array>
#include <cstdint>

#include "command_impl.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::cif::command_detail {

namespace {

using cmd::CommandResult;

constexpr std::int32_t kMaxRescale = 100;  // CIFMAXRESCALE

CommandResult boolean(bool value) {
  return CommandResult::success(cmd::Value{std::int64_t{value ? 1 : 0}});
}

CifSettings& settings_of(const CifCall& c) {
  return c.s.editor->mask.cif;
}

// `cif *hier|*array write enable|disable|<other>` (the preview forms are
// output.cpp's).
CommandResult write_switch(const CifCall& c, std::size_t option) {
  if (!c.words[1].starts_with("write")) {
    return wrong_args(c, option);
  }
  bool& enabled = option == kHier ? settings_of(c).hier : settings_of(c).array;
  const std::string& word = c.words[2];
  if (word.starts_with("dis")) {
    enabled = false;
  } else if (word.starts_with("en")) {
    enabled = true;
  } else {
    // Magic prints "Subcell" for both switches.
    cu::print(c.s, std::string{"Subcell interaction output "} + (enabled ? "enabled" : "disabled"));
  }
  return CommandResult::success();
}

// AREALABELS and IDCELL.
CommandResult toggle(const CifCall& c, std::size_t option) {
  if (c.argc() > 3) {
    return wrong_args(c, option);
  }
  bool& value = option == kAreaLabels ? settings_of(c).arealabels : settings_of(c).idcell;
  if (c.argc() == 3) {
    const int yes = yes_no(c.words[1]);
    if (yes < 0) {
      return wrong_args(c, option);
    }
    value = yes == 1;
  }
  const std::string what = option == kAreaLabels ? "Output of CIF area labels is now " : "Output of CIF cell ID labels is now ";
  cu::print(c.s, what + (value ? "enabled" : "disabled"));
  return CommandResult::success();
}

// CIF_LIMIT.
CommandResult limit(const CifCall& c) {
  std::int32_t& value = settings_of(c).limit;
  if (c.argc() == 2) {
    return CommandResult::success(cmd::Value{std::int64_t{value > 0 ? value : kMaxRescale}});
  }
  if (c.argc() != 3 || !cu::str_is_int(c.words[1])) {
    return wrong_args(c, kLimit);
  }
  value = cu::c_atoi(c.words[1]);
  if (value > kMaxRescale) {
    value = kMaxRescale;
    cu::warn(c.s, "Warning:  rescale limit set at maximum value of " + std::to_string(kMaxRescale));
  } else if (value < 1) {
    value = 1;
    cu::warn(c.s, "Warning:  rescale limit set at minimum value of 1");
  }
  return CommandResult::success();
}

// PREFIX.
CommandResult prefix(const CifCall& c) {
  if (c.argc() > 3) {
    return wrong_args(c, kPrefix);
  }
  settings_of(c).prefix = c.argc() == 3 ? c.words[1] : std::string{};
  return CommandResult::success();
}

// RESCALE and CIF_DRC_CHECK: a boolean query, or yes/no.
CommandResult yes_no_option(const CifCall& c, std::size_t option, bool& value) {
  if (c.argc() == 2) {
    return boolean(value);
  }
  if (c.argc() != 3) {
    return wrong_args(c, option);
  }
  const int yes = yes_no(c.words[1]);
  if (yes < 0) {
    return wrong_args(c, option);
  }
  value = yes == 1;
  if (option == kRescale && !value) {
    c.s.editor->mask.warnings.level = mask::WarningLevel::limit;  // CIF_WARN_LIMIT
  }
  return CommandResult::success();
}

// WARNING.
CommandResult warning(const CifCall& c) {
  static constexpr std::array<std::string_view, 6> kLevels{"default", "none", "align", "limit", "redirect", "help"};
  mask::WarningSettings& w = c.s.editor->mask.warnings;
  if (c.argc() == 2) {
    cu::print(c.s, "Warning display options: " + std::string{kLevels.at(static_cast<std::size_t>(w.level))});
    return CommandResult::success();
  }
  int level = cu::lookup_in(c.words[1], kLevels);
  if (level < 0) {
    cu::warn(c.s, "\"" + c.words[1] + "\" isn't a valid cif warning option.");
    level = 5;
  }
  if (level == 5) {
    cu::warn(c.s, "Valid options are: default, align, limit, redirect, and none");
    return CommandResult::success();
  }
  w.level = static_cast<mask::WarningLevel>(level);
  if (w.level == mask::WarningLevel::redirect) {
    w.redirect_file = c.argc() == 4 ? c.words[2] : std::string{};
  }
  return CommandResult::success();
}

// POLYGONS: `cif polygon <any>` queries, `cif polygon <any> yes|no` sets.
CommandResult polygon(const CifCall& c) {
  bool& value = settings_of(c).polygon_subcells;
  if (c.argc() == 3) {
    return boolean(value);
  }
  if (c.argc() != 4) {
    return wrong_args(c, kPolygon);
  }
  const int yes = yes_no(c.words[2]);
  if (yes < 0) {
    return wrong_args(c, kPolygon);
  }
  value = yes == 1;
  return CommandResult::success();
}

// UNFRACTURE (Q-MASK-413): Magic reads the value from the fourth word only.
CommandResult unfracture(const CifCall& c) {
  bool& value = settings_of(c).unfracture;
  if (c.argc() == 3) {
    const int yes = yes_no(c.words[1]);
    if (yes >= 0 && !c.s.magic_compat) {
      value = yes == 1;
      return CommandResult::success();
    }
    if (yes >= 0) {
      c.s.ctx->emit(diag::make_native(kUnfractureIgnored, "Warning: \"" + c.words[1] +
                                                              "\" was not applied; with magic-compat on the value "
                                                              "is the fourth word (cif unfracture <any> yes|no)."));
    }
    return boolean(value);
  }
  if (c.argc() != 4) {
    return wrong_args(c, kUnfracture);
  }
  const int yes = yes_no(c.words[2]);
  if (yes < 0) {
    return wrong_args(c, kUnfracture);
  }
  value = yes == 1;
  return CommandResult::success();
}

}  // namespace

int yes_no(std::string_view word) {
  return cu::lookup(word, {"no", "yes"});
}

CommandResult settings_option(const CifCall& c, std::size_t option) {
  switch (option) {
    case kArray:
    case kHier:
      return write_switch(c, option);
    case kAreaLabels:
    case kIdcell:
      return toggle(c, option);
    case kLimit:
      return limit(c);
    case kPrefix:
      return prefix(c);
    case kRescale:
      return yes_no_option(c, option, settings_of(c).rescale);
    case kDrcCheck:
      return yes_no_option(c, option, settings_of(c).drccheck);
    case kWarning:
      return warning(c);
    case kPolygon:
      return polygon(c);
    default:
      return unfracture(c);
  }
}

}  // namespace hefesto::cif::command_detail
