// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `schematic` dispatcher (capability schematic-command "Command and
// option matching", "Subcommand table", "Return values and errors in
// scripts"): option matching, the active-document check, EDIT's handlers
// first, then CMD's, else CORE-NOTIMPL-007.
//
// Owner: CMD.

#include "hefesto/schem/cmd/commands.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/schem/edit/operations.hpp"
#include "hefesto/schem/not_implemented.hpp"
#include "internal.hpp"

namespace hefesto::schem {

namespace {

using namespace cmdimpl;

std::vector<std::string_view> option_names() {
  std::vector<std::string_view> names;
  for (const SchematicOption& option : schematic_options()) names.push_back(option.name);
  return names;
}

const cmd::Handler* edit_handler(std::string_view option) {
  for (const EditOperation& operation : edit_operations()) {
    if (operation.option == option) return &operation.handler;
  }
  return nullptr;
}

cmd::CommandResult run_option(cmd::CommandContext& ctx, const cmd::Invocation& inv, const SchematicOption& option) {
  editor::EditorState& editor = *ctx.editor();
  if (option.needs_document && active_document(editor.schem) == nullptr) return no_document();
  if (const cmd::Handler* handler = edit_handler(option.name)) {
    cmd::Invocation forwarded = inv;
    forwarded.args.front() = std::string{option.name};
    return (*handler)(ctx, forwarded);
  }
  if (const OptionHandler handler = cmd_handler(option.name)) {
    const std::span<const std::string> args(inv.args.begin() + 1, inv.args.end());
    return handler(ctx, editor.schem, args);
  }
  return fail(not_implemented("schematic " + std::string{option.name}));
}

cmd::CommandResult schematic(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  const auto editor = cmd::require_editor(ctx);
  if (!editor) return fail(editor.error());
  Session& session = editor.value()->schem;
  session.compat = &ctx.project().compat;
  session.files = &ctx.services().files;
  if (inv.args.empty()) return option_help(ctx, session, {});
  const std::vector<std::string_view> names = option_names();
  const auto index = match_keyword(inv.args.front(), names);
  if (!index) return fail(index.error());
  return run_option(ctx, inv, schematic_options()[index.value()]);
}

}  // namespace

std::optional<diag::Diagnostic> register_schematic_command(cmd::CommandRegistry& registry) {
  return registry.add(cmd::CommandSpec{"schematic", "schematic option ?arg ...?",
                                       "Schematic capture: open, edit, netlist and save xschem-compatible "
                                       "schematics and symbols (\"schematic help\" lists the options).",
                                       schematic});
}

namespace cmdimpl {

cmd::CommandResult option_help(cmd::CommandContext& /*ctx*/, Session& /*session*/, std::span<const std::string> args) {
  if (!args.empty()) return usage("help");
  std::string text;
  for (const SchematicOption& option : schematic_options()) {
    text += option.syntax;
    text += '\n';
  }
  text.pop_back();
  return cmd::CommandResult::success(std::move(text));
}

OptionHandler cmd_handler(std::string_view option) {
  struct Entry {
    std::string_view name;
    OptionHandler handler;
  };
  static constexpr Entry kHandlers[] = {
      {"check", option_check},     {"close", option_close},
      {"getprop", option_getprop}, {"help", option_help},
      {"instances", option_instances}, {"netlist", option_netlist},
      {"new", option_new},         {"open", option_open},
      {"path", option_path},       {"pdk", option_pdk},
      {"reload", option_reload},   {"reload_symbols", option_reload_symbols},
      {"resolve", option_resolve}, {"save", option_save},
  };
  const auto* found = std::find_if(std::begin(kHandlers), std::end(kHandlers),
                                   [option](const Entry& entry) { return entry.name == option; });
  return found == std::end(kHandlers) ? nullptr : found->handler;
}

}  // namespace cmdimpl

}  // namespace hefesto::schem
