// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: INTEGRATION (removed at M7 exit with CORE-NOTIMPL-008).

#include "hefesto/cmd/m7_stubs.hpp"

#include <string>

#include "hefesto/diag/codes.hpp"

namespace hefesto::cmd {

diag::Diagnostic m7_not_implemented_diagnostic(std::string_view what) {
  return diag::make(diag::codes::kExtractNotImplemented, "not implemented yet: " + std::string{what});
}

CommandResult m7_not_implemented(std::string_view what) {
  return CommandResult::failure(m7_not_implemented_diagnostic(what));
}

diag::Status m7_not_implemented_status(std::string_view what) {
  return m7_not_implemented_diagnostic(what);
}

}  // namespace hefesto::cmd
