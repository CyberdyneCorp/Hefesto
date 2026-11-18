// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF read, scaffold stub (openspec/changes/m6-lef-def-oasis/design.md D4).
//
// Owner: DEFR (design D2).

#include "hefesto/def/read/read.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::def {

cmd::CommandResult read(editor::EditorState& /*editor*/, const platform::Path& /*path*/,
                        const DefReadOptions& /*options*/) {
  return cmd::not_implemented("def read");
}

}  // namespace hefesto::def
