// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The native commands of the binding: `::magic::<name>` (_tcl_dispatch and
// TxTclDispatch's quiet mode), window paths (_tk_dispatch) and the tag
// callback (TagCallback). Ported from Magic 8.3.694 (fc5daa9e)
// tcltk/tclmagic.c:107-294,390-600 and textio/txCommands.c:1491-1575,
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <utility>

#include "binding_internal.hpp"
#include "hefesto/cmd/pointer.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/binding/keyword_rule.hpp"
#include "hefesto/script/binding/tags.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"
#include "hefesto/ui/diag_codes.hpp"
#include "hefesto/ui/magic_help.hpp"
#include "hefesto/ui/state.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::script::detail {

namespace {

constexpr std::string_view kTclError = "SCRIPT-TCL-001";

/// Magic's window-pixel reference point of a window-path command with no
/// point recorded in that window (_tk_dispatch).
constexpr std::int32_t kWindowPoint = 20;

bool is_dispatch_failure(const cmd::CommandResult& result) {
  return std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [](const diag::Diagnostic& d) {
    return d.code == diag::codes::kCmdDispatch || d.code == ui::codes::kAmbiguous;
  });
}

// WindSendCommand would accept `name` sent to the dispatcher's client: an
// exact command of the `*window` or client table (or a Tcl-side procedure),
// not one of Lookup()'s ambiguous tab-terminated aliases under magic-compat
// (`get`, `id`; Q-SCRIPT-14).
bool dispatchable(const Binding& b, const std::string& name) {
  const std::string client = b.dispatcher.target_client();  // the color window's under `setpoint x y ID`
  const cmd::CommandLookup found = b.registry.resolve(name, client, true, cmd::NameLookup::exact);
  if (found.status != cmd::LookupStatus::found) {
    return false;
  }
  return !b.magic_compat() ||
         !(ui::magic_lookup_ambiguous(cmd::kWindowClient, name) || ui::magic_lookup_ambiguous(client, name));
}

// The first error diagnostic's text (the Tcl error message).
std::string error_message(const cmd::CommandResult& result) {
  if (const auto* first = diag::first_error(result.diagnostics)) {
    return first->text;
  }
  return result.diagnostics.empty() ? std::string{} : result.diagnostics.front().text;
}

// A procedure's Tcl error message: the last error diagnostic, the one it
// raised (`error ...`); the earlier ones are messages it printed on the way.
std::string procedure_error_message(const cmd::CommandResult& result) {
  const auto last = std::find_if(result.diagnostics.rbegin(), result.diagnostics.rend(),
                                 [](const diag::Diagnostic& d) { return d.severity == diag::Severity::error; });
  return last != result.diagnostics.rend() ? last->text : error_message(result);
}

cmd::CommandResult execute(const Binding& b, cmd::Invocation invocation) {
  const cmd::KeywordTerminatorScope keywords{b.magic_compat()};
  const bool quitting = b.dispatcher.session().quit_requested;
  cmd::CommandResult result = b.dispatcher.execute(invocation);
  if (!quitting && b.dispatcher.session().quit_requested) {
    // Magic's `quit` ends the process, so nothing after it runs: unwind the
    // whole evaluation (`catch` cannot absorb it); the caller sees
    // quit_requested and ends the session (app-startup "quit command and
    // exit status").
    b.interp.request_interrupt();
  }
  return result;
}

// %W of a command with no window path: the only layout window's path, else
// unknown (windCheckOnlyWindow).
std::string only_window_path(const editor::EditorState& editor) {
  if (editor.windows.size() != 1) {
    return {};
  }
  const view::LayoutWindow* window = editor.windows.current();
  return window != nullptr ? window->name() : std::string{};
}

// Keeps the command point of a window-path command for one command: the
// recorded point when it is in that window, else window pixel (20, 20).
class WindowPoint {
 public:
  WindowPoint(editor::EditorState* editor, const std::string& path) : editor_(editor) {
    if (editor_ == nullptr) {
      return;
    }
    view::LayoutWindow* window = editor_->windows.find_named(path);
    if (window == nullptr || (editor_->command_point && editor_->command_point->window == window->id())) {
      return;
    }
    saved_point_ = editor_->command_point;
    saved_pixel_ = ui::state(*editor_).pointer_pixel;
    const view::ViewMapping& m = window->mapping();
    const std::int32_t x = m.screen.x0 + kWindowPoint;
    const std::int32_t y = m.screen.y0 + kWindowPoint;
    editor_->command_point = cmd::PointerState{window->id(), m.to_layout(x, y), m.screen_to_surface({x, y, x, y})};
    ui::state(*editor_).pointer_pixel = db::Point{x, y};
    placed_ = true;
  }
  ~WindowPoint() {
    if (placed_) {
      editor_->command_point = saved_point_;
      ui::state(*editor_).pointer_pixel = saved_pixel_;
    }
  }
  WindowPoint(const WindowPoint&) = delete;
  WindowPoint& operator=(const WindowPoint&) = delete;

