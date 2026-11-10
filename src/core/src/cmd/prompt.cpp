// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cmd::PromptHost defaults (design.md D4). Owner: INTEGRATION.

#include "hefesto/cmd/prompt.hpp"

namespace hefesto::cmd {

std::size_t HeadlessPromptHost::ask(std::string_view, std::span<const std::string>, std::size_t default_answer) {
  return default_answer;
}

std::size_t ask(PromptHost* host, std::string_view question, std::span<const std::string> answers,
                std::size_t default_answer) {
  if (host == nullptr) {
    return default_answer;
  }
  const std::size_t chosen = host->ask(question, answers, default_answer);
  return chosen < answers.size() ? chosen : default_answer;
}

}  // namespace hefesto::cmd
