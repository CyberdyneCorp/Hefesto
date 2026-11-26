// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `oasis` command (capability oasis-command "Command name and forms",
// "Option syntax"; openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D3, D15):
// the option table, the unique-prefix lookup (an unknown or ambiguous
// option is an error and nothing changes), the window check of `read`,
// `write` and a bare `oasis` (as for `gds`), `oasis help`, and the
// dispatch to the read and write forms (read_command.cpp,
// write_command.cpp), the read settings shared with `gds` and the `oasis`
// settings (options.cpp). The help text is informative.
//
// Owner: OASCMD (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include "hefesto/oasis/command/command.hpp"

#include <array>
#include <string>
#include <utility>

#include "hefesto/cmd/keywords.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "options.hpp"

namespace hefesto::oasis {

namespace {

using cmd::CommandResult;
using namespace command_detail;  // NOLINT(google-build-using-namespace): the command's internal helpers

// `help`, the `gds` options in cmdCalmaOption's order, then the OASIS-only
// options (oasis-command "Options shared with the gds command", "OASIS-only
// options").
constexpr std::array<std::string_view, 34> kOptions{
    "help\t\tprint this help information",
    "abstract [allow|disallow]\n\t\t[dis]allow writing of OASIS with calls to abstract cells",
    "addendum [yes|no]\toutput only cells that are not type \"readonly\"",
    "arrays [yes|no]\toutput arrays as individual subuses",
    "compress [value]\tCBLOCK compression level 0 to 9 (0: no CBLOCKs)",
    "contacts [yes|no]\t(unimplemented)",
    "datestamp [yes|no|value]\tuse the default or value as the timestamp of cells read",
    "drccheck [yes|no]\tmark all cells as needing DRC checking (shared with gds)",
    "flatten [yes|no|limit]\tflatten simple cells (e.g., contacts) on input (shared with gds)",
    "flatglob [<name>|none]\tflatten cells by name with glob patterning (shared with gds)",
    "labels [yes|no]\tcause labels to be output when writing OASIS",
    "library [yes|no]\tdo not output the top level, only subcells",
    "lower [yes|no]\t\tallow both upper and lower case in labels",
    "magscale [value]\taccepted; no effect on OASIS",
    "maskhints [yes|no]\tgenerate mask hint properties on input (shared with gds)",
    "merge [yes|no]\t\tmerge tiles into polygons in the output",
    "nodatestamp [yes|no]\tgive the cells read a zero timestamp",
    "noduplicates [yes|no]\tdo not read cells that exist before reading (shared with gds)",
    "ordering [on|off]\tcause cells to be read in post-order (shared with gds)",
    "read file\t\tread OASIS from \"file\"\n\t\tinto edit cell",
    "readonly [yes|no]\tset cells read as read-only vendor views (shared with gds)",
    "rescale [yes|no]\tallow or disallow internal grid subdivision (shared with gds)",
    "savepaths [yes|no]\tsave path centerlines as cell properties (shared with gds)",
    "warning [option]\tset warning information level (shared with gds)",
    "write file\t\toutput OASIS to \"file\"\n\t\tfor the window's root cell",
    "polygon subcells [yes|no]\n\t\tput non-Manhattan polygons into subcells (shared with gds)",
    "path subcells [yes|no]\n\t\tput wire paths into individual subcells (shared with gds)",
    "undefined [allow|disallow]\n\t\t[dis]allow writing of OASIS with calls to undefined cells",
    "unique [yes|no]\trename any cells with names duplicated in the file (shared with gds)",
    "strict [yes|no]\twrite a strict-mode file with name tables",
    "validation [none|crc32|checksum]\n\t\tEND record validation scheme on write",
    "repetitions [yes|no]\tdetect shape repetitions on write",
    "layernames [yes|no]\twrite LAYERNAME records",
    "circlepoints [value]\tvertices of a CIRCLE on read, 8 to 4096",
};

static_assert(kOptions[kCirclePoints].starts_with("circlepoints"), "the option indices follow the table");
static_assert(kOptions[kWrite].starts_with("write"), "the option indices follow the table");

CommandResult help(const cu::Session& s) {
  std::string text = "OASIS commands have the form \":oasis option\", where option is one of:\n";
  for (const std::string_view entry : kOptions) {
    text += "    " + std::string{entry} + "\n";
  }
  text += "If no option is given, an OASIS file is\n";
  text += "    produced for the root cell.\n";
  cu::print_block(s, std::move(text));
  return CommandResult::success();
}

CommandResult oasis_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  const cu::Session& s = session.value();
  std::size_t option = kWrite;  // a bare `oasis` writes
  if (!invocation.args.empty()) {
    const cmd::LookupResult found = cmd::lookup(invocation.args.front(), kOptions);
    if (found.match != cmd::Match::found) {
      // oasis-command "Option syntax" 1: an error, and nothing changes.
      return CommandResult::failure(diag::make(
          diag::codes::kCmdUsage, "\"" + invocation.args.front() + "\" isn't a valid oasis option."));
    }
    option = found.index;
  }
  // Only `read` and `write` check for a window, as for `gds`.
  const bool needs_window = option == kRead || option == kWrite;
  if (needs_window && s.editor->windows.size() == 0 && !s.editor->command_point) {
    return CommandResult::failure(diag::make(kNoWindow, "Point to a window first"));
  }
  switch (option) {
    case kHelp:
      return help(s);
    case kRead:
      return read_option(ctx, s, invocation);
    case kWrite:
      return write_option(ctx, s, invocation);
    default:
      break;
  }
  if (auto shared = shared_option(s, option, invocation)) {
    return std::move(*shared);
  }
  return settings_option(ctx, option, invocation, s.editor->mask.oasis).value_or(CommandResult::success());
}

}  // namespace

std::span<const std::string_view> oasis_options() noexcept {
  return kOptions;
}

bool register_oasis_command(cmd::CommandRegistry& registry) {
  return !registry
              .add(cmd::CommandSpec{"oasis", "oasis [option]",
                                    "OASIS file processor; type \"oasis help\" for information on options",
                                    oasis_command},
                   cmd::kLayoutClient)
              .has_value();
}

}  // namespace hefesto::oasis
