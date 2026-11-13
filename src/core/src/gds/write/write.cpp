// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold (openspec/changes/m5-mask-gds-cif/design.md D3/D4): write()
// runs the two phases; prepare_write and emit_write fail with
// CORE-NOTIMPL-005 and merge_polygons answers nothing until GDSW ports
// CalmaWrite.c (tasks.md section 10). Owner: GDSW.

#include "hefesto/gds/write/write.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::gds {

diag::Outcome<PreparedWrite> prepare_write(editor::EditorState& /*editor*/, db::CellDef& /*root*/,
                                           const GdsWriteOptions& /*options*/) {
  return cmd::not_implemented_diagnostic("gds write");
}

cmd::CommandResult emit_write(const PreparedWrite& /*prepared*/, const platform::Path& /*path*/,
                              const mask::Progress* /*progress*/) {
  return cmd::not_implemented("gds write");
}

cmd::CommandResult write(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                         const GdsWriteOptions& options, const mask::Progress* progress) {
  diag::Outcome<PreparedWrite> prepared = prepare_write(editor, root, options);
  if (!prepared) {
    return cmd::CommandResult::failure(prepared.error());
  }
  return emit_write(prepared.value(), path, progress);
}

std::vector<mask::Polygon> merge_polygons(const mask::MaskPlane& /*plane*/) {
  return {};
}

}  // namespace hefesto::gds
