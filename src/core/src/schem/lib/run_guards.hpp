// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Guards around the evaluation of a script a schematic carries (an
// `xschemrc`, a `tcleval` value, a Tcl symbol generator), shared by schem/lib
// and schem/tcl (design D14; hefesto-security "Schematic script permission
// model"):
//   DeadlineHook   interrupts the running evaluation once a deadline has
//                  passed, through the interpreter's progress hook (an
//                  interrupt cannot be caught by the script, so `while 1 {}`
//                  and `catch` loops stop too);
//   is_sandbox_denial  tells the SCRIPT-SANDBOX-* diagnostics of an
//                  evaluation apart, so callers can forward them.
//
// Owner: SYMLIB.

#pragma once

#include <chrono>
#include <string_view>
#include <utility>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/progress.hpp"

namespace hefesto::schem::detail {

/// Installs a progress hook that interrupts the running evaluation once
/// `deadline` has passed, chained to the hook that was there; restores it.
class DeadlineHook {
 public:
  using Clock = std::chrono::steady_clock;

  DeadlineHook(script::ProgressReporting& progress, script::Interpreter& interp, Clock::time_point deadline)
      : progress_(progress), saved_(progress.handler) {
    progress_.handler = [this, &interp, deadline](const script::ScriptProgress& report) {
      if (saved_) saved_(report);
      if (!report.done && Clock::now() >= deadline) {
        expired_ = true;
        interp.request_interrupt();
      }
    };
  }
  ~DeadlineHook() { progress_.handler = std::move(saved_); }
  DeadlineHook(const DeadlineHook&) = delete;
  DeadlineHook& operator=(const DeadlineHook&) = delete;

  [[nodiscard]] bool expired() const noexcept { return expired_; }

 private:
  script::ProgressReporting& progress_;
  script::ProgressHandler saved_;
  bool expired_ = false;
};

/// True for the diagnostic of an operation the sandbox denied.
[[nodiscard]] inline bool is_sandbox_denial(const diag::Diagnostic& diagnostic) noexcept {
  return std::string_view{diagnostic.code}.starts_with("SCRIPT-SANDBOX-");
}

}  // namespace hefesto::schem::detail
