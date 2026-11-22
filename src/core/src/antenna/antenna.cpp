// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `antennacheck` command and RuleView (antenna/antenna.hpp;
// openspec/changes/m7-extraction/design.md D4, D17): Magic's option table,
// the first-word rule and `help`. The run and `debug` answer
// CORE-NOTIMPL-008 until ANTENNA lands them.
//
// Ported from Magic 8.3.694 (fc5daa9e) extflat/EFantenna.c CmdAntennaCheck
// (cmdAntennaCheckOption, the lookup and the usage loop), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION (the registration and dispatcher); ANTENNA (RuleView,
// check() and the run).

#include "hefesto/antenna/antenna.hpp"

#include <array>

#include "hefesto/cmd/m7_stubs.hpp"
#include "netlist/table_command.hpp"

namespace hefesto::antenna {

namespace {

// cmdAntennaCheckOption (ANTENNACHECK_RUN .. ANTENNACHECK_HELP).
constexpr std::array<std::string_view, 3> kOptions{
    "[run] [options]\trun antennacheck on current cell\n\t\t\tuse \"run -help\" to get standard options",
    "debug\t\t\tprint detailed information about each error",
    "help\t\t\tprint help information",
};

constexpr std::size_t kHelp = 2;

cmd::CommandResult antenna_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  const netlist::detail::TableCommand command{kOptions, kHelp,
                                              [](cmd::CommandContext& /*c*/, std::vector<std::string> /*args*/) {
                                                return cmd::m7_not_implemented("antennacheck run");
                                              },
                                              {}};
  return netlist::detail::dispatch(ctx, invocation, command);
}

}  // namespace

RuleView RuleView::from_style(const extract::ExtStyle& style) {
  RuleView view;
  view.rules_ = style.antenna;
  view.tiedown_ = style.tiedown;
  return view;
}

std::optional<extract::AntennaRule> RuleView::rules(tech::TileType type) const {
  for (const extract::AntennaRule& rule : rules_) {
    if (rule.type == type) {
      return rule;
    }
  }
  return std::nullopt;
}

cmd::CommandResult check(editor::EditorState& /*editor*/, db::CellDef& /*root*/, const AntennaOptions& /*options*/) {
  return cmd::m7_not_implemented("antennacheck run");
}

std::span<const std::string_view> antennacheck_options() noexcept {
  return kOptions;
}

// dbwind/DBWcommands.c:235.
bool register_antenna_command(cmd::CommandRegistry& registry) {
  return !registry
              .add(cmd::CommandSpec{"antennacheck", "antennacheck [path]", "check for antenna violations",
                                    antenna_command},
                   cmd::kLayoutClient)
              .has_value();
}

}  // namespace hefesto::antenna
