// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the `ext2spice` options other than the run and `help`
// (capability netlist-spice-options "Command options"; Q-NETLIST-41 per
// mode).
//
// Owner: SPICE.

#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"

namespace hefesto::netlist::spice::detail {

/// Option indices of cmdExtToSpcOption (EXTTOSPC_*).
enum Option : std::size_t {
  kRun = 0,
  kDefault,
  kFormat,
  kRthresh,
  kCthresh,
  kMerge,
  kExtresist,
  kResistorTee,
  kScale,
  kShort,
  kSubcircuits,
  kHierarchy,
  kBlackbox,
  kRenumber,
  kGlobal,
  kLvs,
  kHelp,
};

/// Handles option `option` of `invocation` (its first argument is the
/// option word); `table` is the option table, printed on a usage error.
[[nodiscard]] std::optional<cmd::CommandResult> spice_option(cmd::CommandContext& ctx,
                                                             const cmd::Invocation& invocation, std::size_t option,
                                                             std::span<const std::string_view> table);

}  // namespace hefesto::netlist::spice::detail
