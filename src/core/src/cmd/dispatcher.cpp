// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The dispatcher (see dispatcher.hpp). Owner: DISPATCH.

#include "hefesto/cmd/dispatcher.hpp"

#include <exception>
#include <utility>

#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/editor/post_command.hpp"
#include "hefesto/ui/command_log.hpp"
#include "hefesto/ui/diag_codes.hpp"
#include "hefesto/ui/state.hpp"
#include "hefesto/undo/db_recorder.hpp"

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

// The context sink of one command: collects every diagnostic for the result
// and forwards it to the live sinks as it is emitted.
class LiveSink final : public diag::DiagnosticSink {
 public:
  LiveSink(diag::CollectingSink& collected, const std::vector<diag::DiagnosticSink*>& live)
      : collected_(collected), live_(live) {}

  void emit(const diag::Diagnostic& diagnostic) override {
    collected_.emit(diagnostic);
    for (auto* sink : live_) {
      sink->emit(diagnostic);
    }
  }

 private:
  diag::CollectingSink& collected_;
  const std::vector<diag::DiagnosticSink*>& live_;
};

// The failure of a word that names no command, or more than one.
CommandResult lookup_failure(LookupStatus status, const Invocation& invocation) {
  if (status == LookupStatus::ambiguous) {
    return CommandResult::failure(
        diag::make(ui::codes::kAmbiguous, "That command abbreviation is ambiguous.", location_of(invocation)));
  }
  return CommandResult::failure(
      diag::make(diag::codes::kCmdDispatch, "unknown command \"" + invocation.name + "\"", location_of(invocation)));
}

bool interactive(Origin origin) {
  return origin == Origin::gui || origin == Origin::key || origin == Origin::touch;
}

}  // namespace

Dispatcher::Dispatcher(const CommandRegistry& registry, platform::PlatformServices services,
                       settings::ProjectSettings& project, ScriptSession& session)
    : registry_(registry), services_(services), project_(project), session_(session) {}

void Dispatcher::set_editor(editor::EditorState* editor) noexcept {
  editor_ = editor;
  if (editor_ != nullptr && editor_->undo_recorder != nullptr) {
    editor_->undo_recorder->set_compat_source(&project_.compat);
  }
}

CommandResult Dispatcher::execute(const Invocation& invocation) {
  if (invocation.name.empty()) {
    return CommandResult::success();  // a command with no words is a no-op
  }
  const CommandLookup found = registry_.resolve(invocation.name, client_);
  Invocation resolved = invocation;
  CommandResult result;
  if (found.status == LookupStatus::found) {
    resolved.name = found.spec->name;
    result = run_handler(*found.spec, resolved);
  } else {
    result = lookup_failure(found.status, invocation);
  }
  after_command(invocation, resolved, result);
  return result;
}

void Dispatcher::after_command(const Invocation& typed, const Invocation& resolved, CommandResult& result) {
  if (editor_ != nullptr && ui::state(*editor_).interrupt_pending.exchange(false)) {
    result.diagnostics.push_back(diag::make(ui::codes::kInterrupted, "[Interrupted]"));
  }
  attach_location(result.diagnostics, typed);
  if (result.logged_as) {
    result.logged_as->origin = resolved.origin;
    result.logged_as->source = typed.source;
  }
  const Invocation& logged = result.logged_as ? *result.logged_as : resolved;
  if (editor_ != nullptr) {
    ui::log_command(*editor_, services_.files, result.logged_as ? logged : typed);
  }
  publish(logged, result);
  if (editor_ == nullptr) {
    return;
  }
  editor::post_command(*editor_, resolved);
  ui::UiState& ui = ui::state(*editor_);
  if (ui.point_forced && !ui.point_sticky && interactive(resolved.origin) && resolved.name != "setpoint") {
    ui.point_forced = false;
    editor_->command_point.reset();
  }
}

CommandResult Dispatcher::run_handler(const CommandSpec& spec, const Invocation& invocation) {
  diag::CollectingSink collected;
  LiveSink sink{collected, live_sinks_};
  CommandContext context{services_, project_, session_, sink, registry_, editor_};
  std::string saved_client;
  if (editor_ != nullptr) {
    saved_client = std::exchange(ui::state(*editor_).client, client_);
  }
  CommandResult result;
  try {
    result = spec.handler(context, invocation);
  } catch (const std::exception& e) {
    result = CommandResult::failure(diag::make(diag::codes::kCmdInternal,
                                               "command '" + invocation.name + "' failed internally: " + e.what()));
  }
  if (editor_ != nullptr) {
    ui::state(*editor_).client = std::move(saved_client);
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
