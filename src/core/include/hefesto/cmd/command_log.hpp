// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Command logging (capability hefesto-architecture, requirement "Single
// command API": every action is loggable and replayable). The log keeps each
// executed command as a script line; feeding history_script() back through
// run_script() replays the session.

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/platform/logger.hpp"

namespace hefesto::cmd {

class CommandLog final : public CommandObserver {
 public:
  /// `logger` may be null; when set, each command is also written to it as
  /// "cmd[<origin>] <line> -> ok|failed". Commands with Origin::internal
  /// are written to the logger only, never to the history.
  explicit CommandLog(platform::Logger* logger = nullptr) : logger_(logger) {}

  void on_command(const Invocation& invocation, const CommandResult& result) override;

  /// Executed commands as script lines, in execution order.
  [[nodiscard]] std::vector<std::string> history() const;
  /// The history as one script (lines joined by '\n').
  [[nodiscard]] std::string history_script() const;

 private:
  platform::Logger* logger_;
  mutable std::mutex mutex_;
  std::vector<std::string> lines_;
};

}  // namespace hefesto::cmd
