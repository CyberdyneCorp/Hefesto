// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// evaluate_setup (lvs-setup-file "Setup file language and runtime", "Setup
// file evaluation", "Setup evaluation is bounded and cancellable"; design
// D7). A fresh sandboxed interpreter (X-3) per run gets the setup commands
// as `::netgen::<cmd>` and, where Tcl has no command of that name, also
// unqualified. The file is read line by line; each complete command is
// evaluated in the `netgen` namespace under `catch`, so an error is
// reported and skipped. The interpreter's command limit and `info
// cmdcount` bound the whole evaluation by the step budget; cancellation is
// polled before each command, inside every setup command, and from the
// interpreter's progress hook, which interrupts a running loop.
//
// Owner: LVSSETUP.

#include "hefesto/lvs/setup/setup.hpp"

#include <array>
#include <chrono>
#include <memory>
#include <string_view>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lvs/setup/codes.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"
#include "runtime.hpp"

namespace hefesto::lvs {

namespace {

using setup::Runtime;
using setup::Words;

using Handler = cmd::CommandResult (*)(Runtime&, const Words&);

struct CommandEntry {
  std::string_view name;
  Handler handler;
  std::vector<std::vector<std::string>> SetupState::*record;
};

const std::array<CommandEntry, 11> kCommands{{
    {"permute", setup::permute_command, &SetupState::permutes},
    {"property", setup::property_command, &SetupState::properties},
    {"equate", setup::equate_command, &SetupState::equates},
    {"ignore", setup::ignore_command, &SetupState::ignores},
    {"model", setup::model_command, &SetupState::models},
    {"global", setup::global_command, &SetupState::globals},
    {"convert", setup::convert_command, &SetupState::globals},
    {"flatten", setup::flatten_command, &SetupState::flatten},
    {"format", setup::format_command, nullptr},
    {"cells", setup::cells_command, nullptr},
    {"ports", setup::ports_command, nullptr},
}};

constexpr std::string_view kScriptVar = "::hefesto_setup_command";
constexpr std::string_view kCodeVar = "::hefesto_setup_code";
constexpr std::string_view kResultVar = "::hefesto_setup_result";
constexpr std::string_view kEvaluate =
    "::set ::hefesto_setup_code [::catch {::namespace eval ::netgen $::hefesto_setup_command} "
    "::hefesto_setup_result]";

cmd::CommandResult cancelled_command() {
  return setup::command_error("LVS run cancelled");
}

void define(script::Interpreter& interp, std::string_view name, const script::NativeCommand& command) {
  const std::string global = "::" + std::string{name};
  interp.define_command("::netgen::" + std::string{name}, command);
  if (!interp.has_command(global)) {
    interp.define_command(global, command);
  }
}

/// `puts` inside the netgen namespace: stdout and stderr lines go to the
/// setup log; a write to another channel is Tcl's own `puts`.
cmd::CommandResult setup_puts(Runtime& runtime, script::Interpreter& interp, const cmd::Invocation& inv) {
  Words args = inv.args;
  const bool no_newline = !args.empty() && args.front() == "-nonewline";
  if (no_newline) {
    args.erase(args.begin());
  }
  if (args.size() == 1 || (args.size() == 2 && (args[0] == "stdout" || args[0] == "stderr"))) {
    if (args.size() == 2 && args[0] == "stderr") {
      runtime.print_error(args.back());
    } else {
      runtime.print(args.back());
    }
    return cmd::CommandResult::success();
  }
  std::string script = "::puts";
  for (const auto& arg : inv.args) {
    script += " " + pcell::tcl_list_element(arg, false);
  }
  return interp.evaluate_in_frame(script);
}

void install_commands(Runtime& runtime, script::Interpreter& interp) {
  for (const auto& entry : kCommands) {
    define(interp, entry.name, [&runtime, entry](const cmd::Invocation& inv) {
      if (runtime.cancelled()) {
        return cancelled_command();
      }
      auto result = entry.handler(runtime, inv.args);
      if (result.ok() && entry.record != nullptr) {
        Words words{std::string{entry.name}};
        words.insert(words.end(), inv.args.begin(), inv.args.end());
        (runtime.state().*entry.record).push_back(std::move(words));
      }
      return result;
    });
  }
  for (const std::string_view name : setup::diagnostic_command_names()) {
    define(interp, name, [&runtime, name](const cmd::Invocation& inv) {
      return runtime.cancelled() ? cancelled_command() : setup::diagnostic_command(runtime, name, inv.args);
    });
  }
  interp.define_command("::netgen::puts", [&runtime, &interp](const cmd::Invocation& inv) {
    return setup_puts(runtime, interp, inv);
  });
}

std::string strip_newlines(std::string text) {
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
    text.pop_back();
  }
  return text;
}

bool blank(std::string_view text) {
  return text.find_first_not_of(" \t\r\n") == std::string_view::npos;
}

/// One evaluation of a setup file in its interpreter.
class FileRun {
 public:
  FileRun(Runtime& runtime, script::Interpreter& interp, std::string name)
      : runtime_(runtime), interp_(interp), name_(std::move(name)) {}

