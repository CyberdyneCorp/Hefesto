// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (m10-plot-route tasks.md 3.1): both writers report
// CORE-NOTIMPL-010 and write nothing until EXPORT (tasks 8.1, 8.2) lands
// them.
//
// Owner: EXPORT.

#include "hefesto/display/export/export.hpp"

#include "hefesto/plot/not_implemented.hpp"

namespace hefesto::display {

diag::Status export_svg(const LayoutScene& /*scene*/, const ExportOptions& /*options*/, std::ostream& /*out*/) {
  return plot::not_implemented("display::export_svg");
}

diag::Status export_png(const LayoutScene& /*scene*/, const ExportOptions& /*options*/, std::ostream& /*out*/) {
  return plot::not_implemented("display::export_png");
}

}  // namespace hefesto::display
