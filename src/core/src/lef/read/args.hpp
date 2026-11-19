// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `lef read` and `lef datestamp` options of the lef/def dispatcher
// (capability lef-read "lef read arguments", "lef datestamp"; design D14):
// CmdLef's LEF_READ and LEF_DATESTAMP cases (lef/lefCmd.c:223-269,
// 446-482). The dispatcher (lef/command/command.cpp, INTEGRATION) calls
// them after its option lookup and window check.
//
// Owner: LEFR (design D2, D14).

#pragma once

#include <functional>

#include "db/command_util.hpp"
#include "hefesto/cmd/command.hpp"

namespace hefesto::lef {

/// The dispatcher's help listing (wrongNumArgs).
using HelpPrinter = std::function<cmd::CommandResult()>;

/// `lef read <file> [option ...]`: the options after the file name are
/// matched by prefix (`-import` 7 characters, `-anno` 5, `-label` 6,
/// `-noblock` 8; others ignored); without a file name, the help.
[[nodiscard]] cmd::CommandResult read_option(const db::cmdutil::Session& s, const cmd::Invocation& invocation,
                                             const HelpPrinter& help);

/// `lef datestamp [default | <int>]`: the query answers the stamp or
/// `default`; an unrecognizable value reports `Unrecognizable date stamp
/// "<v>".` and prints the help, as more than one value does.
[[nodiscard]] cmd::CommandResult datestamp_option(const db::cmdutil::Session& s, const cmd::Invocation& invocation,
                                                  const HelpPrinter& help);

}  // namespace hefesto::lef
