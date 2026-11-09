// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Behavior of the command bar, independent of the UI toolkit: trimming,
// history navigation and forwarding to a CommandSink. The Raven widget only
// shows text and calls these methods, so this part is unit-tested.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/shell/command_sink.hpp"

namespace hefesto::shell {

class CommandBarModel {
 public:
  explicit CommandBarModel(CommandSink& sink, std::size_t history_limit = 500);

  /// Trims `text`; ignores it when empty, otherwise records it in the history
  /// and sends it to the sink. Empty input returns an ok outcome with no message.
  CommandOutcome submit(std::string_view text);

  /// History navigation, as in a shell: previous() walks back from the newest
  /// entry, next() walks forward and returns "" past the newest one.
  [[nodiscard]] std::string previous();
  [[nodiscard]] std::string next();

  [[nodiscard]] const std::vector<std::string>& history() const noexcept { return history_; }
  /// Text for the status bar after the last command.
  [[nodiscard]] const std::string& status_text() const noexcept { return status_; }

 private:
  CommandSink& sink_;
  std::size_t limit_;
  std::vector<std::string> history_;
  std::size_t cursor_ = 0;  // == history_.size() when not navigating
  std::string status_;
};

/// Removes ASCII blanks (space, tab, CR, LF) at both ends.
[[nodiscard]] std::string_view trim(std::string_view text) noexcept;

/// Console lines for one command: "> <command>", then each line of its result
/// (a multi-line result such as `help` gives one line per row; empty: none).
[[nodiscard]] std::vector<std::string> console_lines(std::string_view command, std::string_view result);

/// The Console text of a command line: every diagnostic it raised, one per
/// line ("error: CODE: text", as Magic's console prints its messages), then
/// its result, which is left out when the line raised diagnostics and the
/// result is only "ok" or repeats one of them.
[[nodiscard]] std::string console_result(const CommandOutcome& outcome, std::string_view result);

}  // namespace hefesto::shell
