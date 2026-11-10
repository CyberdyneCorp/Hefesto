// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// help, send, *bypass, source, sleep, history, logcommands and netlist
// (ui-command-dispatch "help command", "sleep, echo and send", "Script files
// and source", "Command history", "Command logging"; ui-wizard-and-debug
// "Wizard command visibility", `*bypass`). Owner: DISPATCH.
// (Magic ref: windows/windSend.c:402-452, windows/windCmdSZ.c:319-400,
// windows/windCmdAM.c:917-996, commands/CmdFI.c:1472-1552,
// textio/txCommands.c:703-865)

#include <array>
#include <optional>
#include <cctype>
#include <chrono>
#include <ctime>
#include <thread>
#include <utility>

#include "hefesto/cmd/script.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/ui/client.hpp"
#include "hefesto/ui/diag_codes.hpp"
#include "hefesto/ui/state.hpp"
#include "ui/command_util.hpp"

namespace hefesto::ui::detail {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

// --- help ------------------------------------------------------------------

// Magic's Match(): `*`, `?`, `[...]` (ranges, no negation) and `\x`.
bool glob_class(std::string_view pattern, std::size_t& p, char c) {
  bool matched = false;
  ++p;  // past '['
  while (p < pattern.size() && pattern[p] != ']') {
    const char lo = pattern[p];
    if (p + 2 < pattern.size() && pattern[p + 1] == '-' && pattern[p + 2] != ']') {
      matched = matched || (c >= lo && c <= pattern[p + 2]);
      p += 3;
    } else {
      matched = matched || c == lo;
      ++p;
    }
  }
  return matched;
}

// One pattern element at `p` against `c`: the position after it when it
// matches.
std::optional<std::size_t> match_one(std::string_view pattern, std::size_t p, char c) {
  if (p >= pattern.size()) {
    return std::nullopt;
  }
  if (pattern[p] == '?') {
    return p + 1;
  }
  if (pattern[p] == '[') {
    std::size_t next = p;
    const bool ok = glob_class(pattern, next, c);
    return ok ? std::optional<std::size_t>{next < pattern.size() ? next + 1 : next} : std::nullopt;
  }
  const bool escaped = pattern[p] == '\\' && p + 1 < pattern.size();
  if (pattern[escaped ? p + 1 : p] != c) {
    return std::nullopt;
  }
  return p + (escaped ? 2 : 1);
}

bool glob_match(std::string_view pattern, std::string_view text) {
  std::size_t p = 0;
  std::size_t t = 0;
  std::size_t star = std::string_view::npos;
  std::size_t resume = 0;
  while (t < text.size()) {
    if (p < pattern.size() && pattern[p] == '*') {
      star = p++;
      resume = t;
      continue;
    }
    if (const auto next = match_one(pattern, p, text[t])) {
      p = *next;
      ++t;
      continue;
    }
    if (star == std::string_view::npos) {
      return false;
    }
    p = star + 1;
    t = ++resume;
  }
  while (p < pattern.size() && pattern[p] == '*') {
    ++p;
  }
  return p == pattern.size();
}

std::string capitalized(std::string_view name) {
  std::string out{name};
  if (!out.empty() && std::islower(static_cast<unsigned char>(out[0])) != 0) {
    out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
  }
  return out;
}

// windHelp(): one table's listing.
std::string help_table(const cmd::CommandRegistry& registry, std::string_view client, std::string_view title,
                       const std::vector<std::string>& args) {
  const bool wizard = args.size() == 1 && args[0] == "wizard";
  const std::string pattern = !wizard && args.size() == 1 ? "*" + args[0].substr(0, 195) + "*" : "*";
  std::string out = "\n";
  out += wizard ? "Wizard " + capitalized(title) + " Commands\n----------------------"
                : capitalized(title) + " Commands\n---------------";
  for (const cmd::CommandSpec* spec : registry.table(client)) {
    const std::string help = cmd::CommandRegistry::help_string(*spec);
    const bool is_wizard = !help.empty() && help.front() == '*';
    if (is_wizard == wizard && glob_match(pattern, help)) {
      out += '\n';
      out += help;
    }
  }
  return out;
}

CommandResult help(CommandContext& ctx, const Invocation& inv) {
  const std::string client = running_client(ctx);
  const bool both = client != cmd::kWindowClient;
  if (inv.args.size() > 1) {
    CommandResult result = usage("Usage:  help [pattern]");
    if (both) {
      result.diagnostics.push_back(result.diagnostics.front());
    }
    return result;
  }
  std::string text = help_table(ctx.registry(), cmd::kWindowClient, "Global", inv.args);
  if (both) {
    text += "\n" + help_table(ctx.registry(), client, client, inv.args);
  }
  print(ctx, std::move(text));
  return CommandResult::success();
}

// --- send, *bypass, netlist --------------------------------------------------

CommandResult send(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() < 2) {
    return usage("Usage: send type command\nValid window types are:\n" + window_types_listing());
  }
  const auto client = find_client(inv.args[0]);
  if (!client.ok()) {
    return CommandResult::failure(client.error());
  }
  if (!client_available(*client.value())) {
    return CommandResult::failure(diag::make(
        codes::kModuleUnavailable, "the " + std::string{client.value()->name} + " window type is not available yet"));
  }
  editor::EditorState* editor = ctx.editor();
  std::string saved{cmd::kLayoutClient};
  if (editor != nullptr) {
    saved = std::exchange(state(*editor).client, std::string{client.value()->name});
  }
  CommandResult result = run_nested(ctx, {inv.args.begin() + 1, inv.args.end()});
  if (editor != nullptr) {
    state(*editor).client = saved;
  }
  return result;
}

