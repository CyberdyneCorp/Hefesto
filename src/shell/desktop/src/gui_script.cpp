// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/gui_script.hpp"

#include <algorithm>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::shell {

namespace {

constexpr std::string_view kNoInterpreterCode = "SCRIPT-TCL-001";
constexpr std::string_view kInputBlockedCode = "SCRIPT-TCL-004";

int input_blocks = 0;  // ScriptInputBlock nesting (UI thread only)

/// Collects what the Dispatcher reports and runs while a script evaluates.
class Trace final : public diag::DiagnosticSink, public cmd::CommandObserver {
 public:
  Trace(cmd::Dispatcher& dispatcher, GuiScriptRun& run) : dispatcher_(dispatcher), run_(run) {
    dispatcher_.add_sink(*this);
    dispatcher_.add_observer(*this);
  }
  ~Trace() override {
    dispatcher_.remove_observer(*this);
    dispatcher_.remove_sink(*this);
  }
  Trace(const Trace&) = delete;
  Trace& operator=(const Trace&) = delete;

  void emit(const diag::Diagnostic& d) override { run_.reported.push_back(d); }
  void on_command(const cmd::Invocation& /*invocation*/, const cmd::CommandResult& result) override {
    if (!result.ok()) run_.command_failed = true;
  }

 private:
  cmd::Dispatcher& dispatcher_;
  GuiScriptRun& run_;
};

bool already_reported(const GuiScriptRun& run, const diag::Diagnostic& error) {
  return std::any_of(run.reported.begin(), run.reported.end(),
                     [&error](const diag::Diagnostic& d) { return d.text == error.text; });
}

}  // namespace

ScriptInputBlock::ScriptInputBlock() noexcept {
  ++input_blocks;
}

ScriptInputBlock::~ScriptInputBlock() {
  --input_blocks;
}

bool ScriptInputBlock::active() noexcept {
  return input_blocks > 0;
}

cmd::CommandResult run_gui_script(editor::EditorState& editor, cmd::Dispatcher& /*dispatcher*/,
                                  std::string_view script, cmd::Origin origin) {
  if (ScriptInputBlock::active()) {
    return cmd::CommandResult::failure(diag::make(
        kInputBlockedCode, "a script is running: the layout takes no input until it ends (Interrupt stops it)"));
  }
  if (editor.script.interp == nullptr) {
    return cmd::CommandResult::failure(
        diag::make(kNoInterpreterCode, "no project is open: there is no script interpreter to run the command"));
  }
  return editor.script.interp->evaluate(script, origin);
}

GuiScriptRun run_gui_script_traced(editor::EditorState& editor, cmd::Dispatcher& dispatcher, std::string_view script,
                                   cmd::Origin origin) {
  GuiScriptRun run;
  const bool quitting = dispatcher.session().quit_requested;
  {
    const Trace trace(dispatcher, run);
    run.result = run_gui_script(editor, dispatcher, script, origin);
  }
  if (!quitting && dispatcher.session().quit_requested) {
    run.result = cmd::CommandResult::success();  // `quit` unwound the evaluation: not an error
  }
  if (run.result.ok()) return run;
  for (const diag::Diagnostic& d : run.result.diagnostics) {
    if (already_reported(run, d)) continue;
    dispatcher.report(d);
    run.reported.push_back(d);
  }
  return run;
}

std::string gui_result_text(const cmd::CommandResult& result) {
  if (!result.ok() && !result.diagnostics.empty()) return result.diagnostics.front().text;
  return result.value.to_string();
}

}  // namespace hefesto::shell
