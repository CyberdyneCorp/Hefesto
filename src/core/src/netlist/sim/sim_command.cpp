// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `ext2sim` and `exttosim` commands (netlist/sim/sim.hpp;
// openspec/changes/m7-extraction/design.md D4, D15; capability netlist-sim
// "Command options and defaults"): Magic's option table, the first-word
// rule (Q-NETLIST-49), `help` and the option handlers. The run is in
// run.cpp.
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2sim/ext2sim.c CmdExtToSim
// (cmdExtToSimOption, sim_formats, yesno, cmdMergeTypes, the option cases
// and the usage loop), Copyright (C) 1985, 1990 Regents of the University
// of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION (the registration); SIM (the option handlers, run()).

#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>

#include "db/command_util.hpp"
#include "hefesto/extflat/names.hpp"
#include "hefesto/netlist/sim/sim.hpp"
#include "hefesto/netlist/sim/writer.hpp"
#include "netlist/sim/sim_codes.hpp"
#include "netlist/table_command.hpp"

namespace hefesto::netlist::sim {

namespace {

namespace cu = db::cmdutil;
using cmd::CommandResult;

// cmdExtToSimOption, in Magic's order (EXTTOSIM_RUN .. EXTTOSIM_HELP).
constexpr std::array<std::string_view, 10> kOptions{
    "[run] [options]\trun exttosim on current cell\n\t\t\tuse \"run -help\" to get standard options",
    "alias on|off\t\tenable/disable alias (.al) file",
    "labels on|off\t\tenable/disable labels (.nodes) file",
    "default\t\treset to default values",
    "format MIT|SU|LBL\tset output format",
    "rthresh [value]\tset resistance threshold value",
    "cthresh [value]\tset capacitance threshold value",
    "merge [option]\t\tmerge parallel transistors",
    "extresist on|off\tincorporate extresist output",
    "help\t\t\tprint help information",
};

enum Option : std::size_t {
  kAlias = 1,
  kLabels = 2,
  kDefault = 3,
  kFormat = 4,
  kRthresh = 5,
  kCthresh = 6,
  kMerge = 7,
  kExtresist = 8,
  kHelp = 9,
};

constexpr std::array<std::string_view, 3> kFormats{"MIT", "LBL", "SU"};
constexpr std::array<std::string_view, 6> kYesNo{"yes", "true", "on", "no", "false", "off"};
// cmdMergeTypes, terminated as Q-NETLIST-14 decides (Magic's table has no
// NULL entry, so its lookup reads past the array).
constexpr std::array<std::string_view, 3> kMergeTypes{
    "none\t\t\tdon't merge parallel devices",
    "conservative\t\tmerge devices with same L, W",
    "aggressive\t\tmerge devices with same L",
};

// The option handlers' context: the session, the arguments, the settings.
struct OptionCall {
  const cu::Session& s;
  const std::vector<std::string>& args;  // the option word first
  SimSettings& settings;
};

void print_usage(const OptionCall& c) {
  cu::print_block(c.s, sim_help(c.s.magic_compat));
}

// `alias`, `labels`, `extresist`: no value queries, one yes/no value sets.
CommandResult on_off(const OptionCall& c, bool& flag) {
  if (c.args.size() == 1) {
    return CommandResult::success(flag ? "on" : "off");
  }
  const int idx = c.args.size() == 2 ? cu::lookup_in(c.args[1], kYesNo) : -1;
  if (idx < 0) {
    print_usage(c);
    return CommandResult::success();
  }
  flag = idx < 3;
  return CommandResult::success();
}

CommandResult cthresh(const OptionCall& c) {
  double& value = c.settings.cthresh;
  if (c.args.size() == 1) {
    if (std::isinf(value)) {
      return CommandResult::success("infinite");
    }
    return CommandResult::success(value);
  }
  const std::string& word = c.args[1];
  if (word.starts_with("inf")) {
    value = std::numeric_limits<double>::infinity();
  } else if (cu::str_is_numeric(word)) {
    value = static_cast<double>(static_cast<float>(std::atof(word.c_str())));  // atoCap: an EFCapValue (float)
  } else {
    cu::warn(c.s, "exttosim: numeric value or \"infinite\" expected.");
  }
  return CommandResult::success();
}

CommandResult rthresh(const OptionCall& c) {
  std::int64_t& value = c.settings.rthresh;
  if (c.args.size() == 1) {
    if (value == kInfiniteResist) {
      return CommandResult::success("infinite");
    }
    return CommandResult::success(value);
  }
  const std::string& word = c.args[1];
  if (cu::str_is_int(word)) {
    value = cu::c_atoi(word);
  } else if (word.starts_with("inf")) {
    value = kInfiniteResist;
  } else {
    cu::warn(c.s, "exttosim: integer value or \"infinite\" expected.");
  }
  return CommandResult::success();
}

CommandResult format(const OptionCall& c) {
  if (c.args.size() == 1) {
    return CommandResult::success(std::string{kFormats[static_cast<std::size_t>(c.settings.format)]});
  }
  const int idx = cu::lookup_in(c.args[1], kFormats);
  if (idx < 0) {
    cu::warn(c.s, "exttosim: output formats are MIT, LBL, or SU");
  } else {
    c.settings.format = static_cast<SimFormat>(idx);
  }
  return CommandResult::success();
}

CommandResult merge(const OptionCall& c) {
  constexpr std::array<std::string_view, 3> kNames{"none", "conservative", "aggressive"};
  if (c.args.size() == 1) {
    return CommandResult::success(std::string{kNames[static_cast<std::size_t>(c.settings.merge)]});
  }
  const int idx = cu::lookup_in(c.args[1], kMergeTypes);
  if (idx < 0) {
    cu::warn(c.s, "Merge types are:");
    std::string list;
    for (const std::string_view entry : kMergeTypes) {
      list += "    " + std::string{entry} + "\n";
    }
    cu::print_block(c.s, std::move(list));
  } else {
    c.settings.merge = static_cast<SimMerge>(idx);
  }
  return CommandResult::success();
}

// EXTTOSIM_DEFAULT: the thresholds and the output flags (EFOutputFlags = 0).
// EFScale and -T are not session state in Hefesto: each run takes its own
// (design D15, as landed); a left-over -p path (Q-NETLIST-61) is cleared.
CommandResult reset(editor::EditorState& editor, SimSettings& settings) {
  settings.cthresh = 2.0;
  settings.rthresh = 10;
  editor.netlist.search_path.clear();
  editor.netlist.names = extflat::NameFlags{false, false, false, false, false};
  return CommandResult::success();
}

std::optional<CommandResult> option(cmd::CommandContext& ctx, const cmd::Invocation& invocation,
                                    std::size_t index) {
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  editor::EditorState& editor = *session.value().editor;
  const OptionCall call{session.value(), invocation.args, editor.netlist.sim};
  switch (index) {
    case kAlias:
      return on_off(call, call.settings.alias);
    case kLabels:
      return on_off(call, call.settings.labels);
    case kExtresist:
      return on_off(call, call.settings.extresist);
    case kCthresh:
      return cthresh(call);
    case kRthresh:
      return rthresh(call);
    case kFormat:
      return format(call);
    case kMerge:
      return merge(call);
    case kDefault:
      return reset(editor, call.settings);
    case kHelp:
      print_usage(call);
      return CommandResult::success();
    default:
      return std::nullopt;
  }
}

cmd::CommandResult sim_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  // `help` goes to option() (the dispatcher's help index is out of range),
  // which adds the sim2simp note with magic-compat OFF.
  const detail::TableCommand command{kOptions, kOptions.size(),
                                     [](cmd::CommandContext& c, std::vector<std::string> args) {
                                       auto session = cu::session(c);
                                       if (!session) {
                                         return cu::fail(session.error());
                                       }
                                       SimRun request;
                                       request.args = std::move(args);
                                       request.magic_compat = session.value().magic_compat;
                                       request.files = &c.services().files;
                                       request.emit = [&c](diag::Diagnostic d) { c.emit(d); };
                                       const view::LayoutWindow* w = session.value().editor->windows.current();
                                       if (w != nullptr && w->root() != nullptr) {
                                         request.window_cell = w->root()->name();
                                       }
                                       return run(*c.editor(), request);
                                     },
                                     option};
  return detail::dispatch(ctx, invocation, command);
}

}  // namespace

