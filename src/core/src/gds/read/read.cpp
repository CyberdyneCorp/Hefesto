// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (openspec/changes/m5-mask-gds-cif/design.md D3): gds::read
// fails with CORE-NOTIMPL-005 until GDSR ports the reader (tasks.md
// section 9). Owner: GDSR.

#include "hefesto/gds/read/read.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::gds {

cmd::CommandResult read(editor::EditorState& /*editor*/, const platform::Path& /*path*/,
                        const GdsReadOptions& /*options*/, const mask::Progress* /*progress*/) {
  return cmd::not_implemented("gds read");
}

}  // namespace hefesto::gds
