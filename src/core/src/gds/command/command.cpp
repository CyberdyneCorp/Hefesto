// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `gds` and `calma` commands, scaffold version
// (openspec/changes/m5-mask-gds-cif/design.md D3, D15): Magic's option
// table, the option lookup, the window check of `read` and `write` and
// `gds help`; every other option fails with CORE-NOTIMPL-005 until GDSW
// ports CmdCalma (tasks.md section 10).
//
// Ported from Magic 8.3.694 (fc5daa9e) commands/CmdCD.c CmdCalma (the
// option table, the lookup, the window check and CALMA_HELP), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/gds/command/command.hpp"

#include <array>
#include <string>

#include "db/command_util.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/cmd/stubs.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::gds {

namespace {

namespace cu = db::cmdutil;
using cmd::CommandResult;

constexpr std::string_view kNoWindow = "VIEW-WINDOW-003";

// cmdCalmaOption, in Magic's order (Lookup() reads each entry up to its
// first blank; the help prints the whole entry).
constexpr std::array<std::string_view, 29> kOptions{
    "help\t\tprint this help information",
    "abstract [allow|disallow]\n\t\t[dis]allow writing of GDS with calls to abstract cells",
    "addendum [yes|no]\toutput only cells that are not type \"readonly\"",
    "arrays [yes|no]\toutput arrays as individual subuses (like in CIF)",
    "compress [value]\tcompress output with zlib compression 0 to 6",
    "contacts [yes|no]\t(unimplemented)",
    "datestamp [yes|value]\tuse current time or value as the creation date stamp",
    "drccheck [yes|no]\tmark all cells as needing DRC checking",
    "flatten [yes|no|limit]\tflatten simple cells (e.g., contacts) on input",
    "flatglob [<name>|none]\tflatten cells by name with glob patterning",
    "labels [yes|no]\tcause labels to be output when writing GDS-II",
    "library [yes|no]\tdo not output the top level, only subcells",
    "lower [yes|no]\t\tallow both upper and lower case in labels",
    "magscale [value]\tscale to interpret text magnification 1 in microns",
    "maskhints [yes|no]\tgenerate mask hint properties on input",
    "merge [yes|no]\t\tmerge tiles into polygons in the output",
    "nodatestamp [yes|no]\twrite a zero value creation date stamp",
    "noduplicates [yes|no]\tdo not read cells that exist before reading GDS",
    "ordering [on|off]\tcause cells to be read in post-order",
    "read file\t\tread Calma GDS-II format from \"file\"\n\t\tinto edit cell",
    "readonly [yes|no]\tset cell as read-only and generate output from GDS file",
    "rescale [yes|no]\tallow or disallow internal grid subdivision",
    "savepaths [yes|no]\tsave path centerlines as cell properties",
    "warning [option]\tset warning information level",
    "write file\t\toutput Calma GDS-II format to \"file\"\n\t\tfor the window's root cell",
    "polygon subcells [yes|no]\n\t\tput non-Manhattan polygons into subcells",
    "path subcells [yes|no]\n\t\tput wire paths into individual subcells",
    "undefined [allow|disallow]\n\t\t[dis]allow writing of GDS with calls to undefined cells",
    "unique [yes|no]\trename any cells with names duplicated in the GDS",
};

enum Option : std::size_t { kHelp = 0, kRead = 19, kWrite = 24 };

std::string option_name(std::size_t option) {
  const std::string_view entry = kOptions.at(option);
  return std::string{entry.substr(0, entry.find_first_of(" \t\n"))};
}

// CALMA_HELP.
CommandResult help(const cu::Session& s) {
  std::string text = "GDS commands have the form \":gds option\", where option is one of:\n";
  for (const std::string_view entry : kOptions) {
    text += "    " + std::string{entry} + "\n";
  }
  text += "If no option is given, a CALMA GDS-II stream file is\n";
  text += "    produced for the root cell.\n";
  text += "The current CIF output style (\"cif ostyle\") is used\n";
  text += "    to select the mask layers output by :gds write.\n";
  text += "The current CIF input style (\"cif istyle\") is used\n";
  text += "    to select the mask layers read by :gds read.\n";
  cu::print_block(s, std::move(text));
  return CommandResult::success();
}

// The option of a call: a bare `gds` writes; an unknown option prints the
// error and the help (Magic's Lookup() < 0).
std::size_t find_option(const cu::Session& s, const cmd::Invocation& invocation) {
  if (invocation.args.empty()) {
    return kWrite;
  }
  const cmd::LookupResult found = cmd::lookup(invocation.args.front(), kOptions);
  if (found.match != cmd::Match::found) {
    cu::warn(s, "\"" + invocation.args.front() + "\" isn't a valid gds option.");
    return kHelp;
  }
  return found.index;
}

CommandResult gds_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  const cu::Session& s = session.value();
  const std::size_t option = find_option(s, invocation);
  // Only `read` and `write` check for a window (windCheckOnlyWindow).
  const bool needs_window = option == kRead || option == kWrite;
  if (needs_window && s.editor->windows.size() == 0 && !s.editor->command_point) {
    return CommandResult::failure(diag::make(kNoWindow, "Point to a window first"));
  }
  if (option == kHelp) {
    return help(s);
  }
  return cmd::not_implemented("gds " + option_name(option));
}

}  // namespace

std::span<const std::string_view> gds_options() noexcept {
  return kOptions;
}

// The window command table entries (dbwind/DBWCommands.c:514-520).
bool register_gds_command(cmd::CommandRegistry& registry) {
  const bool calma =
      !registry
           .add(cmd::CommandSpec{"calma", "calma option",
                                 "Calma GDS-II stream file processor; type \"calma help\" for information on options",
                                 gds_command},
                cmd::kLayoutClient)
           .has_value();
  const bool gds = !registry
                        .add(cmd::CommandSpec{"gds", "gds option", "alias for the \"calma\" command", gds_command},
                             cmd::kLayoutClient)
                        .has_value();
  return calma && gds;
}

}  // namespace hefesto::gds
