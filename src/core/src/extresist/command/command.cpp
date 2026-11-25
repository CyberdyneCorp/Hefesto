// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `extresist` command (extresist/command/command.hpp;
// openspec/changes/m7-extraction/design.md D4, D16): Magic's option table,
// the lookup with `Ambiguous option: <w>` and `Unknown option: <w>`,
// `help`, the setting options (extresist/command/options.cpp) and the
// options that run (extresist/command/run.cpp). `box` prints a network,
// which needs RESNET's network: it answers CORE-NOTIMPL-008 after its
// window and type checks.
//
// Ported from Magic 8.3.694 (fc5daa9e) resis/ResRex.c CmdExtResis
// (cmdExtresisCmd, the lookup, RES_HELP, RES_AMBIG and RES_BAD), Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION (the registration); RESCMD (the dispatcher, the
// option parsers and run()).

#include "hefesto/extresist/command/command.hpp"

#include <array>
#include <span>
#include <string>
#include <utility>

#include "db/command_util.hpp"
#include "extresist/command/options.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/editor/editor_state.hpp"

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

// The words after the option.
std::span<const std::string> option_args(const cmd::Invocation& invocation) {
  return std::span<const std::string>{invocation.args}.subspan(1);
}

ResistRun run_request(const cu::Session& s, cmd::CommandContext& ctx) {
  ResistRun request;
  request.magic_compat = s.magic_compat;
  request.files = &ctx.services().files;
  request.emit = [&ctx](diag::Diagnostic d) { ctx.emit(d); };
  return request;
}

// `box <type>`: the window and type checks; the network print is RESNET's.
CommandResult box_option(const cu::Session& s, std::span<const std::string> args, const detail::Console& console) {
  if (!s.editor->box || s.editor->box_root == nullptr) {
    console.message("Sorry, the box must appear in one of the windows.");
    return CommandResult::success();
  }
  if (args.size() != 1 || s.tech == nullptr) {
    return CommandResult::success();
  }
  const auto type = s.tech->find_type(args.front());
  if (!type.ok() || *type.id == tech::kSpace) {
    console.message("Unrecognized layer (type) name \"" + args.front() + "\"");
    return CommandResult::success();
  }
  return cmd::m7_not_implemented("extresist box");
}

// `cell`: the selected cell (CmdGetSelectedCell).
CommandResult cell_option(const cu::Session& s, ResistRun request, const detail::Console& console) {
  const db::CellUse* use = s.editor->selection.cycle.last_selected_use;
  if (use == nullptr || use->def == nullptr) {
    console.message("No cell selected");
    return CommandResult::success();
  }
  s.editor->extresist.options.all = false;
  request.cell = use->def->name();
  return run(*s.editor, request, nullptr);
}

// The options that run (all, cell, fasthenry, geometry) and box.
CommandResult run_option(const cu::Session& s, cmd::CommandContext& ctx, std::size_t option,
                         std::span<const std::string> args, detail::OptionContext& octx) {
  ResistRun request = run_request(s, ctx);
  switch (option) {
    case detail::kAll:
      s.editor->extresist.options.all = true;
      request.all = true;
      return run(*s.editor, request, nullptr);
    case detail::kGeometry:
      request.geometry = true;
      return run(*s.editor, request, nullptr);
    case detail::kFasthenry:
      detail::fasthenry_frequency(args, octx);
      request.fasthenry = true;
      return run(*s.editor, request, nullptr);
    case detail::kCell:
      return cell_option(s, request, octx.console);
    case detail::kBox:
      return box_option(s, args, octx.console);
    default:
      return cmd::m7_not_implemented("extresist " + option_name(kOptions.at(option)));
  }
}

CommandResult extresist_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  const cu::Session& s = session.value();
  if (invocation.args.empty()) {
    s.editor->extresist.options.all = false;  // RES_RUN
    return run(*s.editor, run_request(s, ctx), nullptr);
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
  if (found.index == detail::kHelp) {
    return help(s);
  }
  detail::OptionContext octx{s.editor->extresist.options, s.tech, s.magic_compat,
                             detail::Console{[&ctx](diag::Diagnostic d) { ctx.emit(d); }}};
  if (std::optional<CommandResult> set = detail::setting_option(found.index, option_args(invocation), octx)) {
    return std::move(*set);
  }
  return run_option(s, ctx, found.index, option_args(invocation), octx);
}

}  // namespace

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
