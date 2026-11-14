// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `gds` and `calma` commands (capabilities gds-read, gds-write;
// openspec/changes/m5-mask-gds-cif/design.md D3, D15): Magic's option
// table and lookup, the window check of `read` and `write`, `gds help`,
// `write` (and a bare `gds`) with the output file naming of Q-MASK-210,
// and the write options (gds-write "Write option settings and defaults").
// The read options, `warning`, `rescale` and `read` itself still fail
// with CORE-NOTIMPL-005 (task 10.8).
//
// Ported from Magic 8.3.694 (fc5daa9e) commands/CmdCD.c CmdCalma (the
// option table, the lookup, the window check, CALMA_HELP, the write
// options and outputCalma), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md). The option
// queries answer as Magic's Tcl build does (Tcl_SetObjResult).
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/gds/command/command.hpp"

#include <array>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <string>

#include "db/command_util.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/cmd/stubs.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/write/subtree.hpp"
#include "hefesto/gds/write/write.hpp"

namespace hefesto::gds {

namespace {

namespace cu = db::cmdutil;
using cmd::CommandResult;

constexpr std::string_view kNoWindow = "VIEW-WINDOW-003";
constexpr std::string_view kWrongArgs = "GDS-CMD-001";
constexpr std::string_view kBadValue = "GDS-CMD-002";
constexpr std::string_view kUnimplemented = "GDS-CMD-003";

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

enum Option : std::size_t {
  kHelp = 0,
  kAbstract = 1,
  kAddendum = 2,
  kArrays = 3,
  kCompress = 4,
  kContacts = 5,
  kDatestamp = 6,
  kLabels = 10,
  kLibrary = 11,
  kLower = 12,
  kMerge = 15,
  kNoStamp = 16,
  kRead = 19,
  kWrite = 24,
  kUndefined = 27,
};

constexpr std::array<std::string_view, 8> kYesNo{"no", "false", "off", "0", "yes", "true", "on", "1"};
constexpr std::array<std::string_view, 4> kAllowDisallow{"disallow", "0", "allow", "1"};

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

// wrongNumArgs.
CommandResult wrong_args(const cmd::Invocation& invocation, std::string prefix = {}) {
  return CommandResult::failure(diag::make(kWrongArgs, prefix + "Wrong number of arguments in \"" + invocation.name +
                                                           "\" command.  Try \":" + invocation.name +
                                                           " help\" for help."));
}

CommandResult boolean(bool value) {
  return CommandResult::success(cmd::Value{std::int64_t{value ? 1 : 0}});
}

// Lookup() of the option's argument in `table`; nullopt when it fails.
template <std::size_t N>
std::optional<std::size_t> argument_index(const cmd::Invocation& invocation,
                                          const std::array<std::string_view, N>& table) {
  const cmd::LookupResult found = cmd::lookup(invocation.args.at(1), table);
  if (found.match != cmd::Match::found) {
    return std::nullopt;
  }
  return found.index;
}

// A yes/no option: no argument reports the value; yes (index >= 4) sets it.
CommandResult yes_no_option(const cmd::Invocation& invocation, bool& field) {
  if (invocation.args.size() == 1) {
    return boolean(field);
  }
  const auto index = invocation.args.size() == 2 ? argument_index(invocation, kYesNo) : std::nullopt;
  if (!index) {
    return wrong_args(invocation);
  }
  field = *index >= 4;
  return CommandResult::success();
}

// An allow/disallow option: allow (index >= 2) sets it.
CommandResult allow_option(const cmd::Invocation& invocation, bool& field) {
  if (invocation.args.size() == 1) {
    return boolean(field);
  }
  const auto index = invocation.args.size() == 2 ? argument_index(invocation, kAllowDisallow) : std::nullopt;
  if (!index) {
    return wrong_args(invocation);
  }
  field = *index >= 2;
  return CommandResult::success();
}

// C atoi(): optional blanks and sign, then digits; 0 without digits; the
// strtol value clamped to long and narrowed to int as glibc does.
std::int32_t c_atoi(const std::string& text) {
  const long long value = std::strtoll(text.c_str(), nullptr, 10);
  const long long clamped = value < LONG_MIN ? LONG_MIN : (value > LONG_MAX ? LONG_MAX : value);
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<unsigned long long>(clamped)));
}

// StrIsInt: an optional sign, then only digits.
bool str_is_int(std::string_view text) {
  if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
    text.remove_prefix(1);
  }
  for (const char c : text) {
    if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
      return false;
    }
  }
  return true;
}

CommandResult compress_option(const cmd::Invocation& invocation, GdsSettings& settings) {
  if (invocation.args.size() == 1) {
    return CommandResult::success(cmd::Value{std::int64_t{settings.compress}});
  }
  if (invocation.args.size() != 2) {
    return wrong_args(invocation);
  }
  const std::int32_t value = c_atoi(invocation.args[1]);
  if (value < 0 || value > 9) {
    return CommandResult::failure(diag::make(kBadValue, "Bad compression value " + std::to_string(value) +
                                                            ".  Value must be in the range 0 to 9."));
  }
  settings.compress = value;
  return CommandResult::success();
}

