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
enum class Origin { cli, script, gui, key, touch, plugin };

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

  [[nodiscard]] bool ok() const noexcept { return status == Status::ok; }

  [[nodiscard]] static CommandResult success(Value value = {}) { return {Status::ok, std::move(value), {}, false}; }
  [[nodiscard]] static CommandResult failure(diag::Diagnostic error) {
    CommandResult result{Status::failed, {}, {}, false};
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

/// Renders an invocation as one command line that the script reader parses
/// back into the same name and arguments (used by the command log for replay).
[[nodiscard]] std::string format_command_line(const Invocation& invocation);

}  // namespace hefesto::cmd
