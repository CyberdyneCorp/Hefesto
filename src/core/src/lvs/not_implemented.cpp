// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: SCAFFOLD (removed at M9 exit with CORE-NOTIMPL-009).

#include "hefesto/lvs/not_implemented.hpp"

#include <string>

#include "hefesto/diag/codes.hpp"

namespace hefesto::lvs {

diag::Diagnostic not_implemented(std::string_view what) {
  return diag::make(diag::codes::kM9NotImplemented, "not implemented yet: " + std::string{what});
}

}  // namespace hefesto::lvs