std::span<const std::string_view> sim_options() noexcept {
  return kOptions;
}

std::string sim_help(bool magic_compat) {
  std::string text;
  for (const std::string_view entry : kOptions) {
    text += "    " + std::string{entry} + "\n";
  }
  if (!magic_compat) {
    // Q-NETLIST-48: Magic ships sim2simp as source only; ext2sim merge does its work.
    text += "    (sim2simp is not provided: use \"ext2sim merge conservative|aggressive\" to merge parallel "
            "devices)\n";
  }
  return text;
}

// ext2sim.c Exttosim_Init: "exttosim" and its alias "ext2sim".
bool register_sim_commands(cmd::CommandRegistry& registry) {
  bool ok = !registry
                 .add(cmd::CommandSpec{"ext2sim", "ext2sim [args]",
                                       "convert extracted file(s) to a sim format file; type \"ext2sim help\" for "
                                       "information on options",
                                       sim_command},
                      cmd::kLayoutClient)
                 .has_value();
  ok = !registry
            .add(cmd::CommandSpec{"exttosim", "exttosim [args]",
                                  "convert extracted file(s) to a sim format file; type \"exttosim help\" for "
                                  "information on options",
                                  sim_command},
                 cmd::kLayoutClient)
            .has_value() &&
       ok;
  return ok;
}

}  // namespace hefesto::netlist::sim
