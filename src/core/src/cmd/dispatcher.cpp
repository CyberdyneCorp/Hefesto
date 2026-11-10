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
#include "hefesto/view/color_window.hpp"
#include "hefesto/view/window.hpp"
#include "hefesto/ui/magic_help.hpp"

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

// The failure of a word that names no command, or more than one. A word
// that is a command of another client only (`color` sent to a layout
// window) is a Tcl command in Magic's Tcl build, which dispatches it
// quietly (_tcl_dispatch -> TxTclDispatch(..., quiet = TRUE)): the
// WindSendCommand failure is a Tcl error with an empty result and nothing
// printed. Hefesto keeps a diagnostic for its front ends but makes the empty
// string the command's result.
CommandResult lookup_failure(LookupStatus status, const Invocation& invocation, const CommandRegistry& registry,
                             const std::string& client) {
  if (status == LookupStatus::ambiguous) {
    return CommandResult::failure(
        diag::make(ui::codes::kAmbiguous, "That command abbreviation is ambiguous.", location_of(invocation)));
  }
  if (registry.find(strip_namespace(invocation.name)) == nullptr) {
    return CommandResult::failure(diag::make(diag::codes::kCmdDispatch, invalid_command_name(invocation.name),
                                             location_of(invocation)));
  }
  CommandResult result = CommandResult::failure(
      diag::make(diag::codes::kCmdDispatch, "\"" + invocation.name + "\" is not a command of the " + client + " window",
                 location_of(invocation)));
  result.value = std::string{};
  return result;
}

bool interactive(Origin origin) {
  return origin == Origin::gui || origin == Origin::key || origin == Origin::touch;
}

// magic-compat: Magic's Lookup() finds `id` and `get` ambiguous (their
// table entries end the name with a tab), and its quiet Tcl dispatch turns
// that into an empty error result with nothing printed.
CommandResult quiet_ambiguity(const Invocation& invocation) {
  CommandResult result = CommandResult::failure(
      diag::make(ui::codes::kAmbiguous, "That command abbreviation is ambiguous.", location_of(invocation)));
  result.value = std::string{};
  return result;
}

}  // namespace

std::string invalid_command_name(std::string_view word) {
  return "invalid command name \"" + std::string{word} + "\"";
}

Dispatcher::Dispatcher(const CommandRegistry& registry, platform::PlatformServices services,
                       settings::ProjectSettings& project, ScriptSession& session)
    : registry_(registry), services_(services), project_(project), session_(session) {}

void Dispatcher::set_editor(editor::EditorState* editor) noexcept {
  editor_ = editor;
  if (editor_ != nullptr && editor_->undo_recorder != nullptr) {
    editor_->undo_recorder->set_compat_source(&project_.compat);
  }
}

std::string Dispatcher::target_client() const {
  // "Command dispatch across global and client tables" steps 1-2: a default
  // point on a window's chrome (its frame, caption or scroll bars) sends the
  // command to the `*window` client (windSend.c: rc = windClient); a command
  // point in a color window (`setpoint x y ID`) sends it to the `color`
  // client; otherwise the front end's client applies.
  if (chrome_point_) {
    return std::string{kWindowClient};
  }
  if (editor_ != nullptr && editor_->command_point) {
    const view::WindowId id = editor_->command_point->window;
    if (editor_->windows.find(id) == nullptr && editor_->color_map.find(id) != nullptr) {
      return std::string{kColorClient};
    }
  }
  return client_;
}

bool Dispatcher::magic_ambiguous(const std::string& word, const std::string& client) const {
  if (!project_.compat.magic_compat) {
    return false;
  }
  const std::string_view name = strip_namespace(word);
  return ui::magic_lookup_ambiguous(kWindowClient, name) || ui::magic_lookup_ambiguous(client, name);
}

bool Dispatcher::place_default_point() {
  if (!default_point_ || editor_ == nullptr || editor_->command_point || ui::state(*editor_).point_forced) {
    return false;
  }
  const db::Point px = *default_point_;
  const auto at = view::window_at(*editor_, px.x, px.y);
  if (!at) {
    return false;  // no window there: Magic's windowless layout client
  }
  view::ViewMapping mapping;
  if (at->color) {
    mapping = view::color_window_mapping(*editor_, *editor_->color_map.find(at->id));
  } else {
    mapping = editor_->windows.find(at->id)->mapping();
  }
  editor_->command_point = PointerState{at->id, mapping.to_layout(px.x, px.y),
                                        mapping.screen_to_surface({px.x, px.y, px.x, px.y})};
  ui::state(*editor_).pointer_pixel = px;
  chrome_point_ = !at->inside;
  return true;
}

void Dispatcher::drop_default_point(const db::Point& saved_pixel) {
  chrome_point_ = false;
  ui::UiState& ui = ui::state(*editor_);
  if (!ui.point_forced) {  // the command did not set a point of its own (`setpoint`)
    editor_->command_point.reset();
    ui.pointer_pixel = saved_pixel;
  }
}

void Dispatcher::print_command(const Invocation& invocation) {
  // `*winddebug` (windPrintCommands): WindSendCommand prints each command
  // it is sent with its point (windows/windDebug.c:windPrintCommand).
  if (editor_ == nullptr || !editor_->windows.print_commands()) {
    return;
  }
  std::string text = "Sending command:\n '" + invocation.name + "'";
  for (const auto& arg : invocation.args) {
    text += " '" + arg + "'";
  }
  const db::Point px = ui::state(*editor_).pointer_pixel;
  text += " at (" + std::to_string(px.x) + ", " + std::to_string(px.y) + ")";
  report(diag::make(ui::codes::kReport, std::move(text)));
}

CommandResult Dispatcher::execute(const Invocation& invocation) {
  if (invocation.name.empty()) {
    return CommandResult::success();  // a command with no words is a no-op
  }
  const db::Point saved_pixel = editor_ != nullptr ? ui::state(*editor_).pointer_pixel : db::Point{};
  const bool defaulted = place_default_point();
  const std::string client = target_client();
  const CommandLookup found = registry_.resolve(invocation.name, client, true, name_lookup(invocation.origin));
  if (found.status == LookupStatus::found || registry_.find(strip_namespace(invocation.name)) != nullptr) {
    print_command(invocation);
  }
  Invocation resolved = invocation;
  CommandResult result;
  if (found.status == LookupStatus::found && magic_ambiguous(invocation.name, client)) {
    result = quiet_ambiguity(invocation);
  } else if (found.status == LookupStatus::found) {
    resolved.name = found.spec->name;
    result = run_handler(*found.spec, resolved);
  } else {
    result = lookup_failure(found.status, invocation, registry_, client);
  }
  after_command(invocation, resolved, result);
  if (defaulted && editor_ != nullptr) {
    drop_default_point(saved_pixel);
  }
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
    saved_client = std::exchange(ui::state(*editor_).client, target_client());
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
