// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/stubs.hpp"

#include <utility>

#include "hefesto/diag/codes.hpp"

namespace hefesto::cmd {

diag::Diagnostic not_implemented_diagnostic(std::string_view what) {
  return diag::make(diag::codes::kCoreNotImplemented, "not implemented yet: " + std::string{what});
}

CommandResult not_implemented(std::string_view what) {
  return CommandResult::failure(not_implemented_diagnostic(what));
}

CommandSpec stub_command(std::string name, std::string synopsis, std::string help) {
  CommandSpec spec{std::move(name), std::move(synopsis), std::move(help), {}};
  spec.handler = [command = spec.name](CommandContext&, const Invocation&) { return not_implemented(command); };
  return spec;
}

}  // namespace hefesto::cmd
