// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `antennacheck` command (antenna/antenna.hpp; capability antenna-check
// "Antenna check command"; openspec/changes/m7-extraction/design.md D4,
// D17): Magic's option table, the first-word rule (Q-MISC-91, compat, as
// Q-NETLIST-13), `help`, `debug` (armed for the next run) and the run.
//
// `debug` is Magic's efAntennaDebug, kept per session
// (`EditorState::antenna_debug`): it is set by `antennacheck debug` and
// cleared when a run reaches its end (an early return, such as a missing
// plane order, leaves it armed).
//
// Ported from Magic 8.3.694 (fc5daa9e) extflat/EFantenna.c CmdAntennaCheck
// (cmdAntennaCheckOption, the lookup and the usage loop), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION (the registration); ANTENNA (the dispatcher and run).

#include "hefesto/antenna/antenna.hpp"

#include <array>
#include <utility>

#include "db/command_util.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "netlist/table_command.hpp"

namespace hefesto::antenna {

namespace {

// cmdAntennaCheckOption (ANTENNACHECK_RUN .. ANTENNACHECK_HELP).
constexpr std::array<std::string_view, 3> kOptions{
    "[run] [options]\trun antennacheck on current cell\n\t\t\tuse \"run -help\" to get standard options",
    "debug\t\t\tprint detailed information about each error",
    "help\t\t\tprint help information",
};

constexpr std::size_t kDebug = 1;
constexpr std::size_t kHelp = 2;

cmd::CommandResult run_command(cmd::CommandContext& ctx, std::vector<std::string> args) {
  auto session = db::cmdutil::session(ctx);
  if (!session) {
    return db::cmdutil::fail(session.error());
  }
  AntennaOptions options;
  options.args = std::move(args);
  editor::EditorState& editor = *session.value().editor;
  options.debug = editor.antenna_debug;
  options.magic_compat = session.value().magic_compat;
  options.files = &ctx.services().files;
  options.emit = [&ctx](diag::Diagnostic d) { ctx.emit(d); };
  const view::LayoutWindow* w = editor.windows.current();
  db::CellDef* root = w != nullptr ? w->root() : nullptr;
  options.finished = [&editor] { editor.antenna_debug = false; };  // cleared at the end of the run
  return check(editor, root, options);
}

std::optional<cmd::CommandResult> option(cmd::CommandContext& ctx, const cmd::Invocation& /*invocation*/,
                                         std::size_t index) {
  if (index != kDebug) {
    return std::nullopt;
  }
  auto session = db::cmdutil::session(ctx);
  if (!session) {
    return db::cmdutil::fail(session.error());
  }
  session.value().editor->antenna_debug = true;
  return cmd::CommandResult::success();
}

cmd::CommandResult antenna_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  const netlist::detail::TableCommand command{kOptions, kHelp, run_command, option};
  return netlist::detail::dispatch(ctx, invocation, command);
}

}  // namespace

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
