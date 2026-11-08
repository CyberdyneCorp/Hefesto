// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/command_bar_model.hpp"

#include <algorithm>

namespace hefesto::shell {

std::string_view trim(std::string_view text) noexcept {
  constexpr std::string_view kBlanks = " \t\r\n";
  const std::size_t first = text.find_first_not_of(kBlanks);
  if (first == std::string_view::npos) {
    return {};
  }
  const std::size_t last = text.find_last_not_of(kBlanks);
  return text.substr(first, last - first + 1);
}

CommandBarModel::CommandBarModel(CommandSink& sink, std::size_t history_limit)
    : sink_(sink), limit_(std::max<std::size_t>(history_limit, 1)) {}

CommandOutcome CommandBarModel::submit(std::string_view text) {
  const std::string_view line = trim(text);
  if (line.empty()) {
    cursor_ = history_.size();
    return CommandOutcome{OutcomeStatus::ok, {}, {}};
  }
  if (history_.empty() || history_.back() != line) {
    history_.emplace_back(line);
    if (history_.size() > limit_) {
      history_.erase(history_.begin());
    }
  }
  cursor_ = history_.size();
  CommandOutcome outcome = sink_.submit(line);
  status_ = outcome.message.empty() ? std::string(outcome.ok() ? "ok" : "failed") : outcome.message;
  return outcome;
}

std::string CommandBarModel::previous() {
  if (history_.empty()) {
    return {};
  }
  if (cursor_ > 0) {
    --cursor_;
  }
  return history_[cursor_];
}

std::string CommandBarModel::next() {
  if (cursor_ + 1 >= history_.size()) {
    cursor_ = history_.size();
    return {};
  }
  ++cursor_;
  return history_[cursor_];
}

}  // namespace hefesto::shell
