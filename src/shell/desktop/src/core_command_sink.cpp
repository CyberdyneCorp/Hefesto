// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/core_command_sink.hpp"

#include <string>
#include <utility>

#include "hefesto/cmd/builtins.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/version.hpp"

namespace hefesto::shell {

namespace {

constexpr std::string_view kSourceName = "command bar";

void append_diagnostics(CommandOutcome& outcome, const std::vector<diag::Diagnostic>& diagnostics) {
  for (const diag::Diagnostic& d : diagnostics) {
    outcome.diagnostics.push_back(diag::format_text(d));
  }
}

CommandOutcome failure(const diag::Diagnostic& error) {
  CommandOutcome outcome{OutcomeStatus::failed, error.text, {}};
  append_diagnostics(outcome, {error});
  return outcome;
}

std::string first_error_text(const cmd::CommandResult& result) {
  for (const diag::Diagnostic& d : result.diagnostics) {
    if (d.severity == diag::Severity::error) {
      return d.text;
    }
  }
  return "failed";
}

platform::SessionLog open_session_log(platform::PortableServices& services, const std::optional<platform::Path>& dir) {
  if (!dir) {
    return {};
  }
  return platform::install_session_log(services, *dir, "hefesto", diag::codes::kPlatformLog);
}

}  // namespace

CoreSession::CoreSession(platform::PortableOptions options, std::optional<platform::Path> log_dir)
    : services_(std::move(options)),
      log_(open_session_log(services_, log_dir)),
      dispatcher_(registry_, services_.view(), project_, session_) {
  cmd::register_builtins(registry_);
  dispatcher_.add_observer(command_log_);
  if (log_.logger != nullptr) {
    log_.logger->write(diag::Severity::info, "session start hefesto " + std::string{version()});
  }
}

CoreSession::~CoreSession() {
  if (log_.logger != nullptr) {
    log_.logger->write(diag::Severity::info, "session end");
  }
}

CommandOutcome CoreCommandSink::submit(std::string_view line) {
  const cmd::ParsedScript parsed = cmd::parse_script(line, kSourceName);
  if (parsed.error) {
    dispatcher_.report(*parsed.error);
    return failure(*parsed.error);
  }
  CommandOutcome outcome;
  for (const cmd::ParsedCommand& command : parsed.commands) {
    cmd::Invocation invocation{command.words.front(),
                               {command.words.begin() + 1, command.words.end()},
                               cmd::Origin::gui,
                               std::nullopt};
    const cmd::CommandResult result = dispatcher_.execute(invocation);
    append_diagnostics(outcome, result.diagnostics);
    if (!result.ok()) {
      outcome.status = OutcomeStatus::failed;
      outcome.message = first_error_text(result);
      break;
    }
    outcome.message = result.value.to_string();
  }
  return outcome;
}

}  // namespace hefesto::shell