CommandResult bypass(CommandContext& ctx, const Invocation& inv) {
  return run_nested(ctx, inv.args, inv.origin);
}

CommandResult netlist(CommandContext& /*ctx*/, const Invocation& /*inv*/) {
  return CommandResult::failure(
      diag::make(codes::kModuleUnavailable, "netlist: the netlist module is not available yet (netlist-window, M12)"));
}

// --- source ----------------------------------------------------------------

CommandResult source(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() != 1) {
    return usage("Usage: source filename");
  }
  const auto text = ctx.services().files.read(inv.args[0]);
  if (!text.ok()) {
    return CommandResult::failure(diag::make(codes::kSource, "Couldn't read from " + inv.args[0] + "."));
  }
  const cmd::ParsedScript parsed = cmd::parse_script(text.value(), inv.args[0]);
  if (parsed.error) {
    return CommandResult::raised(*parsed.error);
  }
  editor::EditorState* editor = ctx.editor();
  const bool sticky = editor != nullptr && std::exchange(state(*editor).point_sticky, true);
  CommandResult last = CommandResult::success();
  for (const cmd::ParsedCommand& command : parsed.commands) {
    last = run_forwarded(ctx, command.words);
    if (!last.ok() && (ctx.session().strict || last.raises)) {
      break;
    }
    if (ctx.session().quit_requested) {
      break;
    }
  }
  if (editor != nullptr) {
    state(*editor).point_sticky = sticky;
  }
  return last;
}

// --- sleep -------------------------------------------------------------------

bool default_sleep(UiState* ui, std::chrono::milliseconds total) {
  using namespace std::chrono_literals;
  const auto end = std::chrono::steady_clock::now() + total;
  while (std::chrono::steady_clock::now() < end) {
    if (ui != nullptr && ui->interrupt_pending.load()) {
      return false;
    }
    std::this_thread::sleep_for(std::min<std::chrono::steady_clock::duration>(50ms, end - std::chrono::steady_clock::now()));
  }
  return true;
}

CommandResult sleep(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() != 1) {
    return usage("Usage: sleep seconds");
  }
  const std::int64_t seconds = atoi_like(inv.args[0]);
  if (seconds <= 0) {
    return CommandResult::success();
  }
  const std::chrono::milliseconds total{seconds * 1000};
  UiState* ui = ctx.editor() == nullptr ? nullptr : &state(*ctx.editor());
  if (ui != nullptr && ui->sleeper) {
    (void)ui->sleeper(total);
  } else {
    (void)default_sleep(ui, total);
  }
  return CommandResult::success();
}

// --- history -----------------------------------------------------------------

constexpr std::string_view kHistoryUsage =
    "Usage: history [n] [r] [<num>]\n"
    "         'n'     suppresses printing numbers\n"
    "         'r'     prints the list in reverse\n"
    "         '<num>' is the number of entries to print\n"
    "         'help'  prints this message";

CommandResult history(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  bool numbers = true;
  bool reverse = false;
  std::int64_t count = 0;
  for (const std::string& arg : inv.args) {
    if (arg == "help") {
      return usage(std::string{kHistoryUsage});
    }
    if (arg == "n" || arg == "r") {
      (arg == "n" ? numbers : reverse) = arg == "r";
      continue;
    }
    count = atoi_like(arg);
    if (count == 0) {
      CommandResult result = usage(std::string{kHistoryUsage});
      result.diagnostics.insert(result.diagnostics.begin(),
                                diag::make(codes::kUsage, "Bad arg to history: '" + arg + "'"));
      return result;
    }
  }
  if (count < 0) {
    return CommandResult::success();
  }
  const std::string listing =
      state(*editor.value()).history.listing(static_cast<std::size_t>(count), numbers, reverse);
  if (!listing.empty()) {
    print(ctx, listing);
  }
  return CommandResult::success();
}

// --- logcommands -------------------------------------------------------------

std::string ctime_now(CommandContext& ctx) {
  const std::time_t t = std::chrono::system_clock::to_time_t(ctx.services().clock.wall_now());
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &t);
#else
  localtime_r(&t, &local);
#endif
  std::array<char, 64> buf{};
  const std::size_t n = std::strftime(buf.data(), buf.size(), "%a %b %e %H:%M:%S %Y", &local);
  return std::string{buf.data(), n};
}

