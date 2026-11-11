// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Invocation and CommandResult: the two halves of every command call
// (capability hefesto-architecture, requirement "Single command API";
// capability hefesto-diagnostics, requirement "Command results and errors").

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/value.hpp"
#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::cmd {

/// Which front end issued the command. Recorded in the command log.
/// `internal`: issued by Hefesto itself, not by the user (e.g. the crash
/// backup timer's `crash save`); written to the session log but kept out of
/// the command history, so a replay never repeats it.
enum class Origin { cli, script, gui, key, touch, plugin, internal };

[[nodiscard]] std::string_view origin_name(Origin origin) noexcept;

struct Invocation {
  std::string name;
  std::vector<std::string> args;
  Origin origin = Origin::script;
  /// Where the command text came from, when it was read from a script.
  std::optional<diag::SourceLocation> source;

  friend bool operator==(const Invocation&, const Invocation&) = default;
};

enum class Status { ok, failed };

struct CommandResult {
  Status status = Status::ok;
  Value value;
  std::vector<diag::Diagnostic> diagnostics;
  /// A failure that raises a script error in both modes, not only in strict
  /// mode (e.g. a usage error that the spec says "SHALL raise a Tcl error").
  bool raises = false;
  /// How the command is logged and replayed when it differs from what was
  /// typed (Magic's TxRebuildCommand): a handler that read the pointer sets
  /// the explicit form, e.g. `box values 10i 10i 20i 20i` for `box move bl
  /// cursor`. The dispatcher writes it to the command log and passes it to
  /// its observers; nullopt: the typed command.
  std::optional<Invocation> logged_as;

  [[nodiscard]] bool ok() const noexcept { return status == Status::ok; }

  [[nodiscard]] static CommandResult success(Value value = {}) {
    CommandResult result;
    result.value = std::move(value);
    return result;
  }
  [[nodiscard]] static CommandResult failure(diag::Diagnostic error) {
    CommandResult result;
    result.status = Status::failed;
    result.diagnostics.push_back(std::move(error));
    return result;
  }
  /// A failure that aborts the calling script whatever its strict mode.
  [[nodiscard]] static CommandResult raised(diag::Diagnostic error) {
    CommandResult result = failure(std::move(error));
    result.raises = true;
    return result;
  }
};

/// Renders an invocation as one Tcl command line (the words as a Tcl list)
/// that the project's interpreter evaluates back into the same name and
/// arguments (used by the command log for replay).
[[nodiscard]] std::string format_command_line(const Invocation& invocation);

}  // namespace hefesto::cmd
