// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the `oasis` command (src/core/src/oasis/command/): the option
// indices, the error codes and the option handlers (capability
// oasis-command "Option syntax", "Options shared with the gds command",
// "OASIS-only options", "Settings shared with the gds command";
// openspec/changes/m6-lef-def-oasis/design.md D15).
//
// The option table keeps the `gds` table's order for its first 29 entries,
// so the read settings shared with `gds` (MaskSession::stream_read, the
// `rescale` and `warning` session fields) are handled by the `gds`
// command's own read_settings_option (gds/command/options.hpp) and are
// one setting with it; every other option reads and writes
// MaskSession::oasis.
//
// Owner: OASCMD (openspec/changes/m6-lef-def-oasis/design.md D2).

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "db/command_util.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/oasis/command/settings.hpp"

namespace hefesto::oasis::command_detail {

namespace cu = db::cmdutil;

inline constexpr std::string_view kWrongArgs = "OASIS-CMD-001";
inline constexpr std::string_view kBadValue = "OASIS-CMD-002";
inline constexpr std::string_view kUnimplemented = "OASIS-CMD-003";
inline constexpr std::string_view kCannotOpen = "OASIS-CMD-004";
inline constexpr std::string_view kNoWindow = "VIEW-WINDOW-003";

/// Indices in the option table (the `gds` indices, then the OASIS-only
/// options).
enum Option : std::size_t {
  kHelp = 0,
  kAbstract = 1,
  kAddendum = 2,
  kArrays = 3,
  kCompress = 4,
  kContacts = 5,
  kDatestamp = 6,
  kLabels = 10,
  kLibrary = 11,
  kLower = 12,
  kMagScale = 13,
  kMerge = 15,
  kNoStamp = 16,
  kRead = 19,
  kWrite = 24,
  kUndefined = 27,
  kStrict = 29,
  kValidation = 30,
  kRepetitions = 31,
  kLayerNames = 32,
  kCirclePoints = 33,
};

/// "Wrong number of arguments" (an argument error; nothing changes).
[[nodiscard]] cmd::CommandResult wrong_args(const cmd::Invocation& invocation, std::string prefix = {});

/// The options stored in MaskSession::oasis (write options, datestamp,
/// nodatestamp, magscale and the OASIS-only options); nullopt for another
/// option.
[[nodiscard]] std::optional<cmd::CommandResult> settings_option(cmd::CommandContext& ctx, std::size_t option,
                                                                const cmd::Invocation& invocation,
                                                                OasisSettings& settings);

/// The read settings shared with `gds`, through the `gds` command's
/// handlers, with their GDS-CMD-* argument errors renumbered to the
/// OASIS-CMD-* ones; nullopt for another option.
[[nodiscard]] std::optional<cmd::CommandResult> shared_option(const cu::Session& s, std::size_t option,
                                                              const cmd::Invocation& invocation);

/// `oasis read <file>` (read_command.cpp).
[[nodiscard]] cmd::CommandResult read_option(cmd::CommandContext& ctx, const cu::Session& s,
                                             const cmd::Invocation& invocation);

/// `oasis write <file>` and a bare `oasis` (write_command.cpp).
[[nodiscard]] cmd::CommandResult write_option(cmd::CommandContext& ctx, const cu::Session& s,
                                              const cmd::Invocation& invocation);

/// The output file of `oasis write <name>` (oasis-write "Write command and
/// output file naming" rule 3): `name`, with `.oas` appended when it holds
/// no `.` anywhere.
[[nodiscard]] std::string output_file_name(std::string_view name);

/// A command of the layout client run from this one (`load`), its messages
/// passed on.
cmd::CommandResult run_nested(cmd::CommandContext& ctx, std::vector<std::string> words);

}  // namespace hefesto::oasis::command_detail