// TxLogStart: the Tcl-build header, then the window view and the box.
CommandResult log_start(CommandContext& ctx, editor::EditorState& editor, const std::string& file) {
  CommandLogState& log = state(editor).log;
  if (log.path) {
    return CommandResult::failure(diag::make(codes::kLog, "There is already a log file open!"));
  }
  std::string header = "# Magic command log file\n# Using technology: " + editor.technology->name() + "\n";
  const view::LayoutWindow* window = editor.windows.current();
  if (window != nullptr) {
    header += "# Title: " + window->caption() + "\n";
  }
  header += "# Date: " + ctime_now(ctx) + "\n";
  if (window != nullptr) {
    const db::Rect& s = window->mapping().surface;
    header += "view " + std::to_string(s.x0) + "i " + std::to_string(s.y0) + "i " + std::to_string(s.x1) + "i " +
              std::to_string(s.y1) + "i\n";
    if (editor.box) {
      const db::Rect& b = *editor.box;
      header += "box values " + std::to_string(b.x0) + "i " + std::to_string(b.y0) + "i " + std::to_string(b.x1) +
                "i " + std::to_string(b.y1) + "i\n";
    }
  }
  if (!ctx.services().files.write_atomic(file, header).ok()) {
    return CommandResult::failure(diag::make(codes::kLog, "Could not open file '" + file + "' for writing."));
  }
  log = CommandLogState{file, false, false};
  print(ctx, "Logging commands to file \"" + file + "\"");
  return CommandResult::success();
}

void log_update(CommandContext& ctx, CommandLogState& log) {
  log.update = !log.update;
  print(ctx, log.update ? "Forcing display refresh after logged commands." : "No display refresh after logged commands.");
}

enum class LogAction : std::uint8_t { start, stop, update, suspend, resume };

struct LogRequest {
  LogAction action = LogAction::stop;
  std::optional<std::string> file;
  bool update = false;
};

std::optional<LogRequest> parse_log_request(const std::vector<std::string>& args) {
  static const std::vector<std::string_view> kKeywords = {"start", "stop", "update", "suspend", "resume"};
  LogRequest req;
  if (args.size() > 2) {
    return std::nullopt;
  }
  if (!args.empty()) {
    const cmd::NameMatch m = cmd::match_name(args[0], kKeywords);
    if (m.status == cmd::LookupStatus::found) {
      req.action = static_cast<LogAction>(m.index);
    } else {
      req.action = LogAction::start;
      req.file = args[0];
    }
  }
  if (args.size() == 2) {
    if (!req.file) {
      req.file = args[1];
    } else if (const cmd::NameMatch m = cmd::match_name(args[1], kKeywords);
               m.status == cmd::LookupStatus::found && m.index == 2) {
      req.update = true;
    } else {
      return std::nullopt;
    }
  }
  return req;
}

CommandResult logcommands(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  const auto req = parse_log_request(inv.args);
  if (!req || (req->action == LogAction::start && !req->file)) {
    return usage("Usage: " + inv.name + " [start|stop|update|suspend|resume [filename]]");
  }
  CommandLogState& log = state(*editor.value()).log;
  switch (req->action) {
    case LogAction::start: {
      CommandResult started = log_start(ctx, *editor.value(), *req->file);
      if (started.ok() && req->update) {
        log_update(ctx, log);
      }
      return started;
    }
    case LogAction::stop:
      if (log.path) {
        print(ctx, "Ending command logging to file.");
        log = CommandLogState{};
      }
      return CommandResult::success();
    case LogAction::update:
      if (!log.path) {
        return CommandResult::failure(diag::make(codes::kLog, "There is no log file to set an update flag on."));
      }
      log_update(ctx, log);
      return CommandResult::success();
    case LogAction::suspend:
    case LogAction::resume:
      log.suspended = log.path && req->action == LogAction::suspend;
      return CommandResult::success();
  }
  return CommandResult::success();
}

}  // namespace

bool register_session_commands(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "help", "help [pattern]",
                "Lists the global commands and the commands of the window's client, or those whose help string "
                "matches the glob *pattern*; `help wizard` lists the wizard (`*`) commands.",
                help);
  ok = add(registry, "send", "send type command ?args?",
           "Sends a command to the window client `type` (any unique prefix; hidden clients refused).", send) &&
       ok;
  ok = add(registry, "*bypass", "*bypass command ?args?",
           "Runs a command without counting it as a user command and without logging it.", bypass) &&
       ok;
  ok = add(registry, "source", "source filename",
           "Runs the commands of a script file (the scripting interpreter's source command; Tcl line rules).",
           source) &&
       ok;
  ok = add(registry, "sleep", "sleep seconds", "Pauses for a whole number of seconds; an interrupt ends it early.",
           sleep) &&
       ok;
  ok = add(registry, "history", "history ?n? ?r? ?num? ?help?",
           "Lists the command history: the last num entries, without numbers (n), newest first (r).", history) &&
       ok;
  ok = add(registry, "logcommands", "logcommands ?start|stop|update|suspend|resume? ?filename?",
           "Starts, stops, suspends or resumes the command log, or toggles its display-update mode.", logcommands) &&
       ok;
  ok = add(registry, "netlist", "netlist option ?args?",
           "Netlist tool actions (netlist-window, M12): reports that the netlist module is unavailable.", netlist) &&
       ok;
  return ok;
}

}  // namespace hefesto::ui::detail
