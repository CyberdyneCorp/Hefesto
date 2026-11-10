// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// macro, imacro and autoincr (ui-command-dispatch "macro and imacro
// commands", "Macro tables"; ui-box-stack-and-measure "Macro
// auto-increment"). Owner: DISPATCH.
// (Magic ref: windows/windCmdAM.c:997-1384, tcltk/tools.tcl:297-313)

#include <optional>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/ui/bindings.hpp"
#include "hefesto/ui/client.hpp"
#include "hefesto/ui/commands.hpp"
#include "hefesto/ui/diag_codes.hpp"
#include "hefesto/ui/input.hpp"
#include "hefesto/ui/macros.hpp"
#include "ui/command_util.hpp"

namespace hefesto::ui {

namespace {

using cmd::CommandResult;

diag::Diagnostic note(std::string text) {
  return diag::make(codes::kReport, std::move(text));
}

struct MacroOptions {
  bool list = false;
  bool help = false;
  bool reverse = false;
  std::optional<std::string> search;
};

// The parsed command: the table, the options and the remaining words.
struct MacroRequest {
  std::string table;
  bool named = false;  // the first word named a client or tool
  MacroOptions options;
  std::span<const std::string> rest;
};

// The table named by the first word, if it names a client or a tool.
std::optional<std::string> named_table(const editor::EditorState& editor, const std::string& word) {
  if (const ClientInfo* client = find_client_exact(word)) {
    return client->id == ClientId::layout ? macro_table_for(editor, "layout") : word;
  }
  if (editor.tools.find(word) != nullptr) {
    return word;
  }
  return std::nullopt;
}

// One option word: true when it was `list`, `help` or `-reverse`.
bool flag_option(MacroOptions& options, const std::string& word) {
  bool* flag = word == "list" ? &options.list : word == "help" ? &options.help : word == "-reverse" ? &options.reverse
                                                                                                 : nullptr;
  if (flag != nullptr) {
    *flag = true;
  }
  return flag != nullptr;
}

// Consumes `list`, `help`, `search TEXT` and `-reverse`; stops at `copy`
// and at the first other word.
void read_options(MacroRequest& req) {
  while (!req.rest.empty()) {
    const std::string& w = req.rest.front();
    if (flag_option(req.options, w)) {
      req.rest = req.rest.subspan(1);
      continue;
    }
    if (w != "search") {
      return;
    }
    if (req.rest.size() > 1) {
      req.options.search = req.rest[1];
      req.rest = req.rest.subspan(1);
    }
    req.rest = req.rest.subspan(1);
  }
}

CommandResult copy_table(editor::EditorState& editor, const std::string& from, const std::string& to) {
  editor.macros.copy(from, to);
  if (editor.tools.find(to) == nullptr && !editor.tools.add(Tool{to, {}, 0, false})) {
    return CommandResult::failure(
        diag::make(codes::kToolSpace, "Can't add tool \"" + to + "\":  no space in button handler"));
  }
  return CommandResult::success();
}

std::string shown_text(const Macro& macro, bool help) {
  return help && !macro.help.empty() ? macro.help : macro.text;
}

std::string listing_line(const Macro& macro, const std::string& key, const std::string& text, bool help) {
  return std::string{macro.interactive ? "Interactive macro '" : "Macro '"} + key + "' " + (help ? "" : "contains") +
         " \"" + text + "\"";
}

CommandResult list_table(const editor::EditorState& editor, const MacroRequest& req, bool interactive) {
  if (!editor.macros.has_table(req.table)) {
    cmd::Value::List known;
    for (std::string& name : editor.macros.table_names()) {
      known.emplace_back(std::move(name));
    }
    CommandResult result = CommandResult::failure(
        diag::make(codes::kMacroTable, "Cannot get macro list from current window."));
    result.value = std::move(known);
    return result;
  }
  cmd::Value::List pairs;
  std::string printed;
  bool any = false;
  for (const auto& [key, macro] : editor.macros.entries(req.table)) {
    if (macro == nullptr) {
      break;  // Magic stops at a deleted entry
    }
    if (interactive && !macro->interactive) {
      continue;
    }
    const std::string text = shown_text(*macro, req.options.help);
    if (req.options.search && text.find(*req.options.search) == std::string::npos) {
      continue;
    }
    const std::string name = key_display_name(key);
    if (req.options.list) {
      pairs.emplace_back(req.options.reverse ? text : name);
      pairs.emplace_back(req.options.reverse ? name : text);
    } else {
      printed += (printed.empty() ? "" : "\n") + listing_line(*macro, name, text, req.options.help);
    }
    any = true;
  }
  CommandResult result = req.options.list ? CommandResult::success(std::move(pairs)) : CommandResult::success();
  if (!any && !req.options.list) {
    printed = "No macros are defined for this client.";
  }
  if (!printed.empty()) {
    result.diagnostics.push_back(note(std::move(printed)));
  }
  return result;
}

CommandResult query(const editor::EditorState& editor, const MacroRequest& req) {
  const auto key = parse_key_name(req.rest[0]);
  if (!key.ok()) {
    return CommandResult::failure(key.error());
  }
  const Macro* macro = editor.macros.find(req.table, key.value());
  if (macro == nullptr || (req.options.help && macro->help.empty())) {
    return CommandResult::success();
  }
  const std::string text = req.options.help ? macro->help : macro->text;
  if (req.options.list) {
    return CommandResult::success(text);
  }
  CommandResult result = CommandResult::success();
  result.diagnostics.push_back(note(listing_line(*macro, key_display_name(key.value()), text, false)));
  return result;
}

CommandResult define(editor::EditorState& editor, const MacroRequest& req, bool interactive) {
  const auto key = parse_key_name(req.rest[0]);
  if (!key.ok()) {
    return CommandResult::failure(key.error());
  }
  const std::string& value = req.rest[1];
  if (value.empty()) {
    (void)editor.macros.erase(req.table, key.value());
  } else if (req.rest.size() == 2 && req.options.help) {
    (void)editor.macros.set_help(req.table, key.value(), value);
  } else {
    std::optional<std::string> help;
    if (req.rest.size() == 3) {
      help = req.rest[2];
    }
    editor.macros.define(req.table, key.value(), value, std::move(help), interactive);
  }
  return CommandResult::success();
}

// "If the first remaining word is not a valid key but the word after it is,
// the command SHALL silently do nothing."
bool unregistered_client(const MacroRequest& req) {
  return !req.named && req.rest.size() > 1 && !parse_key_name(req.rest[0]).ok() && parse_key_name(req.rest[1]).ok();
}

CommandResult run_macro(editor::EditorState& editor, MacroRequest& req, bool interactive) {
  read_options(req);
  if (!req.rest.empty() && req.rest.front() == "copy") {
    return req.rest.size() > 1 ? copy_table(editor, req.table, req.rest[1]) : CommandResult::success();
  }
  if (unregistered_client(req)) {
    return CommandResult::success();
  }
  switch (req.rest.size()) {
    case 0:
      return list_table(editor, req, interactive);
    case 1:
      return query(editor, req);
    case 2:
    case 3:
      return define(editor, req, interactive);
    default:
      return detail::usage(std::string{"Usage: "} + (interactive ? "imacro" : "macro") +
                           " [client] [macro_name [string] [help_text]]");
  }
}

CommandResult macro_handler(cmd::CommandContext& ctx, const cmd::Invocation& inv, bool interactive) {
  auto editor = detail::session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  return macro_command(*editor.value(), inv.args, interactive, detail::running_client(ctx));
}

// --- autoincr ------------------------------------------------------------------

// The first match of `[+-]*[[:digit:]]+`: its position and length.
std::optional<std::pair<std::size_t, std::size_t>> first_number(std::string_view text) {
  for (std::size_t i = 0; i < text.size(); ++i) {
    std::size_t j = i;
    while (j < text.size() && (text[j] == '+' || text[j] == '-')) {
      ++j;
    }
    std::size_t k = j;
    while (k < text.size() && text[k] >= '0' && text[k] <= '9') {
      ++k;
    }
    if (k > j) {
      return std::pair{i, k - i};
    }
  }
  return std::nullopt;
}

CommandResult autoincr(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  auto editor = detail::session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  if (inv.args.size() > 1) {
    return detail::usage("Usage: autoincr ?amount?");
  }
  const auto amount = inv.args.empty() ? std::optional<std::int64_t>{1} : detail::parse_int(inv.args[0]);
  if (!amount) {
    return detail::raise(codes::kUsage, "expected integer but got \"" + inv.args[0] + "\"");
  }
  ensure_defaults(*editor.value());
  const std::string table = macro_table_for(*editor.value(), "layout");
  const Macro* repeat = editor.value()->macros.find(table, Key{'.', 0});
  const std::string text = repeat == nullptr ? std::string{} : repeat->text;
  const auto found = first_number(text);
  if (!found) {
    return CommandResult::success();
  }
  const auto number = detail::parse_int(std::string_view{text}.substr(found->first, found->second));
  if (!number) {
    return detail::raise(codes::kUsage, "expected integer but got \"" + text.substr(found->first, found->second) +
                                            "\"");
  }
  std::string updated = text;
  updated.replace(found->first, found->second, std::to_string(*number + *amount));
  CommandResult result = detail::run_line(ctx, updated);
  editor.value()->macros.set(table, Key{'.', 0}, Macro{updated, {}, false});
  return result;
}

}  // namespace

CommandResult macro_command(editor::EditorState& editor, std::span<const std::string> args, bool interactive,
                            std::string_view client) {
  ensure_defaults(editor);
  MacroRequest req{macro_table_for(editor, client), false, {}, args};
  if (!args.empty()) {
    if (auto table = named_table(editor, args[0])) {
      req.table = std::move(*table);
      req.named = true;
      req.rest = args.subspan(1);
    }
  }
  return run_macro(editor, req, interactive);
}

namespace detail {

bool register_macro_commands(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "macro", "macro ?client|tool? ?list|help|search text|-reverse|copy name? ?key ?text ?help???",
                "Lists, shows, defines or deletes key and button macros of a client's or tool's table.",
                [](cmd::CommandContext& ctx, const cmd::Invocation& inv) { return macro_handler(ctx, inv, false); });
  ok = add(registry, "imacro", "imacro ?client|tool? ?list|help|search text|-reverse? ?key ?text ?help???",
           "Like macro, for interactive macros: the key opens the command prompt pre-filled with the text.",
           [](cmd::CommandContext& ctx, const cmd::Invocation& inv) { return macro_handler(ctx, inv, true); }) &&
       ok;
  ok = add(registry, "autoincr", "autoincr ?amount?",
           "Adds amount (default 1) to the first number of the repeat (.) macro, runs it and keeps it as the repeat "
           "macro.",
           autoincr) &&
       ok;
  return ok;
}

}  // namespace detail

}  // namespace hefesto::ui
