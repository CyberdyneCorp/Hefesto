// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Command registry: name -> CommandSpec. Commands are matched by exact name
// in M0; the registry keeps names sorted so that unique-prefix abbreviation
// (ui-command-dispatch) can be added in M1 without changing callers.

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"

namespace hefesto::cmd {

class CommandContext;

using Handler = std::function<CommandResult(CommandContext&, const Invocation&)>;

struct CommandSpec {
  std::string name;
  std::string synopsis;  // e.g. "setting get|set <name> [<value>]"
  std::string help;      // one paragraph
  Handler handler;
};

class CommandRegistry {
 public:
  /// Registers a command. A duplicate name is a programming error: the
  /// existing command is kept and CMD-REGISTRY-001 is returned.
  [[nodiscard]] std::optional<diag::Diagnostic> add(CommandSpec spec);

  [[nodiscard]] const CommandSpec* find(std::string_view name) const;

  /// Registered names in byte-wise order.
  [[nodiscard]] std::vector<std::string> names() const;

 private:
  std::map<std::string, CommandSpec, std::less<>> commands_;
};

}  // namespace hefesto::cmd
