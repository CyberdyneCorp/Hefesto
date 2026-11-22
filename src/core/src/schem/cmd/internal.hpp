// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the `schematic` and `xschem` commands: the option and
// keyword matching of "Command and option matching", the argument parser,
// the error diagnostics, and the options CMD implements itself.
//
// Owner: CMD.

#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/schem/cmd/commands.hpp"
#include "hefesto/schem/lib/xschemrc.hpp"
#include "hefesto/schem/session.hpp"

namespace hefesto::schem::cmdimpl {

// ---- codes (codes.inc) -----------------------------------------------------
inline constexpr std::string_view kBadOption = "SCHEM-CMD-001";
inline constexpr std::string_view kUsage = "SCHEM-CMD-002";
inline constexpr std::string_view kNoDocument = "SCHEM-CMD-003";
inline constexpr std::string_view kUnsaved = "SCHEM-CMD-004";
inline constexpr std::string_view kNetlistFailed = "SCHEM-CMD-005";
inline constexpr std::string_view kNoObject = "SCHEM-CMD-006";
inline constexpr std::string_view kNoPdkData = "SCHEM-CMD-007";
inline constexpr std::string_view kXschemUnsupported = "SCHEM-CMD-008";
inline constexpr std::string_view kXschemNetlistType = "SCHEM-CMD-009";
inline constexpr std::string_view kXschemNeedsEditor = "SCHEM-CMD-010";

// ---- errors ----------------------------------------------------------------
/// A failure that raises a script error (Requirement "Return values and
/// errors in scripts").
[[nodiscard]] cmd::CommandResult fail(std::string_view code, std::string text);
[[nodiscard]] cmd::CommandResult fail(diag::Diagnostic error);
/// `Usage: schematic <syntax>` for `option`.
[[nodiscard]] cmd::CommandResult usage(std::string_view option);
[[nodiscard]] cmd::CommandResult no_document();

// ---- matching --------------------------------------------------------------
/// Unique-prefix, case-insensitive match of `word` in `names`; the index,
/// or the `ambiguous option` / `unknown option` error.
[[nodiscard]] diag::Outcome<std::size_t> match_keyword(std::string_view word, std::span<const std::string_view> names);

/// The option table entry `name` (exact); nullptr when absent.
[[nodiscard]] const SchematicOption* find_option(std::string_view name);

// ---- arguments -------------------------------------------------------------
struct FlagSpec {
  std::string_view name;  // e.g. "-window"
  bool takes_value = false;
};

struct ParsedArgs {
  std::vector<std::string> positional;
  /// One entry per flag spec: nullopt when absent, the value (empty for a
  /// flag without value) when given.
  std::vector<std::optional<std::string>> flags;

  [[nodiscard]] bool has(std::size_t flag) const { return flags.at(flag).has_value(); }
};

/// Splits `args` into positional words and flags. A word is a flag when it
/// starts with `-` followed by a letter (so `-30` is a number); flags are
/// keywords (unique prefixes, any case). An unknown flag, or a flag without
/// its value, gives the usage error of `option`.
[[nodiscard]] diag::Outcome<ParsedArgs> parse_args(std::span<const std::string> args, std::span<const FlagSpec> flags,
                                                   std::string_view option);

/// Parses a decimal floating-point coordinate.
[[nodiscard]] std::optional<double> parse_number(std::string_view text);

/// Tcl `string match` glob matching (`*`, `?`, `[...]`, `\x`).
[[nodiscard]] bool glob_match(std::string_view pattern, std::string_view text);

// ---- documents -------------------------------------------------------------
/// The index in `document.instances` of the instance named `name` (its
/// `name` attribute, exact); nullopt when there is none.
[[nodiscard]] std::optional<std::size_t> find_instance(const Document& document, std::string_view name);

/// `path` made absolute against the process's working folder, lexically
/// normalised.
[[nodiscard]] platform::Path absolute_path(const platform::Path& path);

/// Evaluates the `xschemrc` files for a document in `document_folder`
/// ("xschemrc as data"): the active PDK's (`pdk`, else the one selected
/// before, else the project's) and the folder's own. When at least one
/// exists, the result becomes `session.xschemrc` and `session.search_path`,
/// its diagnostics are emitted, and its variables are set in the project
/// interpreter for `tcleval`. A PDK named explicitly without xschem data
/// fails with SCHEM-CMD-007.
[[nodiscard]] diag::Status evaluate_xschemrc(cmd::CommandContext& ctx, Session& session,
                                             const platform::Path& document_folder,
                                             const std::optional<std::string>& pdk = std::nullopt);

/// The active PDK name: the one the last `xschemrc` evaluation used, else
/// the project's (env PDK of its interpreter); empty when none.
[[nodiscard]] std::string active_pdk(const cmd::CommandContext& ctx, const Session& session);

/// The synthetic `env(<name>)` of the project interpreter's sandbox
/// (PDK_ROOT, PDK, CAD_ROOT); empty when unset or without an editor.
[[nodiscard]] std::string sandbox_env(const cmd::CommandContext& ctx, std::string_view name);

/// The `xschemrc` files a document in `document_folder` evaluates with the
/// active PDK, as evaluate_xschemrc() reads them (a missing PDK file left
/// out); nullopt when neither the PDK's nor the folder's exists.
[[nodiscard]] std::optional<XschemrcInput> xschemrc_files(cmd::CommandContext& ctx, const Session& session,
                                                         const platform::Path& document_folder);

// ---- options implemented by CMD ---------------------------------------------
using OptionHandler = cmd::CommandResult (*)(cmd::CommandContext&, Session&, std::span<const std::string>);

/// CMD's handler of `option`, or nullptr when CMD does not implement it
/// (EDIT, SIMINT or a later milestone does). The arguments exclude the
/// option word.
[[nodiscard]] OptionHandler cmd_handler(std::string_view option);

cmd::CommandResult option_help(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_open(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_new(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_save(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_close(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_reload(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_reload_symbols(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_path(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_resolve(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_pdk(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_getprop(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_instances(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_netlist(cmd::CommandContext&, Session&, std::span<const std::string>);
cmd::CommandResult option_check(cmd::CommandContext&, Session&, std::span<const std::string>);

/// The value of `getprop` for one property string: the whole string without
/// a key, else the decoded value of `key` (empty when absent).
[[nodiscard]] std::string property_value(std::string_view props, const std::optional<std::string>& key);

}  // namespace hefesto::schem::cmdimpl