 private:
  editor::EditorState* editor_;
  std::optional<cmd::PointerState> saved_point_;
  db::Point saved_pixel_;
  bool placed_ = false;
};

// A window-path command word that names no command or several: the
// diagnostic is printed (not quiet) and the window command returns ok.
void report_lookup_failure(const Binding& b, const cmd::CommandLookup& found, const std::string& word) {
  if (found.status == cmd::LookupStatus::ambiguous) {
    b.dispatcher.report(diag::make(ui::codes::kAmbiguous, "That command abbreviation is ambiguous."));
  } else {
    b.dispatcher.report(diag::make(diag::codes::kCmdDispatch, cmd::invalid_command_name(word)));
  }
}

}  // namespace

bool Binding::magic_compat() const {
  const editor::EditorState* state = editor();
  return state == nullptr || state->drc.compat_settings().magic_compat;
}

cmd::Origin command_origin(const Binding& b, const cmd::Invocation& call) {
  if (call.origin == cmd::Origin::script) {
    return cmd::Origin::script;
  }
  // A procedure a script file defined keeps its file in `info frame`, so
  // only commands the user's own typed line runs keep the line's origin.
  return current_script_location(b.interp).file.empty() ? call.origin : cmd::Origin::script;
}

cmd::CommandResult tcl_error(std::string message) {
  return cmd::CommandResult::failure(diag::make(kTclError, std::move(message)));
}

cmd::CommandResult dispatch_command(const Binding& b, const std::string& name, const cmd::Invocation& call) {
  if (!dispatchable(b, name)) {
    return tcl_error({});  // quiet dispatch: an error with an empty result
  }
  cmd::Invocation invocation{name, call.args, command_origin(b, call), call.source};
  cmd::CommandResult result = execute(b, std::move(invocation));
  if (is_dispatch_failure(result)) {
    return tcl_error({});
  }
  if (!result.ok() && (result.raises || b.dispatcher.session().strict)) {
    return tcl_error(error_message(result));
  }
  return run_tag(b, call.name, call.args, std::nullopt, cmd::CommandResult::success(std::move(result.value)));
}

cmd::CommandResult dispatch_procedure(const Binding& b, const std::string& name, const cmd::Invocation& call) {
  cmd::CommandResult result = execute(b, cmd::Invocation{name, call.args, command_origin(b, call), call.source});
  if (!result.ok()) {
    return tcl_error(procedure_error_message(result));
  }
  return cmd::CommandResult::success(std::move(result.value));
}

cmd::CommandResult dispatch_window(const Binding& b, const std::string& path, const cmd::Invocation& call) {
  if (call.args.empty()) {
    return cmd::CommandResult::success();
  }
  const std::string& word = call.args.front();
  const std::vector<std::string> args(call.args.begin() + 1, call.args.end());
  // Window-path commands accept abbreviations (WindSendCommand's Lookup).
  const cmd::CommandLookup found = b.registry.resolve(word, b.dispatcher.client(), true, cmd::NameLookup::abbreviated);
  cmd::Value value;
  if (found.status != cmd::LookupStatus::found) {
    report_lookup_failure(b, found, word);
  } else {
    const WindowPoint point{b.editor(), path};
    cmd::CommandResult result =
        execute(b, cmd::Invocation{found.spec->name, args, command_origin(b, call), call.source});
    if (!result.ok() && !is_dispatch_failure(result) && (result.raises || b.dispatcher.session().strict)) {
      return tcl_error(error_message(result));
    }
    value = std::move(result.value);
  }
  return run_tag(b, word, args, path, cmd::CommandResult::success(std::move(value)));
}

cmd::CommandResult run_tag(const Binding& b, std::string_view invoked, const std::vector<std::string>& args,
                           const std::optional<std::string>& window, cmd::CommandResult result) {
  editor::EditorState* editor = b.editor();
  if (editor == nullptr) {
    return result;
  }
  const std::optional<std::string> script = editor->script.tags.get(tag_key(invoked));
  if (!script) {
    return result;
  }
  TagContext context;
  context.window = window ? *window : only_window_path(*editor);
  context.result = result.value.to_string();
  context.args.emplace_back(invoked);
  context.args.insert(context.args.end(), args.begin(), args.end());
  context.command_count = editor->command_count;
  // The tag does not advance the command number (TagCallback).
  const std::uint64_t saved = editor->command_count;
  cmd::CommandResult tag = b.interp.evaluate_in_frame(substitute_tag(*script, context));
  editor->command_count = saved;
  if (!tag.ok()) {
    return tcl_error(error_message(tag));
  }
  if (tag_resets_result(*script)) {
    result.value = std::move(tag.value);
  }
  return result;
}

}  // namespace hefesto::script::detail
