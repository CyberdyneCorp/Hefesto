// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/registry.hpp"

#include "hefesto/diag/codes.hpp"

namespace hefesto::cmd {

std::optional<diag::Diagnostic> CommandRegistry::add(CommandSpec spec) {
  std::string name = spec.name;
  const auto [it, inserted] = commands_.try_emplace(std::move(name), std::move(spec));
  if (!inserted) {
    return diag::make(diag::codes::kCmdRegistryDuplicate, "command '" + it->first + "' is already registered");
  }
  return std::nullopt;
}

const CommandSpec* CommandRegistry::find(std::string_view name) const {
  const auto it = commands_.find(name);
  return it == commands_.end() ? nullptr : &it->second;
}

std::vector<std::string> CommandRegistry::names() const {
  std::vector<std::string> out;
  out.reserve(commands_.size());
  for (const auto& kv : commands_) {
    out.push_back(kv.first);
  }
  return out;
}

}  // namespace hefesto::cmd
