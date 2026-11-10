// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Command registry: one command table per window client (capability
// ui-command-dispatch, requirements "Window client model", "Command
// registration and help text", "Command name lookup", "Command dispatch
// across global and client tables"; m2-interactive-editing design.md D7).
//
// Every command is registered in one client's table: `*window` (the global
// window commands) or a client such as `layout` or `color`. Areas keep
// registering by area; add(spec) without a client puts the command in the
// table Magic's Tcl build registers it in (default_client()), so the M1
// registrations did not change. The same name may exist in several tables
// (the `color` client's `load` beside the layout `load`).
//
// Lookup follows Magic's Lookup() (utils/lookup.c): case-insensitive letters,
// an exact name wins, otherwise a unique prefix; a leading `::magic::` or
// `magic::` is ignored. resolve() applies the two-table dispatch rule of
// WindSendCommand (windows/windSend.c:86-352).
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"

namespace hefesto::cmd {

class CommandContext;

using Handler = std::function<CommandResult(CommandContext&, const Invocation&)>;

struct CommandSpec {
  std::string name;
  std::string synopsis;  // e.g. "setting get|set <name> [<value>]"
  std::string help;      // one paragraph
  Handler handler;
};

/// The global window-command client (hidden: never listed nor opened).
inline constexpr std::string_view kWindowClient = "*window";
/// The default client (layout windows, and commands with no window).
inline constexpr std::string_view kLayoutClient = "layout";

enum class LookupStatus : std::uint8_t { found, ambiguous, not_found };

/// The outcome of looking a command word up.
struct CommandLookup {
  LookupStatus status = LookupStatus::not_found;
  const CommandSpec* spec = nullptr;  // set when `status == found`
  std::string client;                 // the table `spec` came from
};

/// Magic's Lookup() over a list of entry names: the index of the chosen
/// name, or `ambiguous` / `not_found`. Exposed for the keyword tables of
/// the ui area (tool names, `logcommands` keywords, ...).
struct NameMatch {
  LookupStatus status = LookupStatus::not_found;
  std::size_t index = 0;
};
[[nodiscard]] NameMatch match_name(std::string_view word, const std::vector<std::string_view>& names);

class CommandRegistry {
 public:
  /// Registers a command in default_client(spec.name). A duplicate name in
  /// that table is a programming error: the existing command is kept and
  /// CMD-REGISTRY-001 is returned.
  [[nodiscard]] std::optional<diag::Diagnostic> add(CommandSpec spec);
  /// Registers a command in `client`'s table (created on first use).
  [[nodiscard]] std::optional<diag::Diagnostic> add(CommandSpec spec, std::string_view client);
  /// Registers or replaces a command in `client`'s table. Used when an area
  /// takes over a command registered earlier by another (the ui area's
  /// `help`, which replaces the M0 built-in).
  void replace(CommandSpec spec, std::string_view client);

  /// Exact (case-sensitive) name in the `*window` table, then `layout`, then
  /// the other tables in name order; nullptr when absent.
  [[nodiscard]] const CommandSpec* find(std::string_view name) const;
  /// Exact name in one table.
  [[nodiscard]] const CommandSpec* find(std::string_view name, std::string_view client) const;

  /// Magic's Lookup() in one table.
  [[nodiscard]] CommandLookup lookup(std::string_view word, std::string_view client) const;
  /// The dispatch rule of "Command dispatch across global and client
  /// tables" for a command sent to `client` (`*window` lookup first, unless
  /// `window_commands` is false, then the client's table; an exact name
  /// beats an abbreviation; otherwise the client's command wins).
  [[nodiscard]] CommandLookup resolve(std::string_view word, std::string_view client,
                                      bool window_commands = true) const;

  /// Every registered name once, in byte-wise order.
  [[nodiscard]] std::vector<std::string> names() const;
  /// One client's commands in ascending byte order (strcmp) of their help
  /// string (help_string()); empty for an unknown client.
  [[nodiscard]] std::vector<const CommandSpec*> table(std::string_view client) const;
  /// The clients that have a table, in name order.
  [[nodiscard]] std::vector<std::string> clients() const;

  /// The table Magic registers `name` in: `*window` for the global window
  /// commands of "Command registration and help text" (and Hefesto's
  /// interpreter-level commands: `echo`, `send`, `source`, `history`,
  /// `setting`, `hefesto::...`, the tools.tcl helper commands), `layout`
  /// for everything else.
  [[nodiscard]] static std::string_view default_client(std::string_view name);
  /// The help string of a command: its synopsis when the synopsis starts
  /// with the command name, otherwise the name followed by the synopsis.
  [[nodiscard]] static std::string help_string(const CommandSpec& spec);

 private:
  using Table = std::map<std::string, CommandSpec, std::less<>>;
  [[nodiscard]] const Table* table_of(std::string_view client) const;

  std::map<std::string, Table, std::less<>> tables_;
};

}  // namespace hefesto::cmd
