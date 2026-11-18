// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF write, scaffold stubs (openspec/changes/m6-lef-def-oasis/design.md D4).
//
// Owner: LEFW (design D2).

#include "hefesto/lef/write/write.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::lef {

cmd::CommandResult write(editor::EditorState& /*editor*/, db::CellDef& /*cell*/, const platform::Path& /*path*/,
                         const LefWriteOptions& /*options*/) {
  return cmd::not_implemented("lef write");
}

cmd::CommandResult write_all(editor::EditorState& /*editor*/, db::CellDef& /*root*/, const platform::Path& /*path*/,
                             const LefWriteOptions& /*options*/) {
  return cmd::not_implemented("lef writeall");
}

}  // namespace hefesto::lef
