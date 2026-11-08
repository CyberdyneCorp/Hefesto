// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/command.hpp"

#include <array>

#include "hefesto/cmd/script.hpp"

namespace hefesto::cmd {

std::string_view origin_name(Origin origin) noexcept {
  constexpr std::array<std::string_view, 6> kNames = {"cli", "script", "gui", "key", "touch", "plugin"};
  return kNames.at(static_cast<std::size_t>(origin));
}

std::string format_command_line(const Invocation& invocation) {
  std::string line = quote_word(invocation.name);
  for (const auto& arg : invocation.args) {
    line += ' ';
    line += quote_word(arg);
  }
  return line;
}

}  // namespace hefesto::cmd
