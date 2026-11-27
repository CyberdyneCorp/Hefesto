// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The dispatcher shape of ext2spice, ext2sim and antennacheck
// (netlist/table_command.hpp).
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c:375-380,
// 811-820 (the lookup and the usage loop), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION.

#include "netlist/table_command.hpp"

#include "db/command_util.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/netlist/first_word.hpp"

namespace hefesto::netlist::detail {

namespace cu = db::cmdutil;

std::string option_name(std::string_view entry) {
  return std::string{entry.substr(0, entry.find_first_of(" \t\n"))};
}

void print_table(cmd::CommandContext& ctx, std::span<const std::string_view> table) {
  auto session = cu::session(ctx);
  if (!session) {
    return;
  }
  std::string text;
  for (const std::string_view entry : table) {
    text += "    " + std::string{entry} + "\n";
  }
  cu::print_block(session.value(), std::move(text));
}

cmd::CommandResult dispatch(cmd::CommandContext& ctx, const cmd::Invocation& invocation,
                            const TableCommand& command) {
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  const FirstWordResult first = classify_first_word(invocation.args, command.table, session.value().magic_compat);
  switch (first.kind) {
    case FirstWord::ambiguous:
      cu::print(session.value(), "Ambiguous option: " + invocation.args.front());
      return cmd::CommandResult::success();
    case FirstWord::run:
      return command.run(ctx, first.run_args);
    case FirstWord::option:
      break;
  }
  if (first.index == command.help) {
    print_table(ctx, command.table);
    return cmd::CommandResult::success();
  }
  if (command.option) {
    if (std::optional<cmd::CommandResult> handled = command.option(ctx, invocation, first.index)) {
      return std::move(*handled);
    }
  }
  // Unreachable: every table command handles each of its option indices
  // (tasks.md 19.5 replaced the scaffold's CORE-NOTIMPL-008 here).
  return cu::fail(diag::make(diag::codes::kCmdInternal,
                             invocation.name + " " + option_name(command.table[first.index]) + ": option has no handler"));
}

}  // namespace hefesto::netlist::detail
