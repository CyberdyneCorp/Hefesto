// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cmd::PromptHost defaults (design.md D4). Owner: INTEGRATION.

#include "hefesto/cmd/prompt.hpp"

#include <array>
#include <string>

namespace hefesto::cmd {

std::size_t HeadlessPromptHost::ask(std::string_view, std::span<const std::string>, std::size_t default_answer) {
  return default_answer;
}

std::optional<std::size_t> HeadlessPromptHost::try_ask(std::string_view, std::span<const std::string>, std::size_t) {
  return std::nullopt;
}

std::size_t ask(PromptHost* host, std::string_view question, std::span<const std::string> answers,
                std::size_t default_answer) {
  if (host == nullptr) {
    return default_answer;
  }
  const std::size_t chosen = host->ask(question, answers, default_answer);
  return chosen < answers.size() ? chosen : default_answer;
}

std::optional<bool> try_confirm(PromptHost* host, std::string_view question) {
  static const std::array<std::string, 2> kNoYes = {"no", "yes"};
  const std::optional<std::size_t> answer = host != nullptr ? host->try_ask(question, kNoYes, 0) : std::nullopt;
  if (!answer) {
    return std::nullopt;
  }
  return *answer != 0;
}

bool confirm(PromptHost* host, std::string_view question, bool magic_compat) {
  return try_confirm(host, question).value_or(magic_compat);
}

}  // namespace hefesto::cmd
