// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF read, scaffold stub (openspec/changes/m6-lef-def-oasis/design.md D4).
//
// Owner: LEFR (design D2).

#include "hefesto/lef/read/read.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::lef {

cmd::CommandResult read(editor::EditorState& /*editor*/, const platform::Path& /*path*/,
                        const LefReadOptions& /*options*/) {
  return cmd::not_implemented("lef read");
}

}  // namespace hefesto::lef
