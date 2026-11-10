// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of the ui commands: usage errors, printed notes, the
// editor session, and running other commands from a handler ("nested"
// commands: resolved in the running client like a dispatched command, but
// not logged, counted or followed by a post-command update, as Magic's
// tools.tcl procs and `*bypass` run them). Owner: DISPATCH.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/cmd/units.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::ui::detail {

/// UI-USAGE-001 with `text`.
[[nodiscard]] cmd::CommandResult usage(std::string text);
/// The same, raising a script error in both modes (tools.tcl `error`).
[[nodiscard]] cmd::CommandResult raise(std::string_view code, std::string text);
/// Prints `text` as Magic's TxPrintf does (UI-REPORT-001, info).
void print(cmd::CommandContext& ctx, std::string text);

/// The editor session, or CMD-SESSION-001.
[[nodiscard]] diag::Outcome<editor::EditorState*> session(const cmd::CommandContext& ctx);
/// The client the running command was sent to (`layout` without a session).
[[nodiscard]] std::string running_client(const cmd::CommandContext& ctx);

/// Runs `words` as a nested command. An unknown or ambiguous name fails like
/// the dispatcher's lookup (CMD-DISPATCH-001, UI-DISPATCH-001).
[[nodiscard]] cmd::CommandResult run_nested(cmd::CommandContext& ctx, const std::vector<std::string>& words,
                                            cmd::Origin origin = cmd::Origin::internal);
/// run_nested() with the nested diagnostics emitted through `ctx`; the
/// result keeps the value and status only.
cmd::CommandResult run_forwarded(cmd::CommandContext& ctx, const std::vector<std::string>& words);
/// Splits a command line (macro text) and runs its commands nested, stopping
/// at the first failure; returns the last result.
cmd::CommandResult run_line(cmd::CommandContext& ctx, std::string_view line);

/// A result value as words (a list's elements, or the words of a string).
[[nodiscard]] std::vector<std::string> value_words(const cmd::Value& value);
/// A decimal integer (optionally signed); std::nullopt otherwise.
[[nodiscard]] std::optional<std::int64_t> parse_int(std::string_view text);
/// C atoi(): the leading integer of `text`, 0 when there is none.
[[nodiscard]] std::int64_t atoi_like(std::string_view text);

/// Sets the editor's units to `internal` while alive (tools.tcl's
/// `units internal` ... `units {*}$curunits`).
class InternalUnits {
 public:
  explicit InternalUnits(editor::EditorState& editor);
  ~InternalUnits();
  InternalUnits(const InternalUnits&) = delete;
  InternalUnits& operator=(const InternalUnits&) = delete;

 private:
  editor::EditorState& editor_;
  cmd::UnitsState saved_;
};

/// The coordinate units of the session for cmd::format_value() and
/// cmd::parse_coordinate() (the same derivation as the db area's).
[[nodiscard]] cmd::CoordinateUnits coordinate_units(const editor::EditorState& editor);
/// Microns per internal unit (`cif scale out`; 0.01 per centimicron of the
/// CIF output style, scaled by the session grid).
[[nodiscard]] double microns_per_unit(const editor::EditorState& editor);

/// Registers `name` in its default client; false when taken.
bool add(cmd::CommandRegistry& registry, std::string_view name, std::string_view synopsis, std::string_view help,
         cmd::Handler handler);

// Per-file registration functions (commands.cpp calls them all).
bool register_session_commands(cmd::CommandRegistry& registry);
bool register_macro_commands(cmd::CommandRegistry& registry);
bool register_tool_commands(cmd::CommandRegistry& registry);
bool register_stack_commands(cmd::CommandRegistry& registry);
bool register_measure_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::ui::detail
