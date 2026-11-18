// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF write, scaffold stub (openspec/changes/m6-lef-def-oasis/design.md D4).
//
// Owner: DEFW (design D2).

#include "hefesto/def/write/write.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::def {

cmd::CommandResult write(editor::EditorState& /*editor*/, db::CellDef& /*cell*/, const platform::Path& /*path*/,
                         const DefWriteOptions& /*options*/) {
  return cmd::not_implemented("def write");
}

}  // namespace hefesto::def
