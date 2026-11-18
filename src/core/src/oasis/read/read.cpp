// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS read, scaffold stub (openspec/changes/m6-lef-def-oasis/design.md D4).
//
// Owner: OASR (design D2).

#include "hefesto/oasis/read/read.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::oasis {

cmd::CommandResult read(editor::EditorState& /*editor*/, const platform::Path& /*path*/,
                        const OasisReadOptions& /*options*/, const mask::Progress* /*progress*/) {
  return cmd::not_implemented("oasis read");
}

}  // namespace hefesto::oasis
