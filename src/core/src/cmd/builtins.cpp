// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/builtins.hpp"

#include <charconv>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/cmd/context.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/util/magic_hash_table.hpp"
#include "hefesto/version.hpp"

namespace hefesto::cmd {

namespace {

namespace codes = diag::codes;

CommandResult usage(std::string_view synopsis) {
  return CommandResult::failure(diag::make(codes::kCmdUsage, "usage: " + std::string{synopsis}));
}

CommandResult from_status(const diag::Status& status) {
  return status.ok() ? CommandResult::success() : CommandResult::failure(status.error());
}

// --- echo, version, quit ----------------------------------------------------

CommandResult echo(CommandContext& /*ctx*/, const Invocation& inv) {
  std::string text;
  for (const auto& arg : inv.args) {
    if (!text.empty()) {
      text += ' ';
    }
    text += arg;
  }
  return CommandResult::success(std::move(text));
}

CommandResult version(CommandContext& /*ctx*/, const Invocation& inv) {
  if (!inv.args.empty()) {
    return usage("version");
  }
  return CommandResult::success(std::string{hefesto::version()});
}

// app-startup "quit command and exit status" (windQuitCmd): `-noprompt` only
// as the last word, then an optional integer status. Hefesto rejects a
// status outside 0-255 (Q-UI-32); a rejected word stays as an extra
// argument, so the usage error follows and quitting is cancelled.
// (Magic ref: windows/windCmdNR.c:262-334)
std::optional<int> parse_exit_status(std::string_view word) {
  int value = 0;
  const auto [end, ec] = std::from_chars(word.data(), word.data() + word.size(), value);
  if (ec != std::errc{} || end != word.data() + word.size() || value < 0 || value > 255) {
    return std::nullopt;
  }
  return value;
}

CommandResult quit(CommandContext& ctx, const Invocation& inv) {
  std::vector<std::string> args = inv.args;
  bool check_first = true;
  if (!args.empty() && args.back() == "-noprompt") {
    check_first = false;
    args.pop_back();
  }
  int status = 0;
  std::vector<diag::Diagnostic> notes;
  if (!args.empty()) {
    if (const auto parsed = parse_exit_status(args.back())) {
      status = *parsed;
      args.pop_back();
    } else {
      notes.push_back(diag::make(codes::kCmdUsage, "Invalid exit_status: " + args.back()));
    }
  }
  if (!args.empty()) {
    CommandResult result = CommandResult::failure(diag::make(codes::kCmdUsage, "Usage: quit [exit_status] [-noprompt]"));
    result.diagnostics.insert(result.diagnostics.begin(), notes.begin(), notes.end());
    return result;
  }
  ScriptSession& session = ctx.session();
  if (check_first) {
    for (const auto& may_exit : session.exit_checks) {
      if (may_exit && !may_exit()) {
        return CommandResult::success();  // vetoed: the application keeps running
      }
    }
  }
  session.exit_status = status;
  session.quit_requested = true;
  return CommandResult::success();
}

// --- setting ----------------------------------------------------------------

constexpr std::string_view kSettingSynopsis = "setting ?get name | set name on|off?";

CommandResult setting_list(const settings::ProjectSettings& project) {
  Value::List pairs;
  for (const auto name : settings::setting_names()) {
    pairs.emplace_back(std::string{name});
    pairs.emplace_back(settings::get(project, name).value() ? "on" : "off");
  }
  return CommandResult::success(std::move(pairs));
}

CommandResult setting(CommandContext& ctx, const Invocation& inv) {
  const auto& args = inv.args;
  if (args.empty()) {
    return setting_list(ctx.project());
  }
  if (args[0] == "get" && args.size() == 2) {
    const auto value = settings::get(ctx.project(), args[1]);
    return value ? CommandResult::success(value.value() ? "on" : "off") : CommandResult::failure(value.error());
  }
  if (args[0] == "set" && args.size() == 3) {
    return from_status(settings::set(ctx.project(), args[1], args[2]));
  }
  return usage(kSettingSynopsis);
}

// --- hefesto::strict ---------------------------------------------------------

CommandResult strict(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.empty()) {
    return CommandResult::success(ctx.session().strict ? 1 : 0);
  }
  if (inv.args.size() == 1 && (inv.args[0] == "on" || inv.args[0] == "off")) {
    ctx.session().strict = inv.args[0] == "on";
    return CommandResult::success();
  }
  // "Any other argument SHALL raise a Tcl error with a usage message", in both modes.
  return CommandResult::raised(diag::make(codes::kCmdUsage, "usage: hefesto::strict ?on|off?"));
}

// --- hefesto::debug::hashorder (M0 only) ------------------------------------

constexpr std::string_view kHashOrderSynopsis = "hefesto::debug::hashorder size ?+key|-key ...?";

std::optional<std::size_t> parse_size(std::string_view text) {
  std::size_t value = 0;
  const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || end != text.data() + text.size() || value == 0 || value > (std::size_t{1} << 20)) {
    return std::nullopt;
  }
  return value;
}

// Applies "+key" (insert) or "-key" (remove); false for any other form.
bool apply_hash_op(util::MagicHashTable<int>& table, std::string_view op) {
  if (op.size() < 2 || (op.front() != '+' && op.front() != '-')) {
    return false;
  }
  const std::string key{op.substr(1)};
  if (op.front() == '+') {
    table.find_or_insert(key);
  } else {
    table.remove(key);
  }
  return true;
}

CommandResult hash_order(CommandContext& /*ctx*/, const Invocation& inv) {
  const auto size = inv.args.empty() ? std::nullopt : parse_size(inv.args[0]);
  if (!size) {
    return usage(kHashOrderSynopsis);
  }
  util::MagicHashTable<int> table(*size);
  for (std::size_t i = 1; i < inv.args.size(); ++i) {
    if (!apply_hash_op(table, inv.args[i])) {
      return usage(kHashOrderSynopsis);
    }
  }
  std::string order;
  table.for_each([&](const auto& entry) {
    if (!order.empty()) {
      order += ' ';
    }
    order += entry.key;
  });
  return CommandResult::success(std::move(order));
}

}  // namespace

bool register_builtins(CommandRegistry& registry) {
  CommandSpec specs[] = {
      {"echo", "echo ?word ...?", "Returns its arguments joined by single spaces.", echo},
      {"version", "version", "Returns the Hefesto version as major.minor.patch.", version},
      {"quit", "quit ?exit_status? ?-noprompt?",
       "Stops the running script and ends the session with exit_status (0-255, default 0). Without -noprompt "
       "every exit check runs first and may cancel the quit.",
       quit},
      {"setting", std::string{kSettingSynopsis},
       "Lists the project settings (magic-compat, netgen-compat, script-strict), reads one, or changes one. "
       "Each setting is independent of the others.",
       setting},
      {"hefesto::strict", "hefesto::strict ?on|off?",
       "Without an argument returns 1 if script strict mode is in effect for this interpreter and 0 otherwise; "
       "on or off sets it for this interpreter only.",
       strict},
      {"hefesto::debug::hashorder", std::string{kHashOrderSynopsis},
       "M0-only debug command for golden case CORE-HASH-01: builds a Magic string hash table of the requested "
       "size, applies each +key (insert) and -key (remove) in order, and returns the keys in iteration order. "
       "Removed when layout commands can exercise hash order.",
       hash_order},
  };
  bool all_added = true;
  for (auto& spec : specs) {
    all_added = !registry.add(std::move(spec)).has_value() && all_added;
  }
  return all_added;
}

}  // namespace hefesto::cmd
