// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <algorithm>

#include "hefesto/cmd/script.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/sink.hpp"

namespace hefesto::cmd {

namespace {

Invocation to_invocation(const ParsedCommand& command, Origin origin, std::string_view source_name) {
  Invocation invocation;
  invocation.name = command.words.front();
  invocation.args.assign(command.words.begin() + 1, command.words.end());
  invocation.origin = origin;
  if (!source_name.empty()) {
    invocation.source = diag::SourceLocation{std::string{source_name}, command.line};
  }
  return invocation;
}

bool is_dispatch_failure(const CommandResult& result) {
  return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                     [](const diag::Diagnostic& d) { return d.code == diag::codes::kCmdDispatch; });
}

// The error a failed command raises: its first error diagnostic, or a
// generic one when the handler failed without saying why.
diag::Diagnostic raised_error(const CommandResult& result, const Invocation& invocation) {
  if (const auto* first = diag::first_error(result.diagnostics)) {
    return *first;
  }
  return diag::make(diag::codes::kCmdInternal, "command '" + invocation.name + "' failed without a diagnostic");
}

}  // namespace

ScriptRun run_script(Dispatcher& dispatcher, std::string_view text, Origin origin, std::string_view source_name) {
  ScriptRun run;
  const ParsedScript parsed = parse_script(text, source_name);
  if (parsed.error) {
    run.aborted = true;
    run.error = parsed.error;
    dispatcher.report(*parsed.error);
    return run;
  }
  for (const auto& command : parsed.commands) {
    const Invocation invocation = to_invocation(command, origin, source_name);
    const CommandResult result = dispatcher.execute(invocation);
    ++run.commands_run;
    if (!result.ok()) {
      ++run.failures;
      if (dispatcher.session().strict || result.raises || is_dispatch_failure(result)) {
        run.aborted = true;
        run.error = raised_error(result, invocation);
        return run;
      }
    }
    if (dispatcher.session().quit_requested) {
      run.quit = true;
      return run;
    }
  }
  return run;
}

}  // namespace hefesto::cmd
