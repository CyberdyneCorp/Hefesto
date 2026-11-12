// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/command.hpp"

#include <array>
#include <utility>

#include "hefesto/cmd/value.hpp"

namespace hefesto::cmd {

std::string_view origin_name(Origin origin) noexcept {
  constexpr std::array<std::string_view, 7> kNames = {"cli", "script", "gui", "key", "touch", "plugin", "internal"};
  return kNames.at(static_cast<std::size_t>(origin));
}

// A Tcl list of the words is a Tcl command that calls them unchanged (Tcl's
// list quoting: braces, or backslashes when braces cannot hold the word).
std::string format_command_line(const Invocation& invocation) {
  Value::List words;
  words.reserve(invocation.args.size() + 1);
  words.emplace_back(invocation.name);
  for (const auto& arg : invocation.args) {
    words.emplace_back(arg);
  }
  return Value{std::move(words)}.to_string();
}

}  // namespace hefesto::cmd
