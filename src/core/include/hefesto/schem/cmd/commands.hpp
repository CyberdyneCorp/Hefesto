// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `schematic` command and the `xschem` Tcl compatibility command
// (openspec/changes/m8-schematic design.md D12, "Area contracts" CMD;
// capability schematic-command "Command and option matching",
// "Subcommand table", "Return values and errors in scripts",
// "xschem compatibility command", "Command logging", Q-SCHEM-10).
//
// `schematic` is registered by cmd::register_all_commands; `xschem` is
// defined in every project interpreter by script::open_project_interpreter.
//
// Dispatch. `schematic <option> [args...]` matches <option> against
// schematic_options() (unique prefix, any case). An option of EDIT's
// edit_operations() runs EDIT's handler, which receives the invocation with
// args[0] replaced by the full option name; otherwise CMD's own handler runs
// (open, new, save, close, reload, reload_symbols, path, resolve, getprop,
// instances, netlist, check, pdk, help); any other option answers
// CORE-NOTIMPL-007 until its workstream implements it. Every failure raises
// a script error (CommandResult::raises).
//
// The active document is the last of Session::documents (the one opened or
// created last); `close` makes the previous one active again.
//
// Owner: CMD.

#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::script {
class Interpreter;
}

namespace hefesto::schem {

struct Session;
struct OpenDocument;

/// One option of "Subcommand table": its name and its usage syntax.
struct SchematicOption {
  std::string_view name;
  std::string_view syntax;  // starts with the name, e.g. "open <file> [-window new|tab|current]"
  bool needs_document = true;
};

/// The options in the order of "Subcommand table" (the help order).
[[nodiscard]] std::span<const SchematicOption> schematic_options();

/// The `xschem` subcommands Hefesto supports, sorted ("xschem compatibility
/// command"): every inventoried subcommand plus `version`, `load`,
/// `hilight_netname`, `zoom_full`, `get_tok` and `subst_tok`.
[[nodiscard]] std::span<const std::string_view> xschem_subcommands();

/// The active document (the last opened); nullptr when none is open.
[[nodiscard]] OpenDocument* active_document(Session& session);

/// Adds `schematic` to `registry`; CMD-REGISTRY-001 on a duplicate.
[[nodiscard]] std::optional<diag::Diagnostic> register_schematic_command(cmd::CommandRegistry& registry);

/// Defines the `xschem` command in `interpreter` over `session`: the
/// subcommands of xschem_subcommands(), and `xschem command '<cmd>' is not
/// supported` for any other. Subcommands that change a document or need the
/// files run `::magic::schematic` in `interpreter` (bound by
/// script::bind_registry). `session` must outlive the command.
void register_xschem_command(script::Interpreter& interpreter, Session& session);

}  // namespace hefesto::schem
