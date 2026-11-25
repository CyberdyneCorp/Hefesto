// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The option parsers of the `extract` command (private to
// extract/command/; capability extract-commands; tasks.md 9.1-9.4):
// CmdExtract's switch after the option lookup and the window check, which
// commands.cpp's dispatcher has done.
//
// Owner: EXTCMD.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "db/command_util.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"

namespace hefesto::extract::detail {

/// The words after `extract` with the leading `list`/`listall` removed.
struct OptionArgs {
  std::vector<std::string> args;  // args[0] is the option word (absent: bare `extract`)
  bool dolist = false;
  bool doforall = false;
};

/// CmdExtract's cmdExtCmd indices (EXTALL .. EXTWARN).
enum Option : std::size_t {
  kAll = 0,
  kCell = 1,
  kDo = 2,
  kHalo = 3,
  kHelp = 4,
  kLength = 5,
  kNo = 6,
  kParents = 7,
  kPath = 8,
  kShowParents = 9,
  kStepSize = 10,
  kStyle = 11,
  kUnique = 12,
  kWarn = 13,
  kIncremental = 14,  // bare `extract` (EXTINCREMENTAL)
};

/// Runs `option` (the window check is done; `help` is the dispatcher's).
[[nodiscard]] cmd::CommandResult run_option(cmd::CommandContext& ctx, const db::cmdutil::Session& s,
                                            std::size_t option, const OptionArgs& a);

/// The wrong-argument message (`goto wrongNumArgs`).
cmd::CommandResult wrong_args(const db::cmdutil::Session& s);

}  // namespace hefesto::extract::detail
