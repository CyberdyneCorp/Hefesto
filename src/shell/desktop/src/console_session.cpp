// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Tcl console bound to the project's interpreter (see console_session.hpp).

#include "hefesto/shell/console_session.hpp"

#include <utility>
#include <vector>

#include "hefesto/diag/format.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/binding/tags.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace hefesto::shell {

namespace {

constexpr std::string_view kInterruptedCode = "SCRIPT-TCL-002";

}  // namespace

/// Collects the engine's diagnostics while the console evaluates.
class TclConsoleSession::Capture final : public diag::DiagnosticSink {
 public:
  explicit Capture(TclConsoleSession& owner) : owner_(owner) { owner_.dispatcher_.add_sink(*this); }
  ~Capture() override { owner_.dispatcher_.remove_sink(*this); }
  Capture(const Capture&) = delete;
  Capture& operator=(const Capture&) = delete;

  void emit(const diag::Diagnostic& d) override {
    const bool problem = d.severity == diag::Severity::error || d.severity == diag::Severity::warning;
    if (d.text.empty()) return;  // Magic's quiet dispatch failures print nothing
    owner_.write(problem ? ConsoleLineKind::err : ConsoleLineKind::out,
                 problem ? d.text + " (" + d.code + ")" : d.text, true);
  }

 private:
  TclConsoleSession& owner_;
};

std::string script_where(const script::ScriptLocation& location) {
  if (location.line == 0) return {};
  const std::string line = std::to_string(location.line);
  if (location.file.empty()) return "line " + line;
  return platform::Path{location.file}.filename().string() + ":" + line;
}

TclConsoleSession::TclConsoleSession(editor::EditorState& editor, cmd::Dispatcher& dispatcher)
    : editor_(editor), dispatcher_(dispatcher) {
  editor_.script.progress.handler = [this](const script::ScriptProgress& p) { on_progress(p); };
  editor_.script.progress.interval = pump_interval_;
}

TclConsoleSession::~TclConsoleSession() {
  editor_.script.progress.handler = {};
}

script::Interpreter* TclConsoleSession::interpreter() const noexcept {
  return editor_.script.interp.get();
}

bool TclConsoleSession::attach(script::Interpreter& interp) {
  const auto renamed = interp.evaluate("namespace eval ::hefesto {}; rename ::puts ::hefesto::tcl_puts");
  if (!renamed.ok()) return false;
  interp.define_command("::puts", [this](const cmd::Invocation& inv) { return console_puts(inv); });
  attached_ = &interp;
  return true;
}

void TclConsoleSession::set_pump(std::function<void()> pump, std::chrono::milliseconds interval) {
  pump_ = std::move(pump);
  pump_interval_ = interval;
  editor_.script.progress.interval = interval;
}

void TclConsoleSession::on_command() {
  if (depth_ == 0 && !progress_) return;
  if (std::chrono::steady_clock::now() - last_pump_ < pump_interval_) return;
  pump_now();
}

void TclConsoleSession::on_progress(const script::ScriptProgress& progress) {
  if (progress.done) {
    last_where_ = script_where(progress.location);
    progress_.reset();
    return;
  }
  progress_ = progress;
  pump_now();  // the interpreter already spaces its reports by the interval
}

void TclConsoleSession::pump_now() {
  if (!pump_ || pumping_) return;
  last_pump_ = std::chrono::steady_clock::now();
  pumping_ = true;  // a command the frame runs must not pump again
  pump_();
  pumping_ = false;
}

bool TclConsoleSession::complete(std::string_view text) {
  script::Interpreter* interp = interpreter();
  if (interp == nullptr) return console_input_status(text).complete;
  const cmd::CommandResult r = interp->evaluate("info complete " + script::quote_list_element(text));
  return !r.ok() || r.value.to_string() != "0";
}

std::vector<std::string> TclConsoleSession::names(TclWordKind kind, std::string_view glob) {
  script::Interpreter* interp = interpreter();
  if (interp == nullptr || kind == TclWordKind::none || depth_ > 0 || progress_) return {};
  const char* query = kind == TclWordKind::variable ? "info vars " : "info commands ";
  const cmd::CommandResult r = interp->evaluate(query + script::quote_list_element(glob));
  if (!r.ok()) return {};
  auto names = pcell::split_tcl_list(r.value.to_string());
  return names.ok() ? std::move(names).value() : std::vector<std::string>{};
}