  /// Evaluates `text`; the error is a fatal stop (budget or cancel).
  diag::Status run(std::string_view text) {
    std::string pending;
    std::size_t start = 0;
    while (start < text.size()) {
      const std::size_t end = text.find('\n', start);
      const std::string_view line = text.substr(start, end == std::string_view::npos ? end : end - start);
      start = end == std::string_view::npos ? text.size() : end + 1;
      ++line_;
      pending.append(line);
      pending.push_back('\n');
      if (!complete(pending)) {
        continue;
      }
      if (auto status = evaluate(pending); !status.ok()) {
        return status;
      }
      pending.clear();
    }
    if (!pending.empty()) {
      if (auto status = evaluate(pending); !status.ok()) {  // reports the incomplete command
        return status;
      }
    }
    if (runtime_.state().errors > 0) {
      runtime_.print("Warning:  There were errors reading the setup file");
    }
    return diag::Done{};
  }

 private:
  bool complete(const std::string& pending) {
    const auto result = interp_.evaluate("::info complete " + pcell::tcl_list_element(pending));
    return !result.ok() || result.value.to_string() == "1";
  }

  diag::Diagnostic fatal(std::string_view code, std::string text) const {
    return diag::make(code, std::move(text), diag::SourceLocation{name_, static_cast<std::uint32_t>(line_)});
  }

  diag::Status evaluate(const std::string& pending) {
    if (runtime_.cancelled()) {
      return fatal(setup_codes::kCancelled, "Setup evaluation cancelled");
    }
    if (blank(pending)) {
      return diag::Done{};
    }
    interp_.set_var(kScriptVar, cmd::Value{pending});
    const auto result = interp_.evaluate(kEvaluate);
    if (auto stop = stopped(result)) {
      return *stop;
    }
    if (!result.ok()) {
      report(result.diagnostics.empty() ? std::string{} : result.diagnostics.front().text);
    } else if (const auto code = interp_.get_var(kCodeVar); code && code->to_string() != "0") {
      const auto message = interp_.get_var(kResultVar);
      report(message ? message->to_string() : std::string{});
    }
    return budget_left();
  }

  /// A limit or an interrupt ends the evaluation; `catch` cannot absorb it.
  std::optional<diag::Diagnostic> stopped(const cmd::CommandResult& result) {
    if (result.ok() || result.diagnostics.empty()) {
      return std::nullopt;
    }
    const std::string& code = result.diagnostics.front().code;
    if (runtime_.timed_out()) {
      return fatal(setup_codes::kStepBudget, "Setup evaluation stopped: the time budget of " +
                                                 std::to_string(runtime_.context().time_budget.count()) +
                                                 " ms is exhausted");
    }
    if (code == "SCRIPT-TCL-002" || runtime_.context().cancel.requested()) {
      return fatal(setup_codes::kCancelled, "Setup evaluation cancelled");
    }
    if (code == "SCRIPT-TCL-003") {
      return over_budget();
    }
    return std::nullopt;
  }

  diag::Diagnostic over_budget() const {
    return fatal(setup_codes::kStepBudget, "Setup evaluation stopped: the step budget of " +
                                               std::to_string(runtime_.context().step_budget) +
                                               " commands is exhausted");
  }

  diag::Status budget_left() {
    const auto count = interp_.evaluate("::info cmdcount");
    if (count.ok()) {
      runtime_.state().steps = static_cast<std::uint64_t>(setup::parse_integer(count.value.to_string()).value_or(0));
    }
    if (runtime_.state().steps > runtime_.context().step_budget) {
      return over_budget();
    }
    return diag::Done{};
  }

