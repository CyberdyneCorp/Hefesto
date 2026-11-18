// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `lef` and `def` commands, scaffold version
// (openspec/changes/m6-lef-def-oasis/design.md D3, D14): Magic's two option
// tables, the lookup of both commands in the `lef` table, the window check
// of every option but `help`, the help listing, `def writeall`'s refusal and
// the "only for LEF" answers of `def datestamp` and `def nocheck`. Every
// other option fails with CORE-NOTIMPL-006 until its workstream lands it:
// `lef read`, `lef datestamp` (LEFR), `lef write`, `lef writeall`, `lef
// nocheck` (LEFW), `def read` (DEFR), `def write` (DEFW). Each workstream
// adds its argument parser in its own area and calls it from dispatch().
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefCmd.c CmdLef (the option
// tables, the lookup, the window check, the `def` refusals and the help),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION (the dispatcher; design D2, D14).

#include "hefesto/lef/command/command.hpp"

#include <array>
#include <string>

#include "db/command_util.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/cmd/stubs.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::lef {

namespace {

namespace cu = db::cmdutil;
using cmd::CommandResult;

constexpr std::string_view kNoWindow = "VIEW-WINDOW-003";

// cmdLefOption, in Magic's order.
constexpr std::array<std::string_view, 6> kLefOptions{
    "read [filename]\t\tread a LEF file filename[.lef]\n"
    "    read [filename] -import\tread a LEF file; import cells from .mag files\n"
    "    read [filename] -annotate\tread a LEF file for cell annotation only.",
    "write [filename] [-tech]\twrite LEF for current cell\n"
    "    write [filename] -hide\thide all details other than ports\n"
    "    write [filename] -hide <d>\thide details in area set back distance <d>",
    "writeall\t\t\twrite all cells including the top-level cell\n"
    "    writeall -notop\t\twrite all children of the top-level cell\n"
    "    writeall -all\t\trecurse on all subcells of the top-level cell\n"
    "    writeall -hide\t\thide all details other than ports\n"
    "    writeall -hide [dist]\thide details in area set back distance dist",
    "nocheck [netname ...]\t\tignore antenna area checks on named net(s)",
    "datestamp [value]\t\tforce the timestamp of cells read from LEF",
    "help                   \tprint this help information",
};

// cmdDefOption, in Magic's order (help only).
constexpr std::array<std::string_view, 5> kDefOptions{
    "read [filename]\t\tread a DEF file filename[.def]\n"
    "    read [filename] -labels\tread a DEF file with net labeling\n"
    "    read [filename] -annotate\tread a DEF file for net annotation only\n",
    "    read [filename] -noblockage\tread a DEF file (mask layers only).",
    "write [cell] [-allspecial]\twrite DEF for current or indicated cell",
    "writeall\t\t\t(use \"flatten -nosubckt\" + \"def"
    " write\" instead)",
    "help                   \tprint this help information",
};

// LEF_READ .. LEF_HELP (lefCmd.c:70-75).
enum Option : std::size_t { kRead = 0, kWrite = 1, kWriteAll = 2, kNoCheck = 3, kDateStamp = 4, kHelp = 5 };

std::string option_name(std::size_t option) {
  const std::string_view entry = kLefOptions.at(option);
  return std::string{entry.substr(0, entry.find_first_of(" \t\n"))};
}

// LEF_HELP (also reached by an invalid option): "The "<cmd>" options are:"
// and each entry of the command's table.
CommandResult help(const cu::Session& s, const std::string& command, bool is_lef) {
  std::string text = "The \"" + command + "\" options are:\n";
  const std::span<const std::string_view> table = is_lef ? lef_options() : def_options();
  for (const std::string_view entry : table) {
    text += "    " + command + " " + std::string{entry} + "\n";
  }
  cu::print_block(s, std::move(text));
  return CommandResult::success();
}

// The option of a call: none is `help`; an unknown one prints Magic's error
// and the help (Lookup() < 0).
std::size_t find_option(const cu::Session& s, const cmd::Invocation& invocation) {
  if (invocation.args.empty()) {
    return kHelp;
  }
  const cmd::LookupResult found = cmd::lookup(invocation.args.front(), kLefOptions);
  if (found.match != cmd::Match::found) {
    cu::warn(s, "\"" + invocation.args.front() + "\" isn't a valid " + invocation.name + " option.");
    return kHelp;
  }
  return found.index;
}

// Every option but `help` needs a cell: the selection, the only window's
// root or the box's cell (CmdGetSelectedCell, windCheckOnlyWindow,
// ToolGetBox). The scaffold checks for a window or a command point, as the
// `gds` command does; LEFR, LEFW, DEFR and DEFW refine it.
bool has_window(const cu::Session& s) {
  return s.editor->windows.size() != 0 || s.editor->command_point.has_value();
}

// The `def` side of the options that are `lef`-only or refused.
CommandResult def_only_answer(const cu::Session& s, std::size_t option) {
  switch (option) {
    case kWriteAll:
      cu::warn(s, "Sorry, can't write hierarchical DEF at this time.");
      cu::warn(s, "Try \"def write\"");
      break;
    case kDateStamp:
      cu::print(s, "The \"datestamp\" option is only for LEF reads.");
      break;
    default:
      cu::print(s, "The \"nocheck\" option is only for LEF writes.");
      break;
  }
  return CommandResult::success();
}

bool is_def_only_answer(std::size_t option) {
  return option == kWriteAll || option == kDateStamp || option == kNoCheck;
}

CommandResult lef_def_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  const cu::Session& s = session.value();
  // Magic tells the commands apart by the first letter of argv[0].
  const bool is_lef = !invocation.name.starts_with('d');
  const std::size_t option = find_option(s, invocation);
  if (option == kHelp) {
    return help(s, invocation.name, is_lef);
  }
  if (!has_window(s)) {
    return CommandResult::failure(diag::make(kNoWindow, "Point to a window first"));
  }
  if (!is_lef && is_def_only_answer(option)) {
    return def_only_answer(s, option);
  }
  return cmd::not_implemented((is_lef ? "lef " : "def ") + option_name(option));
}

}  // namespace

std::span<const std::string_view> lef_options() noexcept {
  return kLefOptions;
}

std::span<const std::string_view> def_options() noexcept {
  return kDefOptions;
}

// The window command table entries (dbwind/DBWCommands.c:648-657).
bool register_lef_def_commands(cmd::CommandRegistry& registry) {
  const bool lef =
      !registry
           .add(cmd::CommandSpec{"lef", "lef [options]",
                                 "LEF-format input/output; type \"lef help\" for information on options",
                                 lef_def_command},
                cmd::kLayoutClient)
           .has_value();
  const bool def = !registry
                        .add(cmd::CommandSpec{"def", "def [options]",
                                              "DEF-format input; type \"def help\" for information on options",
                                              lef_def_command},
                             cmd::kLayoutClient)
                        .has_value();
  return lef && def;
}

}  // namespace hefesto::lef
