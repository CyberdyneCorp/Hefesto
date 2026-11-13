// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold (openspec/changes/m5-mask-gds-cif/design.md D3/D4): write() runs
// the two phases; prepare_write and emit_write fail with CORE-NOTIMPL-005
// until CIF ports CIFwrite.c (tasks.md section 11). Owner: CIF.

#include "hefesto/cif/write/write.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::cif {

namespace {

std::string_view command_name(const CifWriteOptions& options) {
  return options.flat ? "cif flat" : "cif write";
}

}  // namespace

diag::Outcome<PreparedWrite> prepare_write(editor::EditorState& /*editor*/, db::CellDef& /*root*/,
                                           const CifWriteOptions& options) {
  return cmd::not_implemented_diagnostic(command_name(options));
}

cmd::CommandResult emit_write(const PreparedWrite& prepared, const platform::Path& /*path*/,
                              const mask::Progress* /*progress*/) {
  return cmd::not_implemented(command_name(prepared.options));
}

cmd::CommandResult write(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                         const CifWriteOptions& options, const mask::Progress* progress) {
  diag::Outcome<PreparedWrite> prepared = prepare_write(editor, root, options);
  if (!prepared) {
    return cmd::CommandResult::failure(prepared.error());
  }
  return emit_write(prepared.value(), path, progress);
}

}  // namespace hefesto::cif