  void report(std::string message) {
    message = strip_newlines(std::move(message));
    SetupState& state = runtime_.state();
    ++state.errors;
    const std::string line = "Error " + name_ + ":" + std::to_string(line_) + " (ignoring), " + message;
    runtime_.print_error(line);
    state.diagnostics.push_back(
        diag::make(setup_codes::kLineIgnored, line, diag::SourceLocation{name_, static_cast<std::uint32_t>(line_)}));
  }

  Runtime& runtime_;
  script::Interpreter& interp_;
  std::string name_;
  std::size_t line_ = 0;
};

diag::Status evaluate_text(Runtime& runtime, const std::string& name, std::string_view text) {
  SetupContext& context = runtime.context();
  script::SandboxPolicy policy = context.sandbox;
  if (policy.limits.command_limit == 0 || policy.limits.command_limit > context.step_budget) {
    policy.limits.command_limit = context.step_budget;
  }
  policy.stdin_readable = false;  // a read of stdin blocks out of reach of the budget (S8-04)
  editor::EditorState editor;
  const auto start = std::chrono::steady_clock::now();
  editor.script.progress.handler = [&runtime, start](const script::ScriptProgress&) {
    if (!runtime.cancelled() && std::chrono::steady_clock::now() - start > runtime.context().time_budget) {
      runtime.time_out();
    }
  };
  auto interp = script::create_project_interpreter(editor, policy);
  runtime.set_interpreter(interp.get());
  install_commands(runtime, *interp);
  FileRun run(runtime, *interp, name);
  auto status = run.run(text);
  runtime.set_interpreter(nullptr);
  interp.reset();  // before `editor`, whose events it posts to
  return status;
}

/// The setup file's text; nullopt for a missing `nosetup` name.
diag::Outcome<std::optional<std::string>> load(const Source& setup, SetupContext& context, Runtime& runtime) {
  if (setup.bytes) {
    if (setup.bytes->size() > kMaxSetupFileBytes) {
      return diag::make(setup_codes::kTooLarge, "Setup file " + setup.name + " is larger than 16 MiB");
    }
    return std::optional<std::string>{*setup.bytes};
  }
  const platform::Path path{setup.name};
  if (context.files == nullptr || !context.files->exists(path)) {
    if (setup.name.find("nosetup") != std::string::npos) {
      runtime.print("No setup file specified.  Continuing without a setup.");
      return std::optional<std::string>{};
    }
    return diag::make(setup_codes::kMissingFile, "Error:  Setup file " + setup.name + " does not exist.");
  }
  auto bytes = context.files->read_at_most(path, kMaxSetupFileBytes + 1);
  if (!bytes) {
    return diag::make(setup_codes::kUnreadable, "Error:  Cannot read the setup file " + setup.name);
  }
  if (bytes.value().size() > kMaxSetupFileBytes) {
    return diag::make(setup_codes::kTooLarge, "Setup file " + setup.name + " is larger than 16 MiB");
  }
  return std::optional<std::string>{std::move(bytes).value()};
}

}  // namespace

void apply_default_setup(SetupContext& context, SetupState& state) {
  Runtime runtime(context, state);
  setup::apply_permute_groups(runtime, setup::PermuteGroups{true, true, true, false});
  setup::apply_property_default(runtime);
}

diag::Outcome<SetupState> evaluate_setup(const Source& setup, SetupContext& context) {
  if (context.first == nullptr || context.second == nullptr || context.step_budget == 0 ||
      context.step_budget > 2 * kSetupStepBudget || context.time_budget.count() <= 0 ||
      context.time_budget > 2 * kSetupTimeBudget) {
    return diag::make(setup_codes::kBadContext,
                      "Setup evaluation needs both circuits, and step and time budgets from 1 to twice the default");
  }
  SetupState state;
  Runtime runtime(context, state);
  if (setup.name.empty() && !setup.bytes) {
    runtime.print("");
    runtime.print("No setup file specified.  Using trivial default setup.");
    runtime.print("");
    apply_default_setup(context, state);
    return state;
  }
  auto text = load(setup, context, runtime);
  if (!text) {
    return text.error();
  }
  if (!text.value()) {
    return state;
  }
  runtime.print("");
  runtime.print("Reading setup file " + setup.name);
  runtime.print("");
  state.evaluated = true;
  if (auto status = evaluate_text(runtime, setup.name, *text.value()); !status.ok()) {
    return status.error();
  }
  return state;
}

}  // namespace hefesto::lvs
