// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `tcleval(...)` and `@tcleval(...)` values in the sandboxed Tcl
// interpreter (openspec/changes/m8-schematic design.md D7, D14, "Area
// contracts" SYMLIB; capability schematic-symbols-and-libraries "tcleval
// values", "Script execution permission", Q-SCHEM-8, Q-SCHEM-25;
// hefesto-security "Schematic script permission model").
//
// Owner: SYMLIB.

#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::diag {
class DiagnosticSink;
}

namespace hefesto::script {
class Interpreter;
struct ProgressReporting;
}  // namespace hefesto::script

namespace hefesto::schem {

/// The value of a failed or refused evaluation: `?` followed by LF.
inline constexpr std::string_view kTclFailure = "?\n";

/// The Tcl globals set before an evaluation ("tcleval values" step 3 and the
/// `@tcleval` token).
struct TclContext {
  std::string path;          // hierarchical path without the leading `.`
  std::string sch_basename;  // base name of the schematic being netlisted
  std::string instname;      // `@tcleval` only: the instance name
  std::string symname;       // `@tcleval` only: the symbol reference
  /// What the value belongs to (`instance R1 (res.sym)`), named in the
  /// diagnostics of the evaluation; not a Tcl variable. Empty: `symname`,
  /// else `instname`, else nothing is named.
  std::string owner = {};
};

/// The longest one `tcleval` value may run, in both modes of `xschem-compat`
/// (xschem bounds none; the `xschemrc` limit of Q-SCHEM-32). An endless
/// value hung netlisting (security review, task 12.1).
inline constexpr std::chrono::milliseconds kTclevalTimeLimit{10'000};

/// The longest the `tcleval` values of one TclEnvironment (one netlist run)
/// may run in total; past it no value runs and each gives kTclFailure (as
/// the symbol generators share kGeneratorTotalTimeLimit).
inline constexpr std::chrono::milliseconds kTclevalTotalTimeLimit{60'000};

/// What the evaluations of one bridge share (TclEnvironment owns it): the
/// time limits, the progress hook they are enforced through, and where the
/// diagnostics of the evaluations go.
struct TclRun {
  /// The interpreter's progress hook; nullptr: no time limit.
  script::ProgressReporting* progress = nullptr;
  std::chrono::milliseconds each = kTclevalTimeLimit;
  std::chrono::milliseconds total = kTclevalTotalTimeLimit;
  std::chrono::milliseconds left = kTclevalTotalTimeLimit;
  /// Sandbox denials (SCRIPT-SANDBOX-*) and stops (SCHEM-TCL-003); nullptr
  /// drops them. Not owned.
  diag::DiagnosticSink* sink = nullptr;
  bool spent_reported = false;  // the total-time warning was given
  std::string owner;            // TclContext::owner of the value being evaluated
};

/// Reports `diagnostic` to `run.sink` (when set), naming `run.owner`.
void report_tcleval(const TclRun& run, diag::Diagnostic diagnostic);

/// Steps 1 and 2 of "tcleval values": every `\}` replaced by `}`, then the
/// leading `tcleval(` and the last `)` (with the blanks after it) removed.
[[nodiscard]] std::string tcleval_body(std::string_view value);

class TclBridge {
 public:
  /// `interpreter` is the sandboxed interpreter holding the inventoried
  /// procedures; nullptr makes every evaluation fail. `run` holds the time
  /// limits and the diagnostic sink (nullptr: no limit, diagnostics
  /// dropped). Neither is owned.
  explicit TclBridge(script::Interpreter* interpreter = nullptr, TclRun* run = nullptr) noexcept
      : interpreter_(interpreter), run_(run) {}

  /// Evaluates a value that begins with `tcleval(` ("tcleval values" steps
  /// 1-5): sets the globals `path` and `sch_basename` (and `instname` and
  /// `symname` when the context gives them, else unsets them), then runs the
  /// body as `subst {<body>}` at global level. kTclFailure on failure.
  /// Every operation the sandbox denies (also one the value catches) is
  /// reported to the run's sink, naming the owner; the evaluation stops
  /// after `run.each` or what is left of `run.total` (SCHEM-TCL-003).
  [[nodiscard]] std::string evaluate(std::string_view value, const TclContext& context) const;

 private:
  [[nodiscard]] std::string run_script(const std::string& script) const;
  void report(diag::Diagnostic diagnostic) const;

  script::Interpreter* interpreter_;
  TclRun* run_;
};

}  // namespace hefesto::schem
