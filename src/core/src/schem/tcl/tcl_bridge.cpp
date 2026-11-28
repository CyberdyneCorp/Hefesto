// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `tcleval(...)` values evaluated in the sandboxed interpreter (capability
// schematic-symbols-and-libraries "tcleval values", steps 1-5; Q-SCHEM-25:
// they always run, in the sandbox). Security review (task 12.1), both modes
// of `xschem-compat`: the sandbox's denials are reported to the run's sink
// (they were dropped with the result), and every value is bounded in time.
//
// Owner: SYMLIB.

#include "hefesto/schem/tcl/tcl_bridge.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "hefesto/cmd/value.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/script/interp.hpp"
#include "schem/lib/run_guards.hpp"

namespace hefesto::schem {

namespace {

// `10 s`, or `250 ms` for a limit that is not whole seconds.
std::string limit_text(std::chrono::milliseconds limit) {
  const auto ms = limit.count();
  return ms % 1000 == 0 ? std::to_string(ms / 1000) + " s" : std::to_string(ms) + " ms";
}

constexpr std::string_view kPrefix = "tcleval(";

bool is_blank(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

}  // namespace

std::string tcleval_body(std::string_view value) {
  std::string body;
  body.reserve(value.size());
  // Step 1: every `\}` becomes `}`.
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '\\' && i + 1 < value.size() && value[i + 1] == '}') continue;
    body.push_back(value[i]);
  }
  // Step 2: the leading `tcleval(` and the last `)` with the blanks after it.
  if (std::string_view{body}.starts_with(kPrefix)) body.erase(0, kPrefix.size());
  const std::size_t close = body.rfind(')');
  if (close != std::string::npos) {
    std::size_t end = close + 1;
    while (end < body.size() && is_blank(body[end])) ++end;
    body.erase(close, end - close);
  }
  return body;
}

std::string TclBridge::evaluate(std::string_view value, const TclContext& context) const {
  if (interpreter_ == nullptr) return std::string{kTclFailure};
  // Step 3: the globals; instname and symname only for `@tcleval`.
  interpreter_->set_var("::path", cmd::Value{context.path});
  interpreter_->set_var("::sch_basename", cmd::Value{context.sch_basename});
  if (context.instname.empty() && context.symname.empty()) {
    (void)interpreter_->evaluate("unset -nocomplain ::instname ::symname");
  } else {
    interpreter_->set_var("::instname", cmd::Value{context.instname});
    interpreter_->set_var("::symname", cmd::Value{context.symname});
  }
  // Steps 4 and 5: the body of `subst {...}` in the global scope.
  if (run_ != nullptr) {
    run_->owner = !context.owner.empty()     ? context.owner
                  : !context.symname.empty() ? context.symname
                                             : context.instname;
  }
  return run_script("subst {" + tcleval_body(value) + "}");
}

// One evaluation under the run's limits: at most `each`, and at most what is
// left of `total`; once that is spent nothing runs (reported once).
std::string TclBridge::run_script(const std::string& script) const {
  using Clock = std::chrono::steady_clock;
  const bool limited = run_ != nullptr && run_->progress != nullptr;
  if (limited && run_->left <= std::chrono::milliseconds{0}) {
    if (!std::exchange(run_->spent_reported, true)) {
      report(diag::make("SCHEM-TCL-003", "tcleval values ran for their total of " + limit_text(run_->total) +
                                             "; no further value is evaluated in this run"));
    }
    return std::string{kTclFailure};
  }
  const Clock::time_point start = Clock::now();
  const std::chrono::milliseconds limit = limited ? std::min(run_->each, run_->left) : std::chrono::milliseconds{0};
  std::optional<detail::DeadlineHook> hook;
  if (limited) hook.emplace(*run_->progress, *interpreter_, start + limit);
  const cmd::CommandResult result = interpreter_->evaluate(script);
  const bool stopped = hook.has_value() && hook->expired() && !result.ok();
  hook.reset();
  if (limited) run_->left -= std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
  for (const diag::Diagnostic& d : result.diagnostics) {
    if (detail::is_sandbox_denial(d)) report(d);
  }
  if (stopped) report(diag::make("SCHEM-TCL-003", "tcleval value stopped after " + limit_text(limit)));
  return result.ok() ? result.value.to_string() : std::string{kTclFailure};
}

std::optional<std::string> TclBridge::global_variable(std::string_view name) const {
  if (interpreter_ == nullptr) return std::nullopt;
  const std::optional<cmd::Value> value = interpreter_->get_var("::" + std::string{name});
  if (!value) return std::nullopt;
  return value->to_string();
}

void TclBridge::report(diag::Diagnostic diagnostic) const {
  if (run_ != nullptr) report_tcleval(*run_, std::move(diagnostic));
}

void report_tcleval(const TclRun& run, diag::Diagnostic diagnostic) {
  if (run.sink == nullptr) return;
  if (!run.owner.empty()) diagnostic.text += " (tcleval of " + run.owner + ")";
  run.sink->emit(diagnostic);
}

}  // namespace hefesto::schem