ConsoleEvaluation TclConsoleSession::evaluate(std::string_view command) {
  ConsoleEvaluation out;
  script::Interpreter* interp = interpreter();
  if (interp == nullptr) {
    out.result = cmd::CommandResult::failure(
        diag::make("SCRIPT-TCL-001", "no project is open: the console has no interpreter"));
    return out;
  }
  ++depth_;
  last_pump_ = std::chrono::steady_clock::now();
  last_where_.clear();
  const undo::StepMark mark = editor_.undo.mark();
  {
    const Capture capture(*this);
    out.result = interp->evaluate(command, cmd::Origin::gui);
  }
  --depth_;
  if (!pending_out_.empty()) write(pending_kind_, "", true);  // end a `puts -nonewline` line
  for (const diag::Diagnostic& d : out.result.diagnostics) {
    if (d.code == kInterruptedCode) out.interrupted = true;
  }
  if (out.interrupted) {
    // The edits made before the interrupt stay, as one step named after the
    // command (board M4-Console tile 8).
    out.edits = editor_.undo.merge_since(mark, undo::script_label(command));
    out.where = last_where_;
  }
  return out;
}

void TclConsoleSession::interrupt() noexcept {
  if (script::Interpreter* interp = interpreter()) interp->request_interrupt();
}

std::size_t TclConsoleSession::undo_steps() const {
  return editor_.undo.step_count();
}

CommandOutcome TclConsoleSession::outcome_of(const ConsoleEvaluation& evaluation) {
  CommandOutcome outcome;
  const cmd::CommandResult& r = evaluation.result;
  for (const diag::Diagnostic& d : r.diagnostics) outcome.diagnostics.push_back(diag::format_text(d));
  if (r.ok()) {
    outcome.message = r.value.to_string();
    outcome.output = outcome.message;
    if (outcome.message.empty()) outcome.message = "ok";
    return outcome;
  }
  outcome.status = OutcomeStatus::failed;
  outcome.message = "failed";
  for (const diag::Diagnostic& d : r.diagnostics) {
    if (d.severity == diag::Severity::error) {
      outcome.message = d.text.empty() ? "failed" : d.text;
      break;
    }
  }
  return outcome;
}

// `puts ?-nonewline? ?channelId? string` (Tcl's argument rules).
cmd::CommandResult TclConsoleSession::console_puts(const cmd::Invocation& inv) {
  const auto& a = inv.args;
  const bool nonewline = (a.size() == 2 || a.size() == 3) && a.front() == "-nonewline";
  const std::size_t first = nonewline ? 1 : 0;
  if (a.size() - first == 0 || a.size() - first > 2) {
    return cmd::CommandResult::failure(
        diag::make("SCRIPT-TCL-001", R"(wrong # args: should be "puts ?-nonewline? ?channelId? string")"));
  }
  const std::string channel = a.size() - first == 2 ? a[first] : "stdout";
  if ((channel != "stdout" && channel != "stderr") || transcript_ == nullptr) {
    std::string script = "::hefesto::tcl_puts";
    for (const auto& word : a) script += ' ' + script::quote_list_element(word);
    return attached_ != nullptr ? attached_->evaluate(script) : cmd::CommandResult::success();
  }
  write(channel == "stdout" ? ConsoleLineKind::out : ConsoleLineKind::err, a.back(), !nonewline);
  return cmd::CommandResult::success();
}

void TclConsoleSession::write(ConsoleLineKind kind, std::string_view text, bool newline) {
  if (transcript_ == nullptr) return;
  if (!pending_out_.empty() && pending_kind_ != kind) {  // another stream: end the partial line
    transcript_->add(pending_kind_, pending_out_);
    pending_out_.clear();
  }
  pending_kind_ = kind;
  pending_out_ += text;
  if (!newline) {
    const std::size_t last = pending_out_.rfind('\n');
    if (last == std::string::npos) return;
    transcript_->add(kind, pending_out_.substr(0, last));
    pending_out_.erase(0, last + 1);
    return;
  }
  transcript_->add(kind, pending_out_);
  pending_out_.clear();
}

}  // namespace hefesto::shell
