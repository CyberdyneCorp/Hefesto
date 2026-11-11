// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The run-time side of the script sandbox (design D5), private to
// src/core/src/script/tcl/ (the only directory that includes tcl.h).
//
//   SandboxSession  the sandbox state of one project interpreter: its path
//                   decision, the denials the filesystem layer recorded and
//                   not yet reported, and the SCRIPT-SANDBOX-* diagnostics
//                   of the evaluation in progress.
//   ActiveSession   makes a session the current one of the calling thread
//                   while Tcl runs for its interpreter. Tcl's filesystem
//                   hooks get no interpreter, so the filesystem layer asks
//                   for the thread's current session; with none, every path
//                   outside the embedded library is denied.
//
// Reporting: the filesystem layer records a denial (EACCES to Tcl); the
// command that caused it (a guarded command, sandbox_commands.cpp) turns it
// into a Tcl error naming the operation, with location and event. Denials
// that no guarded command claimed are reported when the top-level
// evaluation ends.

#pragma once

#include <tcl.h>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/script/events.hpp"
#include "hefesto/script/tcl/sandbox.hpp"

namespace hefesto::script::tcl_detail {

/// A path access the filesystem layer refused.
struct PathDenial {
  std::string access;  // the filesystem call: "open", "stat", "chdir", ...
  std::string path;    // the normalized path
};

/// A refused operation, as reported to the script and the host.
struct Refusal {
  std::string_view code;  // SCRIPT-SANDBOX-*
  std::string operation;  // "exec", "open", "file delete", "cd", ...
  std::string target;     // the path or command words; may be empty
  std::string message;    // the Tcl error text
};

class SandboxSession {
 public:
  SandboxSession(Tcl_Interp* interp, const SandboxPolicy& policy, ScriptEvents* events);

  [[nodiscard]] Tcl_Interp* interp() const noexcept { return interp_; }
  [[nodiscard]] const SandboxFilesystem& filesystem() const noexcept { return filesystem_; }

  /// Called by the filesystem layer.
  void record(PathDenial denial);
  /// The number of recorded, unreported path denials (a mark for claim()).
  [[nodiscard]] std::size_t pending() const noexcept { return pending_.size(); }
  /// Removes and returns the denials recorded after `mark`.
  [[nodiscard]] std::vector<PathDenial> claim(std::size_t mark);

  /// Reports `refusal` at the command being executed: SCRIPT-SANDBOX-*
  /// diagnostic with the script location, DeniedOperation event, and the
  /// Tcl result set to the message. Returns TCL_ERROR.
  int refuse(const Refusal& refusal);
  /// Reports the denials recorded after `mark` that no guarded command
  /// claimed (a `source` call, the end of an evaluation): diagnostics and
  /// events only, located at `file` (line unknown); `operation` names them
  /// (empty: the filesystem call).
  void report_unclaimed(std::string_view operation, std::string_view file, std::size_t mark = 0);

  /// The diagnostics reported since `mark` (a size of diagnostics()).
  [[nodiscard]] std::vector<diag::Diagnostic> diagnostics_since(std::size_t mark) const;
  [[nodiscard]] std::size_t diagnostic_count() const noexcept { return diagnostics_.size(); }
  /// Forgets denials and diagnostics (after the interpreter's own setup).
  void reset() noexcept;

 private:
  void publish(const Refusal& refusal, const diag::SourceLocation& location);

  Tcl_Interp* interp_;
  SandboxFilesystem filesystem_;
  ScriptEvents* events_;
  std::vector<PathDenial> pending_;
  std::vector<diag::Diagnostic> diagnostics_;
};

class ActiveSession {
 public:
  explicit ActiveSession(SandboxSession& session);
  ~ActiveSession();
  ActiveSession(const ActiveSession&) = delete;
  ActiveSession& operator=(const ActiveSession&) = delete;

 private:
  SandboxSession* previous_;
};

/// The calling thread's current session, or null.
[[nodiscard]] SandboxSession* active_session() noexcept;

/// Registers the filesystem layer with Tcl (once per process; call after
/// Tcl_FindExecutable).
void register_filesystem();

/// The audit observer of set_filesystem_audit().
[[nodiscard]] std::function<void(const FilesystemAccess&)>& filesystem_audit();

/// The file and line of the command being executed in `interp` (Tcl's
/// `info frame 0`; `info script` when the frame has no file); empty file
/// and line 0 when unknown. Leaves the interpreter result unchanged.
[[nodiscard]] diag::SourceLocation current_location(Tcl_Interp* interp);

/// Installs the restricted commands of design D5 in `interp` (removed
/// commands, `open`, `interp`, `package`, and the path commands that report
/// filesystem denials).
void install_sandbox_commands(SandboxSession& session, const SandboxPolicy& policy);

/// Replaces `interp`'s `env` array by the synthesized one (D-31).
void synthesize_environment(Tcl_Interp* interp, const SandboxPolicy& policy);

/// Tcl's string length type for `text` (Tcl 8.6 measures in int bytes).
[[nodiscard]] int tcl_length(std::string_view text) noexcept;
/// The string form of `object`.
[[nodiscard]] std::string_view as_view(Tcl_Obj* object);

}  // namespace hefesto::script::tcl_detail
