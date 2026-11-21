// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: SCAFFOLD (removed at M8 exit with CORE-NOTIMPL-007).

#include "hefesto/schem/not_implemented.hpp"

#include <string>

#include "hefesto/diag/codes.hpp"

namespace hefesto::schem {

diag::Diagnostic not_implemented(std::string_view what) {
  return diag::make(diag::codes::kSchemNotImplemented, "not implemented yet: " + std::string{what});
}

}  // namespace hefesto::schem
