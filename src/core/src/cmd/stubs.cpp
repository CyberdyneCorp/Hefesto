// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/stubs.hpp"

#include <string>

#include "hefesto/diag/codes.hpp"

namespace hefesto::cmd {

diag::Diagnostic not_implemented_diagnostic(std::string_view what) {
  return diag::make(diag::codes::kCoreNotImplemented, "not implemented yet: " + std::string{what});
}

CommandResult not_implemented(std::string_view what) {
  return CommandResult::failure(not_implemented_diagnostic(what));
}

diag::Status not_implemented_status(std::string_view what) {
  return not_implemented_diagnostic(what);
}

}  // namespace hefesto::cmd
