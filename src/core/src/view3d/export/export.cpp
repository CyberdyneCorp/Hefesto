// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// glTF and STL writers (openspec/changes/m10-3d-view/design.md D14, P2).
// Scaffold stubs (M10 task 2.3): nothing is written and no command calls
// them yet (`export3d` arrives with them). EXPORT replaces them in tasks
// 11.3 and 11.4.
//
// Owner: EXPORT (design D3).

#include "hefesto/view3d/export/export.hpp"

namespace hefesto::view3d {

diag::Status write_gltf(const MeshModel& /*mesh*/, const LayerStack& /*stack*/, const ExportOptions& /*options*/,
                        gds::ByteSink& /*out*/, diag::DiagnosticSink& /*sink*/) {
  return diag::Done{};
}

diag::Status write_stl(const MeshModel& /*mesh*/, const LayerStack& /*stack*/, const ExportOptions& /*options*/,
                       gds::ByteSink& /*out*/, diag::DiagnosticSink& /*sink*/) {
  return diag::Done{};
}

}  // namespace hefesto::view3d
