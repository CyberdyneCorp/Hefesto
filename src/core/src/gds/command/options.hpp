// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the `gds`/`calma` command (src/core/src/gds/command/): the
// argument forms CmdCalma shares between its options (wrongNumArgs, the
// yes/no and allow/disallow tables, the Tcl boolean result) and the read
// side of the option table (read_options.cpp: every read option, `warning`,
// `rescale` and `read`).
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "db/command_util.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/gds/command/settings.hpp"

namespace hefesto::gds::command_detail {

namespace cu = db::cmdutil;

inline constexpr std::string_view kNoWindow = "VIEW-WINDOW-003";
inline constexpr std::string_view kWrongArgs = "GDS-CMD-001";
inline constexpr std::string_view kBadValue = "GDS-CMD-002";
inline constexpr std::string_view kUnimplemented = "GDS-CMD-003";
inline constexpr std::string_view kCannotOpen = "GDS-CMD-004";

/// cmdCalmaOption indices (CALMA_* of commands/CmdCD.c).
enum Option : std::size_t {
  kHelp = 0,
  kAbstract = 1,
  kAddendum = 2,
  kArrays = 3,
  kCompress = 4,
  kContacts = 5,
  kDatestamp = 6,
  kDrcCheck = 7,
  kFlatten = 8,
  kFlatGlob = 9,
  kLabels = 10,
  kLibrary = 11,
  kLower = 12,
  kMagScale = 13,
  kMaskHints = 14,
  kMerge = 15,
  kNoStamp = 16,
  kNoDuplicates = 17,
  kOrdering = 18,
  kRead = 19,
  kReadOnly = 20,
  kRescale = 21,
  kSavePaths = 22,
  kWarning = 23,
  kWrite = 24,
  kPolygons = 25,
  kPaths = 26,
  kUndefined = 27,
  kUnique = 28,
};

inline constexpr std::array<std::string_view, 8> kYesNo{"no", "false", "off", "0", "yes", "true", "on", "1"};
inline constexpr std::array<std::string_view, 4> kAllowDisallow{"disallow", "0", "allow", "1"};

/// wrongNumArgs, with an optional first line.
[[nodiscard]] cmd::CommandResult wrong_args(const cmd::Invocation& invocation, std::string prefix = {});
/// Tcl_NewBooleanObj.
[[nodiscard]] cmd::CommandResult boolean(bool value);

/// Lookup() of `word` in `table`; nullopt when it fails.
template <std::size_t N>
[[nodiscard]] std::optional<std::size_t> lookup_index(std::string_view word,
                                                      const std::array<std::string_view, N>& table) {
  const cmd::LookupResult found = cmd::lookup(word, table);
  if (found.match != cmd::Match::found) {
    return std::nullopt;
  }
  return found.index;
}

/// A yes/no option of one argument: none reports the value; yes (index >=
/// 4) sets it, no clears it. `arg` is the index of the value word.
[[nodiscard]] cmd::CommandResult yes_no_option(const cmd::Invocation& invocation, bool& field, std::size_t arg = 1);

/// The read side of the option table: drccheck, flatten, flatglob,
/// magscale, maskhints, noduplicates, ordering, readonly, rescale,
/// savepaths, warning, polygon subcells, path subcells, unique; nullopt for
/// another option.
[[nodiscard]] std::optional<cmd::CommandResult> read_settings_option(const cu::Session& s, std::size_t option,
                                                                     const cmd::Invocation& invocation);

/// `gds read <file>` (CALMA_READ): the file lookup, an edit cell when none
/// exists, gds::read, then the library cell loaded into the window.
[[nodiscard]] cmd::CommandResult read_option(cmd::CommandContext& ctx, const cu::Session& s,
                                             const cmd::Invocation& invocation);

}  // namespace hefesto::gds::command_detail
