// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the `cif` command (src/core/src/cif/command/): one parsed
// call and the option handlers, split by file:
// - command.cpp: dispatch, help, the style and scale options;
// - options.cpp: the session settings (arealabels, drccheck, idcell,
//   limit, prefix, rescale, warning, polygon, unfracture and the `write`
//   form of `*hier` and `*array`);
// - output.cpp: write, flat, read, see, paint, coverage, statistics and the
//   `*hier`/`*array` previews.
// Ported from Magic 8.3.694 (fc5daa9e) commands/CmdCD.c CmdCif, Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
// Owner: CIF.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "db/command_util.hpp"
#include "hefesto/cmd/command.hpp"

namespace hefesto::cif::command_detail {

namespace cu = db::cmdutil;

/// The options in Magic's order (cmdCifOption).
enum Option : std::size_t {
  kArray = 0,
  kHier = 1,
  kAreaLabels = 2,
  kCoverage = 3,
  kDrcCheck = 4,
  kHelp = 5,
  kIdcell = 6,
  kIstyle = 7,
  kLambda = 8,
  kLimit = 9,
  kOstyle = 10,
  kPaint = 11,
  kPrefix = 12,
  kRead = 13,
  kRescale = 14,
  kScale = 15,
  kSee = 16,
  kStats = 17,
  kWarning = 18,
  kWrite = 19,
  kFlat = 20,
  kPolygon = 21,
  kUnfracture = 22,
};

/// The CIF-CMD-* codes (codes.inc).
inline constexpr std::string_view kUnfractureIgnored = "CIF-CMD-001";
inline constexpr std::string_view kProblems = "CIF-CMD-002";

/// One parsed `cif` call: the list mode words removed (Magic's argv++).
struct CifCall {
  cu::Session s;
  std::vector<std::string> words;  // words[0] is the option
  bool list = false;
  bool listall = false;
  /// Magic's argc: the command name, the option and its arguments.
  [[nodiscard]] std::size_t argc() const noexcept { return words.size() + 1; }
};

/// The whole help entry of `option`.
[[nodiscard]] std::string_view option_entry(std::size_t option);
/// Magic's wrongNumArgs: three lines, the command still succeeds.
cmd::CommandResult wrong_args(const CifCall& c, std::size_t option);
/// Magic's Lookup of a yes/no word: 0 no, 1 yes, < 0 when neither.
[[nodiscard]] int yes_no(std::string_view word);

/// The session options of options.cpp.
[[nodiscard]] cmd::CommandResult settings_option(const CifCall& c, std::size_t option);

/// The output options of output.cpp.
[[nodiscard]] cmd::CommandResult write_option(const CifCall& c, const std::string& name, bool flat);
[[nodiscard]] cmd::CommandResult read_option(const CifCall& c);
[[nodiscard]] cmd::CommandResult see_option(const CifCall& c, std::size_t option);
[[nodiscard]] cmd::CommandResult paint_option(const CifCall& c);
[[nodiscard]] cmd::CommandResult coverage_option(const CifCall& c);
[[nodiscard]] cmd::CommandResult statistics_option(const CifCall& c);

}  // namespace hefesto::cif::command_detail
