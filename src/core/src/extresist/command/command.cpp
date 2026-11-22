// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `extresist` command (extresist/command/command.hpp;
// openspec/changes/m7-extraction/design.md D4, D16): Magic's option table,
// the lookup with `Ambiguous option: <w>` and `Unknown option: <w>`, and
// `help`. The run and every other option answer CORE-NOTIMPL-008 until
// RESCMD lands them.
//
// Ported from Magic 8.3.694 (fc5daa9e) resis/ResRex.c CmdExtResis
// (cmdExtresisCmd, the lookup, RES_HELP, RES_AMBIG and RES_BAD), Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION (the registration and dispatcher); RESCMD (the option
// parsers and run()).

#include "hefesto/extresist/command/command.hpp"

#include <array>
#include <string>

#include "db/command_util.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/cmd/m7_stubs.hpp"

namespace hefesto::extresist {

namespace {

namespace cu = db::cmdutil;
using cmd::CommandResult;

// cmdExtresisCmd, in Magic's order (RES_ALL .. RES_HELP).
constexpr std::array<std::string_view, 21> kOptions{
    "all \t\t       extract all the nets",
    "threshold [value]    set minimum network resistance threshold (milliohms)",
    "minresist [value]    set minimum individual resistance threshold (milliohms)",
    "mindelay  [value]    set minimum network delay threshold (picoseconds)",
    "tolerance [value]    set ratio between resistor and device resistance (deprecated)",
    "simplify [on/off]    turn on/off simplification of resistor nets",
    "extout   [on/off]    turn on/off writing of .res.ext file",
    "lumped   [on/off]    turn on/off writing of updated lumped resistances",
    "silent   [on/off]    turn on/off printing of net statistics",
    "debug\t  [on/off]    turn on/off printing of detailed information",
    "skip     mask        don't extract these types",
    "force\t  names\t      force these nets to be extracted",
    "ignore\t  names\t      don't extract these nets",
    "include  names\t      extract only these nets",
    "box      type        extract the signal under the box on layer type",
    "cell\t  cellname    extract the network for the cell named cellname",
    "blackbox [on/off]    treat subcircuits with ports as black boxes",
    "fasthenry [freq]     extract subcircuit network geometry into .fh file",
    "geometry\t      extract network centerline geometry (experimental)",
    "stats\t\t      print extresist statistics",
    "help                 print this message",
};

constexpr std::size_t kHelp = 20;

std::string option_name(std::string_view entry) {
  return std::string{entry.substr(0, entry.find_first_of(" \t\n"))};
}

// RES_HELP: each entry on its own line, unindented.
CommandResult help(const cu::Session& s) {
  std::string text;
  for (const std::string_view entry : kOptions) {
    text += std::string{entry} + "\n";
  }
  cu::print_block(s, std::move(text));
  return CommandResult::success();
}

CommandResult extresist_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  const cu::Session& s = session.value();
  if (invocation.args.empty()) {
    return run(*s.editor, ResistRun{}, nullptr);
  }
  const std::string& word = invocation.args.front();
  const cmd::LookupResult found = cmd::lookup(word, kOptions);
  if (found.match == cmd::Match::ambiguous) {
    cu::print(s, "Ambiguous option: " + word);
    return CommandResult::success();
  }
  if (found.match == cmd::Match::unrecognized) {
    cu::print(s, "Unknown option: " + word);
    return CommandResult::success();
  }
  if (found.index == kHelp) {
    return help(s);
  }
  return cmd::m7_not_implemented("extresist " + option_name(kOptions.at(found.index)));
}

}  // namespace

CommandResult run(editor::EditorState& /*editor*/, const ResistRun& /*request*/, const mask::Progress* /*progress*/) {
  return cmd::m7_not_implemented("extresist run");
}

std::span<const std::string_view> extresist_options() noexcept {
  return kOptions;
}

bool register_extresist_command(cmd::CommandRegistry& registry) {
  return !registry
              .add(cmd::CommandSpec{"extresist", "extresist [args]", "patch .ext file with resistance info",
                                    extresist_command},
                   cmd::kLayoutClient)
              .has_value();
}

}  // namespace hefesto::extresist
