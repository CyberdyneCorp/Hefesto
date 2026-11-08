// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/command_log.hpp"

namespace hefesto::cmd {

void CommandLog::on_command(const Invocation& invocation, const CommandResult& result) {
  std::string line = format_command_line(invocation);
  if (logger_ != nullptr) {
    logger_->write(diag::Severity::info, "cmd[" + std::string{origin_name(invocation.origin)} + "] " + line +
                                             (result.ok() ? " -> ok" : " -> failed"));
  }
  const std::scoped_lock lock{mutex_};
  lines_.push_back(std::move(line));
}

std::vector<std::string> CommandLog::history() const {
  const std::scoped_lock lock{mutex_};
  return lines_;
}

std::string CommandLog::history_script() const {
  const std::scoped_lock lock{mutex_};
  std::string script;
  for (const auto& line : lines_) {
    script += line;
    script += '\n';
  }
  return script;
}

}  // namespace hefesto::cmd
