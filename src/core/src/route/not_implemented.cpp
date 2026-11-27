// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: SCAFFOLD (removed at M10 exit with CORE-NOTIMPL-010).

#include "hefesto/route/not_implemented.hpp"

#include <string>

#include "hefesto/diag/codes.hpp"

namespace hefesto::route {

diag::Diagnostic not_implemented(std::string_view what) {
  return diag::make(diag::codes::kPlotRouteNotImplemented, "not implemented yet: " + std::string{what});
}

}  // namespace hefesto::route
