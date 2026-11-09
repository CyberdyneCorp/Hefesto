// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/dispatcher.hpp"

#include <exception>
#include <utility>

#include "hefesto/diag/codes.hpp"

namespace hefesto::cmd {

namespace {

diag::Location location_of(const Invocation& invocation) {
  if (invocation.source) {
    return *invocation.source;
  }
  return {};
}

// Diagnostics without a location inherit the script position of the command.
void attach_location(std::vector<diag::Diagnostic>& diagnostics, const Invocation& invocation) {
  if (!invocation.source) {
    return;
  }
  for (auto& d : diagnostics) {
    if (std::holds_alternative<std::monostate>(d.location)) {
      d.location = *invocation.source;
    }
  }
}

}  // namespace

Dispatcher::Dispatcher(const CommandRegistry& registry, platform::PlatformServices services,
                       settings::ProjectSettings& project, ScriptSession& session)
    : registry_(registry), services_(services), project_(project), session_(session) {}

CommandResult Dispatcher::execute(const Invocation& invocation) {
  CommandResult result = run_handler(invocation);
  attach_location(result.diagnostics, invocation);
  publish(invocation, result);
  return result;
}

CommandResult Dispatcher::run_handler(const Invocation& invocation) {
  const CommandSpec* spec = registry_.find(invocation.name);
  if (spec == nullptr) {
    return CommandResult::failure(
        diag::make(diag::codes::kCmdDispatch, "unknown command \"" + invocation.name + "\"", location_of(invocation)));
  }
  diag::CollectingSink collected;
  CommandContext context{services_, project_, session_, collected, registry_, editor_};
  CommandResult result;
  try {
    result = spec->handler(context, invocation);
  } catch (const std::exception& e) {
    result = CommandResult::failure(diag::make(diag::codes::kCmdInternal,
                                               "command '" + invocation.name + "' failed internally: " + e.what()));
  }
  std::vector<diag::Diagnostic> all = collected.take();
  all.insert(all.end(), std::make_move_iterator(result.diagnostics.begin()),
             std::make_move_iterator(result.diagnostics.end()));
  result.diagnostics = std::move(all);
  return result;
}

void Dispatcher::report(const diag::Diagnostic& diagnostic) {
  for (auto* sink : sinks_) {
    sink->emit(diagnostic);
  }
  services_.logger.log(diagnostic);
}

void Dispatcher::publish(const Invocation& invocation, const CommandResult& result) {
  for (const auto& d : result.diagnostics) {
    report(d);
  }
  for (auto* observer : observers_) {
    observer->on_command(invocation, result);
  }
}

}  // namespace hefesto::cmd
