// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `extract` and `ext` commands (extract/command/commands.hpp;
// openspec/changes/m7-extraction/design.md D4, D12): Magic's option tables,
// the leading `list`/`listall` word, the option lookup with Magic's
// invalid-option message, the window check of every option but `style` and
// `help`, the help listing and the wrong-argument message; the options
// themselves are EXTCMD's parsers (options.cpp, M7 R4).
//
// Ported from Magic 8.3.694 (fc5daa9e) commands/CmdE.c CmdExtract (the
// option tables, the lookup, the window check and the help), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION (the dispatcher); EXTCMD (the option parsers).

#include "hefesto/extract/command/commands.hpp"

#include <array>
#include <string>
#include <vector>

#include "db/command_util.hpp"
#include "extract/command/options.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::extract {

namespace {

namespace cu = db::cmdutil;
using cmd::CommandResult;

constexpr std::string_view kNoWindow = "VIEW-WINDOW-003";

// cmdExtCmd, in Magic's order.
constexpr std::array<std::string_view, 14> kOptions{
    "all\t\t\textract root cell and all its children",
    "cell name\t\textract selected cell into file \"name\"",
    "do [option]\t\tenable extractor option",
    "halo [value]\t\tprint or set the sidewall halo distance",
    "help\t\t\tprint this help information",
    "length [option]\tcontrol pathlength extraction information",
    "no [option]\t\tdisable extractor option",
    "parents\t\textract selected cell and all its parents",
    "path [path]\t\tif set, extract into the indicated path",
    "showparents\t\tshow all parents of selected cell",
    "stepsize [value]\tprint or set the extraction step size",
    "style [stylename]\tset current extraction parameter style",
    "unique [option]\tgenerate unique names when different nodes\n\t\t\thave the same name",
    "warn [ [no] option]\tenable/disable reporting of non-serious errors",
};

// cmdExtOption.
constexpr std::array<std::string_view, 14> kDoOptions{
    "adjust\t\t\tcompensate R and C hierarchically",
    "all\t\t\tall options",
    "capacitance\t\textract substrate capacitance",
    "corners [value]\textract corner fringe capacitance,\n\t\t\toptionally setting the model scale factor",
    "coupling\t\textract coupling capacitance",
    "extresist\t\textract resistance",
    "length\t\t\tcompute driver-receiver pathlengths",
    "local\t\t\tput all generated files in the current directory",
    "lumped\t\t\testimate lumped resistance",
    "labelcheck\t\tcheck for connections through sticky labels",
    "aliases\t\toutput all net name aliases",
    "unique\t [notopports]\tensure unique node names during extraction",
    "resistance\t\textract resistance (same as \"do extresist\")",
    "fringehalo\t\tdistribute fringe capacitance over a halo",
};

// cmdExtWarn.
constexpr std::array<std::string_view, 4> kWarnOptions{
    "all\t\t\tenable all warnings",
    "dup\t\t\twarn when different nodes have the same name",
    "fets\t\t\twarn about badly constructed fets",
    "labels\t\twarn when subcell nodes are unlabelled",
};

// cmdExtLength.
constexpr std::array<std::string_view, 3> kLengthOptions{
    "clear\t\t\tclear the driver and receiver tables",
    "driver termName(s)\tidentify a driving (output) terminal",
    "receiver termName(s)\tidentify a receiving (input) terminal",
};

// cmdExtUniq (EXT_UNIQ_* order).
constexpr std::array<std::string_view, 4> kUniqueOptions{
    "all\t\t\textract matching labels as unique nodes",
    "#\t\t\textract tagged labels as unique nodes",
    "noports\t\tignore ports when making labels unique",
    "notopports\t\tignore top-level ports when making labels unique",
};

// EXTALL .. EXTWARN (CmdE.c:1019-1032).
enum Option : std::size_t { kHelp = 4, kStyle = 11 };

CommandResult wrong_args(const cu::Session& s) {
  return detail::wrong_args(s);
}

// EXTHELP.
CommandResult help(const cu::Session& s, std::size_t argc) {
  if (argc != 1) {
    return wrong_args(s);
  }
  std::string text = "Extract commands have the form \"extract option\",\nwhere option is one of:\n";
  for (const std::string_view entry : kOptions) {
    text += "  " + std::string{entry} + "\n";
  }
  text += "If no option is given, the root cell and all its\nchildren are extracted incrementally.\n";
  cu::print_block(s, std::move(text));
  return CommandResult::success();
}

// The scaffold's window check (windCheckOnlyWindow, else ToolGetBox), as
// the `lef` command does; EXTCMD refines it with the box-tool cell.
bool has_window(const cu::Session& s) {
  return s.editor->windows.size() != 0 || s.editor->command_point.has_value();
}

CommandResult extract_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  const cu::Session& s = session.value();
  std::vector<std::string> args = invocation.args;
  bool dolist = false;
  bool doforall = false;
  if (!args.empty() && args.front().starts_with("list")) {
    dolist = true;
    doforall = args.front().starts_with("listall");
    // `list`/`listall` select list-style results for `path` and `style`
    // and are removed before matching. Magic then looks up argv[1], a
    // NULL pointer when nothing follows, and crashes; Hefesto reports the
    // wrong-argument message instead, in both modes.
    args.erase(args.begin());
    if (args.empty()) {
      return wrong_args(s);
    }
  }
  std::optional<std::size_t> option;
  if (!args.empty()) {
    const cmd::LookupResult found = cmd::lookup(args.front(), kOptions);
    if (found.match != cmd::Match::found) {
      cu::warn(s, "\"" + args.front() + "\" isn't a valid extract option.  Type \"extract help\" for a list of "
                  "valid options.");
      return CommandResult::success();
    }
    option = found.index;
  }
  if (option == kHelp) {
    return help(s, args.size());
  }
  if (option != kStyle && !has_window(s)) {
    return CommandResult::failure(diag::make(kNoWindow, "Point to a window first"));
  }
  detail::OptionArgs parsed{std::move(args), dolist, doforall};
  return detail::run_option(ctx, s, option.value_or(detail::kIncremental), parsed);
}

}  // namespace

std::span<const std::string_view> extract_options() noexcept {
  return kOptions;
}

std::span<const std::string_view> extract_do_options() noexcept {
  return kDoOptions;
}

std::span<const std::string_view> extract_warn_options() noexcept {
  return kWarnOptions;
}

std::span<const std::string_view> extract_length_options() noexcept {
  return kLengthOptions;
}

std::span<const std::string_view> extract_unique_options() noexcept {
  return kUniqueOptions;
}

// The layout command table entries (dbwind/DBWcommands.c).
bool register_extract_commands(cmd::CommandRegistry& registry) {
  bool ok = !registry
                 .add(cmd::CommandSpec{"extract", "extract option",
                                       "circuit extractor; type \"extract help\" for information on options",
                                       extract_command},
                      cmd::kLayoutClient)
                 .has_value();
  ok = !registry
            .add(cmd::CommandSpec{"ext", "ext option", "circuit extractor (same as \"extract\")", extract_command},
                 cmd::kLayoutClient)
            .has_value() &&
       ok;
  return ok;
}

}  // namespace hefesto::extract