CommandResult contacts_option(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  if (invocation.args.size() == 1) {
    return boolean(false);
  }
  const auto index = invocation.args.size() == 2 ? argument_index(invocation, kYesNo) : std::nullopt;
  if (!index) {
    return wrong_args(invocation);
  }
  if (*index >= 4) {
    ctx.emit(diag::make(kUnimplemented, "Unimplemented function:  Contacts will be output as individual cuts."));
  }
  return CommandResult::success();
}

CommandResult datestamp_option(const cmd::Invocation& invocation, GdsSettings& settings) {
  if (invocation.args.size() == 1) {
    return settings.datestamp ? CommandResult::success(cmd::Value{*settings.datestamp})
                              : CommandResult::success(cmd::Value{"default"});
  }
  if (invocation.args.size() != 2) {
    return wrong_args(invocation);
  }
  if (const auto index = argument_index(invocation, kYesNo)) {
    settings.datestamp = *index >= 4 ? std::nullopt : std::optional<std::int64_t>{0};
    return CommandResult::success();
  }
  if (!str_is_int(invocation.args[1])) {
    return wrong_args(invocation, "Unrecognizable date stamp \"" + invocation.args[1] + "\".\n");
  }
  settings.datestamp = c_atoi(invocation.args[1]);
  return CommandResult::success();
}

// CALMA_NO_STAMP, kept for backward compatibility.
CommandResult nodatestamp_option(const cmd::Invocation& invocation, GdsSettings& settings) {
  if (invocation.args.size() == 1) {
    if (!settings.datestamp) {
      return boolean(false);
    }
    return *settings.datestamp == 0 ? boolean(true) : CommandResult::success(cmd::Value{"fixed"});
  }
  const auto index = invocation.args.size() == 2 ? argument_index(invocation, kYesNo) : std::nullopt;
  if (!index) {
    return wrong_args(invocation);
  }
  settings.datestamp = *index >= 4 ? std::optional<std::int64_t>{0} : std::nullopt;
  return CommandResult::success();
}

// The window's root cell, as windCheckOnlyWindow finds it.
db::CellDef* window_root(const cu::Session& s) {
  const view::LayoutWindow* w = s.editor->windows.current();
  return w != nullptr ? w->root() : nullptr;
}

// outputCalma.
CommandResult write_gds(cmd::CommandContext& ctx, const cu::Session& s, db::CellDef& root, const std::string& name) {
  editor::EditorState& editor = *s.editor;
  GdsWriteOptions options;
  options.settings = editor.mask.gds;
  options.typed_name = name;
  options.loader = command_subtree_loader(ctx, editor);
  options.files = &ctx.services().files;
  const std::string file = output_file_name(name, options.settings.compress, editor.drc.compat_settings());
  return write(editor, root, file, options);
}

CommandResult write_option(cmd::CommandContext& ctx, const cu::Session& s, const cmd::Invocation& invocation) {
  db::CellDef* root = window_root(s);
  if (root == nullptr) {
    return CommandResult::failure(diag::make(kNoWindow, "Point to a window first"));
  }
  if (invocation.args.empty()) {
    const std::string& cell = root->name();
    const std::size_t slash = cell.rfind('/');
    return write_gds(ctx, s, *root, slash == std::string::npos ? cell : cell.substr(slash + 1));
  }
  if (invocation.args.size() != 2) {
    return wrong_args(invocation);
  }
  return write_gds(ctx, s, *root, invocation.args[1]);
}

CommandResult settings_option(std::size_t option, const cmd::Invocation& invocation, GdsSettings& g) {
  switch (option) {
    case kAbstract:
      return allow_option(invocation, g.allow_abstract);
    case kUndefined:
      return allow_option(invocation, g.allow_undefined);
    case kAddendum:
      return yes_no_option(invocation, g.addendum);
    case kArrays:
      return yes_no_option(invocation, g.arrays);
    case kLabels:
      return yes_no_option(invocation, g.labels);
    case kLibrary:
      return yes_no_option(invocation, g.library);
    case kLower:
      return yes_no_option(invocation, g.lower);
    case kMerge:
      return yes_no_option(invocation, g.merge);
    case kCompress:
      return compress_option(invocation, g);
    case kDatestamp:
      return datestamp_option(invocation, g);
    case kNoStamp:
      return nodatestamp_option(invocation, g);
    default:
      return cmd::not_implemented("gds " + option_name(option));
  }
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
  switch (option) {
    case kHelp:
      return help(s);
    case kWrite:
      return write_option(ctx, s, invocation);
    case kContacts:
      return contacts_option(ctx, invocation);
    default:
      return settings_option(option, invocation, s.editor->mask.gds);
  }
}

}  // namespace

std::span<const std::string_view> gds_options() noexcept {
  return kOptions;
}

std::string output_file_name(std::string_view name, std::int32_t compress, const compat::CompatSettings& compat) {
  // Magic tests strrchr(name, '.'), so a dot in a directory counts too;
  // with magic-compat OFF only the last path component is tested.
  const std::size_t slash = name.rfind('/');
  const std::string_view tested =
      compat.magic_compat || slash == std::string_view::npos ? name : name.substr(slash + 1);
  std::string file{name};
  if (tested.find('.') == std::string_view::npos) {
    file += compress > 0 ? ".gds.gz" : ".gds";
  }
  return file;
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
